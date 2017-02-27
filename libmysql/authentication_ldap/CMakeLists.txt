# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
INCLUDE(CheckIncludeFiles)

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
  CROAK_AND_RETURN("Required SASL library is missing.")
ENDIF()
IF(HAVE_LBER_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES lber.h)
ELSE()
  CROAK_AND_RETURN("Required LBER library is missing.")
ENDIF()

FIND_PATH(
  CYRUS_SASL_INCLUDE_DIR NAMES sasl/sasl.h
  PATHS /include /usr/include /usr/local/include /usr/share/include /opt/include
  DOC "Searching for sasl/sasl.h")

CHECK_SYMBOL_EXISTS(
  sasl_client_done
  ${CYRUS_SASL_INCLUDE_DIR}/sasl/sasl.h
  HAVE_SASL_CLIENT_DONE)

IF (HAVE_SASL_CLIENT_DONE)
  SET(HAVE_SASL_CLIENT_DONE 1)
ELSE ()
  SET(HAVE_SASL_CLIENT_DONE 0)
ENDIF ()

# LDAP authentication SASL client will not build on windows, as windows don't have cyrus sasl.
# IF someone like can build the cyrus sasl library on windows and build LDAP authentication sasl client as well.
SET(HEADERS_CLIENT auth_ldap_sasl_client.h log.h)
SET(PLUGIN_CLIENT_SOURCES auth_ldap_sasl_client.cc log.cc)
ADD_CONVENIENCE_LIBRARY(authentication_ldap_sasl_client ${PLUGIN_CLIENT_SOURCES} ${HEADERS_CLIENT})
IF (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  TARGET_LINK_LIBRARIES(authentication_ldap_sasl_client sasl)
ELSE ()
  TARGET_LINK_LIBRARIES(authentication_ldap_sasl_client sasl2)
ENDIF ()
