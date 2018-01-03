/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"

#include <assert.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/bitset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

typedef site_def *site_def_ptr;

struct site_def_ptr_array {
  u_int count;
  u_int site_def_ptr_array_len;
  site_def_ptr *site_def_ptr_array_val;
};
typedef struct site_def_ptr_array site_def_ptr_array;

define_xdr_funcs(site_def_ptr)

    /* FIFO of site definitions */
    static site_def_ptr_array site_defs;
static site_def *incoming = 0;
static inline node_no _get_maxnodes(site_def const *site);

/* purecov: begin deadcode */
/* Save incoming site def, but do not make it available yet */
site_def *begin_site_def(site_def *s) {
  assert(!incoming);
  incoming = s;
  assert(s->global_node_set.node_set_len == _get_maxnodes(s));
  return incoming;
}

/* Push saved site def, making it active from synode start */
site_def *end_site_def(synode_no start) {
  assert(incoming);
  incoming->start = start;
  return push_site_def(incoming);
}
/* purecov: end */

/* Return pointer to array of site defs */
void get_all_site_defs(site_def ***s, uint32_t *n) {
  *s = site_defs.site_def_ptr_array_val;
  *n = site_defs.site_def_ptr_array_len;
}

/* Module initialization */
void init_site_vars() {
  init_site_def_ptr_array(&site_defs);
  site_defs.count = 0;
  incoming = 0;
}

/* Recursively free a complete site_def.  Only free the site_def, not
 the servers that it points to, since servers are shared by multiple
 site_defs, and will eventually be deallocated by garbage_collect_servers
*/
void free_site_def(site_def *s) {
  if (s) {
    invalidate_detector_sites(s);
    xdr_free((xdrproc_t)xdr_node_list, (char *)(&s->nodes));
    free_node_set(&s->global_node_set);
    free_node_set(&s->local_node_set);
    free(s);
  }
}

/* Free all resources in this module */
void free_site_defs() {
  u_int i;
  for (i = 0; i < site_defs.count; i++) {
    free_site_def(site_defs.site_def_ptr_array_val[i]);
  }
  free_site_def_ptr_array(&site_defs);
  site_defs.count = 0;
  free_site_def(incoming);
}

/* Add a new site definition to the list */
site_def *push_site_def(site_def *s) {
  uint32_t i;
  set_site_def_ptr(&site_defs, 0, site_defs.count);
  DBGOUT(FN; NDBG(site_defs.count, u); PTREXP(s); if (s) {
    SYCEXP(s->start);
    SYCEXP(s->boot_key);
  });
  for (i = site_defs.count; i > 0; i--) {
    DBGOUT(NDBG(i - 1, d); PTREXP(site_defs.site_def_ptr_array_val[i - 1]);
           if (site_defs.site_def_ptr_array_val[i - 1]) {
             SYCEXP(site_defs.site_def_ptr_array_val[i - 1]->start);
             SYCEXP(site_defs.site_def_ptr_array_val[i - 1]->boot_key);
           });
    site_defs.site_def_ptr_array_val[i] =
        site_defs.site_def_ptr_array_val[i - 1];
  }
  set_site_def_ptr(&site_defs, s, 0);
  if (s) {
    s->x_proto = set_latest_common_proto(common_xcom_version(s));
    G_DEBUG("latest common protocol is now %d", s->x_proto);
  }
  site_defs.count++;
  assert(!s || (s->global_node_set.node_set_len == _get_maxnodes(s)));
  return s;
}

/* Return first site def */
static inline site_def const *_get_site_def() {
  assert(site_defs.count == 0 || !site_defs.site_def_ptr_array_val[0] ||
         site_defs.site_def_ptr_array_val[0]->global_node_set.node_set_len ==
             _get_maxnodes(site_defs.site_def_ptr_array_val[0]));
  if (site_defs.count > 0)
    return site_defs.site_def_ptr_array_val[0];
  else
    return 0;
}

/* Return first site def */
site_def *get_site_def_rw() {
  if (site_defs.count > 0)
    return site_defs.site_def_ptr_array_val[0];
  else
    return 0;
}

/* purecov: begin deadcode */
/* Return previous site def */
static inline site_def const *_get_prev_site_def() {
  assert(site_defs.count == 0 || !site_defs.site_def_ptr_array_val[1] ||
         site_defs.site_def_ptr_array_val[1]->global_node_set.node_set_len ==
             _get_maxnodes(site_defs.site_def_ptr_array_val[1]));
  if (site_defs.count > 0)
    return site_defs.site_def_ptr_array_val[1];
  else
    return 0;
}
/* purecov: end */

/* Return first site def as const ptr */
site_def const *get_site_def() { return _get_site_def(); }

/* purecov: begin deadcode */

/* Return previous site def */
site_def const *get_prev_site_def() { return _get_prev_site_def(); }

/* purecov: end */

static inline int match_def(site_def const *site, synode_no synode) {
  return site &&
         (synode.group_id == 0 || synode.group_id == site->start.group_id) &&
         !synode_lt(synode, site->start);
}

/* Return first site def which has start less than or equal to synode */
site_def const *find_site_def(synode_no synode) {
  site_def const *retval = 0;
  u_int i;

  for (i = 0; i < site_defs.count; i++)
    if (match_def(site_defs.site_def_ptr_array_val[i], synode)) {
      retval = site_defs.site_def_ptr_array_val[i];
      break;
    }
  assert(!retval ||
         retval->global_node_set.node_set_len == _get_maxnodes(retval));
  return retval;
}

/* As find_site_def, but return pointer to non-const object */
site_def *find_site_def_rw(synode_no synode) {
  site_def *retval = 0;
  u_int i;

  for (i = 0; i < site_defs.count; i++)
    if (match_def(site_defs.site_def_ptr_array_val[i], synode)) {
      retval = site_defs.site_def_ptr_array_val[i];
      break;
    }
  assert(!retval ||
         retval->global_node_set.node_set_len == _get_maxnodes(retval));
  return retval;
}

static inline int prev_def(site_def const *site, synode_no synode) {
  return (site &&
          (synode.group_id == 0 || synode.group_id == site->start.group_id));
}

site_def const *find_prev_site_def(synode_no synode) {
  site_def const *retval = 0;
  u_int i;

  for (i = site_defs.count; i > 0; i--)
    if (prev_def(site_defs.site_def_ptr_array_val[i - 1], synode)) {
      retval = site_defs.site_def_ptr_array_val[i - 1];
      break;
    }
  assert(!retval ||
         retval->global_node_set.node_set_len == _get_maxnodes(retval));
  return retval;
}

void garbage_collect_site_defs(synode_no x) {
  u_int i;
  u_int s_max = site_defs.count;

  DBGOUT(FN; NDBG(site_defs.count, u); SYCEXP(x););
  for (i = 3; i < s_max; i++) {
    if (match_def(site_defs.site_def_ptr_array_val[i], x)) {
      break;
    }
  }
  i++;
  for (; i < s_max; i++) {
    site_def *site = site_defs.site_def_ptr_array_val[i];
    DBGOUT(NDBG(i, d); PTREXP(site_defs.site_def_ptr_array_val[i]););
    if (site) {
      DBGOUT(SYCEXP(site->start); SYCEXP(site->boot_key););
      free_site_def(site);
      site_defs.site_def_ptr_array_val[i] = 0;
    }
    site_defs.count--;
  }
}

/* purecov: begin deadcode */
char *dbg_site_def(site_def const *site) {
  assert(site->global_node_set.node_set_len == _get_maxnodes(site));
  return dbg_list(&site->nodes);
}
/* purecov: end */

/* Create a new empty site_def */
site_def *new_site_def() {
  site_def *retval = (site_def *)calloc((size_t)1, sizeof(site_def));
  retval->nodeno = VOID_NODE_NO;
  return retval;
}

/* {{{ Clone a site definition */

site_def *clone_site_def(site_def const *site) {
  site_def *retval = new_site_def();
  assert(site->global_node_set.node_set_len == _get_maxnodes(site));
  *retval = *site;
  init_node_list(site->nodes.node_list_len, site->nodes.node_list_val,
                 &retval->nodes);
  retval->global_node_set = clone_node_set(site->global_node_set);
  retval->local_node_set = clone_node_set(site->local_node_set);
  assert(retval->global_node_set.node_set_len == _get_maxnodes(retval));
  DBGOUT(FN; PTREXP(site); PTREXP(retval));
  return retval;
}

/* }}} */

/* {{{ Initialize a site definition from array of string pointers */

void init_site_def(u_int n, node_address *names, site_def *site) {
  site->start = null_synode;
  site->boot_key = null_synode;
  site->nodeno = VOID_NODE_NO;
  init_detector(site->detected);
  init_node_list(n, names, &site->nodes);
  site->global_node_count = 0;
  alloc_node_set(&site->global_node_set, NSERVERS);
  site->global_node_set.node_set_len = site->nodes.node_list_len;
  set_node_set(&site->global_node_set); /* Assume everyone is there */
  assert(site->global_node_set.node_set_len == _get_maxnodes(site));
  alloc_node_set(&site->local_node_set, NSERVERS);
  site->local_node_set.node_set_len = site->nodes.node_list_len;
  set_node_set(&site->local_node_set); /* Assume everyone is there */
  assert(site->local_node_set.node_set_len == _get_maxnodes(site));
  site->detector_updated = 0;
  site->x_proto = my_xcom_version;
}

/* }}} */

/* Add nodes to site definition, avoid duplicates */
void add_site_def(u_int n, node_address *names, site_def *site) {
  if (n > 0) {
    add_node_list(n, names, &site->nodes);
  }
  realloc_node_set(&site->global_node_set, _get_maxnodes(site));
  realloc_node_set(&site->local_node_set, _get_maxnodes(site));
}

/* Remove nodes from site definition, ignore missing nodes */
void remove_site_def(u_int n, node_address *names, site_def *site) {
  if (n > 0) {
    remove_node_list(n, names, &site->nodes);
  }
  init_detector(site->detected); /* Zero all unused timestamps */
  realloc_node_set(&site->global_node_set, _get_maxnodes(site));
  realloc_node_set(&site->local_node_set, _get_maxnodes(site));
}

/* purecov: begin deadcode */
/* Return boot_key of first site def */
synode_no get_boot_key() {
  assert(!_get_site_def() ||
         _get_site_def()->global_node_set.node_set_len ==
             _get_maxnodes(_get_site_def()));
  if (get_site_def()) {
    return get_site_def()->boot_key;
  } else {
    return null_synode;
  }
}

/* Set boot_key of first site def */
void set_boot_key(synode_no const x) {
  assert(_get_site_def());
  assert(_get_site_def()->global_node_set.node_set_len ==
         _get_maxnodes(_get_site_def()));
  if (site_defs.site_def_ptr_array_val[0]) {
    site_defs.site_def_ptr_array_val[0]->boot_key = x;
  }
}
/* purecov: end */

/* Return group id of site */
uint32_t get_group_id(site_def const *site) {
  if (site) {
    uint32_t group_id = site->start.group_id;
    assert(site->global_node_set.node_set_len == _get_maxnodes(site));
    MAY_DBG(FN; NDBG((unsigned long) group_id, lu); );
    return group_id;
  } else {
    return null_id;
  }
}

#if 0
void	set_group_id(site_def *site, uint32_t id)
{
	MAY_DBG(FN; STRLIT("changing group id from ");
	    NDBG(get_group_id(site), lx);
	    STRLIT("to ");
	    NDBG(id, lu);
	    );
	site->group_id = id;
}

#endif

static inline node_no _get_maxnodes(site_def const *site) {
  if (site) {
    return site->nodes.node_list_len;
  } else
    return 0;
}

/* Return maxnodes of site */
node_no get_maxnodes(site_def const *site) { return _get_maxnodes(site); }

/* purecov: begin deadcode */
node_no get_prev_maxnodes() { return _get_maxnodes(_get_prev_site_def()); }
/* purecov: end */

/* Return nodeno of site */
static inline node_no _get_nodeno(site_def const *site) {
  if (site) {
    assert(site->global_node_set.node_set_len == _get_maxnodes(site));
    return site->nodeno;
  } else
    return VOID_NODE_NO;
}

/* purecov: begin deadcode */
/* Return nodeno of site */
node_no get_nodeno(site_def const *site) { return _get_nodeno(site); }
/* purecov: end */

node_no get_prev_nodeno() { return _get_nodeno(_get_prev_site_def()); }

/* Find the maximum config number known on this node */
synode_no config_max_boot_key(gcs_snapshot const *gcs_snap) {
  int i;
  synode_no max = null_synode;

  /* Loop over all configs looking for max */
  for (i = (int)gcs_snap->cfg.configs_len - 1; i >= 0; i--) {
    config_ptr cp = gcs_snap->cfg.configs_val[i];
    if (cp && synode_gt(cp->boot_key, max)) {
      max = cp->boot_key;
    }
  }
  return max;
}

/* Import configs from snapshot */
void import_config(gcs_snapshot *gcs_snap) {
  int i;
  DBGOUT(FN; SYCEXP(gcs_snap->log_start));
  for (i = (int)gcs_snap->cfg.configs_len - 1; i >= 0; i--) {
    config_ptr cp = gcs_snap->cfg.configs_val[i];
    if (cp) {
      site_def *site = new_site_def();
      DBGOUT(FN; SYCEXP(cp->start); SYCEXP(cp->boot_key));
      init_site_def(cp->nodes.node_list_len, cp->nodes.node_list_val, site);
      site->start = cp->start;
      site->boot_key = cp->boot_key;
      site_install_action(site, app_type);
    }
  }
}

extern synode_no executed_msg;

/* Export configs to snapshot */
gcs_snapshot *export_config() {
  u_int i;
  gcs_snapshot *gcs_snap = calloc((size_t)1, sizeof(gcs_snapshot));
  gcs_snap->cfg.configs_val =
      calloc((size_t)site_defs.count, sizeof(config_ptr));
  gcs_snap->cfg.configs_len = site_defs.count;

  for (i = 0; i < site_defs.count; i++) {
    site_def *site = site_defs.site_def_ptr_array_val[i];
    if (site) {
      config_ptr cp = calloc((size_t)1, sizeof(config));
      init_node_list(site->nodes.node_list_len, site->nodes.node_list_val,
                     &cp->nodes);
      cp->start = site->start;
      cp->boot_key = site->boot_key;
      DBGOUT(FN; SYCEXP(cp->start); SYCEXP(cp->boot_key));
      gcs_snap->cfg.configs_val[i] = cp;
    }
  }
  gcs_snap->log_start = get_delivered_msg();
  return gcs_snap;
}

/* Return the global minimum delivered message number, based on incoming gossip
 */
synode_no get_min_delivered_msg(site_def const *s) {
  u_int i;
  synode_no retval = null_synode;
  int init = 1;

  for (i = 0; i < s->nodes.node_list_len; i++) {
    if (s->servers[i]->detected + DETECTOR_LIVE_TIMEOUT > task_now()) {
      if (init) {
        init = 0;
        retval = s->delivered_msg[i];
      } else {
        if (synode_lt(s->delivered_msg[i], retval)) {
          retval = s->delivered_msg[i];
        }
      }
    }
  }
  DBGOUT(FN; SYCEXP(retval));
  return retval;
}

/* Track the minimum delivered message numbers based on incoming messages */
void update_delivered(site_def *s, node_no node, synode_no msgno) {
  if (node < s->nodes.node_list_len) {
    s->delivered_msg[node] = msgno;
    DBGOUT(FN; SYCEXP(s->delivered_msg[node]); NDBG(node, u));
  }
}
