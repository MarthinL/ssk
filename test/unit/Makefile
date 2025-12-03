# SSK Codec Unit Tests
# Standalone C compilation (no PostgreSQL, no PGXS)

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -g -O2
INCLUDES = -I../../include

TEST_RUNNER_OBJS = \
	test_runner.o \
	tests/test_sequences.o \
	tests/test_hand_crafted.o \
	../../src/codec/vlq_p.o \
	../../src/codec/segment.o \
	../../src/codec/partition.o \
	../../src/codec/ordinality.o \
	../../src/codec/combinadic/combinadic_init.o \
	../../src/codec/chunks/chunk_token.o \
	../../src/codec/chunks/chunk_raw.o \
	../../src/codec/chunks/chunk_enum.o

test_runner: $(TEST_RUNNER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

.PHONY: run
run: test_runner
	./test_runner

.PHONY: clean
clean:
	rm -f *.o tests/*.o test_runner