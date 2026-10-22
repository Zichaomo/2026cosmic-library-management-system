#ifndef COSMIC_PINYIN_SEARCH_H
#define COSMIC_PINYIN_SEARCH_H
/* UTF-8 substring/full-pinyin/initials matching, ASCII case insensitive.
   Spaces, apostrophes and hyphens separate spelling; no edit-distance guesses.
   A query may mix Chinese, ASCII, full syllables and initials. */
int cl_search_match(const char *text,const char *query);
#endif
