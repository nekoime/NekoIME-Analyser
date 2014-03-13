/* 
 * File:   main.c
 * Author: zh99998
 *
 * Created on 2014年3月9日, 上午9:18
 */

#include <stdio.h>
#include <stdbool.h> //bool
#include <assert.h> //assert
#include <string.h> //memset, memcpy
#include <math.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <iconv.h>
#include <opencc/opencc.h>

#include "cp_strlen_utf8.c"
#include "word_statistics.h"

//配置
const int max_length = 6; //最大长度
const int min_frequencies = 20; //最小词频
const double min_cohesion = 300; //最小凝固度, 参考 http://www.matrix67.com/blog/archives/5044
const double min_entropy = 1; //自由程度， 同上

const size_t utf8_max_char_length = 6; //UTF-8 编码一个字符最大长度

int suffix_compare(const ucs4_t **ptr1, const ucs4_t **ptr2) {
    //偷个懒用双字节比较, 反正都是中文应该没问题的吧..
    return memcmp(*ptr1, *ptr2, (max_length+1) * sizeof (ucs4_t));
}
int suffix_compare_left(const ucs4_t **ptr1, const ucs4_t **ptr2){
    for(int i=0; i<=max_length; i++){
        if(*((*ptr1) - i) < *((*ptr2) - i)){
            return -1;
        }else if(*((*ptr1) - i) > *((*ptr2) - i)){
            return 1;
        }
    }
    return 0;
}
int word_compare(const word *key, const word_statistics *elem){
    //同上
    return memcmp(key->start, elem->word, key->length * sizeof (ucs4_t));
}

int main(int argc, char** argv) {  
    
    if (argc == 1) {
        puts("Usage: nekoime-analyser files");
        return (EXIT_FAILURE);
    }
    
    iconv_t iconv_input = iconv_open("UTF-32//IGNORE", " UTF-8");
    iconv_t iconv_output = iconv_open("UTF-8//IGNORE", "UTF-32");
    opencc_t opencc = opencc_open(OPENCC_DEFAULT_CONFIG_TRAD_TO_SIMP);

    int total_count = 0;
    int articles_count = argc - 1;
    size_t articles_length[articles_count];
    ucs4_t * articles[articles_count];

    for (int i = 1; i < argc; i++) { //i: 文件
        uint8_t * file;
        int origin_count;
        int count;
        struct stat sb;
        int fd = open(argv[i], O_RDONLY);
        fstat(fd, &sb); // 取得文件大小

        file = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file == MAP_FAILED) // 判断是否映射成功 
            return EXIT_FAILURE;

        // 转换 UTF-8 到 UTF-32
        origin_count = cp_strlen_utf8(file);
        size_t inbytesleft = sb.st_size;
        size_t outbytesleft = origin_count * sizeof (ucs4_t);
        size_t conv_result;
        ucs4_t *article_origin = malloc(outbytesleft);
        conv_result = iconv(iconv_input, (char**) &file, &inbytesleft, (char**) &article_origin, &outbytesleft);
        file -= sb.st_size;
        article_origin -= origin_count;

        // 去除非中文字符
        ucs4_t *article_cleaned = malloc(origin_count * sizeof (ucs4_t));
        ucs4_t *article_cleaned_current = article_cleaned;
        count = 0;
        for (int j = 0; j < origin_count; j++) { //j: 在原始文章中的序号
            if ((article_origin[j] >= 0x4E00) && (article_origin[j] <= 0x9FA5)) {
                *article_cleaned_current = article_origin[j];
                article_cleaned_current++;
                count++;
            }
        }
        total_count += count;
        free(article_origin);

        // 繁体转简体
        ucs4_t *article = malloc(count * sizeof (ucs4_t));
        size_t inbufleft = count;
        size_t outbufleft = count;
        opencc_convert(opencc, &article_cleaned, &inbufleft, &article, &outbufleft);
        article_cleaned -= count - inbufleft;
        article -= count - outbufleft;
        free(article_cleaned);
        articles[i - 1] = article;
        articles_length[i - 1] = count;
    }

    ucs4_t **sorted_index = malloc(total_count * sizeof (ucs4_t*));
    ucs4_t **sorted_index_left = malloc(total_count * sizeof (ucs4_t*));
    ucs4_t **sorted_index_current = sorted_index;    
    for (int i = 0; i < articles_count; i++) { //i: 文章
        for (int j = 0; j < articles_length[i]; j++) { //j: 文章中的字
            *sorted_index_current = articles[i] + j;
            *sorted_index_current++;
        }
    }
    memcpy(sorted_index_left, sorted_index, total_count * sizeof (ucs4_t*));
    qsort(sorted_index, total_count, sizeof (size_t), (int (*)(const void *, const void *))suffix_compare);
    qsort(sorted_index_left, total_count, sizeof (size_t), (int (*)(const void *, const void *))suffix_compare_left);
    word_statistics * words[max_length];
    word_statistics * words_current[max_length];
    size_t word_statistics_count[max_length];
    int *right_neighbors[max_length];
    int *left_neighbors[max_length];
    int *right_neighbors_current[max_length];
    int *left_neighbors_current[max_length];
    for (int i = 0; i < max_length; i++) { //i: 词长-1
        left_neighbors_current[i] = left_neighbors[i] = malloc(total_count * sizeof (int));
        *left_neighbors_current[i] = 0;
        right_neighbors_current[i] = right_neighbors[i] = malloc(total_count * sizeof (int));
        *right_neighbors_current[i] = 0;
        words_current[i] = words[i] = malloc(total_count * sizeof (word_statistics));
        words_current[i]->word = sorted_index[0];
        words_current[i]->count = 0;
        words_current[i]->left_neighbors = left_neighbors_current[i];
        words_current[i]->right_neighbors = right_neighbors_current[i];
    }
    ucs4_t *last_word = sorted_index[0]; //因为计算长度 max_length 的词的邻字时已经没有 words_current 可用了
    for (int i = 0; i < total_count; i++) { //i: 字
        bool different = false;
        for (int j = 0; j < max_length; j++) { //j:  词长-1
            if (different || (*(sorted_index[i] + j) != *(words_current[j]->word + j))) {
                if (j != 0) {
                    right_neighbors_current[j - 1]++;
                    (*right_neighbors_current[j - 1]) = 1;
                }
                if (!different) {
                    different = true;
                }
                words_current[j]++;
                words_current[j]->word = sorted_index[i];
                words_current[j]->count = 1;
                //因为遍历到下一个长度的时候right_neighbors_current[j]才会++, 现在这里还是旧词的邻字, 所以要+1
                words_current[j]->right_neighbors = right_neighbors_current[j]+1;
            } else {
                words_current[j]->count++;
                if (j != 0) {
                    (*right_neighbors_current[j - 1])++;
                }
            }
        }
        // 长度 max_length 的词的邻字
        if (different || (*(sorted_index[i] + max_length) != *(last_word + max_length))) {
            last_word = sorted_index[i];
            right_neighbors_current[max_length - 1]++;
            (*right_neighbors_current[max_length - 1]) = 1;
        }else{
            (*right_neighbors_current[max_length - 1])++;
        }
    }
    
    for (int i = 0; i < max_length; i++){
        word_statistics_count[i] = words_current[i] - words[i] + 1;
        words_current[i] = NULL;
    }
    
    //左邻字
    last_word = sorted_index_left[0];
    for (int i = 0; i < total_count; i++) { //i: 字
        bool different = false;
        for (int j = 0; j < max_length; j++) { //j:  词长-1
            if (different || (words_current[j] == NULL) || (*(sorted_index_left[i] - j) != *(words_current[j]->word))) {
                if (j != 0 ) {
                    left_neighbors_current[j - 1]++;
                    (*left_neighbors_current[j - 1]) = 1;
                }
                if (!different) {
                    different = true;
                }
                word word;
                word.start = sorted_index_left[i] - j;
                word.length = j+1;
                words_current[j] = bsearch(&word, words[j], word_statistics_count[j], sizeof(word_statistics),  (int (*)(const void *, const void *))word_compare);
                if(words_current[j] == NULL){ //涉及到边界问题
                    break;  
                }
                words_current[j]->left_neighbors = left_neighbors_current[j]+1;
            }else{
                if (j != 0) {
                    (*left_neighbors_current[j - 1])++;
                }
            }
        }
        // 长度 max_length 的词的邻字
        if (different || (*(sorted_index_left[i] - max_length) != *(last_word - max_length))) {
            last_word = sorted_index_left[i];
            left_neighbors_current[max_length - 1]++;
            (*left_neighbors_current[max_length - 1]) = 1;
        }else{
            (*left_neighbors_current[max_length - 1])++;
        }
    }
    
    
    //words_current[i]
    //开始计算!
    
    uint8_t *output = malloc((max_length*utf8_max_char_length+1) * sizeof(uint8_t));
    for (int i = 1; i < max_length; i++) { //i: 词长-1
        for(int j=0; j<word_statistics_count[i]; j++){

            //词频
            if(words[i][j].count < min_frequencies){
                continue;
            }
            
            //右邻字信息熵
            double right_entropy = 0;
            right_neighbors_current[i] = words[i][j].right_neighbors;
            for(int count=words[i][j].count; count>0; right_neighbors_current[i]++){
                double p = (double)*right_neighbors_current[i] / (words[i][j].count);
                right_entropy += -log(p) * p;
                count -= *right_neighbors_current[i];
            }
            if(right_entropy < min_entropy){
                continue;
            }
            
            //左邻字信息熵
            double left_entropy = 0;
            left_neighbors_current[i] = words[i][j].left_neighbors;
            for(int count=words[i][j].count; count>0;left_neighbors_current[i]++){
                double p = (double)*left_neighbors_current[i] / (words[i][j].count);
                left_entropy += -log(p) * p;
                count -= *left_neighbors_current[i];                
            }
            if(left_entropy < min_entropy){
                continue;
            }

            //凝固
            bool flags[i]; //true = 新开一个词; false = 连在最后一个词上
            memset(flags, false, i-1);
            flags[i-1]=true;
            double best_p = 0;
            while(true){ //分隔词
                word word = {words[i][j].word, 1};
                double p = 1;
                for(int k = 0; k < i; k++) //k: 分隔词里的字
                {
                    if(!flags[k]){
                        word.length++;
                    }else{
                        word_statistics *statistics = bsearch(&word, words[word.length-1], word_statistics_count[word.length-1], sizeof(word_statistics),  (int (*)(const void *, const void *))word_compare);
                        assert(statistics); 
                        p *= (double)(statistics->count) / total_count;
                        word.start += word.length;
                        word.length = 1;
                    }
                }
                word_statistics *statistics = bsearch(&word, words[word.length-1], word_statistics_count[word.length-1], sizeof(word_statistics),  (int (*)(const void *, const void *))word_compare);
                assert(statistics); 
                p *= (double)(statistics->count) / total_count;
                if(p > best_p){
                    best_p = p;
                }
                int l = i-1;
                while(l >= 0)
                {
                    if(flags[l] == false)   // have we tried all possible status of l?
                    {
                        flags[l] = true;    // status switch, as we only have 2 status here, I use a boolean rather than an array.
                        break;          // break to output the updated status, and further enumeration for this status change, just like a recursion
                    }
                    else                // We have tried all possible cases for l-th number, now let's consider the previous one
                    {
                        flags[l] = false;   // resume l to its initial status
                        l--;            // let's consider l-1
                    }
                }

                if(l < 0) break;     // all the numbers in the set has been processed, we are done!
            }
            double cohesion = (double)(words[i][j].count) / total_count / best_p;
            if(cohesion < min_cohesion){
                continue;
            }
            
            // 转换 UTF-32 到 UTF-8
            size_t inbytesleft = (i+1) * sizeof(ucs4_t);
            size_t outbytesleft = (max_length*utf8_max_char_length+1) * sizeof(uint8_t);
            size_t conv_result;
            conv_result = iconv(iconv_output, (char**) &(words[i][j].word), &inbytesleft, (char**) &output, &outbytesleft);
            assert(inbytesleft == 0);
            words[i][j].word -= i+1;
            *output = '\0';
            output -= (max_length*utf8_max_char_length+1) - outbytesleft / sizeof(uint8_t);
            
            printf("%s %d %g %g\n", output, words[i][j].count, cohesion, left_entropy < right_entropy ? left_entropy : right_entropy);
        }
    }
    
    return (EXIT_SUCCESS);
}