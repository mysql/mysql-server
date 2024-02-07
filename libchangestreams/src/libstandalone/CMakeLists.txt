# Copyright (c) 2021, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

ADD_WSHADOW_WARNING()
DISABLE_MISSING_PROFILE_WARNING()

ADD_DEFINITIONS(-DSTANDALONE_BINLOG)

ADD_STATIC_LIBRARY(changestreams_standalone_static
  ${CS_LIB_SOURCES}
  LINK_LIBRARIES ext::zlib
  )

IF(WITH_CS_PROTOBUF)
  SET_TARGET_PROPERTIES(changestreams_standalone_static
    PROPERTIES COMPILE_FLAGS "${CS_LIB_PB_COMPILE_FLAGS}")
  TARGET_LINK_LIBRARIES(changestreams_standalone_static
    changestreams_pb_static)
ENDIF()

SET_TARGET_PROPERTIES(changestreams_standalone_static
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${CS_LIBS_ARCHIVE_OUTPUT_DIR})
