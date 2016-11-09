# Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
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

# Exclude unused headers and avoid messed up includes
# regarding winsock.h and winsock2.h
# See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms737629%28v=vs.85%29.aspx
ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)
ADD_DEFINITIONS(-DNOMINMAX)

#
# On windows we bundle the rpc header and some code as well
# Thence, recheck
#
SET (CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/windeps/sunrpc
                             ${CMAKE_CURRENT_SOURCE_DIR}/windeps/include)

CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_putint32 rpc/xdr.h HAVE_XDR_OPS_X_PUTINT32_WIN)
CHECK_STRUCT_HAS_MEMBER("struct xdr_ops" x_getint32 rpc/xdr.h HAVE_XDR_OPS_X_GETINT32_WIN)
CHECK_C_SOURCE_COMPILES(
  "
  #include <rpc/types.h>
  int main(void) { rpc_inline_t x; return 0; }
  "
  HAVE_RPC_INLINE_T_WIN)

IF (HAVE_RPC_INLINE_T_WIN)
  ADD_DEFINITIONS(-DHAVE_RPC_INLINE_T)
ENDIF()

IF (HAVE_XDR_OPS_X_PUTINT32_WIN)
  ADD_DEFINITIONS(-DHAVE_XDR_OPS_X_PUTINT32)
ENDIF()

IF (HAVE_XDR_OPS_X_GETINT32_WIN)
  ADD_DEFINITIONS(-DHAVE_XDR_OPS_X_GETINT32)
ENDIF()
