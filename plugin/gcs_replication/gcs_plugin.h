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

#ifndef GCS_PLUGIN_INCLUDE
#define GCS_PLUGIN_INCLUDE

#include "gcs_utils.h" //defines have_replication and mysql_server
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "my_global.h"
#include <mysql/plugin.h>
#include <mysql/plugin_gcs_rpl.h>
#include <../include/mysql_com.h>
#include <mysqld.h>               // UUID_LENGTH
#include <rpl_gtid.h>             // rpl_sidno
#include "gcs_applier.h"

//Plugin variables
typedef st_mysql_sys_var SYS_VAR;
extern char gcs_replication_group[UUID_LENGTH+1];
extern rpl_sidno gcs_cluster_sidno;
extern char gcs_replication_boot;
extern ulong handler_pipeline_type;
extern Applier_module *applier;
extern bool wait_on_engine_initialization;
extern ulong gcs_applier_thread_timeout;

//Plugin methods
int gcs_replication_init(MYSQL_PLUGIN plugin_info);
int gcs_replication_deinit(void *p);
int configure_and_start_applier();
int gcs_rpl_start();
int gcs_rpl_stop();

#endif /* GCS_PLUGIN_INCLUDE */
