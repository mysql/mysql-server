# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

SET (KEYRING_FILE_SOURCES
     common/keyring_key.cc
     common/keys_container.cc
     common/keys_iterator.cc
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
     file_io.cc)

# On Windows we always link the ssl library into each plugin
# because the MySQL server binary's symbols on windows are hidden
# so we need a separate copy of the SSL library in each plugin on that OS
#
# On unix by default all mysqld symbols
# (including the SSL library symbols of the library copy linked to it)
# are visible.
# So technicaly all plugins can use the mysqld's ssl library symbols on that OS.
# But since the plugins are already initializing the SSL library
# because of windows we still are linking a copy of the SSL library
# relying on the fact that the dynamic linker will prefer the symbols
# in the shared library to the ones exported by the hosting executable (mysqld).
# This causes problems with yaSSL, as it, unlike openssl,
# does not hide all of its global variables.
# So we get address sanitizer errors because of the global variable symbols
# duplicated when we statically link a copy of yassl into the plugins.
#
# So for yaSSL we are using the mysqld copy and rely on the fact
# that the initialization/deinitialization of the library is reentrant
# and can be done by both the server and the plugin.
IF (WIN32 OR (
       OPENSSL_INCLUDE_DIR   AND
       OPENSSL_LIBRARY       AND
       CRYPTO_LIBRARY        AND
       OPENSSL_MAJOR_VERSION STREQUAL "1"
             )
   )
    MYSQL_ADD_PLUGIN(keyring_file
                     ${KEYRING_FILE_SOURCES}
                     LINK_LIBRARIES ${SSL_LIBRARIES}
                     MODULE_ONLY
                     MODULE_OUTPUT_NAME "keyring_file")
ELSE()
    MYSQL_ADD_PLUGIN(keyring_file
                     ${KEYRING_FILE_SOURCES}
                     MODULE_ONLY
                     MODULE_OUTPUT_NAME "keyring_file")
ENDIF()

IF(SSL_DEFINES)
  ADD_DEFINITIONS(${SSL_DEFINES})
ENDIF()

IF(WITH_KEYRING_TEST)
  ADD_SUBDIRECTORY(keyring-test)
ENDIF()

IF(HAVE_DLOPEN)
    SET(DYNLIB_EXTENSION "so")
ENDIF()
