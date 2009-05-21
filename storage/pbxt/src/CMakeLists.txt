# Copyright (c) 2008 PrimeBase Technologies GmbH
# 
# PrimeBase XT
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# 2006-03-22	Paul McCullagh
#
# H&G2JCtL
#
# This file is used to make the Windows version

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMYSQL_SERVER")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMYSQL_SERVER")

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DMYSQL_SERVER -DSAFEMALLOC -DSAFE_MUTEX -DDEBUG")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DMYSQL_SERVER -DSAFEMALLOC -DSAFE_MUTEX -DDEBUG")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/sql
                    ${CMAKE_SOURCE_DIR}/regex
                    ${CMAKE_SOURCE_DIR}/extra/yassl/include)

SET(PBXT_SOURCES ha_pbxt.cc bsearch_xt.cc index_xt.cc strutil_xt.cc cache_xt.cc linklist_xt.cc
                 ccutils_xt.cc lock_xt.cc table_xt.cc database_xt.cc thread_xt.cc
                 datadic_xt.cc memory_xt.cc trace_xt.cc datalog_xt.cc myxt_xt.cc util_xt.cc
                 filesys_xt.cc pthread_xt.cc xaction_xt.cc restart_xt.cc xactlog_xt.cc
                 hashtab_xt.cc sortedlist_xt.cc heap_xt.cc streaming_xt.cc tabcache_xt.cc
				 systab_xt.cc ha_xtsys.cc discover_xt.cc
                 bsearch_xt.h linklist_xt.h tabcache_xt.h cache_xt.h lock_xt.h table_xt.h
                 ccutils_xt.h thread_xt.h database_xt.h memory_xt.h trace_xt.h
                 datadic_xt.h pbms.h util_xt.h datalog_xt.h myxt_xt.h xaction_xt.h
                 filesys_xt.h pthread_xt.h xactlog_xt.h ha_pbxt.h restart_xt.h xt_config.h
                 hashtab_xt.h sortedlist_xt.h xt_defs.h heap_xt.h streaming_xt.h xt_errno.h
				 systab_xt.h ha_xtsys.h discover_xt.h
                 index_xt.h strutil_xt.h)

IF(NOT SOURCE_SUBLIBS)
  ADD_LIBRARY(pbxt ${PBXT_SOURCES})
  ADD_DEPENDENCIES(pbxt GenError)
ENDIF(NOT SOURCE_SUBLIBS)

