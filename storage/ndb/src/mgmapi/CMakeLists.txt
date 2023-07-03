# Copyright (c) 2008, 2022, Oracle and/or its affiliates.
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


INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/mgmapi
  ${CMAKE_SOURCE_DIR}/storage/ndb/include/kernel
)
#ADD_DEFINITIONS(-DNDB_MGMAPI)
ADD_CONVENIENCE_LIBRARY(ndbmgmapi
  mgmapi.cpp
  mgmapi_error.cpp
  ndb_logevent.cpp
  mgmapi_configuration.cpp
  LocalConfig.cpp
  ndbd_exit_codes.cpp
  LINK_LIBRARIES ndbmgmcommon ndbportlib ndbgeneral
  )

NDB_ADD_TEST(mgmapi-t mgmapi-t.cpp LIBS ndbmgmapi ndbgeneral)
