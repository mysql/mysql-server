# Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
# Rules for checking that the abi/api has not changed.
#
# The following steps are followed in the do_abi_check rule below
#
# 1) Generate preprocessor output for the files that need to
#    be tested for abi/api changes. use -nostdinc to prevent
#    generation of preprocessor output for system headers. This
#    results in messages in stderr saying that these headers
#    were not found. Redirect the stderr output to /dev/null
#    to prevent seeing these messages.
# 2) sed the output to
#    2.1) remove blank lines and lines that begin with "# "
#    2.2) When gcc -E is run on the Mac OS  and solaris sparc platforms it
#         introduces a line of output that shows up as a difference between
#         the .pp and .out files. Remove these OS specific preprocessor text
#         inserted by the preprocessor.
# 3) diff the generated file and the canons (.pp files already in
#    the repository).
# 4) delete the .out file that is generated.
#
# If the diff fails, the generated file is not removed. This will
# be useful for analysis of ABI differences (e.g. using a visual
# diff tool).
#
# A ABI change that causes a build to fail will always be accompanied
# by new canons (.out files). The .out files that are not removed will
# be replaced as the new .pp files.
#
# e.g. If include/mysql/plugin.h has an ABI change then this rule would
# leave a <build directory>/abi_check.out file.
#
# A developer with a justified API change will then do a
# mv <build directory>/abi_check.out include/mysql/plugin.pp
# to replace the old canons with the new ones.
#

SET(abi_check_out ${BINARY_DIR}/abi_check.out)


SET(ABI_SOURCE_DIR ${SOURCE_DIR})
SET(ABI_BINARY_DIR ${BINARY_DIR})

# If we're using WSL on Windows, we have to translate paths using wslpath
IF(WSL_EXECUTABLE)
  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} wslpath ${ABI_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE ABI_SOURCE_DIR)
  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} wslpath ${ABI_BINARY_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE ABI_BINARY_DIR)
  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} wslpath ${abi_check_out}
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE abi_current)
ELSE()
  SET (abi_current ${abi_check_out})
ENDIF()

FOREACH(file ${ABI_HEADERS})
  GET_FILENAME_COMPONENT(header_basename ${file} NAME)
  SET(tmpfile ${BINARY_DIR}/${header_basename}.pp.tmp)
  SET(abi_file ${file})
  IF(WSL_EXECUTABLE)
    EXECUTE_PROCESS(
      COMMAND ${WSL_EXECUTABLE} wslpath ${abi_file}
      OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE abi_file)
  ENDIF()

  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} ${COMPILER}
      -E -nostdinc -dI -DMYSQL_ABI_CHECK -I${ABI_SOURCE_DIR}/include
      -I${ABI_BINARY_DIR}/include -I${ABI_SOURCE_DIR}/include/mysql
      -I${ABI_SOURCE_DIR}/sql -I${ABI_SOURCE_DIR}/libbinlogevents/export
      ${abi_file}
      ERROR_QUIET OUTPUT_FILE ${tmpfile})
  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} sed -e "/^# /d"
                -e "/^[	]*$/d"
                -e "/^#pragma GCC set_debug_pwd/d"
                -e "/^#ident/d"
    RESULT_VARIABLE result OUTPUT_FILE ${abi_check_out} INPUT_FILE ${tmpfile})
  IF(NOT ${result} EQUAL 0)
    MESSAGE(FATAL_ERROR "sed returned error ${result}")
  ENDIF()
  FILE(REMOVE ${tmpfile})

  IF(WSL_EXECUTABLE)
    EXECUTE_PROCESS(
      COMMAND ${WSL_EXECUTABLE} wslpath ${file}
      OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE abi_original)
  ELSE()
    SET (abi_original ${file})
  ENDIF()

  EXECUTE_PROCESS(
    COMMAND ${WSL_EXECUTABLE} diff -w ${abi_original}.pp ${abi_current}
    RESULT_VARIABLE result)
  IF(NOT ${result} EQUAL 0)
    MESSAGE(FATAL_ERROR
      "ABI check found difference between ${file}.pp and ${abi_check_out}")
  ENDIF()
ENDFOREACH()
