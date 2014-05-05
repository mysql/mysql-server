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
      ERROR_QUIET
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

## gives the current username
macro(whoami out)
  execute_process(
    COMMAND whoami
    OUTPUT_VARIABLE ${out}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(whoami)

## gives the current hostname, minus .tokutek.com if it's there
macro(hostname out)
  execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE fullhostname
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX REPLACE "\\.tokutek\\.com$" "" ${out} "${fullhostname}")
endmacro(hostname)

## gather machine info
uname("-m" machine_type)
real_executable_name("${CMAKE_CXX_COMPILER}" real_cxx_compiler)
get_filename_component(branchname "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
hostname(host)
whoami(user)

## construct SITE, seems to have to happen before include(CTest)
set(SITE "${user}@${host}")
if (USE_GCOV)
  set(buildname_build_type "Coverage")
else (USE_GCOV)
  set(buildname_build_type "${CMAKE_BUILD_TYPE}")
endif (USE_GCOV)
## construct BUILDNAME, seems to have to happen before include(CTest)
set(BUILDNAME "${branchname} ${buildname_build_type} ${CMAKE_SYSTEM} ${machine_type} ${CMAKE_CXX_COMPILER_ID} ${real_cxx_compiler} ${CMAKE_CXX_COMPILER_VERSION}" CACHE STRING "CTest build name" FORCE)

include(CTest)

set(TOKUDB_DATA "${TokuDB_SOURCE_DIR}/../tokudb.data" CACHE FILEPATH "Path to data files for tests")

if (BUILD_TESTING OR BUILD_FT_TESTS OR BUILD_SRC_TESTS)
  set(WARNED_ABOUT_DATA 0)
  if (NOT EXISTS "${TOKUDB_DATA}/" AND NOT WARNED_ABOUT_DATA AND CMAKE_PROJECT_NAME STREQUAL TokuDB)
    message(WARNING "Test data files are missing from ${TOKUDB_DATA}, which will cause some tests to fail.  Please put them there or modify TOKUDB_DATA to avoid this.")
    set(WARNED_ABOUT_DATA 1)
  endif ()

  ## set up full valgrind suppressions file (concatenate the suppressions files)
  file(READ ft/valgrind.suppressions valgrind_suppressions)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" "${valgrind_suppressions}")
  file(READ bash.suppressions bash_suppressions)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/valgrind.suppressions" "${bash_suppressions}")

  include(CMakeDependentOption)
  set(helgrind_drd_depend_conditions "")
  ## Helgrind and DRD explicitly state that they only run with the Linux
  ## glibc-2.3 NPTL threading implementation [1,2].  If this ever changes
  ## we can enable helgrind and drd on other systems.
  ## [1]: http://valgrind.org/docs/manual/hg-manual.html#hg-manual.effective-use
  ## [2]: http://valgrind.org/docs/manual/drd-manual.html#drd-manual.limitations
  list(APPEND helgrind_drd_depend_conditions "CMAKE_SYSTEM_NAME STREQUAL Linux")
  ## no point doing it with gcov
  list(APPEND helgrind_drd_depend_conditions "NOT USE_GCOV")
  cmake_dependent_option(RUN_DRD_TESTS "Run some tests under drd." ON
    "${helgrind_drd_depend_conditions}" OFF)
  cmake_dependent_option(RUN_HELGRIND_TESTS "Run some tests under helgrind." ON
    "${helgrind_drd_depend_conditions}" OFF)

  macro(setup_toku_test_properties test str)
    set_tests_properties(${test} PROPERTIES ENVIRONMENT "TOKU_TEST_FILENAME=${str}.ctest-data")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${str}.ctest-data")
  endmacro(setup_toku_test_properties)

  macro(add_toku_test_aux pfx name bin)
    add_test(${pfx}/${name} ${bin} ${ARGN})
    setup_toku_test_properties(${pfx}/${name} ${name})
  endmacro(add_toku_test_aux)
  macro(add_toku_test pfx bin)
    add_toku_test_aux(${pfx} ${bin} ${bin} ${ARGN})
  endmacro(add_toku_test)

  ## setup a function to write tests that will run with helgrind
  set(CMAKE_HELGRIND_COMMAND_STRING "valgrind --quiet --tool=helgrind --error-exitcode=1 --soname-synonyms=somalloc=*tokuportability* --suppressions=${TokuDB_SOURCE_DIR}/src/tests/helgrind.suppressions --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,grep,*/grep,date,*/date,test,*/tokudb_dump* --trace-children-skip-by-arg=--only_create,--test,--no-shutdown,novalgrind")
  function(add_helgrind_test pfx name)
    separate_arguments(CMAKE_HELGRIND_COMMAND_STRING)
    add_test(
      NAME ${pfx}/${name}
      COMMAND ${CMAKE_HELGRIND_COMMAND_STRING} ${ARGN}
      )
    setup_toku_test_properties(${pfx}/${name} ${name})
  endfunction(add_helgrind_test)

  ## setup a function to write tests that will run with drd
  set(CMAKE_DRD_COMMAND_STRING "valgrind --quiet --tool=drd --error-exitcode=1 --soname-synonyms=somalloc=*tokuportability* --suppressions=${TokuDB_SOURCE_DIR}/src/tests/drd.suppressions --trace-children=yes --trace-children-skip=sh,*/sh,basename,*/basename,dirname,*/dirname,rm,*/rm,cp,*/cp,mv,*/mv,cat,*/cat,diff,*/diff,grep,*/grep,date,*/date,test,*/tokudb_dump* --trace-children-skip-by-arg=--only_create,--test,--no-shutdown,novalgrind")
  function(add_drd_test pfx name)
    separate_arguments(CMAKE_DRD_COMMAND_STRING)
    add_test(
      NAME ${pfx}/${name}
      COMMAND ${CMAKE_DRD_COMMAND_STRING} ${ARGN}
      )
    setup_toku_test_properties(${pfx}/${name} ${name})
  endfunction(add_drd_test)

  option(RUN_LONG_TESTS "If set, run all tests, even the ones that take a long time to complete." OFF)
  option(RUN_STRESS_TESTS "If set, run the stress tests." OFF)
  option(RUN_PERF_TESTS "If set, run the perf tests." OFF)

  configure_file(CTestCustom.cmake.in CTestCustom.cmake @ONLY)
endif (BUILD_TESTING OR BUILD_FT_TESTS OR BUILD_SRC_TESTS)
