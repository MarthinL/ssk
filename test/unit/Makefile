# SSK Codec Unit Tests
# Standalone C compilation (no PostgreSQL, no PGXS)

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -g -O2
ifeq ($(TRIVIAL),1)
CFLAGS += -DTRIVIAL
endif
INCLUDES = -I../../include

TEST_RUNNER_OBJS = \
	test_runner.o \
	tests/test_sequences.o \
	tests/test_hand_crafted.o \
	ssk_codec_standalone.o \
	cdu_standalone.o \
	segment_standalone.o \
	partition_standalone.o \
	ordinality_standalone.o \
	combinadic_init_standalone.o \
	ranking_standalone.o \
	chunk_token_standalone.o \
	chunk_raw_standalone.o \
	chunk_enum_standalone.o

test_runner: $(TEST_RUNNER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

ssk_codec_standalone.o: ../../src/codec/ssk_codec.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

cdu_standalone.o: ../../src/codec/cdu.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

segment_standalone.o: ../../src/codec/segment.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

partition_standalone.o: ../../src/codec/partition.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

ordinality_standalone.o: ../../src/codec/ordinality.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

combinadic_init_standalone.o: ../../src/codec/combinadic/combinadic_init.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

ranking_standalone.o: ../../src/codec/combinadic/ranking.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

chunk_token_standalone.o: ../../src/codec/chunks/chunk_token.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

chunk_raw_standalone.o: ../../src/codec/chunks/chunk_raw.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

chunk_enum_standalone.o: ../../src/codec/chunks/chunk_enum.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

cdu_standalone.o: ../../src/codec/cdu.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

.PHONY: run
run: test_runner
	./test_runner

.PHONY: clean
clean:
	rm -f *.o tests/*.o test_runner *_standalone.o