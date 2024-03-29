# Copyright (c) 2006, 2023, Oracle and/or its affiliates.
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

DISABLE_MISSING_PROFILE_WARNING()

# These are the headers required to #include <mysql.h>, which we ship
# to the client as part of our API.
#
# FIXME: mysqld_error.h, which is also part of our API, should be included
# from mysql.h, but this needs more build system tweaking, since it's
# automatically generated.
SET(HEADERS
  errmsg.h
  my_command.h
  my_compress.h
  my_list.h
  mysql.h
  mysql_com.h
  mysql_time.h
  ${CMAKE_CURRENT_BINARY_DIR}/mysql_version.h
  ${CMAKE_CURRENT_BINARY_DIR}/mysqld_error.h
  field_types.h
)
SET(HEADERS_MYSQL_DIR
  mysql/client_plugin.h
  mysql/plugin_auth_common.h
  mysql/udf_registration_types.h
)

INSTALL(FILES ${HEADERS}
  DESTINATION ${INSTALL_INCLUDEDIR}
  COMPONENT Development
  )

INSTALL(FILES ${HEADERS_MYSQL_DIR}
  DESTINATION ${INSTALL_INCLUDEDIR}/mysql
  COMPONENT Development
  )

# openssl/applink.c must be built into applications linking with libmysql.dll
IF (WIN32 AND OPENSSL_APPLINK_C)
  INSTALL(FILES ${OPENSSL_APPLINK_C}
    DESTINATION ${INSTALL_INCLUDEDIR}/openssl
    COMPONENT Development
    )
ENDIF()

SET(INSTALLED_HEADERS_CC ${CMAKE_CURRENT_BINARY_DIR}/installed_headers.cc)
SET(INSTALLED_HEADERS_CONTENT "")
FOREACH(HEADER ${HEADERS} ${HEADERS_MYSQL_DIR})
  STRING_APPEND(INSTALLED_HEADERS_CONTENT "#include \"${HEADER}\"")
  STRING_APPEND(INSTALLED_HEADERS_CONTENT "\n")
ENDFOREACH()
STRING_APPEND(INSTALLED_HEADERS_CONTENT
  "int main(int, char **) { return 0; }"
  )

CONFIGURE_FILE_CONTENT("${INSTALLED_HEADERS_CONTENT}"
  ${INSTALLED_HEADERS_CC}
  )

# Verifies that all installed headers comply with -std=c++11
# i.e. no c++17 features used.
# VS/clang on windows do not accept /std:c++11
MYSQL_ADD_EXECUTABLE(installed_headers
  ${INSTALLED_HEADERS_CC}
  DEPENDENCIES GenError
  SKIP_INSTALL
  )
# This will add -std=gnu++11 for compilers which support it.
# It will be silently ignored on Windows.
SET_PROPERTY(TARGET installed_headers PROPERTY CXX_STANDARD 11)
