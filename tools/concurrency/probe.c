/* Isolated target-CRT feasibility probe. Never used by the application. */
#include <stdio.h>
#include <string.h>
#include <conio.h>

int main(int argc,char **argv) {
    if (argc==4 && !strcmp(argv[1],"rename"))
        return rename(argv[2],argv[3])?10:0;
    if (argc==2 && !strcmp(argv[1],"input")) {
        fputs("WAITING_FOR_GETCH\n",stderr);
        int c=getch();
        fprintf(stderr,"GETCH_RETURNED %d\n",c);
        return 0;
    }
    if (argc!=5) return 2;
    FILE *file=fopen(argv[2],argv[3]);
    if (!file) {fputs("OPEN_REJECTED\n",stderr);return 10;}
    if (fputs(argv[4],file)==EOF || fseek(file,0,SEEK_SET)) {
        fclose(file);return 3;
    }
    if (!strcmp(argv[1],"hold")) {
        fputs("OPEN_HELD\n",stderr);
        /* External test harness supplies a byte or kills this child only. */
        if (getchar()==EOF) {fclose(file);return 4;}
    } else if (strcmp(argv[1],"create")) {fclose(file);return 2;}
    return fclose(file)?3:0;
}
