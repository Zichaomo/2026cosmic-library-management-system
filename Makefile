CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
LDLIBS := -lbcrypt
TARGET := build/library.exe
SOURCES := $(wildcard src/*.c src/ui/*.c src/core/*.c src/data/*.c src/utils/*.c)
AUTH_TEST_TARGET := build/auth_tests.exe
AUTH_TEST_SOURCES := tests/test_auth.c src/core/auth.c src/data/csv.c \
	src/data/reader_repository.c src/utils/hash.c src/utils/text.c
CIRCULATION_TEST_TARGET := build/circulation_tests.exe
CIRCULATION_TEST_SOURCES := tests/test_circulation.c src/core/auth.c \
	src/data/csv.c src/data/reader_repository.c \
	src/data/library_repository.c src/utils/hash.c src/utils/text.c
UI_TEST_TARGET := build/ui_tests.exe
UI_TEST_SOURCES := tests/test_ui.c src/ui/widget.c src/ui/console.c

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

run: all
	$(TARGET)

test: $(AUTH_TEST_TARGET) $(CIRCULATION_TEST_TARGET) $(UI_TEST_TARGET)
	$(AUTH_TEST_TARGET)
	$(CIRCULATION_TEST_TARGET)
	$(UI_TEST_TARGET)

$(AUTH_TEST_TARGET): $(AUTH_TEST_SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(AUTH_TEST_SOURCES) -o $(AUTH_TEST_TARGET) $(LDLIBS)

$(CIRCULATION_TEST_TARGET): $(CIRCULATION_TEST_SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(CIRCULATION_TEST_SOURCES) -o $(CIRCULATION_TEST_TARGET) $(LDLIBS)

$(UI_TEST_TARGET): $(UI_TEST_SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(UI_TEST_SOURCES) -o $(UI_TEST_TARGET)

clean:
	@if exist build rmdir /S /Q build
