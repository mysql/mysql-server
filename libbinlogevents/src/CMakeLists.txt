# Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.
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
     statement_events.cpp
     control_events.cpp
     load_data_events.cpp
     rows_event.cpp
     binlog_event.cpp
     binary_log_funcs.cpp
     uuid.cpp
    )

# Configure for building static library
ADD_LIBRARY(binlogevents_static STATIC ${REPLICATION_SOURCES})

IF(STANDALONE_BINLOG)
  TARGET_LINK_LIBRARIES(binlogevents_static ${ZLIB_LIBRARIES})
ELSE()
  TARGET_LINK_LIBRARIES(binlogevents_static ${ZLIB_LIBRARIES} mysys dbug)
  ADD_DEPENDENCIES(binlogevents_static GenError)
ENDIF()

SET_TARGET_PROPERTIES(binlogevents_static PROPERTIES
                      OUTPUT_NAME "binlogevents")

SET_TARGET_PROPERTIES(binlogevents_static PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/libbinlogevents/lib)

