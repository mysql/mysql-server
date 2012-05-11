## some functions for getting system info so we can construct BUILDNAME

## given an executable, follows symlinks and resolves paths until it runs
## out of symlinks, then gives you the basename
macro(real_executable_name filename_input out)
  set(res 0)
  set(filename ${filename_input})
  while(NOT(res))
    execute_process(
      COMMAND which ${filename}
      RESULT_VARIABLE res
      OUTPUT_VARIABLE full_filename
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT(res))
      execute_process(
        COMMAND readlink ${full_filename}
        RESULT_VARIABLE res
        OUTPUT_VARIABLE link_target
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(NOT(res))
        execute_process(
          COMMAND dirname ${full_filename}
          OUTPUT_VARIABLE filepath
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(filename "${filepath}/${link_target}")
      else()
        set(filename ${full_filename})
      endif()
    else()
      set(filename ${filename})
    endif()
  endwhile()
  execute_process(
    COMMAND basename ${filename}
    OUTPUT_VARIABLE real_filename
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${out} ${real_filename})
endmacro(real_executable_name)

## gives you `uname ${flag}`
macro(uname flag out)
  execute_process(
    COMMAND uname ${flag}
    OUTPUT_VARIABLE ${out}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(uname)

## determines the current revision of ${svn_dir}
macro(get_svn_revision svn_dir out)
  find_program(CMAKE_SVN_PROG svn)
  if(CMAKE_SVN_PROG MATCHES "CMAKE_SVN_PROG-NOTFOUND")
    message(ERROR "can't find svn")
  endif()
  find_program(CMAKE_AWK_PROG NAMES awk gawk)
  if(CMAKE_AWK_PROG MATCHES "CMAKE_AWK_PROG-NOTFOUND")
    message(ERROR "can't find awk or gawk")
  endif()
  execute_process(
    COMMAND ${CMAKE_SVN_PROG} info ${svn_dir}
    COMMAND ${CMAKE_AWK_PROG} "/Revision/ { print $2 }"
    OUTPUT_VARIABLE revision
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${out} ${revision})
endmacro(get_svn_revision)

## determines whether you have local changes in ${svn_dir} (${out} will be
## either the empty string if it's clean, or "-dirty" if you have changes)
macro(get_svn_wc_status svn_dir out)
  find_program(CMAKE_SVN_PROG svn)
  if(CMAKE_SVN_PROG MATCHES "CMAKE_SVN_PROG-NOTFOUND")
    message(ERROR "can't find svn")
  endif()
  find_program(CMAKE_AWK_PROG NAMES awk gawk)
  execute_process(
    COMMAND ${CMAKE_SVN_PROG} status -q ${svn_dir}
    OUTPUT_VARIABLE svn_status
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(svn_status MATCHES "^$")
    set(${out} "")
  else()
    set(${out} "-dirty")
  endif()
endmacro(get_svn_wc_status)

## gather machine info
uname("-m" machine_type)
real_executable_name("${CMAKE_C_COMPILER}" real_c_compiler)
get_svn_revision("${CMAKE_CURRENT_SOURCE_DIR}" svn_revision)  ## unused since it confuses cdash about history
get_svn_wc_status("${CMAKE_CURRENT_SOURCE_DIR}" wc_status)    ## unused since it confuses cdash about history
get_filename_component(branchname "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

## construct BUILDNAME, seems to have to happen before include(CTest)
set(BUILDNAME "${branchname} ${CMAKE_BUILD_TYPE} ${CMAKE_SYSTEM} ${machine_type} ${CMAKE_C_COMPILER_ID} ${real_c_compiler} ${CMAKE_C_COMPILER_VERSION}" CACHE STRING "CTest build name" FORCE)

include(CTest)

if (BUILD_TESTING)
  ## set up full valgrind suppressions file (concatenate the suppressions files)
  file(READ newbrt/valgrind.suppressions valgrind_suppressions)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" "${valgrind_suppressions}")
  file(READ src/tests/bdb.suppressions bdb_suppressions)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" "${bdb_suppressions}")
  file(READ bash.suppressions bash_suppressions)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" "${bash_suppressions}")

  ## setup a function to write tests that will run with helgrind
  set(CMAKE_HELGRIND_COMMAND_STRING "valgrind --quiet --tool=helgrind --error-exitcode=1 --suppressions=${TOKUDB_SOURCE_DIR}/src/tests/helgrind.suppressions --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,test,*/tokudb_dump* --trace-children-skip-by-arg=--only_create,--test,--no-shutdown")
  function(add_helgrind_test name)
    if (CMAKE_SYSTEM_NAME MATCHES Darwin OR
        ((CMAKE_C_COMPILER_ID MATCHES Intel) AND
         (CMAKE_BUILD_TYPE MATCHES Release)) OR
        USE_GCOV)
      ## can't use helgrind on osx or with optimized intel, no point in
      ## using it if we're doing coverage
      add_test(
        NAME ${name}
        COMMAND ${ARGN}
        )
    else ()
      separate_arguments(CMAKE_HELGRIND_COMMAND_STRING)
      add_test(
        NAME ${name}
        COMMAND ${CMAKE_HELGRIND_COMMAND_STRING} ${ARGN}
        )
    endif ()
  endfunction(add_helgrind_test)

  ## setup a function to write tests that will run with drd
  set(CMAKE_DRD_COMMAND_STRING "valgrind --quiet --tool=drd --error-exitcode=1 --suppressions=${TOKUDB_SOURCE_DIR}/src/tests/drd.suppressions --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,test,*/tokudb_dump* --trace-children-skip-by-arg=--only_create,--test,--no-shutdown")
  function(add_drd_test name)
    if (CMAKE_SYSTEM_NAME MATCHES Darwin OR
        ((CMAKE_C_COMPILER_ID MATCHES Intel) AND
         (CMAKE_BUILD_TYPE MATCHES Release)) OR
        USE_GCOV)
      ## can't use drd on osx or with optimized intel, no point in
      ## using it if we're doing coverage
      add_test(
        NAME ${name}
        COMMAND ${ARGN}
        )
    else ()
      separate_arguments(CMAKE_DRD_COMMAND_STRING)
      add_test(
        NAME ${name}
        COMMAND ${CMAKE_DRD_COMMAND_STRING} ${ARGN}
        )
    endif ()
  endfunction(add_drd_test)

  option(RUN_LONG_TESTS "If set, run all tests, even the ones that take a long time to complete." OFF)
  option(RUN_STRESS_TESTS "If set, run the stress tests." OFF)
  option(RUN_PERF_TESTS "If set, run the perf tests." OFF)

  configure_file(CTestCustom.cmake . @ONLY)
endif (BUILD_TESTING)
