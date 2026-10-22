#include <stdio.h>
#include <string.h>

static void device(const char *mode,const char *label,const char *sample,int method) {
    FILE *screen=fopen("CONOUT$",mode);
    if (!screen) {printf("PROBE %s %s method=%d OPEN_FAILED\n",mode,label,method);return;}
    int failed=0;size_t wanted=strlen(sample),written=0;
    if (!method) {written=fwrite(sample,1,wanted,screen);failed=written!=wanted;}
    else if (method==1) {failed=fputs(sample,screen)==EOF;}
    else {for (size_t i=0;i<wanted;++i) if (fputc((unsigned char)sample[i],screen)==EOF) {failed=1;break;}}
    int error=ferror(screen),closed=fclose(screen);
    printf("PROBE %s %s method=%d wanted=%llu written=%llu failed=%d error=%d close=%d\n",
        mode,label,method,(unsigned long long)wanted,(unsigned long long)written,failed,error,closed);
}
int main(void) {
    puts("CONSOLE PROBE v1 (0=fwrite, 1=fputs, 2=fputc). No library data accessed.");
    for (int method=0;method<3;++method) {
        device("wb","ASCII","ASCII output test\n",method);
        device("wb","UTF8","Chinese: 中文输出测试\n",method);
        device("wb","BOX","Border: ──────────│┌┐└┘\n",method);
        device("wb","ANSI","\033[38;2;78;78;78mColor: 中文───\033[0m\n",method);
        device("w","ASCII","ASCII output test\n",method);
        device("w","UTF8","Chinese: 中文输出测试\n",method);
        device("w","BOX","Border: ──────────│┌┐└┘\n",method);
        device("w","ANSI","\033[38;2;78;78;78mColor: 中文───\033[0m\n",method);
    }
    int ok=fputs("stderr UTF8: 中文输出测试\n",stderr)!=EOF,error=ferror(stderr);
    clearerr(stderr);printf("PROBE stderr fputs ok=%d error=%d\n",ok,error);
    FILE *file=fopen("build/console-probe.txt","wb");
    if (!file) puts("PROBE ordinary-file OPEN_FAILED");
    else {int a=fputs("Chinese: 中文输出测试\n",file),b=ferror(file),c=fclose(file);
        printf("PROBE ordinary-file fputs=%d error=%d close=%d\n",a,b,c);}
    puts("END PROBE");return 0;
}
