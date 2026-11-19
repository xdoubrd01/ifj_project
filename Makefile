CC = gcc
CPPFLAGS += -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c23 -O2 -Wall -Wextra -Wpedantic

TARGET := ifj26
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -rf build
	rm -f $(TARGET)

-include $(DEPS)