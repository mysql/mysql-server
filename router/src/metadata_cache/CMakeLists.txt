# Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SET(METADATA_CACHE_SOURCES
  src/metadata_factory.cc
  src/cluster_metadata.cc
  src/cluster_metadata_ar.cc
  src/cluster_metadata_gr.cc
  src/metadata_cache.cc
  src/metadata_cache_ar.cc
  src/metadata_cache_gr.cc
  src/cache_api.cc
  src/group_replication_metadata.cc
  src/gr_notifications_listener.cc
)

SET(include_dirs
  ../router/include
  include/
  src/
)

INCLUDE_DIRECTORIES(SYSTEM ${PROTOBUF_INCLUDE_DIR}
  ${MYSQLX_GENERATE_DIR}
  ${CMAKE_SOURCE_DIR}/plugin/x/client
)

add_harness_plugin(metadata_cache SOURCES
  src/metadata_cache_plugin.cc
  src/plugin_config.cc
  ${METADATA_CACHE_SOURCES}
  REQUIRES router_lib
  DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
  )

TARGET_INCLUDE_DIRECTORIES(metadata_cache PRIVATE ${include_dirs})
TARGET_LINK_LIBRARIES(metadata_cache PRIVATE
  mysqlxclient_lite
  mysqlxmessages_lite
  ${PROTOBUF_LITE_LIBRARY}
  )

ADD_DEPENDENCIES(mysqlrouter_all mysqlxclient_lite)

IF(WIN32)
  INSTALL(TARGETS metadata_cache
    RUNTIME DESTINATION "lib"
    COMPONENT Router)
ENDIF()

IF(WITH_UNIT_TESTS AND GMOCK_FOUND)
  ADD_LIBRARY(metadata_cache_static STATIC ${METADATA_CACHE_SOURCES})
  TARGET_LINK_LIBRARIES(metadata_cache_static
    router_lib
    mysqlclient
    mysqlxmessages_lite
    ${PROTOBUF_LITE_LIBRARY}
    )
  TARGET_COMPILE_DEFINITIONS(metadata_cache_static
    PRIVATE -Dmetadata_cache_DEFINE_STATIC=1)
  TARGET_INCLUDE_DIRECTORIES(metadata_cache_static PRIVATE ${include_dirs})

  ADD_SUBDIRECTORY(tests/)
ENDIF()
