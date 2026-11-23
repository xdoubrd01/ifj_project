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
PYTHON ?= python3
INTERPRETER ?=

.PHONY: test test-full test-helpers test-tables test-scanner test-expressions test-semantic test-parser test-semantic-programs test-codegen test-integration

test:
	$(PYTHON) tests/run_tests.py --suite all $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-full: test

test-helpers:
	$(PYTHON) tests/run_tests.py --suite helpers $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-tables:
	$(PYTHON) tests/run_tests.py --suite tables $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-scanner:
	$(PYTHON) tests/run_tests.py --suite scanner $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-expressions:
	$(PYTHON) tests/run_tests.py --suite expressions $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-semantic:
	$(PYTHON) tests/run_tests.py --suite semantic $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-parser:
	$(PYTHON) tests/run_tests.py --suite parser $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-semantic-programs:
	$(PYTHON) tests/run_tests.py --suite semantic_programs $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-codegen:
	$(PYTHON) tests/run_tests.py --suite codegen $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)

test-integration:
	$(PYTHON) tests/run_tests.py --suite integration $(if $(strip $(INTERPRETER)),--interpreter "$(INTERPRETER)",) $(TEST_ARGS)
