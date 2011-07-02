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

MYSQL_ADD_PLUGIN(auth dialog.c 
  MODULE_ONLY)
MYSQL_ADD_PLUGIN(auth_test_plugin test_plugin.c 
  MODULE_ONLY)
MYSQL_ADD_PLUGIN(qa_auth_interface qa_auth_interface.c
  MODULE_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_server qa_auth_server.c
  MODULE_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_client qa_auth_client.c
  MODULE_ONLY)

CHECK_CXX_SOURCE_COMPILES(
"#define _GNU_SOURCE
#include <sys/socket.h>
int main() {
  struct ucred cred;
  getsockopt(0, SOL_SOCKET, SO_PEERCRED, &cred, 0);
}" HAVE_PEERCRED)

IF(HAVE_PEERCRED)
  MYSQL_ADD_PLUGIN(auth_socket auth_socket.c
    MODULE_ONLY)
ENDIF()
