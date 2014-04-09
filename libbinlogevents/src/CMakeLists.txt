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

# This configuration file builds both the static and shared version of
# the library.
SET (REPLICATION_SOURCES
     field_iterator.cpp
     load_data_events.cpp
     row_of_fields.cpp
     rows_event.cpp
     value.cpp
     binlog_event.cpp
     decoder.cpp
    )

# Configure for building static library
ADD_LIBRARY(binlogevents_static STATIC ${REPLICATION_SOURCES})

IF(HAVE_MYSYS)
  TARGET_LINK_LIBRARIES(binlogevents_static ${ZLIB_LIBRARIES} mysys dbug)
ELSE()
  TARGET_LINK_LIBRARIES(binlogevents_static ${ZLIB_LIBRARIES})
ENDIF()

SET_TARGET_PROPERTIES(binlogevents_static PROPERTIES
                      OUTPUT_NAME "binlogevents")

# Configure for building shared library
IF(NOT HAVE_MYSYS)
  ADD_LIBRARY(binlogevents_shared SHARED ${REPLICATION_SOURCES})
  TARGET_LINK_LIBRARIES(binlogevents_shared ${ZLIB_LIBRARIES})
  SET_TARGET_PROPERTIES(binlogevents_shared PROPERTIES
                        OUTPUT_NAME "binlogevents")
  INSTALL(TARGETS binlogevents_shared
          LIBRARY DESTINATION lib)
ENDIF()
