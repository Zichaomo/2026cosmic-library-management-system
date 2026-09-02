#ifndef COSMIC_LIBRARY_APP_H
#define COSMIC_LIBRARY_APP_H

#include <stdbool.h>

#include "ui/console.h"

typedef struct {
    Console console;
    bool running;
} App;

bool app_init(App *app);
int app_run(App *app);
void app_shutdown(App *app);

#endif

