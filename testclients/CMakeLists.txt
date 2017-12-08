# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

MYSQL_ADD_EXECUTABLE(mysql_client_test mysql_client_test.cc COMPONENT Test)
TARGET_LINK_LIBRARIES(mysql_client_test mysqlclient)
SET_TARGET_PROPERTIES(mysql_client_test PROPERTIES LINKER_LANGUAGE CXX)

IF(WITH_UNIT_TESTS)
  MYSQL_ADD_EXECUTABLE(bug25714 bug25714.cc SKIP_INSTALL)
  TARGET_LINK_LIBRARIES(bug25714 mysqlclient)
  SET_TARGET_PROPERTIES(bug25714 PROPERTIES LINKER_LANGUAGE CXX)
ENDIF()
