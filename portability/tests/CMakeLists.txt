include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

if(BUILD_TESTING)
  file(GLOB srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" *.cc)
  foreach(src ${srcs})
    get_filename_component(base ${src} NAME_WE)
    list(APPEND tests ${base})
  endforeach(src)

  foreach(test test-fsync test-fsync-directory)
    set_property(SOURCE ${test} APPEND PROPERTY COMPILE_DEFINITIONS "ENVDIR=\"dir.${test}.test\"")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES dir.${test}.test)
  endforeach(test)

  include(CheckCCompilerFlag)
  check_c_compiler_flag(-Wno-unused-result HAVE_WNO_UNUSED_RESULT)
  if (HAVE_WNO_UNUSED_RESULT)
    add_space_separated_property(SOURCE try-leak-lost COMPILE_FLAGS -Wno-unused-result)
  endif ()
  check_c_compiler_flag(-Wno-maybe-uninitialized HAVE_WNO_MAYBE_UNINITIALIZED)
  if (HAVE_WNO_MAYBE_UNINITIALIZED)
    add_space_separated_property(SOURCE try-uninit COMPILE_FLAGS -Wno-maybe-uninitialized)
  endif ()

  foreach(test ${tests})
    add_executable(${test} ${test})
    target_link_libraries(${test} ${LIBTOKUPORTABILITY})
    set_target_properties(${test} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(HAVE_CLOCK_REALTIME)
      target_link_libraries(${test} rt)
    else()
      target_link_libraries(${test} System)
    endif()
  endforeach(test)

  list(REMOVE_ITEM tests test-pwrite4g)
  add_test(portability/test-pwrite4g test-pwrite4g .)

  configure_file(ensure_memcheck_fails.sh . COPYONLY)
  foreach(test try-leak-lost try-leak-reachable try-uninit)
    list(REMOVE_ITEM tests ${test})

    add_test(NAME portability/${test}
      COMMAND ensure_memcheck_fails.sh $<TARGET_FILE:${test}> valgrind --error-exitcode=1 --quiet --leak-check=full --show-reachable=yes --trace-children=yes --trace-children-skip=sh,*/sh,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,test,wc,*/wc)
  endforeach(test)

  foreach(test ${tests})
    add_test(portability/${test} ${test})
  endforeach(test)

  set(portability_tests_should_fail
    portability/try-assert0
    portability/try-assert-zero
    )

  set_tests_properties(${portability_tests_should_fail} PROPERTIES WILL_FAIL TRUE)
endif(BUILD_TESTING)
