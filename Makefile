.PHONY: all run test clean

PROG = pmk_string_example
TEST = pmk_string_test

CC ?= gcc
CFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -ggdb -std=c99

all: ./build/$(PROG)

run: ./build/$(PROG)
	./build/$(PROG)

test: CFLAGS += -DPMK_STRING_TEST
test: ./build/$(TEST)
	./build/$(TEST)

clean:
	rm -rf build

./build/$(TEST) ./build/$(PROG): examples.c pmk_arena.h pmk_string.h
	mkdir -p build
	$(CC) -o $@ $(CFLAGS) examples.c
