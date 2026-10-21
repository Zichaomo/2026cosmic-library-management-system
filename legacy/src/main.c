#include <stdlib.h>

#include "app.h"

int main(void) {
    App app = {0};

    if (!app_init(&app)) {
        return EXIT_FAILURE;
    }

    const int exit_code = app_run(&app);
    app_shutdown(&app);

    return exit_code;
}

