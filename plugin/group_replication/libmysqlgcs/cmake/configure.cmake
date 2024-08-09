# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

#
# XDR related checks
#

IF (WIN32)
  # On windows we bundle the rpc header and some code as well
  SET (CMAKE_REQUIRED_INCLUDES ${XCOM_BASEDIR}/windeps/sunrpc
                               ${XCOM_BASEDIR}/windeps/include)
ENDIF()

IF (NOT WIN32)
  CMAKE_PUSH_CHECK_STATE()
  SET (CMAKE_REQUIRED_FLAGS "-Wno-error")
  SET (CMAKE_REQUIRED_INCLUDES ${RPC_INCLUDE_DIRS})
ENDIF()

#
# Network interfaces related checks
#

#All the code below needs to be here because CMake is dumb and generates code
# with unused vars

CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_putint32 rpc/xdr.h
                        HAVE_XDR_OPS_X_PUTINT32)
CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_getint32 rpc/xdr.h
                        HAVE_XDR_OPS_X_GETINT32)

CHECK_C_SOURCE_COMPILES(
  "
  #include <rpc/types.h>
  int main(void) { rpc_inline_t x; return 0; }
  "
  HAVE_RPC_INLINE_T)

# Restore CMAKE_REQUIRED_FLAGS
IF (NOT WIN32)
  CMAKE_POP_CHECK_STATE()
ENDIF()


IF(NOT APPLE
   AND NOT WIN32
   AND NOT FREEBSD)

  CMAKE_PUSH_CHECK_STATE()

  SET(CMAKE_REQUIRED_FLAGS "-Werror=sign-conversion")

  CHECK_C_SOURCE_COMPILES(
    "
    #include <rpc/xdr.h>
    int main() { XDR xdr; xdr.x_handy = -1; return (int)xdr.x_handy; }
    "
    OLD_XDR)

  CHECK_C_COMPILER_FLAG("-Wincompatible-pointer-types"
                        HAS_INCOMPATIBLE_POINTER_TYPES)
  IF (HAS_INCOMPATIBLE_POINTER_TYPES)
    SET(CMAKE_REQUIRED_FLAGS
                  "${CMAKE_REQUIRED_FLAGS} -Werror=incompatible-pointer-types")
  ELSE()
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror")
  ENDIF()

  CHECK_C_SOURCE_COMPILES(
    "
    #include <rpc/xdr.h>
    bool_t putlong(XDR* xdr, long *longp)
                   { return (bool_t)(*longp + xdr->x_handy); }
    int main() {
      XDR xdr;
      struct xdr_ops ops;
      long l;

      ops.x_putlong = putlong;
      return (int)ops.x_putlong(&xdr, &l);
    }
    "
    X_PUTLONG_NOT_USE_CONST)

  CMAKE_POP_CHECK_STATE()
ENDIF()
