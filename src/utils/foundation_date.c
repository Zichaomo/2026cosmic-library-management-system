#include "utils/foundation.h"
#include <string.h>
#include <time.h>

_Static_assert(sizeof(time_t)==8 && (time_t)-1<(time_t)0 &&
               (time_t)0.5==(time_t)0, "signed integral 64-bit time_t required");

static int leap(int year) { return year%4==0 && (year%100!=0 || year%400==0); }
static int month_days(int year,int month) {
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1]+(month==2 && leap(year));
}
static int before_year(int year) {
    int y=year-1;return y*365+y/4-y/100+y/400;
}
int cl_date_valid(ClDate d) {
    return d.year>=1 && d.year<=9999 && d.month>=1 && d.month<=12 &&
           d.day>=1 && d.day<=month_days(d.year,d.month);
}
int cl_date_serial(ClDate d,int *serial) {
    if (!serial || !cl_date_valid(d)) return 0;
    int result=before_year(d.year)+d.day-1;
    for (int m=1;m<d.month;++m) result+=month_days(d.year,m);
    *serial=result;return 1;
}
int cl_date_from_serial(int serial,ClDate *out) {
    if (!out || serial<0 || serial>=before_year(10000)) return 0;
    int low=1,high=9999;
    while (low<high) {
        int mid=low+(high-low+1)/2;
        if (before_year(mid)<=serial) low=mid;else high=mid-1;
    }
    ClDate result={low,1,1};int remaining=serial-before_year(low);
    while (remaining>=month_days(low,result.month))
        remaining-=month_days(low,result.month++);
    result.day=remaining+1;*out=result;return 1;
}
int cl_date_add(ClDate d,int days,ClDate *out) {
    int serial;
    if (!out || !cl_date_serial(d,&serial) || days < -serial ||
        days>before_year(10000)-1-serial) return 0;
    return cl_date_from_serial(serial+days,out);
}
int cl_date_difference(ClDate later,ClDate earlier,int *days) {
    int a,b;
    if (!days || !cl_date_serial(later,&a) || !cl_date_serial(earlier,&b)) return 0;
    *days=a-b;return 1;
}
static int digits(const char *s,int n,int *out) {
    int result=0;
    for (int i=0;i<n;++i) {
        if (s[i]<'0' || s[i]>'9') return 0;
        result=result*10+s[i]-'0';
    }
    *out=result;return 1;
}
int cl_date_parse(const char *text,ClDate *out) {
    ClDate d;
    if (!text || !out || strlen(text)!=10 || text[4]!='-' || text[7]!='-' ||
        !digits(text,4,&d.year) || !digits(text+5,2,&d.month) ||
        !digits(text+8,2,&d.day) || !cl_date_valid(d)) return 0;
    *out=d;return 1;
}
static void write_digits(char *out,int width,int value) {
    for (int i=width-1;i>=0;--i) { out[i]=(char)('0'+value%10);value/=10; }
}
int cl_date_format(char *out,int capacity,ClDate d) {
    if (!out || capacity<11 || !cl_date_valid(d)) return 0;
    write_digits(out,4,d.year);out[4]='-';write_digits(out+5,2,d.month);
    out[7]='-';write_digits(out+8,2,d.day);out[10]=0;return 1;
}
static int datetime_valid(ClDateTime t) {
    return cl_date_valid(t.date) && t.hour>=0 && t.hour<24 &&
           t.minute>=0 && t.minute<60 && t.second>=0 && t.second<60;
}
int cl_datetime_parse(const char *text,ClDateTime *out) {
    char date[11];ClDateTime t;
    if (!text || !out || strlen(text)!=19 || text[10]!=' ' ||
        text[13]!=':' || text[16]!=':') return 0;
    for (int i=0;i<10;++i) date[i]=text[i];
    date[10]=0;
    if (!cl_date_parse(date,&t.date) || !digits(text+11,2,&t.hour) ||
        !digits(text+14,2,&t.minute) || !digits(text+17,2,&t.second) ||
        !datetime_valid(t)) return 0;
    *out=t;return 1;
}
int cl_datetime_format(char *out,int capacity,ClDateTime t) {
    if (!out || capacity<20 || !datetime_valid(t)) return 0;
    cl_date_format(out,capacity,t.date);out[10]=' ';
    write_digits(out+11,2,t.hour);out[13]=':';write_digits(out+14,2,t.minute);
    out[16]=':';write_digits(out+17,2,t.second);out[19]=0;return 1;
}
int cl_datetime_from_unix(long long seconds,ClDateTime *out) {
    /* Validate before adding UTC+8; also guards overflow for extreme inputs. */
    if (!out || seconds < -62135625600ll || seconds > 253402271999ll) return 0;
    long long local=seconds+28800ll;
    long long day=local/86400ll,remainder=local%86400ll;
    if (remainder<0) { --day;remainder+=86400ll; }
    ClDateTime t;
    if (!cl_date_from_serial((int)(day+719162ll),&t.date)) return 0;
    t.hour=(int)(remainder/3600);t.minute=(int)(remainder%3600/60);
    t.second=(int)(remainder%60);*out=t;return 1;
}
int cl_datetime_to_unix(ClDateTime t,long long *seconds) {
    int serial;
    if (!seconds || !datetime_valid(t) || !cl_date_serial(t.date,&serial)) return 0;
    *seconds=((long long)serial-719162ll)*86400ll+t.hour*3600+t.minute*60+t.second-28800ll;
    return 1;
}
int cl_datetime_now(ClDateTime *out) {
    time_t now=time(NULL);
    return now!=(time_t)-1 && cl_datetime_from_unix((long long)now,out);
}
