# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All Rights Reversed.

#
# This program is NOT released under the GPL license, but constitutes
# a trade secret of Oracle. Use, publication, and redistribution
# of this program is prohibited without a written permission from
# Oracle.


# ======================================================================
# Packaging
# ======================================================================

SET(CPACK_PACKAGE_NAME    "${MEB_PACKAGE_NAME}")
SET(CPACK_PACKAGE_VERSION "${MEB_PACKAGE_VERSION}")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "MySQL Enterprise Backup, enterprise-grade backup and recovery for MySQL")
SET(CPACK_PACKAGE_VENDOR "Oracle and/or its affiliates")
SET(CPACK_PACKAGE_CONTACT "MySQL Release Engineering <mysql-build@oss.oracle.com>")

SET(CPACK_RESOURCE_FILE_LICENSE             "${MEB_LICENSE_FILE}")
SET(CPACK_RESOURCE_FILE_INSTALL             "${MEB_INSTALL_FILE}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME          "${CPACK_PACKAGE_NAME}-${MEB_PACKAGE_VERSION}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY         "${CPACK_PACKAGE_NAME}-${MEB_PACKAGE_VERSION}-${MEB_PLATFORM}")
SET(CPACK_PACKAGE_FILE_NAME                 "${CPACK_PACKAGE_INSTALL_DIRECTORY}")
IF(WIN32)
  SET(CPACK_GENERATOR "ZIP")
ELSE(WIN32)
  SET(CPACK_GENERATOR "TGZ")
ENDIF(WIN32)

# ======================================================================
# Special packaging
# ======================================================================

IF(UNIX AND NOT NO_CPACK)
  # Path relative to internal/meb
  ADD_SUBDIRECTORY(../../packaging/meb/rpm packaging/meb)
ENDIF()

IF(WIN32)
  # Path relative to internal/meb
  ADD_SUBDIRECTORY(../../packaging/meb/wix packaging/meb)
ENDIF()

IF(NOT NO_CPACK)
  INCLUDE(CPack)
ENDIF()
