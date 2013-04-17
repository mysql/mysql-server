cmake_policy(SET CMP0012 NEW)

## these tests shouldn't run with valgrind
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
  brtloader-test-extractor-1
  brtloader-test-extractor-2
  brtloader-test-extractor-3
  brt-serialize-benchmark
  bnc-insert-benchmark
  test_groupcommit_perf.tdb
  test_groupcommit_perf.bdb
  diskfull.tdb
  stress-gc.tdb
  insert-dup-prelock.tdb
  test_3645.tdb
  test_large_update_broadcast_small_cachetable.tdb
  test_update_broadcast_stress.tdb
  test_update_stress.tdb
  stress-test.tdb
  stress-test.bdb
  hot-optimize-table-tests.tdb
  test4573-logtrim.tdb
  test-xa-prepare.tdb
  test-recover1.tdb
  test-recover2.tdb
  test-recover3.tdb
  test-prepare.tdb
  test-prepare2.tdb
  test-prepare3.tdb
  filesize.tdb
  preload-db-nested.tdb
  upgrade-test-4.tdb
  recovery_fileops_unit.tdb
  helgrind_helgrind1.tdb
  helgrind2.tdb
  helgrind3.tdb
  drd_test_groupcommit_count.tdb
  helgrind_test_groupcommit_count.tdb
  loader-cleanup-test2.tdb
  loader-cleanup-test3.tdb
  loader-stress-test4.tdb
  maxsize-for-loader-B.tdb
  try-leak-lost
  try-leak-reachable
  try-leak-uninit
  )

## tests that are supposed to crash will generate memcheck failures
set(tests_that_should_fail
  try-assert0
  try-assert-zero
  test-assertA
  test-assertB
  test_truncate_txn_abort.tdb
  test_db_no_env.tdb
  recover-missing-dbfile.abortrecover
  recover-missing-dbfile-2.abortrecover
  )
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${tests_that_should_fail})

## don't run drd stress tests with valgrind either (because that would do valgrind twice)
set(stress_tests
  test_stress1.tdb
  test_stress2.tdb
  test_stress3.tdb
  test_stress4.tdb
  test_stress5.tdb
  test_stress6.tdb
  test_stress7.tdb
  test_stress_with_verify.tdb
  )
foreach(test ${stress_tests})
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
    drd_tiny_${test}
    drd_mid_${test}
    drd_large_${test}
    )
  if(NOT test MATCHES "test_stress(1|3).tdb")
    list(APPEND CTEST_CUSTOM_TESTS_IGNORE
      drd_large_${test}
      )
  endif()
  if(NOT @RUN_LONG_TESTS@)
    list(APPEND CTEST_CUSTOM_TESTS_IGNORE
      drd_mid_${test}
      drd_large_${test}
      )
  endif()
endforeach(test)

set(recover_stress_tests
  recover-test_stress1.abortrecover
  recover-test_stress2.abortrecover
  recover-test_stress3.abortrecover
  )

## we run stress tests separately, only run them if asked to
if(NOT @RUN_STRESS_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${stress_tests} ${recover_stress_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${stress_tests} ${recover_stress_tests})
endif()

set(perf_tests
  perf_checkpoint_var.tdb
  perf_cursor_nop.tdb
  perf_malloc_free.tdb
  perf_nop.tdb
  perf_ptquery.tdb
  perf_ptquery2.tdb
  perf_xmalloc_free.tdb
  )

## we also don't need to run perf tests every time
if(NOT @RUN_PERF_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${perf_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${perf_tests})
endif()

## don't run perf tests with valgrind (that's slow)
file(GLOB perf_test_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src/tests" perf_*.c)
string(REGEX REPLACE "\\.c(;|$)" ".tdb\\1" perf_tests "${perf_test_srcs}")
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${perf_tests})

## these tests fail often and aren't helpful
set(known_failing_tests
  diskfull.tdb
  )
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${known_failing_tests})
list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${known_failing_tests})

## these tests take a long time, only run them if asked to
set(long_running_tests
  recover_stress.tdb
  checkpoint_stress.tdb
  root_fifo_1.tdb
  root_fifo_2.tdb
  root_fifo_31.tdb
  root_fifo_32.tdb
  is_empty
  test_txn_nested2.tdb
  test_logmax.tdb
  checkpoint_1.tdb
  manyfiles.tdb
  loader-stress-test4.tdb
  preload-db-nested.tdb
  test_update_stress.tdb
  test_update_broadcast_stress.tdb
  stress-test.tdb
  stress-gc.tdb
  hot-optimize-table-tests.tdb
  hotindexer-with-queries.tdb
  upgrade_test_simple
  test3529.tdb
  )
if(NOT @RUN_LONG_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${long_running_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${long_running_tests})
endif()


list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION
  "xz-4.999.9beta/src/liblzma"              # don't complain about warnings in xz source
  )
