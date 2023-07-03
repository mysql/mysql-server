# Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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

# get the INCLUDE_DIRECTORIES manually, as cmake 3.6.x will fail if
#
#   $<TARGET_PROPERTY,mysqlxmessages_lite:INCLUDE_DIRECTORIES>
#
# is used in TARGET_INCLUDE_DIRECTORIES().

GET_TARGET_PROPERTY(XMSG_INC_DIRS mysqlxmessages_lite
  INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(XMSG_SYS_INC_DIRS mysqlxmessages_lite
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)

# depends on mysqlxmessage_lite to ensure the .pb.h headers
# are already built before they are included.
ADD_CONVENIENCE_LIBRARY(router_mysqlxmessages_exports
  mysqlxmessages_exports.cc
  INCLUDE_DIRECTORIES ${XMSG_INC_DIRS}
  SYSTEM_INCLUDE_DIRECTORIES ${XMSG_SYS_INC_DIRS}
  DEPENDENCIES
    mysqlxmessages_lite
  )

IF(WIN32)
  ADD_SHARED_LIBRARY(router_mysqlxmessages
    mysqlxmessages_exports.cc
    COMPONENT Router
    DESTINATION ${ROUTER_INSTALL_LIBDIR}
    LINK_LIBRARIES
      mysqlxmessages_lite
      ${PROTOBUF_LITE_LIBRARY}
    OUTPUT_NAME "mysqlrouter_mysqlxmessages"
    SOVERSION 1
    )
ELSE()
  MERGE_LIBRARIES_SHARED(router_mysqlxmessages
    router_mysqlxmessages_exports
    EXPORTS
      _mysqlrouter_mysqlxmessages_exports
    COMPONENT Router
    DESTINATION ${ROUTER_INSTALL_LIBDIR}
    LINK_LIBRARIES
      mysqlxmessages_lite
    OUTPUT_NAME "mysqlrouter_mysqlxmessages"
    SOVERSION 1
    NAMELINK_SKIP
    )

  TARGET_LINK_LIBRARIES(router_mysqlxmessages
    PUBLIC
    ${PROTOBUF_LITE_LIBRARY}
    )
ENDIF()

TARGET_INCLUDE_DIRECTORIES(router_mysqlxmessages
  PUBLIC ${XMSG_INC_DIRS}
  )
TARGET_INCLUDE_DIRECTORIES(router_mysqlxmessages
  SYSTEM PUBLIC ${XMSG_SYS_INC_DIRS}
  )
