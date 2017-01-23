# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
INCLUDE(CheckIncludeFiles)

CHECK_INCLUDE_FILES(sasl/sasl.h HAVE_SASL_H)
CHECK_INCLUDE_FILES(lber.h HAVE_LBER_H)
IF(HAVE_SASL_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES sasl/sasl.h)
ENDIF()
IF(HAVE_LBER_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES lber.h)
ENDIF()
IF(NOT HAVE_SASL_H)
  MESSAGE(STATUS "Required SASL library is missing. Skipping the LDAP authentication plugin.")
  RETURN()
ENDIF()
IF(NOT HAVE_LBER_H)
  MESSAGE(STATUS "Required LBER library is missing. Skipping the LDAP authentication plugin.")
  RETURN()
ENDIF()

# LDAP authentication SASL client will not build on windows, as windows don't have cyrus sasl.
# IF someone like can build the cyrus sasl library on windows and build LDAP authentication sasl client as well.
SET(HEADERS_CLIENT auth_ldap_sasl_client.h log.h)
SET(PLUGIN_CLIENT_SOURCES auth_ldap_sasl_client.cc log.cc)
ADD_CONVENIENCE_LIBRARY(authentication_ldap_sasl_client ${PLUGIN_CLIENT_SOURCES} ${HEADERS_CLIENT})
TARGET_LINK_LIBRARIES(authentication_ldap_sasl_client sasl2)