# Test Makefile for SSK
# PGXS provides installcheck, check, and prove targets automatically

REGRESS = test_ssk_basic test_ssk_aggregates
TAP_TESTS = 1

# Standard PGXS - NO conditionals for external extensions
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)