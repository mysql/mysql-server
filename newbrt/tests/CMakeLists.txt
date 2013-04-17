set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

if(BUILD_TESTING)
  file(GLOB srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.c)

  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    foo1.brt foo2.brt foo3.brt foo4.brt
    bar1.brt bar2.brt bar3.brt bar4.brt
    test-dump-brt.out)

  macro(declare_custom_tests)
    foreach(source ${ARGN})
      list(REMOVE_ITEM srcs ${source})
    endforeach(source)
  endmacro(declare_custom_tests)

  declare_custom_tests(logcursor-fw.c logcursor-bw.c)
  add_test(logcursor-fw echo "logcursor-fw must be run manually (needs logs to iterate over).")
  add_test(logcursor-bw echo "logcursor-bw must be run manually (needs logs to iterate over).")

  foreach(src ${srcs})
    if(NOT "${src}" MATCHES "dir[.].*[.]c")
      get_filename_component(base ${src} NAME_WE)
      add_executable(${base} ${src})
      target_link_libraries(${base} newbrt ${LIBTOKUPORTABILITY})
    endif()
  endforeach(src)

  file(GLOB log_tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" log-test*.c)
  foreach(src ${log_tests})
    set_property(SOURCE ${src} APPEND PROPERTY
      COMPILE_DEFINITIONS "dname=\"${src}\"")
  endforeach(src)

  declare_custom_tests(test-assert.c)
  add_test(test-assertA test-assert)
  add_test(test-assertB test-assert notok)
  set_tests_properties(test-assertA test-assertB PROPERTIES WILL_FAIL TRUE)
  add_test(test-assertC test-assert ok)

  declare_custom_tests(benchmark-test.c)
  add_test(benchmark-test benchmark-test -q --fname dir.check_benchmark-test)
  add_test(benchmark-test_256 benchmark-test --valsize 256 --verify 1 --fname dir.check_benchmark-test_256)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    dir.check_benchmark-test dir.check_benchmark-test_256)

  declare_custom_tests(brtloader-test-merge-files-dbufio.c)
  add_test(brtloader-test-merge-files-dbufio brtloader-test-merge-files-dbufio -r 8000 -s dir.check_brtloader-test-merge-files-dbufio)

  declare_custom_tests(brtloader-test.c)
  add_test(brtloader-test brtloader-test dir.check_brtloader-test)

  declare_custom_tests(brtloader-test-bad-generate.c)
  add_test(brtloader-test-bad-generate brtloader-test-bad-generate dir.check_brtloader-test-bad-generate)

  declare_custom_tests(brtloader-test-extractor.c)
  add_test(brtloader-test-extractor-1 brtloader-test-extractor -s -r 1000 --rowsets 1000 --asc dir.check_brtloader-test-extractor-1)
  add_test(brtloader-test-extractor-2 brtloader-test-extractor -s -r 1000 --rowsets 1000 --dsc dir.check_brtloader-test-extractor-2)
  add_test(brtloader-test-extractor-3 brtloader-test-extractor -s -r 1000 --rowsets 1000 --random dir.check_brtloader-test-extractor-3)
  add_test(brtloader-test-extractor-1a brtloader-test-extractor -s -r 1000 --rowsets 1000 --asc dir.check_brtloader-test-extractor-1a)
  add_test(brtloader-test-extractor-2a brtloader-test-extractor -s -r 1000 --rowsets 1000 --dsc dir.check_brtloader-test-extractor-2a)
  add_test(brtloader-test-extractor-3a brtloader-test-extractor -s -r 1000 --rowsets 1000 --random dir.check_brtloader-test-extractor-3a)
  add_test(brtloader-test-extractor-4a brtloader-test-extractor -s -r 1000 --rowsets 3 --asc dir.check_brtloader-test-extractor-4a)
  add_test(brtloader-test-extractor-5a brtloader-test-extractor -s -r 1000 --rowsets 3 --asc --asc-poison dir.check_brtloader-test-extractor-5a)

  declare_custom_tests(brtloader-test-extractor-errors.c)
  add_test(brtloader-test-extractor-errors-1 brtloader-test-extractor-errors -w -m -u -r 1 dir.check_brtloader-test-extractor-errors-1)
  add_test(brtloader-test-extractor-errors-2 brtloader-test-extractor-errors -m -r 10000 dir.check_brtloader-test-extractor-errors-2)

  declare_custom_tests(brtloader-test-open.c)
  add_test(brtloader-test-open brtloader-test-open dir.check_brtloader-test-open)

  declare_custom_tests(brtloader-test-writer.c)
  add_test(brtloader-test-writer-1 brtloader-test-writer -r 1 -s dir.check_brtloader-test-writer-1)
  add_test(brtloader-test-writer-1000 brtloader-test-writer -r 1000 -s dir.check_brtloader-test-writer-1000)
  add_test(brtloader-test-writer-100000 brtloader-test-writer -r 100000 -s dir.check_brtloader-test-writer-100000)
  add_test(brtloader-test-writer-1000000 brtloader-test-writer -r 1000000 -s dir.check_brtloader-test-writer-1000000)
  add_test(brtloader-test-writer-1-x brtloader-test-writer -r 1 -s -x 42 dir.check_brtloader-test-writer-1-x)
  add_test(brtloader-test-writer-1000-x brtloader-test-writer -r 1000 -s -x 42 dir.check_brtloader-test-writer-1000-x)
  add_test(brtloader-test-writer-100000-x brtloader-test-writer -r 100000 -s -x 42 dir.check_brtloader-test-writer-100000-x)
  add_test(brtloader-test-writer-1000000-x brtloader-test-writer -r 1000000 -s -x 42 dir.check_brtloader-test-writer-1000000-x)

  declare_custom_tests(brtloader-test-writer-errors.c)
  add_test(brtloader-test-writer-errors-1 brtloader-test-writer-errors -w -m -u -r 100000 dir.check_brtloader-test-writer-errors-1)
  add_test(brtloader-test-writer-errors-2 brtloader-test-writer-errors -s -w -m -u -r 10000 dir.check_brtloader-test-writer-errors-2)
  add_test(brtloader-test-writer-errors-3 brtloader-test-writer-errors -s -r 20000 --malloc_limit 0 --realloc_errors dir.check_brtloader-test-writer-errors-3)
  add_test(brtloader-test-writer-errors-4 brtloader-test-writer-errors -s -m --malloc_limit 0 -r 10000 dir.check_brtloader-test-writer-errors-4)

  declare_custom_tests(brt-serialize-benchmark.c)
  add_test(brt-serialize-benchmark brt-serialize-benchmark 92 200000)
  declare_custom_tests(bnc-insert-benchmark.c)
  add_test(bnc-insert-benchmark bnc-insert-benchmark 100 4096000 1000)

  foreach(src ${srcs})
    if(NOT "${src}" MATCHES "dir[.].*[.]c")
      get_filename_component(base ${src} NAME_WE)
      add_test(${base} ${base})
    endif()
  endforeach(src)

  ## give some tests, that time out when run with valgrind, 1 hour to complete
  set(long_tests
    subblock-test-checksum
    subblock-test-compression
    benchmark-test
    benchmark-test_256
    is_empty
    )
  set_tests_properties(${long_tests} PROPERTIES TIMEOUT 3600)
endif(BUILD_TESTING)
