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

SET(PBXT_SOURCES
src/bsearch_xt.cc
src/bsearch_xt.h
src/cache_xt.cc
src/cache_xt.h
src/ccutils_xt.cc
src/ccutils_xt.h
src/database_xt.cc
src/database_xt.h
src/datadic_xt.cc
src/datadic_xt.h
src/datalog_xt.cc
src/datalog_xt.h
src/discover_xt.cc
src/discover_xt.h
src/filesys_xt.cc
src/filesys_xt.h
src/hashtab_xt.cc
src/hashtab_xt.h
src/ha_pbxt.cc
src/ha_pbxt.h
src/ha_xtsys.cc
src/ha_xtsys.h
src/heap_xt.cc
src/heap_xt.h
src/index_xt.cc
src/index_xt.h
src/linklist_xt.cc
src/linklist_xt.h
src/locklist_xt.cc
src/locklist_xt.h
src/lock_xt.cc
src/lock_xt.h
src/memory_xt.cc
src/memory_xt.h
src/myxt_xt.cc
src/myxt_xt.h
src/pbms.h
src/pbms_enabled.cc
src/pbms_enabled.h
src/pthread_xt.cc
src/pthread_xt.h
src/restart_xt.cc
src/restart_xt.h
src/sortedlist_xt.cc
src/sortedlist_xt.h
src/strutil_xt.cc
src/strutil_xt.h
src/systab_xt.cc
src/systab_xt.h
src/tabcache_xt.cc
src/tabcache_xt.h
src/table_xt.cc
src/table_xt.h
src/thread_xt.cc
src/thread_xt.h
src/trace_xt.cc
src/trace_xt.h
src/util_xt.cc
src/util_xt.h
src/xaction_xt.cc
src/xaction_xt.h
src/xactlog_xt.cc
src/xactlog_xt.h
src/xt_config.h
src/xt_defs.h
src/xt_errno.h)

MYSQL_STORAGE_ENGINE(PBXT)
