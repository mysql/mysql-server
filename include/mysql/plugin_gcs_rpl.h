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

#ifndef MYSQL_PLUGIN_GCS_REPLICATION_INCLUDED
#define MYSQL_PLUGIN_GCS_REPLICATION_INCLUDED

/* API for gcs replication plugin. (MYSQL_GCS_RPL_PLUGIN) */

#include <mysql/plugin.h>
#define MYSQL_GCS_REPLICATION_INTERFACE_VERSION 0x0100

typedef void (*gcs_stats_cb_t)(void);

struct st_mysql_gcs_rpl
{
  int interface_version;
  gcs_stats_cb_t *stats_callbacks;
  /*
    This function is to used to start the gcs replication based on the
    gcs group that is specified by the user.
  */
  int (*gcs_rpl_start)();
  /*
    This function is used by the stop the gcs replication based in a given
    group.
  */
  int (*gcs_rpl_stop)();
};

#endif

