#ifndef DEMO_SESSION_H
#define DEMO_SESSION_H

#include <stdbool.h>
#include <windows.h>

/* Keep this handle open until the app exits, including all shared terminals. */
bool demo_session_begin(const wchar_t *root, HANDLE *session);
bool demo_session_root(wchar_t root[MAX_PATH]);

#endif
