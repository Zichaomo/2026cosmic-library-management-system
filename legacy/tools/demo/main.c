#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "demo_session.h"

/* Alternate entry point for isolated demonstrations, never the normal build. */
int main(void) {
    wchar_t root[MAX_PATH];
    HANDLE session = NULL;
    if (!demo_session_root(root) || !demo_session_begin(root, &session)) {
        fputs("Demo startup failed. Keep demo-mode.txt, data and the seed backup "
              "beside the build folder. No personal data is reset.\n", stderr);
        return EXIT_FAILURE;
    }
    App app = {0};
    if (!app_init(&app)) {
        CloseHandle(session);
        return EXIT_FAILURE;
    }
    int result = app_run(&app);
    app_shutdown(&app);
    CloseHandle(session);
    return result;
}
