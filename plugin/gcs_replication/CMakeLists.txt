# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(LIBCPG_PATH /usr/lib/corosync /usr/lib64/corosync CACHE STRING "Default locations for libcpg (cluster library)" )

  CHECK_INCLUDE_FILES(corosync/cpg.h HAVE_COROSYNC_CPG_H)
  find_library(LIBCPG cpg ${LIBCPG_PATH})
  find_library(LIBPTHREAD pthread ${LIBPTHREAD_PATH})
  if (LIBCPG)
    CHECK_LIBRARY_EXISTS (${LIBCPG} cpg_local_get "" HAVE_LIBCPG)
    CHECK_INCLUDE_FILES (corosync/cpg.h HAVE_COROSYNC_CPG_H)
  endif (LIBCPG)

  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/plugin/gcs_replication/include)
  SET(GCS_SOURCES
    gcs_plugin.cc gcs_plugin.h
    observer_server_state.cc observer_server_state.h
    observer_trans.cc observer_trans.h
    gcs_certifier.cc gcs_certifier.h
    gcs_event_handlers.cc gcs_event_handlers.h
    gcs_protocol_factory.cc gcs_corosync.cc
    pipeline_factory.h pipeline_factory.cc
    gcs_applier.h gcs_applier.cc
    gcs_commit_validation.cc gcs_commit_validation.h
    gcs_payload.cc gcs_member_info.cc gcs_protocol.cc gcs_message.cc
    handlers/certification_handler.h
    handlers/certification_handler.cc
    handlers/event_cataloger.h
    handlers/event_cataloger.cc
    handlers/applier_sql_thread.h
    handlers/applier_sql_thread.cc
  )
  ADD_LIBRARY(gcs STATIC gcs_protocol_factory.cc gcs_corosync.cc)

  MYSQL_ADD_PLUGIN(gcs_replication ${GCS_SOURCES}
  MODULE_ONLY MODULE_OUTPUT_NAME "gcs_replication_plugin")

  if (LIBCPG)
    TARGET_LINK_LIBRARIES(gcs ${LIBCPG} ${LIBPTHREAD} )
    TARGET_LINK_LIBRARIES(gcs_replication gcs)
  endif(LIBCPG)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")
