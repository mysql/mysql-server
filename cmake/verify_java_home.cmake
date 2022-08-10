# Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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

IF(DEFINED ENV{JAVA_HOME})
  FILE(TO_CMAKE_PATH "$ENV{JAVA_HOME}" JAVA_HOME_ENV_NORMALIZED)
  FILE(TO_CMAKE_PATH "${JAVA_HOME}" JAVA_HOME_NORMALIZED)
  IF(NOT ${JAVA_HOME_ENV_NORMALIZED} STREQUAL "${JAVA_HOME_NORMALIZED}")
    MESSAGE(FATAL_ERROR "JAVA_HOME must have same value as was configured, please do\n"
      "export JAVA_HOME=${JAVA_HOME}")
  ENDIF()
ELSE()
  MESSAGE(FATAL_ERROR "No JAVA_HOME set, cached value is ${JAVA_HOME}, please do\n"
    "export JAVA_HOME=${JAVA_HOME}"
    )
ENDIF()
