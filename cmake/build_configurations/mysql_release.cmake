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

 
SET(FEATURE_SET "community" CACHE STRING 
" Selection of features. Options are
 - xsmall : 
 - small: embedded
 - classic: embedded + archive + federated + blackhole 
 - large :  embedded + archive + federated + blackhole + innodb
 - xlarge:  embedded + archive + federated + blackhole + innodb + partition
 - community:  all  features (currently == xlarge)
"
)

SET(FEATURE_SET_xsmall  1)
SET(FEATURE_SET_small   2)
SET(FEATURE_SET_classic 3)
SET(FEATURE_SET_large   5)
SET(FEATURE_SET_xlarge  6)
SET(FEATURE_SET_community 7)

IF(FEATURE_SET)
  STRING(TOLOWER ${FEATURE_SET} feature_set)
  SET(num ${FEATURE_SET_${feature_set}})
  IF(NOT num)
   MESSAGE(FATAL_ERROR "Invalid FEATURE_SET option '${feature_set}'. 
   Should be xsmall, small, classic, large, or community
   ")
  ENDIF()
  SET(WITH_PARTITION_STORAGE_ENGINE OFF)
  IF(num EQUAL FEATURE_SET_xsmall)
    SET(WITH_NONE ON)
  ENDIF()
  
  IF(num GREATER FEATURE_SET_xsmall)
    SET(WITH_EMBEDDED_SERVER ON CACHE BOOL "")
  ENDIF()
  IF(num GREATER FEATURE_SET_small)
    SET(WITH_ARCHIVE_STORAGE_ENGINE  ON)
    SET(WITH_BLACKHOLE_STORAGE_ENGINE ON)
    SET(WITH_FEDERATED_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_classic)
    SET(WITH_INNOBASE_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_large)
    SET(WITH_PARTITION_STORAGE_ENGINE ON)
  ENDIF()
  IF(num GREATER FEATURE_SET_xlarge)
   # OPTION(WITH_ALL ON) 
   # better no set this, otherwise server would be linked 
   # statically with experimental stuff like audit_null
  ENDIF()
  
  # Update cache with current values, remove engines we do not care about
  # from build.
  FOREACH(eng ARCHIVE BLACKHOLE FEDERATED INNOBASE PARTITION EXAMPLE)
    IF(NOT WITH_${eng}_STORAGE_ENGINE)
      SET(WITHOUT_${eng}_STORAGE_ENGINE ON CACHE BOOL "")
      MARK_AS_ADVANCED(WITHOUT_${eng}_STORAGE_ENGINE)
      SET(WITH_${eng}_STORAGE_ENGINE OFF CACHE BOOL "")
    ELSE()
     SET(WITH_${eng}_STORAGE_ENGINE ON CACHE BOOL "")
	ENDIF()
  ENDFOREACH()
ENDIF()

SET(WITHOUT_AUDIT_NULL ON CACHE BOOL "")
SET(WITHOUT_DAEMON_EXAMPLE ON CACHE BOOL "")

OPTION(ENABLE_LOCAL_INFILE "" ON)
SET(WITH_SSL bundled CACHE STRING "")
SET(WITH_ZLIB bundled CACHE STRING "")


IF(NOT COMPILATION_COMMENT)
  SET(COMPILATION_COMMENT "MySQL Community Server (GPL)")
ENDIF()

IF(WIN32)
  # Sign executables with authenticode certificate
  SET(SIGNCODE 1 CACHE BOOL "")
ENDIF()

IF(UNIX)
  SET(WITH_EXTRA_CHARSETS all CACHE STRING "")
  IF(EXISTS "${CMAKE_SOURCE_DIR}/COPYING")
    OPTION(WITH_READLINE  "" ON)
  ELSE()
    OPTION(WITH_LIBEDIT  "" ON)
  ENDIF()

  OPTION(WITH_PIC "" ON) # Why?
ENDIF()


# Compiler options
IF(UNIX)  
  # Default GCC flags
  IF(CMAKE_COMPILER_IS_GNUCXX)
   SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O3 -static-libgcc -fno-omit-frame-pointer")
   SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3 -static-libgcc -fno-omit-frame-pointer -felide-constructors -fno-exceptions -fno-rtti")
  ENDIF()

  
  # HPUX flags
  IF(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
    IF(CMAKE_C_COMPILER_ID MATCHES "HP")
      IF(CMAKE_SYSTEM_PROCESSOR MATCHES "ia64")
        SET(CMAKE_C_FLAGS 
          "${CMAKE_C_FLAGS} +DD64 +DSitanium2 -mt -AC99")
        SET(CMAKE_CXX_FLAGS 
          "${CMAKE_CXX_FLAGS} +DD64 +DSitanium2 -mt -Aa")
        SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} +O2")
        SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} +O2")
      ENDIF()
    ENDIF()
    SET(WITH_SSL)
  ENDIF()
  
  # Linux flags
  IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    IF(CMAKE_C_COMPILER_ID MATCHES "Intel")
      SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-static-intel -g -O3 -unroll2 -ip -mp -restrict")
      SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-static-intel -g -O3 -unroll2 -ip -mp -restrict")
      SET(WITH_SSL no)
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
        IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
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
          IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
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
