/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SITE_DEF_H
#define SITE_DEF_H

#include "xcom/node_no.h"
#include "xcom/site_struct.h"

site_def *new_site_def();
site_def *clone_site_def(site_def const *site);
void init_site_def(u_int n, node_address *names, site_def *site);
void add_site_def(u_int n, node_address *names, site_def *nodes);
void remove_site_def(u_int n, node_address *names, site_def *nodes);
char *dbg_site_def(site_def const *site);
void init_site_vars();
void free_site_def_body(site_def *s);
void free_site_def(site_def *s);
void free_site_defs();
site_def *push_site_def(site_def *s);
site_def const *find_prev_site_def(synode_no synode);
void garbage_collect_site_defs(synode_no x);
site_def const *get_site_def();
site_def *get_site_def_rw();
site_def const *get_prev_site_def();
uint32_t get_group_id(site_def const *site);
static uint32_t const null_id = 0;
node_no get_maxnodes(site_def const *site);
node_no get_nodeno(site_def const *site);
node_no find_nodeno(site_def const *site, const char *name);
node_no get_prev_nodeno();
site_def const *find_site_def(synode_no synode);
site_def *find_site_def_rw(synode_no synode);
site_def const *find_next_site_def(synode_no synode);
node_set detector_node_set(site_def const *site);
int enough_live_nodes(site_def *site);
void import_config(gcs_snapshot *gcs_snap);
gcs_snapshot *export_config();
void get_all_site_defs(site_def ***s, uint32_t *n);
synode_no get_min_delivered_msg(site_def const *s);
void update_delivered(site_def *s, node_no node, synode_no msgno);
synode_no config_max_boot_key(gcs_snapshot const *gcs_snap);
leader_array alloc_leader_array(u_int n);
leader_array clone_leader_array(leader_array const x);
synode_no get_highest_boot_key(gcs_snapshot *gcs_snap);
synode_no get_lowest_boot_key(gcs_snapshot *gcs_snap);

static inline bool_t node_no_exists(node_no n, site_def const *site) {
  return n < get_maxnodes(site);
}

static inline bool_t is_local_node(node_no n, site_def const *site) {
  return node_no_exists(n, site) && n == get_nodeno(site);
}

/**
  Finds pointer to server given site and node number.
  @param[in]     s    Pointer to site definition
  @param[in]     i    Node number

  @retval Pointer to server if success
  @retval 0 if failure
*/
static inline server *get_server(site_def const *s, node_no i) {
  if (s && i != VOID_NODE_NO && i < s->nodes.node_list_len)
    return s->servers[i];
  else
    return nullptr;
}

#endif
