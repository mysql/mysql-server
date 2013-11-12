# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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
  endif(HAVE_SYSCONF_SYMBOL)
endif(HAVE_UNISTD_H)

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
  endmacro(CHECK_WIN_SOURCE_COMPILES_)

  if(HAVE_GLOBALMEMORYSTATUSEX_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GlobalMemoryStatusEx(NULL)" 
      HAVE_GLOBALMEMORYSTATUSEX)
  endif(HAVE_GLOBALMEMORYSTATUSEX_SYMBOL)

  if(HAVE_GLOBALMEMORYSTATUS_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GlobalMemoryStatus(NULL)" 
      HAVE_GLOBALMEMORYSTATUS)
  endif(HAVE_GLOBALMEMORYSTATUS_SYMBOL)

  if(HAVE_GETSYSTEMINFO_SYMBOL)
    CHECK_WIN_SOURCE_COMPILES_("GetSystemInfo(NULL)" HAVE_GETSYSTEMINFO)
  endif(HAVE_GETSYSTEMINFO_SYMBOL)
endif(HAVE_WINDOWS_H)

configure_file("config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/host_info_config.h")
include_directories(${CMAKE_CURRENT_BINARY_DIR}) 

if(DEFAULT_MYSQL_HOME)
  MYSQL_ADD_EXECUTABLE(host_info host_info.c
    DESTINATION "${MCC_INSTALL_SUBDIR}/host_info/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}"
    COMPONENT ClusterTools)
else()
  # If not building as part of Cluster we don't have the
  # MYSQL_ADD_EXECUTABLE macro available, so we just use the normal
  # add_executable command...
  add_executable(host_info host_info.c)

  # ... and install the old way
  # "Install" binary for this host into source tree
  install(TARGETS host_info
    DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/binaries/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")

  # Install all binaries in source tree into install dir/package
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/binaries" 
    DESTINATION "${MCC_INSTALL_SUBDIR}/host_info"
    USE_SOURCE_PERMISSIONS)
endif()

if (FALSE)
  add_custom_command(TARGET host_info POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E echo Checking/Updating manifest of 
    $(ConfigurationName)/$(TargetFileName)
    COMMAND mt.exe -verbose -inputresource:$(ConfigurationName)/$(TargetFileName) 
        -out:$(ConfigurationName)/$(TargetFileName).binary.manifest
      
        COMMAND ${CMAKE_COMMAND} -E compare_files 
        host_info.dir/$(ConfigurationName)/$(TargetFileName).embed.manifest 
        $(ConfigurationName)/$(TargetFileName).binary.manifest
      
        COMMAND mt.exe -verbose 
        -manifest host_info.dir/$(ConfigurationName)/$(TargetFileName).embed.manifest 
        -outputresource:$(ConfigurationName)/$(TargetFileName))
endif(FALSE)

# "Install" binary for this host into source tree
#install(TARGETS host_info
#  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/binaries/${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")

# Install all binaries in source tree into install dir/package
#install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/binaries" 
#  DESTINATION "${MCC_INSTALL_SUBDIR}/host_info"
#  USE_SOURCE_PERMISSIONS)
