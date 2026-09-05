CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
LDLIBS := -lbcrypt
TARGET := build/library.exe
SOURCES := $(wildcard src/*.c src/ui/*.c src/core/*.c src/data/*.c src/utils/*.c)
TEST_TARGET := build/auth_tests.exe
TEST_SOURCES := tests/test_auth.c src/core/auth.c src/data/csv.c \
	src/data/reader_repository.c src/utils/hash.c src/utils/text.c

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

run: all
	$(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET) $(LDLIBS)

clean:
	@if exist build rmdir /S /Q build
