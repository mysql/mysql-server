
# Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 

# This file includes Linux specific options and quirks, related to system checks

INCLUDE(CheckSymbolExists)

# Something that needs to be set on legacy reasons
SET(TARGET_OS_LINUX 1)
SET(HAVE_NPTL 1)
SET(_GNU_SOURCE 1)

# Fix CMake (< 2.8) flags. -rdynamic exports too many symbols.
FOREACH(LANG C CXX)
  STRING(REPLACE "-rdynamic" "" 
  CMAKE_SHARED_LIBRARY_LINK_${LANG}_FLAGS
  ${CMAKE_SHARED_LIBRARY_LINK_${LANG}_FLAGS}  
  )
ENDFOREACH()

# Ensure we have clean build for shared libraries
# without unresolved symbols
# Not supported with AddressSanitizer
IF(NOT WITH_ASAN)
  SET(LINK_FLAG_NO_UNDEFINED "-Wl,--no-undefined")
ENDIF()

# 64 bit file offset support flag
SET(_FILE_OFFSET_BITS 64)

# Linux specific HUGETLB /large page support
CHECK_SYMBOL_EXISTS(SHM_HUGETLB sys/shm.h  HAVE_DECL_SHM_HUGETLB)
IF(HAVE_DECL_SHM_HUGETLB)
  SET(HAVE_LARGE_PAGES 1)
  SET(HUGETLB_USE_PROC_MEMINFO 1)
  SET(HAVE_LARGE_PAGE_OPTION 1)
ENDIF()
