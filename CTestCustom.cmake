cmake_policy(SET CMP0012 NEW)

## these tests shouldn't run with valgrind
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
  ft/bnc-insert-benchmark
  ft/brt-serialize-benchmark
  ft/ft_loader-test-extractor-1
  ft/ft_loader-test-extractor-2
  ft/ft_loader-test-extractor-3
  ft/upgrade_test_simple
  portability/test-cache-line-boundary-fails
  portability/try-leak-lost
  portability/try-leak-reachable
  portability/try-leak-uninit
  util/helgrind_test_circular_buffer
  util/helgrind_test_partitioned_counter
  util/helgrind_test_partitioned_counter_5833
  ydb/diskfull.tdb
  ydb/drd_test_4015.tdb
  ydb/drd_test_groupcommit_count.tdb
  ydb/filesize.tdb
  ydb/helgrind_helgrind1.tdb
  ydb/helgrind_helgrind2.tdb
  ydb/helgrind_helgrind3.tdb
  ydb/helgrind_test_groupcommit_count.tdb
  ydb/hot-optimize-table-tests.tdb
  ydb/insert-dup-prelock.tdb
  ydb/loader-cleanup-test2.tdb
  ydb/loader-cleanup-test3.tdb
  ydb/loader-stress-test4.tdb
  ydb/maxsize-for-loader-B.tdb
  ydb/preload-db-nested.tdb
  ydb/stress-gc.tdb
  ydb/stress-gc2.tdb
  ydb/stress-test.bdb
  ydb/stress-test.tdb
  ydb/test-5138.tdb
  ydb/test-prepare.tdb
  ydb/test-prepare2.tdb
  ydb/test-prepare3.tdb
  ydb/test-recover1.tdb
  ydb/test-recover2.tdb
  ydb/test-recover3.tdb
  ydb/test-xa-prepare.tdb
  ydb/test4573-logtrim.tdb
  ydb/test_3645.tdb
  ydb/test_groupcommit_perf.bdb
  ydb/test_groupcommit_perf.tdb
  ydb/test_large_update_broadcast_small_cachetable.tdb
  ydb/test_update_broadcast_stress.tdb
  ydb/test_update_stress.tdb
  ydb/upgrade-test-4.tdb
  )

if (NOT @RUN_HELGRIND_TESTS@)
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE
    util/helgrind_test_circular_buffer
    util/helgrind_test_partitioned_counter
    util/helgrind_test_partitioned_counter_5833
    ydb/helgrind_helgrind1.tdb
    ydb/helgrind_helgrind2.tdb
    ydb/helgrind_helgrind3.tdb
    ydb/helgrind_test_groupcommit_count.tdb
    )
endif ()

if (NOT @RUN_DRD_TESTS@)
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE
    ydb/drd_test_groupcommit_count.tdb
    ydb/drd_test_4015.tdb
    )
endif ()

## osx's pthreads prefer writers, so this test will deadlock
if (@CMAKE_SYSTEM_NAME@ STREQUAL Darwin)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE portability/test-pthread-rwlock-rwr)
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE portability/test-pthread-rwlock-rwr)
endif ()

## tests that are supposed to crash will generate memcheck failures
set(tests_that_should_fail
  ft/test-assertA
  ft/test-assertB
  portability/try-assert-zero
  portability/try-assert0
  ydb/recover-missing-dbfile-2.abortrecover
  ydb/recover-missing-dbfile.abortrecover
  ydb/test_db_no_env.tdb
  ydb/test_truncate_txn_abort.tdb
  )
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${tests_that_should_fail})

## don't run drd stress tests with valgrind either (because that would do valgrind twice)
set(stress_tests
  test_stress0.tdb
  test_stress1.tdb
  test_stress2.tdb
  test_stress3.tdb
  test_stress4.tdb
  test_stress5.tdb
  test_stress6.tdb
  test_stress7.tdb
  test_stress_hot_indexing.tdb
  test_stress_openclose.tdb
  test_stress_with_verify.tdb
  )
foreach(test ${stress_tests})
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
    ydb/drd_tiny_${test}
    ydb/drd_mid_${test}
    ydb/drd_large_${test}
    )
  if(NOT @RUN_LONG_TESTS@)
    list(APPEND CTEST_CUSTOM_TESTS_IGNORE
      ydb/drd_large_${test}
      )
  endif()
  if (NOT @RUN_DRD_TESTS@)
    list(APPEND CTEST_CUSTOM_TESTS_IGNORE
      ydb/drd_tiny_${test}
      ydb/drd_mid_${test}
      ydb/drd_large_${test}
      )
  endif ()
endforeach(test)

## upgrade stress tests are 5 minutes long, don't need to run them always
if(NOT @RUN_LONG_TESTS@)
  foreach(test ${stress_tests})
    if (NOT ${test} MATCHES test_stress_openclose)
      foreach(oldver 4.2.0 5.0.8 5.2.7 6.0.0 6.1.0 6.5.1 6.6.3)
        foreach(p_or_s pristine stressed)
          if (NOT (${test} MATCHES test_stress4 AND ${p_or_s} MATCHES stressed))
            foreach(size 2000)
              list(APPEND CTEST_CUSTOM_TESTS_IGNORE ydb/${test}/upgrade/${oldver}/${p_or_s}/${size})
            endforeach(size)
          endif ()
        endforeach(p_or_s)
      endforeach(oldver)
    endif ()
  endforeach(test)
endif()

set(tdb_tests_that_should_fail "ydb/${stress_tests}")
string(REGEX REPLACE ";" ";ydb/" stress_tests "${stress_tests}")

set(recover_stress_tests
  ydb/recover-test_stress1.abortrecover
  ydb/recover-test_stress2.abortrecover
  ydb/recover-test_stress3.abortrecover
  ydb/recover-test_stress_openclose.abortrecover
  )

## we run stress tests separately, only run them if asked to
if(NOT @RUN_STRESS_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${stress_tests} ${recover_stress_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${stress_tests} ${recover_stress_tests})
endif()

set(perf_tests
  ydb/perf_checkpoint_var.tdb
  ydb/perf_cursor_nop.tdb
  ydb/perf_malloc_free.tdb
  ydb/perf_nop.tdb
  ydb/perf_ptquery.tdb
  ydb/perf_ptquery2.tdb
  ydb/perf_read_write.tdb
  ydb/perf_xmalloc_free.tdb
  )

## we also don't need to run perf tests every time
if(NOT @RUN_PERF_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${perf_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${perf_tests})
endif()

## don't run perf tests with valgrind (that's slow)
file(GLOB perf_test_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src/tests" perf_*.cc)
string(REGEX REPLACE "\\.cc(;|$)" ".tdb\\1" perf_tests "${perf_test_srcs}")
set(tdb_tests_that_should_fail "ydb/${perf_tests}")
string(REGEX REPLACE ";" ";ydb/" perf_tests "${perf_tests}")
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${perf_tests})

## these tests fail often and aren't helpful
set(known_failing_tests
  ydb/diskfull.tdb
  )
list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${known_failing_tests})
list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${known_failing_tests})

## these tests take a long time, only run them if asked to
set(long_running_tests
  ft/is_empty
  ft/upgrade_test_simple
  ydb/checkpoint_1.tdb
  ydb/checkpoint_stress.tdb
  ydb/hotindexer-with-queries.tdb
  ydb/hot-optimize-table-tests.tdb
  ydb/loader-cleanup-test0.tdb
  ydb/loader-cleanup-test0z.tdb
  ydb/loader-cleanup-test2.tdb
  ydb/loader-cleanup-test2z.tdb
  ydb/loader-stress-test4.tdb
  ydb/loader-stress-test4z.tdb
  ydb/manyfiles.tdb
  ydb/preload-db-nested.tdb
  ydb/recover_stress.tdb
  ydb/root_fifo_1.tdb
  ydb/root_fifo_2.tdb
  ydb/root_fifo_31.tdb
  ydb/root_fifo_32.tdb
  ydb/stress-gc.tdb
  ydb/stress-test.tdb
  ydb/test3529.tdb
  ydb/test_logmax.tdb
  ydb/test_txn_nested2.tdb
  ydb/test_update_broadcast_stress.tdb
  ydb/test_update_stress.tdb
  )
if(NOT @RUN_LONG_TESTS@)
  list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE ${long_running_tests})
  list(APPEND CTEST_CUSTOM_TESTS_IGNORE ${long_running_tests})
endif()

## ignore log_print.cc in coverage report
list(APPEND CTEST_CUSTOM_COVERAGE_EXCLUDE "log_print.cc")

list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION
  # don't complain about warnings in xz source
  "xz-4.999.9beta/src/liblzma"
  # don't complain about clang missing warnings from xz code
  "clang: warning: unknown warning option"
  # don't complain about warnings in jemalloc source
  "jemalloc/src"
  "jemalloc/internal"
  # don't complain about valgrind headers leaving things unused
  "valgrind/valgrind.h"
  "valgrind/memcheck.h"
  # don't complain about ranlib or libtool on empty archive
  "has no symbols"
  "the table of contents is empty"
  )
