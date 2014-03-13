/* 
 * File:   word_statistics.h
 * Author: zh99998
 *
 * Created on 2014年3月9日, 上午9:20
 */

#ifndef WORD_STATISTICS_H
#define	WORD_STATISTICS_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct{
    ucs4_t *word;
    int count;
    int *left_neighbors;
    int *right_neighbors;
} word_statistics;

typedef struct{
    ucs4_t *start;
    size_t length;
} word;

#ifdef	__cplusplus
}
#endif

#endif	/* WORD_STATISTICS_H */

