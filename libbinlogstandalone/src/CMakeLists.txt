# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
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
