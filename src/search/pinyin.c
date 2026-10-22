#include "search/pinyin.h"
#include "utils/foundation.h"

typedef struct {unsigned cp;const char *readings;} ClReading;
#include "search/pinyin_data.h"

static int separator(unsigned cp) {return cp==' ' || cp=='\t' || cp=='\'' || cp=='-';}
static char folded(char c) {return c>='A' && c<='Z'?(char)(c-'A'+'a'):c;}
static const char *lookup(unsigned cp) {
    int left=0,right=(int)(sizeof(cl_readings)/sizeof(cl_readings[0]));
    while (left<right) {
        int mid=left+(right-left)/2;
        if (cl_readings[mid].cp<cp) left=mid+1;else right=mid;
    }
    return left<(int)(sizeof(cl_readings)/sizeof(cl_readings[0])) && cl_readings[left].cp==cp?cl_readings[left].readings:NULL;
}
static int advance(const char *q,int n,int start,const char *token,int length,unsigned char next[512]) {
    int i=0;
    while (i<length && start+i<n && q[start+i]==token[i]) ++i;
    if (start+i==n) return 1;
    if (i==length) next[start+i]=1;
    return 0;
}
int cl_search_match(const char *text,const char *query) {
    char q[512];int n=0;
    unsigned char states[512]={0},next[512]={0};
    if (!text || !query) return 0;
    for (const char *p=query;*p;) {
        unsigned cp;int bytes=utf8_decode(p,&cp);if (bytes<1) return 0;
        if (!separator(cp)) {
            if (n+bytes>=512) return 0;
            for (int j=0;j<bytes;++j) q[n++]=folded(p[j]);
        }
        p+=bytes;
    }
    q[n]=0;if (!n) return 1;
    for (const char *p=text;*p;) {
        unsigned cp;int bytes=utf8_decode(p,&cp);if (bytes<1) return 0;
        if (separator(cp)) {p+=bytes;continue;}
        for (int j=0;j<=n;++j) next[j]=0;
        states[0]=1; /* A substring may start at every source character. */
        char literal[5];for (int j=0;j<bytes;++j) literal[j]=folded(p[j]);literal[bytes]=0;
        const char *options=lookup(cp);
        for (int j=0;j<n;++j) if (states[j]) {
            if (advance(q,n,j,literal,bytes,next)) return 1;
            for (const char *r=options;r && *r;) {
                int length=0;while (r[length] && r[length]!='|') ++length;
                if (advance(q,n,j,r,length,next) || advance(q,n,j,r,1,next)) return 1;
                r+=length;if (*r=='|') ++r;
            }
        }
        for (int j=0;j<=n;++j) states[j]=next[j];
        p+=bytes;
    }
    return 0;
}
