/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SITE_STRUCT_H
#define SITE_STRUCT_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

struct site_def {
  synode_no start;    /* Config is active from this message number */
  synode_no boot_key; /* The message number of the original unified_boot */
  node_no nodeno;     /* Node number of this node */
  node_list nodes;    /* Set of nodes in this config */
  server *servers[NSERVERS]; /* Connections to other nodes */
  detector_state detected;   /* Time of last incoming message for each node */
  node_no global_node_count; /* Number of live nodes in global_node_set */
  node_set global_node_set;  /* The global view */
  node_set local_node_set;   /* The local view */
  int detector_updated;      /* Has detector state been updated? */
  xcom_proto x_proto;
  synode_no delivered_msg[NSERVERS];
  double install_time;
  xcom_event_horizon event_horizon;
};
typedef struct site_def site_def;

#ifdef __cplusplus
}
#endif

#endif
