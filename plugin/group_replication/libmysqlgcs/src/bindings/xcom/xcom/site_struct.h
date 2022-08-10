/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "xcom/node_no.h"
#include "xcom/server_struct.h"
#include "xcom/synode_no.h"
#include "xcom/xcom_detector.h"
#include "xdr_gen/xcom_vp.h"

/* Reserved value to signal that all nodes should be leaders */
enum { active_leaders_all = 0 };

typedef struct site_def site_def;
typedef struct pax_msg pax_msg;
typedef struct linkage linkage;

typedef void (*msg_handler)(site_def const *site, pax_msg *p,
                            linkage *reply_queue);

struct site_def {
  synode_no start NULL_SYNODE; /* Config is active from this message number */
  synode_no boot_key
      NULL_SYNODE; /* The message number of the original unified_boot */
  node_no nodeno{VOID_NODE_NO};       /* Node number of this node */
  node_list nodes{0, nullptr};        /* Set of nodes in this config */
  server *servers[NSERVERS]{nullptr}; /* Connections to other nodes */
  detector_state detected{
      0.0}; /* Time of last incoming message for each node */
  node_no global_node_count{0}; /* Number of live nodes in global_node_set */
  node_set global_node_set{0, nullptr}; /* The global view */
  node_set local_node_set{0, nullptr};  /* The local view */
  int detector_updated{0};              /* Has detector state been updated? */
  xcom_proto x_proto{x_unknown_proto};
  synode_no delivered_msg[NSERVERS]{NULL_SYNODE};
  double install_time{0.0};
  xcom_event_horizon event_horizon{EVENT_HORIZON_MIN};
  node_no max_active_leaders{active_leaders_all}; /* How many leaders can there
                                 be? >= 1 and <= number of nodes */
  leader_array leaders{0, nullptr};     /* Leaders as defined by client */
  msg_handler *dispatch_table{nullptr}; /* Per-config dispatch table */
  bool cached_leaders{false};           /* Initialized leader cache ? */
  bool active_leader[NSERVERS]{0};      /* Leader cache */
  node_no found_leaders{0};             /* Number of leaders found */
};
#endif
