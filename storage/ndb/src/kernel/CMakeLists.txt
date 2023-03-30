# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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


# workaround for bug#25690926
# see -flifetime-dse at https://gcc.gnu.org/gcc-6/porting_to.html
# Memory allocation in NDB kernel relies on object properties that
# are set before calling the object constructor.
#
IF(CMAKE_COMPILER_IS_GNUCXX
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "6.0.0")
  STRING_APPEND(CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER} " -flifetime-dse=1")
ENDIF()

FOREACH(warning
    "cast-qual"
    # Warnings in Dbtup.hpp
    "class-memaccess"
    "format-overflow"
    "maybe-uninitialized"
    "uninitialized"      # RedoPageCache CTOR: m_hash(m_pool) with gcc 13
    "stringop-overflow"
    "unused-but-set-variable"
    "unused-parameter"
    )
  MY_CHECK_CXX_COMPILER_WARNING("${warning}" HAS_WARN_FLAG)
  IF(HAS_WARN_FLAG)
    STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_WARN_FLAG}")
  ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(error)
ADD_SUBDIRECTORY(blocks)
ADD_SUBDIRECTORY(vm)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/cmvmi
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbacc
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbdict
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbdih
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dblqh
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbtc
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbtup
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/ndbfs
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/ndbcntr
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/qmgr
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/trix
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/backup
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbutil
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/suma
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbtux
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbinfo
  ${CMAKE_CURRENT_SOURCE_DIR}/blocks/dbspj
  ${CMAKE_CURRENT_SOURCE_DIR}/error
  ${CMAKE_CURRENT_SOURCE_DIR}/vm
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  )

SET(NDBD_LIBS
  mysys
  ndbblocks
  ndberror
  ndbgeneral
  ndbkernel
  ndblogger
  ndbmgmapi
  ndbmgmcommon
  ndbportlib
  ndbsignaldata
  ndbtrace
  ndbtransport
)

IF(WIN32)
  # Add the resource files for logging to event log
  SET(NDBD_EXTRA_SRC
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/message.rc
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/MSG00001.bin)
ENDIF()

MYSQL_ADD_EXECUTABLE(ndbd
  ${NDBD_EXTRA_SRC}
  SimBlockList.cpp
  angel.cpp
  main.cpp
  ndbd.cpp

  DESTINATION ${INSTALL_SBINDIR}
  COMPONENT ClusterDataNode
  ENABLE_EXPORTS
  LINK_LIBRARIES ${NDBD_LIBS} ndbsched ${LIBDL}
  )

IF(WITH_NDBMTD)
  MYSQL_ADD_EXECUTABLE(ndbmtd
    ${NDBD_EXTRA_SRC}
    SimBlockList.cpp
    angel.cpp
    main.cpp
    ndbd.cpp

    DESTINATION ${INSTALL_SBINDIR}
    COMPONENT ClusterDataNode
    ENABLE_EXPORTS
    LINK_LIBRARIES ${NDBD_LIBS} ndbsched_mt ${LIBDL}
    )
ENDIF()
