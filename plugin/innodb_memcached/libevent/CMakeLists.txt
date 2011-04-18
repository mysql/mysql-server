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

# Common defines and includes
ADD_DEFINITIONS(-DHAVE_CONFIG_H)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/libevent/compat/sys
		    ${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/libevent)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_SHARED_LIBRARY_C_FLAGS} -I${CMAKE_SOURCE_DIR}/plugin/innodb_memcached/libevent")

SET(LIBEVENT_CORE_SOURCES
		event.h
		event-internal.h
		evutil.h
		log.h
		event.c
		buffer.c
		evbuffer.c
		log.c
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
		strlcpy-internal.h
		strlcpy-internal.h
		strlcpy-internal.h
		signal.c
		poll.c
		select.c
		epoll.c
		epoll_sub.c)

ADD_LIBRARY(event SHARED ${LIBEVENT_CORE_SOURCES} ${LIBEVENT_EXTRA_SOURCES})
ADD_LIBRARY(builtin_event STATIC ${LIBEVENT_CORE_SOURCES} ${LIBEVENT_EXTRA_SOURCES})
