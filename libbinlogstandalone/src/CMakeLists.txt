# Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
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

# This configuration file builds both the static and shared version of
# the library.
SET (REPLICATION_SOURCES
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/load_data_events.cpp
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/rows_event.cpp
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/binlog_event.cpp
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/control_events.cpp
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/statement_events.cpp
     ${CMAKE_SOURCE_DIR}/libbinlogevents/src/uuid.cpp
    )

# Configure for building static library
ADD_LIBRARY(binlogstandalone_static STATIC ${REPLICATION_SOURCES})

TARGET_LINK_LIBRARIES(binlogstandalone_static ${ZLIB_LIBRARIES})

SET_TARGET_PROPERTIES(binlogstandalone_static PROPERTIES
                      OUTPUT_NAME "binlogstandalone")

SET_TARGET_PROPERTIES(binlogstandalone_static PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/libbinlogstandalone/lib)

IF (NOT DISABLE_SHARED)
  # Configure for building shared library
  ADD_LIBRARY(binlogstandalone_shared SHARED ${REPLICATION_SOURCES})
  TARGET_LINK_LIBRARIES(binlogstandalone_shared ${ZLIB_LIBRARIES})
  SET_TARGET_PROPERTIES(binlogstandalone_shared
    PROPERTIES OUTPUT_NAME "binlogstandalone")
  SET_TARGET_PROPERTIES(binlogstandalone_shared
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/libbinlogstandalone/lib)

  # For gcc on SunOS we need to add C flags to the link command
  GET_TARGET_PROPERTY(binlogstandalone_shared_link_flags
    binlogstandalone_shared LINK_FLAGS)
  IF(NOT binlogstandalone_shared_link_flags)
    SET(binlogstandalone_shared_link_flags)
  ENDIF()
  SET(binlogstandalone_shared_link_flags
    "${CMAKE_SHARED_LIBRARY_C_FLAGS} ${binlogstandalone_shared_link_flags}")
  SET_TARGET_PROPERTIES(binlogstandalone_shared
    PROPERTIES LINK_FLAGS "${binlogstandalone_shared_link_flags}")
ENDIF()

# INSTALL(TARGETS binlogstandalone_shared LIBRARY DESTINATION lib)
