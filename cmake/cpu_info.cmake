# Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.
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

# Symbols with information about the CPU.

FIND_PROGRAM(GETCONF getconf)
MARK_AS_ADVANCED(GETCONF)

IF(GETCONF)
  EXECUTE_PROCESS(
    COMMAND ${GETCONF} LEVEL1_DCACHE_LINESIZE
    OUTPUT_VARIABLE CPU_LEVEL1_DCACHE_LINESIZE
    )
ENDIF()
IF(CPU_LEVEL1_DCACHE_LINESIZE AND CPU_LEVEL1_DCACHE_LINESIZE GREATER 0)
ELSE()
  SET(CPU_LEVEL1_DCACHE_LINESIZE 64)
ENDIF()
