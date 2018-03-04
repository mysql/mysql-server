# Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.
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
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  return()
endif()
 
macro(ALIAS_IF_UNSET_AND_SYMBOL_EXISTS alias symbol header)
  if(NOT ${alias})
    CHECK_SYMBOL_EXISTS(${symbol} ${header} HAVE_${symbol})
    if(HAVE_${symbol})
      set(${alias} "${symbol}")
    endif(HAVE_${symbol})
  endif(NOT ${alias})
endmacro()

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

check_include_files("unistd.h" HAVE_UNISTD_H)
check_include_files("Windows.h" HAVE_WINDOWS_H)

if(HAVE_UNISTD_H)
  CHECK_SYMBOL_EXISTS("sysconf" "unistd.h" HAVE_SYSCONF_SYMBOL)
  if(HAVE_SYSCONF_SYMBOL)
    ALIAS_IF_UNSET_AND_SYMBOL_EXISTS(_A__SC_PAGESIZE "_SC_PAGESIZE" "unistd.h")
    ALIAS_IF_UNSET_AND_SYMBOL_EXISTS(_A__SC_PAGESIZE "_SC_PAGE_SIZE" "unistd.h")	
    ALIAS_IF_UNSET_AND_SYMBOL_EXISTS(_A__SC_PAGESIZE "PAGESIZE" "unistd.h")
    if(_A__SC_PAGESIZE)
      add_definitions("-D_A__SC_PAGESIZE=${_A__SC_PAGESIZE}")
      CHECK_SYMBOL_EXISTS("_SC_PHYS_PAGES" "unistd.h" HAVE__SC_PHYS_PAGES)
    endif(_A__SC_PAGESIZE)
    CHECK_SYMBOL_EXISTS("_SC_NPROCESSORS_CONF" "unistd.h" HAVE__SC_NPROCESSORS_CONF)
  endif()
endif()

if(HAVE_WINDOWS_H)
  LIST(APPEND CMAKE_REQUIRED_INCLUDES "Windows.h")
  CHECK_SYMBOL_EXISTS("GlobalMemoryStatusEx" "Windows.h" 
    HAVE_GLOBALMEMORYSTATUSEX_SYMBOL)
  CHECK_SYMBOL_EXISTS("GlobalMemoryStatus" "Windows.h" 
    HAVE_GLOBALMEMORYSTATUS_SYMBOL)
  CHECK_SYMBOL_EXISTS("GetSystemInfo" "Windows.h" 
    HAVE_GETSYSTEMINFO_SYMBOL)

  macro(CHECK_WIN_SOURCE_COMPILES_ code var)
    CHECK_C_SOURCE_COMPILES("#include <Windows.h>
int main() { ${code}; return 0; }" ${var})
  endmacro()

  if(HAVE_GLOBALMEMORYSTATUSEX_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GlobalMemoryStatusEx(NULL)" 
      HAVE_GLOBALMEMORYSTATUSEX)
  endif()

  if(HAVE_GLOBALMEMORYSTATUS_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GlobalMemoryStatus(NULL)" 
      HAVE_GLOBALMEMORYSTATUS)
  endif()

  if(HAVE_GETSYSTEMINFO_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GetSystemInfo(NULL)" HAVE_GETSYSTEMINFO)
  endif()
endif()

configure_file("config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/host_info_config.h")
include_directories(${CMAKE_CURRENT_BINARY_DIR}) 

MYSQL_ADD_EXECUTABLE(host_info host_info.c
  DESTINATION "${MCC_INSTALL_SUBDIR}/host_info/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}"
  COMPONENT ClusterTools)

