# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

INCLUDE_DIRECTORIES(SYSTEM ${BOOST_PATCHES_DIR} ${BOOST_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/common)

SET(
    KEYRING_FILE_SOURCES
    common/keyring_key.cc
    common/keys_container.cc
    common/keys_iterator.cc
    common/keyring_impl.cc
    keyring.cc
    hash_to_buffer_serializer.cc
    buffered_file_io.cc
    file_io.cc
    buffer.cc
)

MYSQL_ADD_PLUGIN(keyring_file
                 ${KEYRING_FILE_SOURCES}
                 MODULE_ONLY
                 MODULE_OUTPUT_NAME "keyring_file")

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(HAVE_DLOPEN)
    SET(DYNLIB_EXTENSION "so")
    SET_TARGET_PROPERTIES(keyring_file
      PROPERTIES LINK_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
ENDIF()

# Boost source has unused local typedefs.
MY_CHECK_CXX_COMPILER_FLAG("-Wunused-local-typedefs" HAVE_UNUSED_TYPEDEFS)
IF(HAVE_UNUSED_TYPEDEFS)
  ADD_COMPILE_FLAGS(${KEYRING_FILE_SOURCES}
    COMPILE_FLAGS "-Wno-unused-local-typedefs")
ENDIF()
