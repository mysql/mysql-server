# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
#

# Avoid system checks on Windows by pre-caching  results. Most of the system checks
# are not relevant for Windows anyway and it takes lot more time to run them,
# since CMake  to creates a Visual Studio project for each tiny test.
# Note that values are cached for VC++ only, MinGW would give slightly
# different results.


IF(MSVC)
SET(HAVE_POSIX_MEMALIGN CACHE INTERNAL "")
SET(HAVE_CLOCK_GETTIME CACHE INTERNAL "")
SET(HAVE_PTHREAD_CONDATTR_SETCLOCK CACHE INTERNAL "")
SET(HAVE_PTHREAD_SELF CACHE INTERNAL "")
SET(HAVE_SCHED_GET_PRIORITY_MIN CACHE INTERNAL "")
SET(HAVE_SCHED_GET_PRIORITY_MAX CACHE INTERNAL "")
SET(HAVE_SCHED_SETAFFINITY CACHE INTERNAL "")
SET(HAVE_SCHED_SETSCHEDULER CACHE INTERNAL "")
SET(HAVE_PROCESSOR_BIND CACHE INTERNAL "")
SET(HAVE_EPOLL_CREATE CACHE INTERNAL "")
SET(HAVE_MEMALIGN CACHE INTERNAL "")
SET(HAVE_SYSCONF CACHE INTERNAL "")
SET(HAVE_DIRECTIO CACHE INTERNAL "")
SET(HAVE_ATOMIC_SWAP32 CACHE INTERNAL "")
SET(HAVE_MLOCK CACHE INTERNAL "")
SET(HAVE_FFS CACHE INTERNAL "")
SET(HAVE_PTHREAD_MUTEXATTR_INIT CACHE INTERNAL "")
SET(HAVE_PTHREAD_MUTEXATTR_SETTYPE CACHE INTERNAL "")
SET(HAVE_PTHREAD_SETSCHEDPARAM CACHE INTERNAL "")
SET(HAVE_SUN_PREFETCH_H CACHE INTERNAL "")
SET(HAVE___BUILTIN_FFS CACHE INTERNAL "")
SET(HAVE___BUILTIN_CTZ CACHE INTERNAL "")
SET(HAVE___BUILTIN_CLZ CACHE INTERNAL "")
SET(HAVE__BITSCANFORWARD 1 CACHE INTERNAL "")
SET(HAVE__BITSCANREVERSE 1 CACHE INTERNAL "")
SET(HAVE_LINUX_SCHEDULING CACHE INTERNAL "")
SET(HAVE_SOLARIS_AFFINITY CACHE INTERNAL "")
SET(HAVE_LINUX_FUTEX CACHE INTERNAL "")
SET(HAVE_ATOMIC_H CACHE INTERNAL "")

SET(NDB_SIZEOF_CHAR 1 CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_CHAR TRUE CACHE INTERNAL "")
SET(NDB_SIZEOF_CHARP ${CMAKE_SIZEOF_VOID_P} CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_CHARP TRUE CACHE INTERNAL "")
SET(NDB_SIZEOF_INT 4 CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_INT TRUE CACHE INTERNAL "")
SET(NDB_SIZEOF_LONG 4 CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_LONG TRUE CACHE INTERNAL "")
SET(NDB_SIZEOF_LONG_LONG 8 CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_LONG_LONG TRUE CACHE INTERNAL "")
SET(NDB_SIZEOF_SHORT 2 CACHE INTERNAL "")
SET(HAVE_NDB_SIZEOF_SHORT TRUE CACHE INTERNAL "")

SET(NDB_BUILD_NDBMTD 1 CACHE INTERNAL "")
ENDIF()
