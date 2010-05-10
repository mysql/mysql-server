# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved. 
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 

CONFIGURE_FILE(unit.pl ${CMAKE_CURRENT_BINARY_DIR}/unit.pl COPYONLY)

SET(unittests "")
LIST(APPEND unittests "mysys")
LIST(APPEND unittests "mytap")
LIST(APPEND unittests "gunit")
LIST(APPEND unittests "../storage")

ADD_CUSTOM_TARGET(
  test-unit
  COMMAND perl unit.pl run ${unittests}
)

# Create whitespace separated list.
FOREACH(arg ${unittests})
  SET(testdirs "${testdirs} ${arg}")
ENDFOREACH()

CONFIGURE_FILE(run-unit-tests.in
  ${CMAKE_CURRENT_BINARY_DIR}/run-unit-tests @ONLY)
IF(UNIX)
  EXECUTE_PROCESS(
    COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/run-unit-tests)
ENDIF()
