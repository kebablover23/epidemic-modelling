CC ?= gcc

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -O2
LDLIBS := -lm

BUILD_DIR := build
OUTPUT_DIR := output
PLOTS_DIR := $(OUTPUT_DIR)/plots

APP := $(BUILD_DIR)/epidemic-model
TEST_RUNNER := $(BUILD_DIR)/test-runner

COMMON_SOURCES := \
	src/input.c \
	src/simulation.c \
	src/plotting.c

APP_SOURCES := $(COMMON_SOURCES) src/main.c
TEST_SOURCES := $(COMMON_SOURCES) tests/test_runner.c

PUBLIC_HEADER := include/epidemic.h

.PHONY: all run test clean directories help

all: $(APP)

directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(OUTPUT_DIR)
	mkdir -p $(PLOTS_DIR)

$(APP): $(APP_SOURCES) $(PUBLIC_HEADER) | directories
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) $(LDLIBS) -o $@

$(TEST_RUNNER): $(TEST_SOURCES) $(PUBLIC_HEADER) | directories
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LDLIBS) -o $@

run: $(APP)
	$(APP) --model SIR --input scenarios/sir_one_day.txt --deterministic --no-plot

test: $(TEST_RUNNER)
	$(TEST_RUNNER)

help: $(APP)
	$(APP) --help

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUTPUT_DIR)/data_file.txt
	rm -f $(OUTPUT_DIR)/stochastic_replicates.txt
	rm -f $(OUTPUT_DIR)/plot.gnu
	rm -f $(PLOTS_DIR)/epidemic.png