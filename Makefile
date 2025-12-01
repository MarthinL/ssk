# SSK extension Makefile
# Standard PGXS build for PostgreSQL extensions

EXTENSION = ssk
MODULE_big = ssk
OBJS = src/ssk.o

DATA = sql/ssk--1.0.sql
REGRESS = ssk_version

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)