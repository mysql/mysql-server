# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2 of the
# License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

CHECK_CXX_SOURCE_COMPILES(
"#define _GNU_SOURCE
#include <sys/socket.h>
int main() {
  struct ucred cred;
  getsockopt(0, SOL_SOCKET, SO_PEERCRED, &cred, 0);
}" HAVE_PEERCRED)
 
IF (HAVE_PEERCRED)
  ADD_DEFINITIONS(-DHAVE_PEERCRED)
  SET(ok 1)
ELSE()

# Hi, OpenBSD!
CHECK_CXX_SOURCE_COMPILES(
"#include <sys/types.h>
#include <sys/socket.h>
int main() {
  struct sockpeercred cred;
  getsockopt(0, SOL_SOCKET, SO_PEERCRED, &cred, 0);
  }" HAVE_SOCKPEERCRED)

IF (HAVE_SOCKPEERCRED)
  ADD_DEFINITIONS(-DHAVE_SOCKPEERCRED)
  SET(ok 1)
ELSE()

# FreeBSD, is that you?
CHECK_CXX_SOURCE_COMPILES(
"#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ucred.h>
int main() {
  struct xucred cred;
  getsockopt(0, 0, LOCAL_PEERCRED, &cred, 0);
  }" HAVE_XUCRED)

IF (HAVE_XUCRED)
  ADD_DEFINITIONS(-DHAVE_XUCRED)
  SET(ok 1)
ELSE()

# Who else? Anyone?
# C'mon, show your creativity, be different! ifdef's are fun, aren't they?

ENDIF()
ENDIF()
ENDIF()

IF(ok)
  MYSQL_ADD_PLUGIN(auth_socket auth_socket.c MODULE_ONLY)
ENDIF()

