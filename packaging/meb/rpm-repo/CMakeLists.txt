# Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reversed.

#
# This program is NOT released under the GPL license, but constitutes
# a trade secret of Oracle. Use, publication, and redistribution
# of this program is prohibited without a written permission from
# Oracle.

# ======================================================================
# RPM packaging
# ======================================================================

# This is a CMake project that creates "mysql-enterprise-backup.spec"
# in the top directory of this source tree, adding version information
# and other release information. Paths to the MEB MySQL Server source
# is passed to "rpmbuild" using "--define", see header of
# "mysql-enterprise-backup.spec.in" in this directory.

cmake_minimum_required(VERSION 2.8.12.2 FATAL_ERROR)

project(MYSQLBACKUP_RPM)

set(NO_CPACK 1)
include(../../version.cmake)
include(../common.cmake)

execute_process(COMMAND "date" "+%Y" OUTPUT_VARIABLE YEAR OUTPUT_STRIP_TRAILING_WHITESPACE)

# If a hotfix, we add the hotfix indicator to the RPM release variable
# https://fedoraproject.org/wiki/Packaging:NamingGuidelines#Post-Release_packages
string(REGEX REPLACE "^-" "." RPM_VERSION_LEVEL "${MEB_VERSION_LEVEL}")

configure_file(${CMAKE_SOURCE_DIR}/mysql-enterprise-backup.spec.in
               ${CMAKE_BINARY_DIR}/../../../mysql-enterprise-backup.spec @ONLY)
