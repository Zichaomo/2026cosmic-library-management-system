CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
TARGET := build/library.exe
SOURCES := $(wildcard src/*.c src/ui/*.c src/core/*.c src/data/*.c src/utils/*.c)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

run: all
	$(TARGET)

clean:
	@if exist build rmdir /S /Q build

