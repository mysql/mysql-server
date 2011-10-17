# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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


ADD_DEFINITIONS(-DHAVE_CONFIG_H)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/compat/sys)

# libevent cannot currently be built with -Werror
IF(CMAKE_C_FLAGS)
  STRING(REPLACE "-Werror" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
ENDIF()

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
                http.c
                evhttp.h
                http-internal.h
                evdns.c
                evdns.h
                evrpc.c
                evrpc.h
                evrpc-internal.h
                strlcpy.c
                strlcpy-internal.h)

IF(HAVE_SIGNAL_H)
        SET(LIBEVENT_SIGNAL_SOURCES signal.c)
ENDIF()

IF(HAVE_POLL_H)
        SET(LIBEVENT_POLL_SOURCES poll.c)
ENDIF()

IF(HAVE_SELECT)
        SET(LIBEVENT_SELECT_SOURCE select.c)
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

ADD_CONVENIENCE_LIBRARY(event 
                         ${LIBEVENT_CORE_SOURCES} 
                         ${LIBEVENT_EXTRA_SOURCES} 
                         ${LIBEVENT_SIGNAL_SOURCES} 
                         ${LIBEVENT_POLL_SOURCES} 
                         ${LIBEVENT_SELECT_SOURCE} 
                         ${LIBEVENT_EPOLL_SOURCES} 
                         ${LIBEVENT_DEVPOLL_SOURCES} 
                         ${LIBEVENT_EVPORT_SOURCES} 
                         ${LIBEVENT_KQUEUE_SOURCES})

if(LIBRT) 
  target_link_libraries(event rt)
endif()

