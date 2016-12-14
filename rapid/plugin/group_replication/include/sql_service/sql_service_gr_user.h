/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef SQL_SERVICE_GR_USER_INCLUDE
#define SQL_SERVICE_GR_USER_INCLUDE

#include "sql_service_interface.h"

#define GROUPREPL_USER      "_gr_user"
#define GROUPREPL_HOST      "localhost"
#define GROUPREPL_ACCOUNT   GROUPREPL_USER "@" GROUPREPL_HOST

int create_group_replication_user(bool threaded,
                                  Sql_service_interface *sql_interface= NULL);

int remove_group_replication_user(bool threaded,
                                  Sql_service_interface *sql_interface= NULL);

int check_group_replication_user(bool threaded,
                                 Sql_service_interface *sql_interface= NULL);

#endif //SQL_SERVICE_GR_USER_INCLUDE
