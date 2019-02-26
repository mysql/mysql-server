# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#TODO If deemed necessary, these checks must go to config.h of the server
# which we must include

INCLUDE(CheckSymbolExists)

# Depending on the platform, we may or may not have this file
CHECK_INCLUDE_FILES(endian.h HAVE_ENDIAN_H)

# The header for glibc versions less than 2.9 will not
# have the endian conversion macros defined
IF(HAVE_ENDIAN_H)
  CHECK_SYMBOL_EXISTS(le64toh endian.h HAVE_LE64TOH)
  CHECK_SYMBOL_EXISTS(le32toh endian.h HAVE_LE32TOH)
  CHECK_SYMBOL_EXISTS(le16toh endian.h HAVE_LE16TOH)
  CHECK_SYMBOL_EXISTS(htole64 endian.h HAVE_HTOLE64)
  CHECK_SYMBOL_EXISTS(htole32 endian.h HAVE_HTOLE32)
  CHECK_SYMBOL_EXISTS(htole16 endian.h HAVE_HTOLE16)
  IF(HAVE_LE32TOH AND HAVE_LE16TOH AND HAVE_LE64TOH AND
     HAVE_HTOLE64 AND HAVE_HTOLE32 AND HAVE_HTOLE16)
    SET(HAVE_ENDIAN_CONVERSION_MACROS 1)
  ENDIF()
ENDIF()

#
# XDR related checks
#

SET (XCOM_BASEDIR
     ${CMAKE_CURRENT_SOURCE_DIR}/libmysqlgcs/src/bindings/xcom/xcom/)

IF (WIN32)
  # On windows we bundle the rpc header and some code as well
  SET (CMAKE_REQUIRED_INCLUDES ${XCOM_BASEDIR}/windeps/sunrpc
                               ${XCOM_BASEDIR}/windeps/include)
ENDIF()

IF (NOT WIN32)
  MYSQL_CHECK_RPC()

  SET (CMAKE_REQUIRED_FLAGS_BACKUP ${CMAKE_REQUIRED_FLAGS})
  SET (CMAKE_REQUIRED_FLAGS "-Wno-error")
  SET (CMAKE_REQUIRED_INCLUDES ${RPC_INCLUDE_DIRS})
ENDIF()

#
# Network interfaces related checks
#

#All the code below needs to be here because CMake is dumb and generates code
# with unused vars
CHECK_STRUCT_HAS_MEMBER("struct sockaddr" sa_len sys/socket.h
                        HAVE_STRUCT_SOCKADDR_SA_LEN)
CHECK_STRUCT_HAS_MEMBER("struct ifreq" ifr_name net/if.h
                        HAVE_STRUCT_IFREQ_IFR_NAME)

CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_putint32 rpc/xdr.h
                        HAVE_XDR_OPS_X_PUTINT32)
CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_getint32 rpc/xdr.h
                        HAVE_XDR_OPS_X_GETINT32)

CHECK_C_SOURCE_COMPILES(
  "
  #include <rpc/rpc.h>
  int main(__const int *i){return *i;}
  "
  HAVE___CONST)

CHECK_C_SOURCE_COMPILES(
  "
  #include <rpc/types.h>
  int main(void) { rpc_inline_t x; return 0; }
  "
  HAVE_RPC_INLINE_T)

# Restore CMAKE_REQUIRED_FLAGS
IF (NOT WIN32)
  SET (CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_BACKUP})
ENDIF()


IF(NOT APPLE
   AND NOT WIN32
   AND NOT CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

  SET(SAVED_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})

  SET(CMAKE_REQUIRED_FLAGS "-Werror=sign-conversion")

  CHECK_C_SOURCE_COMPILES(
    "
    #include <rpc/xdr.h>
    int main() { XDR xdr; xdr.x_handy = -1; return (int)xdr.x_handy; }
    "
    OLD_XDR)

  CHECK_C_SOURCE_COMPILES(
    "
    #include <rpc/xdr.h>
    u_int getpostn(XDR* xdr) { return (u_int)xdr->x_handy; }
    int main() {
      XDR xdr;
      struct xdr_ops ops;

      ops.x_getpostn = getpostn;
      return (int)ops.x_getpostn(&xdr);
    }
    "
    X_GETPOSTN_NOT_USE_CONST)

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

  SET(CMAKE_REQUIRED_FLAGS ${SAVED_CMAKE_REQUIRED_FLAGS})
ENDIF()
