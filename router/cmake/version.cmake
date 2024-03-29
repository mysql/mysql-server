# Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

# Version information of MySQL Router
# we use MySQL server version
INCLUDE(${CMAKE_SOURCE_DIR}/cmake/mysql_version.cmake)

# Project version, has to be an X.Y.Z number since it is used with the
# "project" CMake command
SET(PROJECT_VERSION_TEXT ${MYSQL_NO_DASH_VERSION})

SET(PROJECT_COMPILATION_COMMENT ${COMPILATION_COMMENT})

# Can be arbitrary test that is added to the package file names after
# the version, but before the extensions.
SET(PROJECT_PACKAGE_EXTRAS "")

# create a string that is allowed in a RPM spec "release" field
SET(RPM_EXTRA_VERSION "${PROJECT_PACKAGE_EXTRAS}")
IF(RPM_EXTRA_VERSION)
  STRING(REGEX REPLACE "[^A-Za-z0-9]" "" RPM_EXTRA_VERSION "${RPM_EXTRA_VERSION}")
  SET(RPM_EXTRA_VERSION ".${RPM_EXTRA_VERSION}")
ENDIF()

# Nothing below this line needs change when releasing
