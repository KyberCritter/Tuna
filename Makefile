CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
INCLUDES := -Iinclude -Ithird_party/minimp3
SHARED := $(wildcard shared/*.c)
HOST := $(SHARED) host/convert.c
HEADERS := $(wildcard include/*.h) third_party/minimp3/minimp3.h
.PHONY: host test
host: build/host/tuna-convert test
build/host:
	mkdir -p $@
build/host/tests: $(HOST) tests/test.c $(HEADERS) | build/host
	$(CC) $(CFLAGS) $(INCLUDES) $(HOST) tests/test.c -lm -o $@
build/host/tuna-convert: $(HOST) host/main.c $(HEADERS) | build/host
	$(CC) $(CFLAGS) $(INCLUDES) $(HOST) host/main.c -lm -o $@
test: build/host/tests
	./build/host/tests
