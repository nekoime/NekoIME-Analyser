#! /usr/bin/env coffee

fs = require 'fs'

OpenCC = require('opencc');
opencc = new OpenCC('t2s.json');

_ = require('underscore');
_.str = require('underscore.string');
_.mixin(_.str.exports());


max_length = 6 #最大长度
#min_frequencies = 20 #最小词频改成 Math.log total_length 了，不同规模的输入不好统一词频..
min_cohesion = 300 #最小凝固度, 参考 http://www.matrix67.com/blog/archives/5044
min_entropy = 1 #自由程度， 同上

#求一个词的可能的组合，只想到了递归实现，于是写成函数了
#"喵帕斯" => ["喵", "帕", "斯"], ["喵帕", "斯"], ["喵", "帕斯"]
combinations = (word, allow_whole = false)->
  result = []
  for i in [1..(if allow_whole then word.length else word.length - 1)]
    first_part = word.slice(0, i)
    if i == word.length
      result.push [word]
    else
      rest_part = combinations word.slice(i), true
      for s in rest_part
        s.unshift(first_part)
        result.push s
  result

if process.argv[0]
  console.error "load #{process.argv.length - 2} files"
  words = {} #{word: {count, left_neibors, right_neibors}}
  chars = {} #单字
  total_length = 0
  for file in process.argv.slice(2)
    article = fs.readFileSync file, encoding: 'utf8'
    article = article.replace /[^\u4E00-\u9FA5]+/g, ''
    continue if article.length == 0
    article = opencc.convertSync(article) #如果要禁用繁简转换，注释掉这一行
    total_length += article.length
    console.log '单字统计'
    time = new Date()
    for char in article
      if chars[char]
        chars[char]++
      else
        chars[char] = 1
    console.log new Date() - time


    console.log '右邻字及词频'
    time = new Date()
    #for index in _.sortBy([0..article.length-2], (index)->article.substring(index))
    #  w = article.substring(index, max_length)
    #  for length in [2...w.length]
    #    word = w.substring(0, length)
    #    right_neibor = article[index+length]
    #    statistics = words[word]
    #    if statistics
    #      statistics.count++
    #      statistics.right_neibors[right_neibor]++
    #    else
    #      words[word] = { count: 1, left_neibors: {}, right_neibors: {right_neibor : 1}}
    words = {}

    for index in [0..article.length-2]
      for length in [1..max_length]
        word = article.substr(index, length)
        statistics = words[word]
        if statistics
          statistics.count++
        else
          statistics = words[word] = {count: 1, left_neibors:{}, right_neibors: {}}
        left_neibor = article[index-1]
        if statistics.left_neibors[left_neibor]
          statistics.left_neibors[left_neibor]++
        else
          statistics.left_neibors[left_neibor] = 1

        right_neibor = article[index+length]
        if statistics.right_neibors[right_neibor]
          statistics.right_neibors[right_neibor]++
        else
          statistics.right_neibors[right_neibor] = 1


    console.log new Date() - time

  #重算一下全由一个字构成的词的词频，因为上面遍历出来的会导致有些词的词频增加，例如 "啊啊啊啊啊啊"应该只包含 2 个"啊啊啊"，但是按上面的方法拆分之后变成 4 个了

  #for word of words when words.length >= 2 and _.count(words, words[0]) == words.length
  #  words[word] = _.reduce sentences, (memo, sentence)->
  #    memo + _.count(sentence, word)
  #  , 0
  #console.error "load #{_.size(words)} words"

  min_frequencies = Math.log total_length

  #作为基础词库，已有的词就不再列出了
  #词库取自sunpinyin
  sysdict = _.lines fs.readFileSync('sysdict.txt', encoding: 'utf8')
  console.error "load #{sysdict.length} system words"



  for word, statistics of words when statistics.count >= min_frequencies
    #去除已知词库中的词
    #continue if word in sysdict

    #凝固度
    continue if word.length <= 2 # != "东山奈央东山"
    #候选词在语料中出现的概率
    p = statistics.count / total_length

    #候选词的组成部分在语料中出现的概率
    p_combined = Math.max.apply this, (for combination in combinations word
      _.reduce combination, (memo, part)->
        memo * (if part.length == 1 then chars[part] else words[part].count) / total_length
      , 1)

    cohesion = p / p_combined

    continue if cohesion < min_cohesion

    #自由度


    left_entropy = _.reduce statistics.left_neibors, (memo, c)->
      memo + -Math.log(c / statistics.count) * c / statistics.count
    , 0

    continue if left_entropy < min_entropy

    right_entropy = _.reduce statistics.right_neibors, (memo, c)->
      memo + -Math.log(c / statistics.count) * c / statistics.count
    , 0
    #console.log '-----------', statistics.right_neibors, parseInt(cohesion), right_entropy, left_entropy
    continue if right_entropy < min_entropy

    console.log word, statistics.count, parseInt(cohesion), left_entropy, right_entropy

  console.error 'done'

else
  console.log 'Usage: node analyser.js files'