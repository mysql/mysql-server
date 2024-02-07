# Copyright (c) 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# When configuring with -DWITH_SSL=</path/to/custom/openssl>
# we cannot use system TIRPC libraries, since theses depend on Kerberos
# libs, which again depends on OpenSSL libs. Our binaries linking with TIRPC
# would then be linked with *two* (most likely conflicting) versions of
# libcrypto.so.
# To avoid this, we build a static TIRPC library here, which is configured
# *without* gssapi, i.e. Kerberos.

# Note that we do *not* build 'rpcgen' here. We rely on the one provided
# by the operating system.

INCLUDE(ExternalProject)

SET(TIRPC_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extra/tirpc/libtirpc-1.3.3")

# Set correct search path for executables, libraries, and data files.
IF(ALTERNATIVE_GCC)
  SET(TIRPC_C_B_OPTION "-B${GCC_B_PREFIX} ")
ENDIF()

FIND_PROGRAM(MAKE_EXECUTABLE make)

SET(TIRPC_CONFIGURE_PREFIX "${CMAKE_BINARY_DIR}/tirpc")
SET(TIRPC_C_FLAGS "${CMAKE_C_COMPILER_ARG1} ${TIRPC_C_B_OPTION}")
STRING_APPEND(TIRPC_C_FLAGS " -fPIC")
STRING_APPEND(TIRPC_C_FLAGS " ${CMAKE_C_FLAGS}")
STRING_APPEND(TIRPC_C_FLAGS " ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")

SET(TIRPC_CONFIGURE_ARGS
  "AR=${CMAKE_AR}"
  "RANLIB=${CMAKE_RANLIB}"
  "CC=${CMAKE_C_COMPILER}"
  "--disable-shared"
  "--disable-gssapi"
  "--prefix=${TIRPC_CONFIGURE_PREFIX}"
  "CFLAGS=${TIRPC_C_FLAGS}"
  )

SET(TIRPC_BUILD_COMMAND ${MAKE_EXECUTABLE})

SET(TIRPC_EXTERNAL_PROJECT_ADD_ARGS
  CONFIGURE_COMMAND
  "${TIRPC_SOURCE_DIR}/configure" ${TIRPC_CONFIGURE_ARGS}
  BUILD_COMMAND
  ${TIRPC_BUILD_COMMAND}
  )

EXTERNALPROJECT_ADD(tirpc_ext
  SOURCE_DIR "${TIRPC_SOURCE_DIR}"
  ${TIRPC_EXTERNAL_PROJECT_ADD_ARGS}
  BUILD_BYPRODUCTS "${TIRPC_CONFIGURE_PREFIX}/lib/libtirpc.a"
  BINARY_DIR "${CMAKE_BINARY_DIR}/extra/build_tirpc"
  )
