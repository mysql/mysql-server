# Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

ADD_LIBRARY(harness_net_ts STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/internet.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/socket.cpp
  )
TARGET_INCLUDE_DIRECTORIES(harness_net_ts
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_SOURCES(harness_net_ts
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/executor.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/file.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/io_service_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/kqueue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/kqueue_io_service.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/linux_epoll.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/linux_epoll_io_service.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/netif.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/poll.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/poll_io_service.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/resolver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/socket_constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/socket_error.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/socket_service.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/impl/socket_service_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/internet.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/io_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/local.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/netfwd.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/timer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mysql/harness/net_ts/win32_named_pipe.h
  )
TARGET_LINK_LIBRARIES(harness_net_ts INTERFACE harness_stdx)

IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  TARGET_LINK_LIBRARIES(harness_net_ts INTERFACE socket;nsl)
ENDIF()
