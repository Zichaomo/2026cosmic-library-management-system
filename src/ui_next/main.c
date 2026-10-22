#include "ui_next/app.h"
#include <string.h>

int main(int argc,char **argv) {
    const char *directory=NULL;int seed=0;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--data") && i+1<argc) directory=argv[++i];
        else if (!strcmp(argv[i],"--seed-demo")) seed=1;
        else {fputs("Usage: library-next.exe --data prepared-ascii-directory [--seed-demo]\n",stderr);return 2;}
    }
    if (!directory) {fputs("A prepared data directory is required. Run make run from the project root.\n",stderr);return 2;}
    ClDateTime now;static NextApp app;
    if (!cl_datetime_now(&now) || !cl_app_init(&app,directory,now.date)) {
        fputs("Cannot load or recover data; existing files were not replaced with an empty library.\n",stderr);return 3;
    }
    if (seed) {
        int empty=1;for (int k=0;k<CL_TABLES;++k) if (app.store.table[k].count) empty=0;
        ClSession admin={1,CL_ROLE_ADMIN,0,"admin"};
        ClRegistration r={"reader001","示例读者","reader123","reader123","13800138000"};
        int ok=empty && cl_auth_register(&app.store,&r)==CL_REGISTER_OK &&
            cl_library_seed(&app.store,&admin,now.date)==CL_LIB_OK;
        cl_app_dispose(&app);return ok?0:4;
    }
    fputs("\033[?1049h",stderr);cl_app_scene(&app);
    int ok=cl_terminal_draw(stderr,1);
    while (ok && app.running) {
        Event e=cl_terminal_event();if (e.key==KEY_NONE) continue;
        if (!cl_datetime_now(&now)) {text_copy(app.message,512,"错误：系统时间不可用，请退出后检查。");}
        else cl_app_event(&app,e,now.date);
        if (!app.running) break;
        cl_app_scene(&app);ok=cl_terminal_draw(stderr,e.key==KEY_F5?1:2);
    }
    cl_terminal_reset(stderr);fputs("\033[?1049l",stderr);cl_app_dispose(&app);return ok?0:1;
}
