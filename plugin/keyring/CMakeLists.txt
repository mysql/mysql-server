# Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/common)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${SSL_INCLUDE_DIRS})


MYSQL_ADD_PLUGIN(keyring_file
                 common/keyring_key.cc
                 common/keys_container.cc
                 common/keyring_impl.cc
                 checker/checker.cc
                 checker/checker_factory.cc
                 keyring.cc
                 hash_to_buffer_serializer.cc
                 buffered_file_io.cc
                 buffer.cc
                 digest.cc
                 checker/checker_ver_1_0.cc
                 checker/checker_ver_2_0.cc
                 file_io.cc
                 LINK_LIBRARIES ${SSL_LIBRARIES}
                 MODULE_ONLY
                 MODULE_OUTPUT_NAME "keyring_file")

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(WITH_KEYRING_TEST)
  ADD_SUBDIRECTORY(keyring-test)
ENDIF()

IF(HAVE_DLOPEN)
    SET(DYNLIB_EXTENSION "so")
ENDIF()
