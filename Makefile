# ============================================================
# 宇宙图书馆管理系统 - 构建脚本（MinGW-w64 / w64devkit）
# 用法:
#   make          # 编译生成 library.exe
#   make run      # 编译并运行
#   make clean    # 清理构建产物
# ============================================================

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -fexec-charset=UTF-8
LDFLAGS := -static

SRCS   := $(wildcard src/*.c src/utils/*.c src/data/*.c src/core/*.c src/ui/*.c)
OBJS   := $(SRCS:.c=.o)
TARGET := library.exe

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
