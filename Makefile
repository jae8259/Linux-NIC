.PHONY: all clean test fmt bench-veth bench-shm matrix prereq

CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=

BUILD_DIR := build

BASE_BINS := \
	$(BUILD_DIR)/base/sock_echo \
	$(BUILD_DIR)/base/sock_bench

SHM_BINS := \
	$(BUILD_DIR)/shm/shm_bench

TEST_BINS := \
	$(BUILD_DIR)/tests/test_shm_ring

all: $(BASE_BINS) $(SHM_BINS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/base/sock_echo: base/sock_echo.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/base/sock_bench: base/sock_bench.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/shm/shm_bench: shm/shm_bench.c shm/shm_ring.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) shm/shm_bench.c -o $@ $(LDFLAGS)

$(BUILD_DIR)/tests/test_shm_ring: tests/test_shm_ring.c shm/shm_ring.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_shm_ring.c -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	$(BUILD_DIR)/tests/test_shm_ring

prereq:
	./scripts/prereq_check.sh

bench-veth: all
	sudo ./scripts/run_veth_bench.sh

bench-shm: all
	./scripts/run_shm_bench.sh

matrix: all
	python3 ./tests/run_matrix.py ./tests/matrix.yaml

clean:
	rm -rf $(BUILD_DIR)

