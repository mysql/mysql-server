# Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
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

# Common defines and includes

ADD_DEFINITIONS(-DHAVE_CONFIG_H)
INCLUDE_DIRECTORIES(${LIBEVENT_INCLUDE_DIR}/compat/sys
  ${LIBEVENT_INCLUDE_DIR})

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

IF ( WIN32 )
  INCLUDE_DIRECTORIES(${LIBEVENT_INCLUDE_DIR}/WIN32-Code
    ${LIBEVENT_INCLUDE_DIR}/compat)
ENDIF()

SET(CMAKE_C_FLAGS 
  "${CMAKE_C_FLAGS} -I${LIBEVENT_INCLUDE_DIR}")

SET(LIBEVENT_CORE_SOURCES
  event.h
  event-internal.h
  evutil.h
  log.h
  event.c
  buffer.c
  evbuffer.c
  log.c
  signal.c
  evutil.c)

SET(LIBEVENT_EXTRA_SOURCES
  event_tagging.c
  evhttp.h
  http-internal.h
  evdns.h
  evrpc.c
  evrpc.h
  evrpc-internal.h
  strlcpy.c
  strlcpy-internal.h)

IF ( NOT WIN32 )
  SET(LIBEVENT_OS_SPECIFIC_SOURCES select.c http.c)
ELSE()
  SET(LIBEVENT_OS_SPECIFIC_SOURCES WIN32-Code/win32.c)
ENDIF()


IF(HAVE_POLL_H)
  SET(LIBEVENT_POLL_SOURCES poll.c)
ENDIF()

IF(HAVE_SYS_EPOLL_H)
  SET(LIBEVENT_EPOLL_SOURCES epoll.c epoll_sub.c)
ENDIF()

IF(HAVE_SYS_DEVPOLL_H)
  SET(LIBEVENT_DEVPOLL_SOURCES devpoll.c)
ENDIF()

IF(HAVE_EVENT_PORTS)
  SET(LIBEVENT_EVPORT_SOURCES evport.c)
ENDIF()

IF(HAVE_WORKING_KQUEUE)
  SET(LIBEVENT_KQUEUE_SOURCES kqueue.c)
ENDIF()

SET(LIBEVENT_ALL_SRC ${LIBEVENT_CORE_SOURCES} ${LIBEVENT_EXTRA_SOURCES}
  ${LIBEVENT_OS_SPECIFIC_SOURCES} ${LIBEVENT_POLL_SOURCES}
  ${LIBEVENT_EPOLL_SOURCES} ${LIBEVENT_DEVPOLL_SOURCES}
  ${LIBEVENT_EVPORT_SOURCES} ${LIBEVENT_KQUEUE_SOURCES})

IF(MSVC)
  # The macro identifier is defined twice. The compiler uses the second macro definition.
  ADD_COMPILE_FLAGS(${LIBEVENT_ALL_SRC} COMPILE_FLAGS "/wd4005 /wd4267 /wd4244")
ENDIF()

ADD_CONVENIENCE_LIBRARY(event ${LIBEVENT_ALL_SRC})

# Use EXCLUDE_FROM_ALL to build only if another component
# which dependens on libevent is built
SET_PROPERTY(TARGET event PROPERTY EXCLUDE_FROM_ALL TRUE)
