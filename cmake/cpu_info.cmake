# Copyright (c) 2009, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA 

# Symbols with information about the CPU.

FIND_PROGRAM(GETCONF getconf)
MARK_AS_ADVANCED(GETCONF)

IF(GETCONF AND NOT SOLARIS AND NOT APPLE)
  EXECUTE_PROCESS(
    COMMAND ${GETCONF} LEVEL1_DCACHE_LINESIZE
    OUTPUT_VARIABLE CPU_LEVEL1_DCACHE_LINESIZE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  EXECUTE_PROCESS(
      COMMAND ${GETCONF} PAGE_SIZE
      OUTPUT_VARIABLE CPU_PAGE_SIZE
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
ENDIF()
IF(CPU_LEVEL1_DCACHE_LINESIZE AND CPU_LEVEL1_DCACHE_LINESIZE GREATER 0)
ELSE()
  SET(CPU_LEVEL1_DCACHE_LINESIZE 64)
ENDIF()
IF(CPU_PAGE_SIZE AND CPU_PAGE_SIZE GREATER 0)
ELSE()
  SET(CPU_PAGE_SIZE 4096)
ENDIF()
