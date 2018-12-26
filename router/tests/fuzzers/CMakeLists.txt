# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(CMakePushCheckState)

# check if clang knows about the coverage and trace-pc-guard

# llvm 4.0+
cmake_push_check_state(RESET)
SET(CMAKE_REQUIRED_FLAGS "-fsanitize-coverage=trace-pc-guard")
CHECK_CXX_COMPILER_FLAG("-fsanitize-coverage=trace-pc-guard"
                        COMPILER_HAS_SANITIZE_COVERAGE_TRACE_PC_GUARD)
cmake_pop_check_state()

# llvm 3.8+
cmake_push_check_state(RESET)
SET(CMAKE_REQUIRED_FLAGS "-fsanitize-coverage=edge")
CHECK_CXX_COMPILER_FLAG("-fsanitize-coverage=edge"
                        COMPILER_HAS_SANITIZE_COVERAGE_TRACE_EDGE)
cmake_pop_check_state()

# http://llvm.org/docs/LibFuzzer.html#tracing-cmp-instructions
cmake_push_check_state(RESET)
SET(CMAKE_REQUIRED_FLAGS "-fsanitize-coverage=trace-cmp")
CHECK_CXX_COMPILER_FLAG("-fsanitize-coverage=trace-cmp"
                        COMPILER_HAS_SANITIZE_COVERAGE_TRACE_CMP)
cmake_pop_check_state()

cmake_push_check_state(RESET)
CHECK_CXX_COMPILER_FLAG("-fprofile-instr-generate"
                        COMPILER_HAS_PROFILE_INSTR_GENERATE)
cmake_pop_check_state()

cmake_push_check_state(RESET)
# invalid argument '-fcoverage-mapping' only allowed
# with '-fprofile-instr-generate'
SET(CMAKE_REQUIRED_FLAGS "-fprofile-instr-generate")
CHECK_CXX_COMPILER_FLAG("-fcoverage-mapping" COMPILER_HAS_COVERAGE_MAPPING)
cmake_pop_check_state()


IF(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  IF(COMPILER_HAS_SANITIZE_COVERAGE_TRACE_PC_GUARD)
    SET(SANITIZE_COVERAGE_FLAGS "-fsanitize-coverage=trace-pc-guard")
  ELSEIF(COMPILER_HAS_SANITIZE_COVERAGE_TRACE_EDGE)
    SET(SANITIZE_COVERAGE_FLAGS "-fsanitize-coverage=edge")
  ENDIF()

  # check that libFuzzer is found
  #
  # check_library_exists() doesn't work here as it would provide a main() which
  # calls a test-function ... which collides with libFuzzer's main():
  #
  # if the libFuzzer is found by the compiler it will provide a 'main()' and
  # require that we provide a 'LLVMFuzzerTestOneInput' at link-time.
  IF(SANITIZE_COVERAGE_FLAGS)
    cmake_push_check_state(RESET)
    SET(CMAKE_REQUIRED_LIBRARIES Fuzzer)
    SET(CMAKE_REQUIRED_FLAGS ${SANITIZE_COVERAGE_FLAGS})
    CHECK_CXX_SOURCE_COMPILES("
    extern \"C\" int LLVMFuzzerTestOneInput (void *, int)
    { return 0; }"
    CLANG_HAS_LIBFUZZER)
    cmake_pop_check_state()
  ENDIF()

  IF(CLANG_HAS_LIBFUZZER)
    SET(LIBFUZZER_LIBRARIES Fuzzer)
    SET(LIBFUZZER_LINK_FLAGS ${SANITIZE_COVERAGE_FLAGS})
    SET(LIBFUZZER_COMPILE_FLAGS)
    LIST(APPEND LIBFUZZER_COMPILE_FLAGS ${SANITIZE_COVERAGE_FLAGS})

    IF(COMPILER_HAS_PROFILE_INSTR_GENERATE)
      LIST(APPEND LIBFUZZER_COMPILE_FLAGS -fprofile-instr-generate)
      SET(LIBFUZZER_LINK_FLAGS
          "${LIBFUZZER_LINK_FLAGS} -fprofile-instr-generate")
      LIST(APPEND LIBFUZZER_COMPILE_FLAGS -fcoverage-mapping)
    ENDIF()
  ENDIF()
ENDIF()


SET(FUZZ_TARGETS)
SET(FUZZ_COVERAGE_TARGETS)

MACRO(FUZZ FUZZ_TARGET)
  SET(FUZZ_MAX_TOTAL_TIME 10)
  SET(FUZZ_TIMEOUT 0.1)


  ADD_CUSTOM_TARGET(${FUZZ_TARGET}_mkdir_corpus
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_CURRENT_BINARY_DIR}/corpus/${FUZZ_TARGET}"
  )
  ADD_CUSTOM_TARGET(${FUZZ_TARGET}_mkdir_artificats
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${FUZZ_TARGET}"
  )

  # use cmake -E env to set the LLVM_PROFILE_FILE in a portable way
  ADD_CUSTOM_TARGET(${FUZZ_TARGET}_run
    COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${FUZZ_TARGET}.profraw
            $<TARGET_FILE:${FUZZ_TARGET}>
            -max_total_time=${FUZZ_MAX_TOTAL_TIME} -timeout=${FUZZ_TIMEOUT}
            -artifact_prefix=${CMAKE_CURRENT_BINARY_DIR}/artifacts/${FUZZ_TARGET}/
            ${CMAKE_CURRENT_BINARY_DIR}/corpus/${FUZZ_TARGET}/
            ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${FUZZ_TARGET}/
    DEPENDS ${FUZZ_TARGET} ${FUZZ_TARGET}_mkdir_artificats ${FUZZ_TARGET}_mkdir_corpus
    COMMENT "Runnning ${FUZZ_TARGET} for ${FUZZ_MAX_TOTAL_TIME} seconds"
  )

  LIST(APPEND FUZZ_TARGETS "${FUZZ_TARGET}_run")

  IF(COMPILER_HAS_PROFILE_INSTR_GENERATE
     AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.8")
    # detect the major-minor of the clang++ as we have to call the same-version
    # llvm-profdata
    #
    # if we got clang 4.0, then we have to call llvm-cov 4.0 too. On Ubuntu
    # (and elsewhere) parallel installs use the style:
    #
    # clang-4.0
    # llvm-cov-4.0
    #
    # and may have a fallback to llvm-cov.
    #
    # Prefer the longer names.
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version
                    OUTPUT_VARIABLE clang_full_version_string )
    string (REGEX REPLACE ".*clang version ([0-9]+\\.[0-9]+).*" "\\1"
            CLANG_VERSION_STRING ${clang_full_version_string})

    FIND_PROGRAM(LLVM_PROFDATA llvm-profdata-${CLANG_VERSION_STRING} llvm-profdata)
    add_custom_command(OUTPUT ${FUZZ_TARGET}.profdata
      COMMAND ${LLVM_PROFDATA} merge -sparse ${FUZZ_TARGET}.profraw -o ${FUZZ_TARGET}.profdata
      DEPENDS ${FUZZ_TARGET}_run
    )
    FIND_PROGRAM(LLVM_COV llvm-cov-${CLANG_VERSION_STRING} llvm-cov)
    add_custom_command(OUTPUT ${FUZZ_TARGET}-coverage.html
      COMMAND ${LLVM_COV} show $<TARGET_FILE:${FUZZ_TARGET}>
              -instr-profile=${FUZZ_TARGET}.profdata -format=html >
              ${FUZZ_TARGET}-coverage.html
      DEPENDS ${FUZZ_TARGET}.profdata
    )

    LIST(APPEND FUZZ_COVERAGE_TARGETS "${FUZZ_TARGET}-coverage.html")
  ENDIF()
ENDMACRO()

IF(CLANG_HAS_LIBFUZZER)
  SET(FUZZ_TARGET fuzz_router_uri)

  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  add_executable(${FUZZ_TARGET}
    fuzz_router_uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc
  )

  SET_TARGET_PROPERTIES(
    ${FUZZ_TARGET}
    PROPERTIES
    COMPILE_OPTIONS "${LIBFUZZER_COMPILE_FLAGS}"
    LINK_FLAGS "${LIBFUZZER_LINK_FLAGS}"
    )
  TARGET_LINK_LIBRARIES(${FUZZ_TARGET} harness-library ${MySQL_CLIENT_LIB}
                        ${SSL_LIBRARIES} ${LIBFUZZER_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(${FUZZ_TARGET}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/router/include/)
  fuzz(${FUZZ_TARGET})


  SET(FUZZ_TARGET fuzz_router_uri_tostring)

  # FIXME: uri.cc depends on split_string() from utils.cc which pulls in
  # the whole harness. utils.cc should be split into a smaller chunks
  # (string, file, ...) to allow to build directly against it.
  #
  add_executable(${FUZZ_TARGET}
    fuzz_router_uri_tostring.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/uri.cc
    ${PROJECT_SOURCE_DIR}/src/router/src/utils.cc
  )

  SET_TARGET_PROPERTIES(
    ${FUZZ_TARGET}
    PROPERTIES
    COMPILE_OPTIONS "${LIBFUZZER_COMPILE_FLAGS}"
    LINK_FLAGS "${LIBFUZZER_LINK_FLAGS}"
    )
  TARGET_LINK_LIBRARIES(${FUZZ_TARGET} harness-library ${MySQL_CLIENT_LIB}
                        ${SSL_LIBRARIES} ${LIBFUZZER_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(${FUZZ_TARGET}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/router/include/)
  fuzz(${FUZZ_TARGET})

ENDIF()

ADD_CUSTOM_TARGET(fuzz_coverage
  DEPENDS ${FUZZ_COVERAGE_TARGETS}
  COMMENT "Generatoring Coverage data")

ADD_CUSTOM_TARGET(fuzz
  DEPENDS ${FUZZ_TARGETS}
  COMMENT "Running fuzzers")
