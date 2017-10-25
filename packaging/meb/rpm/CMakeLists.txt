# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All Rights Reversed.

#
# This program is NOT released under the GPL license, but constitutes
# a trade secret of Oracle. Use, publication, and redistribution
# of this program is prohibited without a written permission from
# Oracle.

# ======================================================================
# RPM packaging
# ======================================================================

# The old RPM spec was just a copy of existing binaries, what CPACK
# does for RPMs, so we use CPACK to create RPMs doing the same.
#
# But to be fully compatible with the old spec, we generated a
# template and used that as base to make it the same as before (with
# some corrections and changes), using
#
#   cpack -D CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE=1 -G RPM
#
# Note that we likely want to move to use a real spec file, and the
# install prefix /usr instead of /opt/mysql

if(UNIX)
  execute_process(COMMAND "date" "+%Y" OUTPUT_VARIABLE YEAR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_SOURCE_DIR}/meb.spec.in" PARENT_SCOPE)

# Stupid CMake/CPack, we can't use our own variables in "meb.spec.in",
# we need to use variables prefixed "CPACK_". So we define them here,
# even one we invented ourselves
set(CPACK_RPM_PACKAGE_SUMMARY  "MySQL Enterprise Backup ${MEB_PACKAGE_VERSION}" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_VERSION  "${MEB_PACKAGE_NUMERIC_VERSION}" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_LICENSE  "Copyright (c) ${YEAR}, Oracle and/or its affiliates. All rights reserved." PARENT_SCOPE)
set(CPACK_MEB_PACKAGE_BASE_VERSION "${MEB_PACKAGE_BASE_VERSION}" PARENT_SCOPE)

# To be compatible with the old RPMs we need to set the RPM release
# to the OS name
string(REGEX MATCH "[^\\.]+" OS "${MEB_PLATFORM}")
set(CPACK_RPM_PACKAGE_RELEASE "${OS}" PARENT_SCOPE)

# See comment in spec file, we don't use any of the ways below to
# set prefix, we hard code it in there.
# We would like to set this for RPM only, but it is a global setting for CPack
#set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/mysql/meb-${MEB_PACKAGE_BASE_VERSION}" PARENT_SCOPE)
# Instead honor CMAKE_INSTALL_PREFIX and set it from the "cmake" call to "/opt/mysql/meb-<basever"
#set(CPACK_SET_DESTDIR          ON PARENT_SCOPE)
