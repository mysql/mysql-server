/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/plugin.h>
#include <mysql/plugin_gcs_rpl.h>
#include "sql_plugin.h"

class Gcs_replication_handler
{
public:
  Gcs_replication_handler();
  ~Gcs_replication_handler();
  int gcs_rpl_start();
  int gcs_rpl_stop();

private:
  LEX_STRING plugin_name;
  plugin_ref plugin;
  st_mysql_gcs_rpl* proto;
  int gcs_init();
};

int init_gcs_rpl();
int start_gcs_rpl();
int stop_gcs_rpl();
int cleanup_gcs_rpl();
