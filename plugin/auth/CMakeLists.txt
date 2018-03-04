# Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

MYSQL_ADD_PLUGIN(auth dialog.c 
  MODULE_ONLY TEST_ONLY)
MYSQL_ADD_PLUGIN(auth_test_plugin test_plugin.c 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_interface qa_auth_interface.c 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_server qa_auth_server.c 
  MODULE_ONLY TEST_ONLY)

MYSQL_ADD_PLUGIN(qa_auth_client qa_auth_client.c 
  MODULE_ONLY TEST_ONLY)
  
MYSQL_ADD_PLUGIN(mysql_no_login mysql_no_login.c 
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
  MYSQL_ADD_PLUGIN(auth_socket auth_socket.c
    MODULE_ONLY)
ENDIF()
