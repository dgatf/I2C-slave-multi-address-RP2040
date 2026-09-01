# Makefile for host-side rx queue unit tests

CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -I../../sdk
TARGET  := test_rx_queue

.PHONY: all clean run

all: $(TARGET)

$(TARGET): test_rx_queue.c ../../sdk/i2c_rx_queue.h
	$(CC) $(CFLAGS) -o $@ test_rx_queue.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
