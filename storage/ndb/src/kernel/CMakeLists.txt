# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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
  STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BTYPE)
  SET(CMAKE_CXX_FLAGS_${BTYPE} "${CMAKE_CXX_FLAGS_${BTYPE}} -flifetime-dse=1")
ENDIF()


ADD_SUBDIRECTORY(error)
ADD_SUBDIRECTORY(blocks)
ADD_SUBDIRECTORY(vm)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/blocks/cmvmi
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
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
)

SET(NDBD_LIBS ndbblocks ndbkernel ndberror ndbtransport
    ndbtrace ndbsignaldata ndblogger ndbmgmcommon ndbmgmapi
    ndbportlib ndbgeneral dbug mysys strings
)

IF(WIN32)
  # Add the resource files for logging to event log
  SET(NDBD_EXTRA_SRC
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/message.rc
    ${CMAKE_SOURCE_DIR}/storage/ndb/src/common/logger/msg00001.bin)
ENDIF()

MYSQL_ADD_EXECUTABLE(ndbd
  main.cpp ndbd.cpp angel.cpp SimBlockList.cpp ${NDBD_EXTRA_SRC}
  DESTINATION ${INSTALL_SBINDIR}
  COMPONENT ClusterDataNode)
TARGET_LINK_LIBRARIES(ndbd ${NDBD_LIBS} ndbsched ${LIBDL})

IF(WITH_NDBMTD)
  MYSQL_ADD_EXECUTABLE(ndbmtd
    main.cpp ndbd.cpp angel.cpp SimBlockList.cpp ${NDBD_EXTRA_SRC}
    DESTINATION ${INSTALL_SBINDIR}
    COMPONENT ClusterDataNode)
  TARGET_LINK_LIBRARIES(ndbmtd ${NDBD_LIBS} ndbsched_mt ${LIBDL})
ENDIF()
