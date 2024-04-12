# Copyright (c) 2022, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

EXECUTE_PROCESS(
  COMMAND ./build_id_test
  OUTPUT_VARIABLE out1
  )
EXECUTE_PROCESS(
  COMMAND ${READELF_EXECUTABLE} -n ./build_id_test
  OUTPUT_VARIABLE out2
  )

STRING(REGEX MATCH "BuildID.sha1.=([0-9a-f]+)" match1 "${out1}")
SET(sha1 "${CMAKE_MATCH_1}")
STRING(REGEX MATCH "Build ID:[ ]*([0-9a-f]+)" match2 "${out2}")
SET(sha2 "${CMAKE_MATCH_1}")

IF(NOT "${sha1}" STREQUAL "${sha2}")
  MESSAGE(STATUS "./build_id_test output: ${out1}")
  MESSAGE(STATUS "${READELF_EXECUTABLE} ./build_id_test output: ${out2}")
  MESSAGE(FATAL_ERROR "Bad build id")
ENDIF()
