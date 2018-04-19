# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#
# Configuration for building LDAP SASL Authentication Plugin (client-side)
#

INCLUDE(CheckIncludeFiles)

IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  INCLUDE_DIRECTORIES(SYSTEM /usr/local/include)
  LIST(APPEND CMAKE_REQUIRED_INCLUDES "/usr/local/include")
ENDIF()

CHECK_INCLUDE_FILES(sasl/sasl.h HAVE_SASL_H)
CHECK_INCLUDE_FILES(lber.h HAVE_LBER_H)

# If cmake was invoked with -DWITH_AUTHENTICATION_LDAP=1
# then fail if we are unable to build the LDAP plugin.
MACRO(CROAK_AND_RETURN)
  IF (WITH_AUTHENTICATION_LDAP)
    MESSAGE(SEND_ERROR ${ARGV})
  ELSE()
    MESSAGE(STATUS ${ARGV} " Skipping the LDAP SASL client authentication plugin.")
    RETURN()
  ENDIF()
ENDMACRO()

IF(HAVE_SASL_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES sasl/sasl.h)
ELSE()
  CROAK_AND_RETURN("Required SASL header is missing.")
ENDIF()
IF(HAVE_LBER_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES lber.h)
ELSE()
  CROAK_AND_RETURN("Required LBER header is missing.")
ENDIF()

IF (CMAKE_SYSTEM_NAME MATCHES "SunOS")
  FIND_LIBRARY(SASL_LIBRARY_PATH NAMES "sasl")
ELSE ()
  FIND_LIBRARY(SASL_LIBRARY_PATH NAMES "sasl2")
ENDIF ()

IF(NOT SASL_LIBRARY_PATH)
  SET(SASL_LIBRARY_PATH /usr/lib64/libsasl2.so)
ENDIF ()
MESSAGE(STATUS "SASL_LIBRARY_PATH = ${SASL_LIBRARY_PATH}")

# LDAP authentication SASL client will not build on windows, as windows don't have cyrus sasl.
# IF someone like can build the cyrus sasl library on windows and build LDAP authentication sasl client as well.
MYSQL_ADD_PLUGIN(authentication_ldap_sasl_client
                 auth_ldap_sasl_client.cc log_client.cc
                 LINK_LIBRARIES ${SASL_LIBRARY_PATH}
                 CLIENT_ONLY MODULE_ONLY
                 MODULE_OUTPUT_NAME "authentication_ldap_sasl_client")
