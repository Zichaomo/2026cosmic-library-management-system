SHELL := cmd.exe
.SHELLFLAGS := /D /C
CC = gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -Iinclude
APP := start/CosmicLibrary.exe
DATA_DIR ?= build/library-data
DATA_WIN = $(subst /,\,$(DATA_DIR))
FOUNDATION := src/utils/foundation_utf8.c src/utils/foundation_text.c src/utils/foundation_date.c
STORE := src/migration/csv.c src/migration/schema.c src/migration/store.c src/migration/auth.c src/migration/library.c
UI := src/ui_next/app.c src/ui_next/listing.c src/ui_next/view.c src/ui_next/terminal.c src/ui_next/edit.c src/ui_next/key.c
SEARCH := src/search/pinyin.c
SOURCES := src/ui_next/main.c $(UI) $(STORE) $(FOUNDATION) $(SEARCH)
HEADERS := $(wildcard include/*.h include/ui_next/*.h include/migration/*.h include/utils/*.h include/search/*.h)
TEST_ID := $(shell echo %RANDOM%%RANDOM%)
TEST_RUN := build/application/run-$(TEST_ID)
PREVIEW := tools/preview/preview_model.c tools/preview/preview_view.c src/ui_next/terminal.c src/ui_next/edit.c src/ui_next/key.c $(FOUNDATION)

.PHONY: all run run-compiled prepare migrated run-migrated test test-foundation test-search test-application test-preview clean
all: $(APP)

$(APP): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

# Directory creation and template copying are build preparation, not C runtime.
# An existing directory is never reset or repaired automatically.
prepare:
	@if not exist "$(APP)" (echo Compile first with make. & exit /b 1)
	@if not exist "$(DATA_WIN)" (mkdir "$(DATA_WIN)" && copy /Y resources\empty-data\* "$(DATA_WIN)\" >nul && "$(APP)" --data "$(DATA_DIR)" --seed-demo)

run: all
	@$(MAKE) --no-print-directory run-compiled DATA_DIR="$(DATA_DIR)"

run-compiled: prepare
	chcp 65001 >nul && "$(APP)" --data "$(DATA_DIR)"

migrated: all
run-migrated: run

test: test-foundation test-search test-application test-preview

test-foundation:
	@if not exist build\foundation mkdir build\foundation
	$(CC) $(CFLAGS) tests/test_foundation.c $(FOUNDATION) -o build/foundation/tests.exe
	build\foundation\tests.exe

test-search:
	@if not exist build\search mkdir build\search
	$(CC) $(CFLAGS) tests/test_search.c $(SEARCH) src/utils/foundation_utf8.c -o build/search/tests.exe
	build\search\tests.exe

test-application: all
	@if exist "$(subst /,\,$(TEST_RUN))" (echo Test directory already exists. Retry. & exit /b 1)
	@for %%d in (circulation limits queue failure ui features) do @($(MAKE) --no-print-directory prepare DATA_DIR="$(TEST_RUN)/%%d" || exit /b 1)
	$(CC) $(CFLAGS) -DCL_MIGRATION_TEST tests/test_application.c $(UI) $(STORE) $(FOUNDATION) $(SEARCH) -o $(TEST_RUN)/application-tests.exe
	"$(TEST_RUN)/application-tests.exe" "$(TEST_RUN)"
	"$(TEST_RUN)/application-tests.exe" --restart "$(TEST_RUN)/ui"

test-preview:
	@if not exist build\preview mkdir build\preview
	$(CC) $(CFLAGS) -Itools/preview tools/preview/test_preview.c $(PREVIEW) -o build/preview/preview_tests.exe
	build\preview\preview_tests.exe

# Keep user data and running executables intact.
clean:
	@echo Persistent data retained. Remove individual unused executables manually.

.PHONY: diagnose-console
diagnose-console:
	@if not exist build\console mkdir build\console
	$(CC) $(CFLAGS) tools/console/probe.c -o build/console/probe.exe
	@chcp
	chcp 65001 && build\console\probe.exe
