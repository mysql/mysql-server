# Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

MYSQL_ADD_PLUGIN(auth dialog.cc 
  MODULE_ONLY TEST_ONLY)
MYSQL_ADD_PLUGIN(auth_test_plugin test_plugin.cc 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_interface qa_auth_interface.cc 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_server qa_auth_server.cc 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_client qa_auth_client.cc 
  MODULE_ONLY TEST_ONLY)
  
MYSQL_ADD_PLUGIN(mysql_no_login mysql_no_login.cc 
  MODULE_ONLY)

CHECK_CXX_SOURCE_COMPILES(
"#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/socket.h>
int main() {
  struct ucred cred;
  getsockopt(0, SOL_SOCKET, SO_PEERCRED, &cred, 0);
}" HAVE_PEERCRED)

IF(HAVE_PEERCRED)
  MYSQL_ADD_PLUGIN(auth_socket auth_socket.cc
    MODULE_ONLY)
ENDIF()
