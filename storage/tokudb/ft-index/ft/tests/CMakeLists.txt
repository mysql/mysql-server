set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

if(BUILD_TESTING OR BUILD_FT_TESTS)
  function(add_ft_test bin)
    add_toku_test(ft ${bin} ${ARGN})
  endfunction(add_ft_test)
  function(add_ft_test_aux name bin)
    add_toku_test_aux(ft ${name} ${bin} ${ARGN})
  endfunction(add_ft_test_aux)

  ## get a list of the sources in this directory
  file(GLOB srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cc)
  set(tests)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)
    list(APPEND tests ${base})
  endforeach(src)

  ## this macro will remove the test from the list of source files so it
  ## doesn't end up getting the default test rule applied to it
  macro(declare_custom_tests)
    foreach(test ${ARGN})
      list(REMOVE_ITEM tests ${test})
    endforeach(test)
  endmacro(declare_custom_tests)

  declare_custom_tests(logcursor-fw logcursor-bw)
  add_test(ft/logcursor-fw echo "logcursor-fw must be run manually (needs logs to iterate over).")
  add_test(ft/logcursor-bw echo "logcursor-bw must be run manually (needs logs to iterate over).")

  foreach(test ${tests})
    add_executable(${test} ${test}.cc)
    target_link_libraries(${test} ft ${LIBTOKUPORTABILITY})
    set_target_properties(${test} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_space_separated_property(TARGET ${test} COMPILE_FLAGS -fvisibility=hidden)
  endforeach(test)

  ## declare some tests that should be run with specific options

  declare_custom_tests(test-assert)
  add_ft_test_aux(test-assertA test-assert)
  add_ft_test_aux(test-assertB test-assert notok)
  set_tests_properties(ft/test-assertA ft/test-assertB PROPERTIES WILL_FAIL TRUE)
  add_ft_test_aux(test-assertC test-assert ok)
  setup_toku_test_properties(ft/test-assertC test-assertC)

  declare_custom_tests(benchmark-test)
  add_ft_test(benchmark-test -q 1)
  add_ft_test_aux(benchmark-test_256 benchmark-test --valsize 256 --periter 131072 --verify --verify_period 4096 1)

  declare_custom_tests(ftloader-test-merge-files-dbufio)
  add_ft_test(ftloader-test-merge-files-dbufio -r 8000 -s)

  declare_custom_tests(ftloader-test-extractor)
  add_ft_test_aux(ftloader-test-extractor-1 ftloader-test-extractor -s -r 1000 --rowsets 1000 --asc)
  add_ft_test_aux(ftloader-test-extractor-2 ftloader-test-extractor -s -r 1000 --rowsets 1000 --dsc)
  add_ft_test_aux(ftloader-test-extractor-3 ftloader-test-extractor -s -r 1000 --rowsets 1000 --random)
  add_ft_test_aux(ftloader-test-extractor-1a ftloader-test-extractor -s -r 1000 --rowsets 1000 --asc)
  add_ft_test_aux(ftloader-test-extractor-2a ftloader-test-extractor -s -r 1000 --rowsets 1000 --dsc)
  add_ft_test_aux(ftloader-test-extractor-3a ftloader-test-extractor -s -r 1000 --rowsets 1000 --random)
  add_ft_test_aux(ftloader-test-extractor-4a ftloader-test-extractor -s -r 1000 --rowsets 3 --asc)
  add_ft_test_aux(ftloader-test-extractor-5a ftloader-test-extractor -s -r 1000 --rowsets 3 --asc --asc-poison)

  declare_custom_tests(ftloader-test-extractor-errors)
  add_ft_test_aux(ftloader-test-extractor-errors-1 ftloader-test-extractor-errors -w -m -u -r 1)
  add_ft_test_aux(ftloader-test-extractor-errors-2 ftloader-test-extractor-errors -m -r 10000)

  declare_custom_tests(ftloader-test-writer)
  add_ft_test_aux(ftloader-test-writer-1 ftloader-test-writer -r 1 -s)
  add_ft_test_aux(ftloader-test-writer-1000 ftloader-test-writer -r 1000 -s)
  add_ft_test_aux(ftloader-test-writer-100000 ftloader-test-writer -r 100000 -s)
  add_ft_test_aux(ftloader-test-writer-1000000 ftloader-test-writer -r 1000000 -s)
  add_ft_test_aux(ftloader-test-writer-1-x ftloader-test-writer -r 1 -s -x 42)
  add_ft_test_aux(ftloader-test-writer-1000-x ftloader-test-writer -r 1000 -s -x 42)
  add_ft_test_aux(ftloader-test-writer-100000-x ftloader-test-writer -r 100000 -s -x 42)
  add_ft_test_aux(ftloader-test-writer-1000000-x ftloader-test-writer -r 1000000 -s -x 42)

  declare_custom_tests(ftloader-test-writer-errors)
  add_ft_test_aux(ftloader-test-writer-errors-1 ftloader-test-writer-errors -w -m -u -r 100000)
  add_ft_test_aux(ftloader-test-writer-errors-2 ftloader-test-writer-errors -s -w -m -u -r 10000)
  add_ft_test_aux(ftloader-test-writer-errors-3 ftloader-test-writer-errors -s -r 20000 --malloc_limit 0 --realloc_errors)
  add_ft_test_aux(ftloader-test-writer-errors-4 ftloader-test-writer-errors -s -m --malloc_limit 0 -r 10000)

  declare_custom_tests(ft-serialize-benchmark)
  add_ft_test(ft-serialize-benchmark 92 200000)
  declare_custom_tests(bnc-insert-benchmark)
  add_ft_test(bnc-insert-benchmark 100 4096000 1000)

  add_ft_test_aux(ftdump-test-generate ft-test)
  add_test(NAME ft/ftdump-test
    COMMAND $<TARGET_FILE:tokuftdump> "ftdump-test-generate.ctest-data"
    )
  set_tests_properties(ft/ftdump-test PROPERTIES
    DEPENDS ft/ftdump-test-generate
    REQUIRED_FILES "ftdump-test-generate.ctest-data"
    )

  ## keyrange has some inequality assumptions that were broken by
  ## promotion, they seem benign but are complicated, so for now, skip
  ## this test, when we get some time, we should fix it and re-enable it.
  list(REMOVE_ITEM tests keyrange)

  foreach(test ${tests})
    add_ft_test(${test})
  endforeach(test)

  set_property(TEST ft/upgrade_test_simple APPEND PROPERTY ENVIRONMENT "TOKUDB_DATA=${TOKUDB_DATA}")

  # should be a file GLOB and a loop
  declare_custom_tests(test-upgrade-recovery-logs)
  add_ft_test_aux(test-upgrade-recovery-logs-24-clean test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-24-clean)
  add_ft_test_aux(test-upgrade-recovery-logs-24-dirty test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-24-dirty)
  add_ft_test_aux(test-upgrade-recovery-logs-25-clean test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-25-clean)
  add_ft_test_aux(test-upgrade-recovery-logs-25-dirty test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-25-dirty)
  add_ft_test_aux(test-upgrade-recovery-logs-26-clean test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-26-clean)
  add_ft_test_aux(test-upgrade-recovery-logs-26-dirty test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-26-dirty)
  add_ft_test_aux(test-upgrade-recovery-logs-27-clean test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-27-clean)
  add_ft_test_aux(test-upgrade-recovery-logs-27-dirty test-upgrade-recovery-logs ${TOKUDB_DATA}/upgrade-recovery-logs-27-dirty)

  ## give some tests, that time out normally, 1 hour to complete
  set(long_tests
    ft/ftloader-test-extractor-3a
    ft/log-test7
    ft/recovery-bad-last-entry
    ft/subblock-test-compression
    ft/upgrade_test_simple
    )
  set_tests_properties(${long_tests} PROPERTIES TIMEOUT 3600)
  ## some take even longer, with valgrind
  set(extra_long_tests
    ft/benchmark-test
    ft/benchmark-test_256
    ft/is_empty
    ft/subblock-test-checksum
    )
  set_tests_properties(${extra_long_tests} PROPERTIES TIMEOUT 7200)
endif(BUILD_TESTING OR BUILD_FT_TESTS)
