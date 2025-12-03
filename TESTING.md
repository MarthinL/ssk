# SSK Testing

SSK uses PostgreSQL's standard testing frameworks. This document explains how testing works and how to add tests.

## Prerequisites

- PostgreSQL 12+ installed
- Extension compiled: `make && sudo make install`

## Testing Frameworks

PostgreSQL extensions use two complementary testing approaches:

### 1. pg_regress (SQL Regression Tests)

**What it is:** PostgreSQL's standard SQL testing framework. You write SQL scripts, run them against a database, and compare output to expected results.

**When to use:** Testing SQL-visible behavior - functions, types, aggregates, operators. Most extension tests should be pg_regress tests.

**How it works:**
- SQL test scripts go in `test/sql/`
- Expected output files go in `test/expected/`
- The `REGRESS` variable in `Makefile` lists which tests to run
- PostgreSQL compares actual vs expected output and reports differences

**Requirements:** PostgreSQL installed with createdb privileges.

### 2. TAP (Test Anything Protocol)

**What it is:** Perl-based testing framework for complex scenarios. Used extensively in PostgreSQL core.

**When to use:** Tests that need to control the server lifecycle, test concurrent scenarios, verify log output, or test installation/upgrade paths.

**How it works:**
- Perl test scripts go in `test/t/`
- Tests use `PostgreSQL::Test::Cluster` to create isolated temporary PostgreSQL instances
- The `TAP_TESTS = 1` variable in `Makefile` enables TAP testing
- Tests can start/stop servers, create/destroy databases, inspect logs

**Requirements:** System PostgreSQL installation. TAP modules create temporary instances automatically.

### 3. make check vs make installcheck

**make installcheck:**
- Tests against your system's installed PostgreSQL
- Extension must be installed to system directories
- Requires sudo for installation
## Current Tests

### pg_regress Tests (test/sql/)

Defined in `Makefile` as: `REGRESS = test_ssk_basic test_ssk_aggregates`

| Test | Purpose | What It Tests |
|------|---------|---------------|
| `test_ssk_basic.sql` | Extension loads | Verifies SSK extension can be created |
| `test_ssk_aggregates.sql` | Aggregate function | Tests `ssk_agg()` with sample IDs |

Each test has a corresponding expected output file in `test/expected/`.

### TAP Tests (test/t/)

Defined in `Makefile` as: `TAP_TESTS = 1`

| Test | Purpose | What It Tests |
|------|---------|---------------|
| `001_basic.pl` | Extension lifecycle | Extension creation, basic UDT/aggregate operations |

## Running Tests

### Prerequisites

- PostgreSQL 12+ installed
- Extension compiled and installed: `make && sudo make install`
- Database creation privileges

### Quick Start

```bash
# Build and install extension
make
sudo make install

# Run SQL regression tests
make installcheck

# Run TAP tests
make prove

# Run all tests
make check
```

### Run Specific Tests

```bash
# Single SQL test
cd test && make installcheck REGRESS=test_ssk_basic

# Single TAP test with verbose output
prove -v test/t/001_basic.pl

# All TAP tests
make prove
```

# Run tests (no sudo needed, no database permissions needed)
cd test && make check
```

This is the recommended approach for development - you have full control over the PostgreSQL instance.

## Test Output

### pg_regress Output

Pass:
```
test test_ssk_basic          ... ok
test test_ssk_aggregates     ... ok
```

Fail:
```
test test_ssk_basic          ... FAILED
```
See `regression.diffs` for differences between expected and actual output.

### TAP Output

Pass:
```
ok 1 - UDT in/out works
ok 2 - Aggregate creates SSK
```

Fail:
```
not ok 1 - UDT in/out works
```
Failure details are shown inline.

## Adding Tests

### Adding a pg_regress Test

1. Create SQL script: `test/sql/test_myfeature.sql`
   ```sql
   -- Test my feature
   SELECT ssk_agg(id) FROM (VALUES (1), (2)) AS t(id);
   ```

2. Generate expected output:
   ```bash
   psql -U postgres -f test/sql/test_myfeature.sql > test/expected/test_myfeature.out
   ```

3. Add to Makefile:
   ```makefile
   REGRESS = test_ssk_basic test_ssk_aggregates test_myfeature
   ```

4. Run:
   ```bash
   make installcheck USE_PGXS=1 REGRESS=test_myfeature
   ```

### Adding a TAP Test

1. Create Perl script: `test/t/002_myfeature.pl`
   ```perl
   #!/usr/bin/env perl
   use strict;
   use warnings;
   use PostgreSQL::Test::Cluster;
   use Test::More;

   my $node = PostgreSQL::Test::Cluster->new('test');
   $node->init;
   $node->start;

   $node->safe_psql('postgres', 'CREATE EXTENSION ssk;');
   my $result = $node->safe_psql('postgres', 'SELECT ...');
   ok($result =~ /expected/, 'test description');

   done_testing();
   ```

2. Make executable:
   ```bash
   chmod +x test/t/002_myfeature.pl
   ```

3. Run:
   ```bash
   make installcheck-tap USE_PGXS=1
   ```

## Unit Tests (C Code)

PostgreSQL extensions typically don't have separate C unit tests—pg_regress and TAP tests exercise the C code through the SQL interface. However, SSK includes optional C unit tests for codec development.

**Location:** `tests/test_sequences.c` (called from `test_runner.c`)

**Run:** `./test_runner` (after `gcc test_runner.c tests/*.c src/codec/*.c -Iinclude -o test_runner`)

**Use case:** Rapid iteration on codec logic without database overhead.

These are **not** part of the standard PostgreSQL test suite. They're a development aid.

## Debugging Test Failures

### pg_regress Failures

Check `regression.diffs`:
```bash
cat regression.diffs
```

This shows the diff between expected and actual output.

### TAP Failures

TAP tests show inline output. For more detail:
```bash
prove -v test/t/001_basic.pl
```

### C Code Issues

Add `elog(LOG, "Debug: value=%ld", value);` to C functions, then:
```bash
tail -f /var/log/postgresql/postgresql-16-main.log
```

Run your test and watch the log output.

## References

- [PostgreSQL Testing Documentation](https://www.postgresql.org/docs/current/regress.html)
- [TAP Tests in PostgreSQL](https://www.postgresql.org/docs/current/regress-tap.html)
- [Extension Building Infrastructure](https://www.postgresql.org/docs/current/extend-pgxs.html)

## Summary

SSK uses standard PostgreSQL testing:
- **pg_regress** for SQL tests (test/sql/ → test/expected/)
- **TAP** for complex scenarios (test/t/*.pl)
- **make installcheck** runs SQL tests against system PostgreSQL
- **make prove** runs TAP tests with temporary PostgreSQL instances
- **make check** runs both

All standard PostgreSQL extension practices. No custom frameworks.
