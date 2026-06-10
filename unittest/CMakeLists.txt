# Copyright (c) 2010, 2026, Oracle and/or its affiliates.
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

ADD_WSHADOW_WARNING()

ADD_SUBDIRECTORY(gunit)

INCLUDE(ProcessorCount)
ProcessorCount(CORES)
IF (CORES EQUAL 0)
  SET(CORES 1)
ENDIF()

# Use env var CTEST_PARALLEL_LEVEL if found.
# Note that this must be set permanently in the environment.
# CTEST_PARALLEL_LEVEL=42 make test-unit will not catch the new value.
IF(DEFINED ENV{CTEST_PARALLEL_LEVEL})
  SET(CORES $ENV{CTEST_PARALLEL_LEVEL})
ENDIF()

IF(DEFINED CTEST_PARALLEL_LEVEL)
  SET(CORES ${CTEST_PARALLEL_LEVEL})
ENDIF()

FIND_PROGRAM(CTEST_EXECUTABLE NAMES ctest3 ctest)

IF(WITH_RAPID OR ENABLE_GCOV)
  SET(TIMEOUT 600)
ELSE()
  SET(TIMEOUT 300)
ENDIF()

# 'man ctest' is confusing: --timeout vs. --test-timeout?
# The latter affects BUILD AND TEST MODE.
ADD_CUSTOM_TARGET(
  test-unit
  COMMAND ${CTEST_EXECUTABLE}
  --output-on-failure
  --parallel ${CORES}
  --progress
  --timeout ${TIMEOUT}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running ctest with --parallel=${CORES}"
  USES_TERMINAL
  )

# An alias target.
ADD_CUSTOM_TARGET(run-unit-tests DEPENDS test-unit)
# CMake 4.1.0 adds test-unit to the default build when run-unit-tests DEPENDS on it,
# which is an undesired side-effect, so we explicitly exclude it again.
SET_PROPERTY(TARGET test-unit PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
