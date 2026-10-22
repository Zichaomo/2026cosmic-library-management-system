#include "search/pinyin.h"
#include <stdio.h>

static int passed,failed;
static void check(int ok,const char *name) {
    printf("[%s] %s\n",ok?"PASS":"FAIL",name);
    if (ok) ++passed;else ++failed;
}
int main(void) {
    check(cl_search_match("深入理解计算机系统","深入"),"original Chinese substring");
    check(cl_search_match("深入理解计算机系统","shenrulijie"),"full spelling across Chinese characters");
    check(cl_search_match("深入理解计算机系统","srlj"),"Chinese initials");
    check(cl_search_match("深入理解计算机系统","SHEN RU LI JIE"),"case insensitive separated spelling");
    check(cl_search_match("深入理解计算机系统","理解jisuanji"),"Chinese and pinyin mixed query");
    check(cl_search_match("深入理解计算机系统","jisuan"),"match starts within the title");
    check(cl_search_match("C程序设计语言","cchengxusheji"),"ASCII and full Chinese spelling");
    check(cl_search_match("C程序设计语言","ccxsj"),"ASCII and Chinese initials");
    check(cl_search_match("Brian W. Kernighan","BRIAN"),"ASCII author case insensitive");
    check(cl_search_match("978-7-111-28069","9787111"),"hyphenated ISBN normalization");
    check(cl_search_match("西安","xi'an"),"syllable apostrophe separators");
    check(cl_search_match("重庆","chongqing"),"polyphonic alternative chong");
    check(cl_search_match("重量","zhongliang"),"polyphonic alternative zhong");
    check(cl_search_match("银行","yinhang"),"polyphonic hang for bank");
    check(cl_search_match("绿色","lvse") && cl_search_match("绿色","luse"),"umlaut supports v and unmarked u");
    check(!cl_search_match("算法导论","jisuanji"),"unrelated word does not match");
    check(!cl_search_match("计算#机","jsj"),"unknown punctuation does not bridge a match");
    check(!cl_search_match("计算机","jisuanki"),"no unrequested spelling correction");
    check(cl_search_match("计算机","") && cl_search_match("计算机","  "),"empty query retains all rows");
    check(!cl_search_match(NULL,"a") && !cl_search_match("a",NULL),"NULL is rejected");
    check(!cl_search_match("计算机","\xc0\xaf"),"malformed UTF-8 query rejected");
    char large[513];for (int i=0;i<512;++i) large[i]='a';large[512]=0;
    check(!cl_search_match("aaaa",large),"oversized query bounded without truncation");
    printf("\n%d passed, %d failed\n",passed,failed);return failed?1:0;
}
