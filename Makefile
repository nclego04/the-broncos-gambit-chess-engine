CC = gcc
CFLAGS = -O3 -Wall -Wextra

SRC_DIR = src
BIN_DIR = bin
TOOLS_DIR = tools

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = $(BIN_DIR)/broncos_engine

.PHONY: all clean arena

all: $(TARGET) arena

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

arena:
	javac $(TOOLS_DIR)/UciBoardArena2.java

clean:
	rm -f $(OBJS)
	rm -rf $(BIN_DIR)
	rm -f $(TOOLS_DIR)/*.class