Prepare
-------

* clang 3.9 and higher
* libFuzzer


cmake will check if the clang version works as expected::

  -- Performing Test COMPILER_HAS_SANITIZE_COVERAGE_TRACE_PC_GUARD
  -- Performing Test COMPILER_HAS_SANITIZE_COVERAGE_TRACE_PC_GUARD - Success
  -- Performing Test COMPILER_HAS_PROFILE_INSTR_GENERATE
  -- Performing Test COMPILER_HAS_PROFILE_INSTR_GENERATE - Success
  -- Performing Test COMPILER_HAS_COVERAGE_MAPPING
  -- Performing Test COMPILER_HAS_COVERAGE_MAPPING - Success
  -- Performing Test CLANG_HAS_LIBFUZZER
  -- Performing Test CLANG_HAS_LIBFUZZER - Success

Run
---

$ make fuzz

runs all fuzzers for 10 seconds

Coverage
--------

$ make fuzz-coverage.html

creates

./tests/fuzzers/*-coverage.html
