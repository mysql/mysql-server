# Copyright (C) 2010 Sun Microsystems, Inc
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

# This file includes build settings used for MySQL release

OPTION(WITH_INNOBASE_STORAGE_ENGINE "" ON)
OPTION(WITH_ARCHIVE_STORAGE_ENGINE "" ON)
OPTION(WITH_BLACKHOLE_STORAGE_ENGINE "" ON)
OPTION(WITH_FEDERATED_STORAGE_ENGINE "" ON )
OPTION(WITHOUT_EXAMPLE_STORAGE_ENGINE "" ON)
OPTION(WITH_EMBEDDED_SERVER "" ON)
OPTION(ENABLE_LOCAL_INFILE "" ON)
SET(WITH_SSL bundled CACHE STRING "")
SET(WITH_ZLIB bundled CACHE STRING "")
  

IF(NOT COMPILATION_COMMENT)
  SET(COMPILATION_COMMENT "MySQL Community Server (GPL)")
ENDIF()

IF(UNIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local/mysql") 
  SET(WITH_EXTRA_CHARSETS complex CACHE STRING "")
  OPTION(WITH_READLINE  "" ON)
  OPTION(WITH_PIC "" ON)
  
  # Default GCC flags
  IF(CMAKE_COMPILER_IS_GNUCXX)
   SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O3 -static-libgcc -fno-omit-frame-pointer")
   SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3 -static-libgcc -fno-omit-frame-pointer -felide-constructors -fno-exceptions -fno-rtti")
  ENDIF()

  
  # HPUX flags
  IF(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
    IF(CMAKE_C_COMPILER_ID MATCHES "HP")
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "ia64")
        SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-g +O2 +DD64 +DSitanium2 -mt -AC99")
        SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g +O2 +DD64 +DSitanium2 -mt -Aa")
      ENDIF()
    ENDIF()
    SET(WITH_SSL)
  ENDIF()
  
  # Linux flags
  IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    IF(CMAKE_C_COMPILER_ID MATCHES "Intel")
      SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-static-intel -g -O3 -unroll2 -ip -mp -restrict")
      SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-static-intel -g -O3 -unroll2 -ip -mp -restrict")
    ENDIF()
  ENDIF()
  
  # OSX flags
  IF(APPLE)
   SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
   SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  ENDIF()
  
  # Solaris flags
  IF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    IF(CMAKE_SYSTEM_VERSION VERSION_GREATER "5.9")
      # Link mysqld with mtmalloc on Solaris 10 and later
      SET(WITH_MYSQLD_LDFLAGS "-lmtmalloc" CACHE STRING "")
    ENDIF()
    IF(CMAKE_C_COMPILER_ID MATCHES "SunPro")
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "i386")
        IF(CMAKE_SIZEOF_VOIDP EQUAL 4)
          # Solaris x86
          SET(CMAKE_C_FLAGS_RELWITHDEBINFO
            "-g -xO2 -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -xlibmil -xlibmopt -xtarget=generic")
          SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO
            "-g0 -xO2 -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -features=no%except -xlibmil -xlibmopt -xtarget=generic")
        ELSE()
          #  Solaris x64
          SET(CMAKE_C_FLAGS_RELWITHDEBINFO 
            "-g -xO3 -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -xlibmil -xlibmopt -xtarget=generic")
          SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
            "-g0 -xO3 -mt -fsimple=1 -ftrap=%none -nofstore -xbuiltin=%all -features=no%except -xlibmil -xlibmopt -xtarget=generic")
         ENDIF()
       ELSE() 
          
          IF(CMAKE_SIZEOF_VOIDP EQUAL 4)
            # Solaris sparc 32 bit
            SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -xO3 -Xa -xstrconst -mt -xarch=sparc")
            SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g0 -xO3 -noex -mt -xarch=sparc")
          ELSE()
            # Solaris sparc 64 bit
            SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -xO3 -Xa -xstrconst -mt")
            SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g0 -xO3 -noex -mt")
          ENDIF()
       ENDIF()
    ENDIF()
  ENDIF()
  
  IF(CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}"
      CACHE STRING "Compile flags")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}"
      CACHE STRING "Compile flags")
  ENDIF()
ENDIF()
