# Copyright (C) 2007 MySQL AB
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
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmsrv
                    ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
)

LINK_LIBRARIES(ndbcmvmi
               ndbdbacc
               ndbdbdict
               ndbdbdih
               ndbdblqh
               ndbdbtc
               ndbdbtup
               ndbndbfs
               ndbndbcntr
               ndbqmgr
               ndbtrix
               ndbbackup
               ndbdbutil
               ndbsuma
               ndbdbtux
               ndbkernel
               ndberror
               ndbtransport
               ndbtrace
               ndbsignaldata
               ndblogger
               ndbmgmsrv
               ndbmgmapi
               ndbportlib
               ndbgeneral
               ndbblocks
               ndbsched
               dbug
               mysys
               strings
               ndbconf
               ${NDB_SCI_LIBS})
ADD_EXECUTABLE(ndbd
               main.cpp
               SimBlockList.cpp
              ../mgmsrv/ConfigInfo.cpp
              ../mgmsrv/Config.cpp
              ../mgmsrv/InitConfigFileParser.cpp
)
