# SSK PostgreSQL Extension
# Uses standard PGXS build system

MODULE_big = ssk
OBJS = \
	src/codec/ssk_codec.o \
	src/codec/cdu.o \
	src/codec/ordinality.o \
	src/codec/segment.o \
	src/codec/partition.o \
	src/codec/combinadic/combinadic_init.o \
	src/codec/combinadic/ranking.o \
	src/codec/chunks/chunk_token.o \
	src/codec/chunks/chunk_raw.o \
	src/codec/chunks/chunk_enum.o \
	src/keystore/ssk_keystore.o \
	src/udt/ssk_udt.o \
	src/agg/ssk_agg.o

EXTENSION = ssk
DATA = sql/ssk--1.0.sql

PG_CPPFLAGS = -Iinclude -fno-lto -DUSE_PG
SHLIB_LINK = -fno-lto

override CFLAGS := -fPIC $(filter-out -flto=auto -ffat-lto-objects, $(CFLAGS))
override LDFLAGS := $(filter-out -flto=auto -ffat-lto-objects, $(LDFLAGS))

PGXS_DISABLE_LTO = 1
LTO = none
LTO_SUPPORTED = no
INSTALL_LTO_OBJS = 

# Disable LTO .bc generation
%.bc: ;

# Standard PGXS - NO conditionals
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Testing
REGRESS = test_ssk_basic test_ssk_aggregates
TAP_TESTS = 1

# Integration targets
.PHONY: unit-test
unit-test:
	$(MAKE) -C test/unit run

.PHONY: regression-test
regression-test: install
	$(MAKE) -C test installcheck

.PHONY: test
test: unit-test regression-test

# Workshop for algorithm experimentation
.PHONY: workshop
workshop:
	$(MAKE) -C workshop run