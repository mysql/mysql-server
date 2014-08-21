set(CTEST_PROJECT_NAME "tokudb")
get_filename_component(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/.." ABSOLUTE)

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

## gather machine info
uname("-m" machine_type)
get_filename_component(branchname "${CTEST_SOURCE_DIRECTORY}" NAME)

set(ncpus 2)
execute_process(
  COMMAND grep bogomips /proc/cpuinfo
  COMMAND wc -l
  RESULT_VARIABLE res
  OUTPUT_VARIABLE proc_ncpus
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(NOT res)
  set(ncpus ${proc_ncpus})
endif()

## construct BUILDNAME
set(BUILDNAME "${branchname} ${CMAKE_SYSTEM} ${machine_type}" CACHE STRING "CTest build name" FORCE)
set(CTEST_BUILD_NAME "${BUILDNAME}")
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
site_name(CTEST_SITE)

find_program(CTEST_SVN_COMMAND NAMES svn)
find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
find_program(CTEST_COVERAGE_COMMAND NAMES gcov)

list(APPEND CTEST_NOTES_FILES
  "${CTEST_SCRIPT_DIRECTORY}/${CTEST_SCRIPT_NAME}"
  "${CMAKE_CURRENT_LIST_FILE}"
  )

set(all_opts
  -DBUILD_TESTING=ON
  -DUSE_CILK=OFF
  )
set(rel_opts
  ${all_opts}
  -DCMAKE_BUILD_TYPE=Release
  )
set(dbg_opts
  ${all_opts}
  -DCMAKE_BUILD_TYPE=Debug
  )
set(cov_opts
  ${all_opts}
  -DCMAKE_BUILD_TYPE=Debug
  -DUSE_GCOV=ON
  )

set(CTEST_BINARY_DIRECTORY "${CTEST_SOURCE_DIRECTORY}/NightlyRelease")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
ctest_start(Nightly ${CTEST_SOURCE_DIRECTORY} ${CTEST_BINARY_DIRECTORY})
ctest_update(SOURCE ${CTEST_SOURCE_DIRECTORY})
ctest_configure(BUILD ${CTEST_BINARY_DIRECTORY} SOURCE ${CTEST_SOURCE_DIRECTORY}
  OPTIONS "${rel_opts}")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake" "${CTEST_BINARY_DIRECTORY}/CTestConfig.cmake")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestCustom.cmake" "${CTEST_BINARY_DIRECTORY}/CTestCustom.cmake")
ctest_build(BUILD ${CTEST_BINARY_DIRECTORY})
ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")
ctest_test(BUILD ${CTEST_BINARY_DIRECTORY} PARALLEL_LEVEL ${ncpus})
ctest_submit()

set(CTEST_BINARY_DIRECTORY "${CTEST_SOURCE_DIRECTORY}/NightlyDebug")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
ctest_start(Nightly ${CTEST_SOURCE_DIRECTORY} ${CTEST_BINARY_DIRECTORY})
ctest_configure(BUILD ${CTEST_BINARY_DIRECTORY} SOURCE ${CTEST_SOURCE_DIRECTORY}
  OPTIONS "${dbg_opts}")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake" "${CTEST_BINARY_DIRECTORY}/CTestConfig.cmake")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestCustom.cmake" "${CTEST_BINARY_DIRECTORY}/CTestCustom.cmake")
ctest_build(BUILD ${CTEST_BINARY_DIRECTORY})
ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")
ctest_test(BUILD ${CTEST_BINARY_DIRECTORY} PARALLEL_LEVEL ${ncpus})
ctest_memcheck(BUILD ${CTEST_BINARY_DIRECTORY} PARALLEL_LEVEL ${ncpus})
ctest_submit()

set(CTEST_BINARY_DIRECTORY "${CTEST_SOURCE_DIRECTORY}/NightlyCoverage")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
ctest_start(Nightly ${CTEST_SOURCE_DIRECTORY} ${CTEST_BINARY_DIRECTORY})
ctest_configure(BUILD ${CTEST_BINARY_DIRECTORY} SOURCE ${CTEST_SOURCE_DIRECTORY}
  OPTIONS "${cov_opts}")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake" "${CTEST_BINARY_DIRECTORY}/CTestConfig.cmake")
configure_file("${CTEST_SOURCE_DIRECTORY}/CTestCustom.cmake" "${CTEST_BINARY_DIRECTORY}/CTestCustom.cmake")
ctest_build(BUILD ${CTEST_BINARY_DIRECTORY})
ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")
ctest_test(BUILD ${CTEST_BINARY_DIRECTORY} PARALLEL_LEVEL ${ncpus})
ctest_coverage(BUILD ${CTEST_BINARY_DIRECTORY} LABELS RUN_GCOV)
ctest_submit()
