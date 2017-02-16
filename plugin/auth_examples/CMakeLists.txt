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

MYSQL_ADD_PLUGIN(dialog_examples dialog_examples.c 
  MODULE_ONLY COMPONENT Test)
MYSQL_ADD_PLUGIN(auth_test_plugin test_plugin.c 
  MODULE_ONLY COMPONENT Test)
MYSQL_ADD_PLUGIN(qa_auth_interface qa_auth_interface.c
  MODULE_ONLY COMPONENT Test)

MYSQL_ADD_PLUGIN(qa_auth_server qa_auth_server.c
  MODULE_ONLY COMPONENT Test)

MYSQL_ADD_PLUGIN(qa_auth_client qa_auth_client.c
  MODULE_ONLY COMPONENT Test)

MYSQL_ADD_PLUGIN(auth_0x0100 auth_0x0100.c MODULE_ONLY COMPONENT Test)

MYSQL_ADD_PLUGIN(mysql_clear_password clear_password_client.c
  MODULE_ONLY COMPONENT SharedLibraries)
