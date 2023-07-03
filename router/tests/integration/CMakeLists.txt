# Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

ADD_DEFINITIONS(-DSSL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/mysql-test/std_data/")
ADD_DEFINITIONS(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
FOREACH(test_file
    test_routing_direct.cc
    test_routing_reuse.cc
    test_routing_sharing.cc
    test_routing_sharing_constrained_pools.cc
    test_routing_sharing_restart.cc
    )
ADD_TEST_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
  MODULE "integration"
  SYSTEM_INCLUDE_DIRS ${GMOCK_INCLUDE_DIRS}
                      ${PROTOBUF_INCLUDE_DIR}
  LIB_DEPENDS
  mysqlxclient
  mysqlxmessages
  mysql_protocol
  ${PROTOBUF_LIBRARY}
  )
ENDFOREACH()
