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

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/site_def.h"

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <iterator>

#include "xcom/bitset.h"
#include "xcom/node_list.h"
#include "xcom/node_no.h"
#include "xcom/node_set.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_struct.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xdr_gen/xcom_vp.h"

typedef site_def *site_def_ptr;

struct site_def_ptr_array {
  u_int count;
  u_int site_def_ptr_array_len;
  site_def_ptr *site_def_ptr_array_val;
};
typedef struct site_def_ptr_array site_def_ptr_array;

init_xdr_array(site_def_ptr) free_xdr_array(site_def_ptr)
    set_xdr_array(site_def_ptr)

    /* FIFO of site definitions */
    static site_def_ptr_array site_defs;

static inline node_no _get_maxnodes(site_def const *site);

/* Return pointer to array of site defs */
void get_all_site_defs(site_def ***s, uint32_t *n) {
  *s = site_defs.site_def_ptr_array_val;
  *n = site_defs.site_def_ptr_array_len;
}

/* Module initialization */
void init_site_vars() {
  init_site_def_ptr_array(&site_defs);
  site_defs.count = 0;
}

/* Recursively free a complete site_def.  Only free the site_def, not
 the servers that it points to, since servers are shared by multiple
 site_defs, and will eventually be deallocated by garbage_collect_servers
*/

void free_site_def_body(site_def *s) {
  if (s) {
    invalidate_detector_sites(s);
    xdr_free((xdrproc_t)xdr_node_list, (char *)(&s->nodes));
    free_node_set(&s->global_node_set);
    free_node_set(&s->local_node_set);
    xdr_free((xdrproc_t)xdr_leader_array, (char *)(&s->leaders));
    IFDBG(D_BUG, FN; STRLIT("free "); PTREXP(s); PTREXP(s->dispatch_table));
    free(s->dispatch_table);
  }
}

void free_site_def(site_def *s) {
  if (s) {
    free_site_def_body(s);
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
}

/* Add a new site definition to the list */
site_def *push_site_def(site_def *s) {
  uint32_t i;
  set_site_def_ptr(&site_defs, nullptr, site_defs.count);
  IFDBG(
      D_NONE, FN; NDBG(site_defs.count, u); PTREXP(s); if (s) {
        SYCEXP(s->start);
        SYCEXP(s->boot_key);
      });
  for (i = site_defs.count; i > 0; i--) {
    IFDBG(
        D_NONE, NDBG(i - 1, d); PTREXP(site_defs.site_def_ptr_array_val[i - 1]);
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
    return nullptr;
}

/* Return first site def */
site_def *get_site_def_rw() {
  if (site_defs.count > 0)
    return site_defs.site_def_ptr_array_val[0];
  else
    return nullptr;
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
    return nullptr;
}
/* purecov: end */

/* Return first site def as const ptr */
site_def const *get_site_def() { return _get_site_def(); }

/* purecov: begin deadcode */

/* Return previous site def */
site_def const *get_prev_site_def() { return _get_prev_site_def(); }

/* purecov: end */

/* Checks if site->start >= synode. */
static inline int match_def(site_def const *site, synode_no synode) {
  return site &&
         (synode.group_id == 0 || synode.group_id == site->start.group_id) &&
         !synode_lt(synode, site->start);
}

/* Return first site def which has start less than or equal to synode */
site_def const *find_site_def(synode_no synode) {
  site_def const *retval = nullptr;
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
  site_def *retval = nullptr;
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

/* Checks if site->start > synode. */
static inline int start_gt(site_def const *site, synode_no synode) {
  return site &&
         (synode.group_id == 0 || synode.group_id == site->start.group_id) &&
         synode_gt(site->start, synode);
}

/* Retrieve the first site_def which has start greater than synode. */
site_def const *find_next_site_def(synode_no synode) {
  site_def const *retval = nullptr;
  u_int i;

  for (i = site_defs.count; i > 0; i--)
    if (start_gt(site_defs.site_def_ptr_array_val[i - 1], synode)) {
      retval = site_defs.site_def_ptr_array_val[i - 1];
      break;
    }
  assert(retval == nullptr ||
         retval->global_node_set.node_set_len == _get_maxnodes(retval));
  return retval;
}

static inline int prev_def(site_def const *site, synode_no synode) {
  return (site &&
          (synode.group_id == 0 || synode.group_id == site->start.group_id));
}

site_def const *find_prev_site_def(synode_no synode) {
  site_def const *retval = nullptr;
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

  IFDBG(D_NONE, FN; NDBG(site_defs.count, u); SYCEXP(x););
  for (i = 3; i < s_max; i++) {
    if (match_def(site_defs.site_def_ptr_array_val[i], x)) {
      break;
    }
  }
  i++;
  for (; i < s_max; i++) {
    site_def *site = site_defs.site_def_ptr_array_val[i];
    IFDBG(D_NONE, NDBG(i, d); PTREXP(site_defs.site_def_ptr_array_val[i]););
    if (site) {
      IFDBG(D_NONE, SYCEXP(site->start); SYCEXP(site->boot_key););
      free_site_def(site);
      site_defs.site_def_ptr_array_val[i] = nullptr;
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
  site_def *retval = (site_def *)xcom_calloc((size_t)1, sizeof(site_def));
  retval->nodeno = VOID_NODE_NO;
  return retval;
}

static void clone_leader(leader *l, leader const *x) {
  l->address = strdup(x->address);
}

leader_array alloc_leader_array(u_int n) {
  leader_array a{};
  a.leader_array_val =
      static_cast<leader *>(xcom_calloc((size_t)n, sizeof(leader)));
  if (a.leader_array_val) a.leader_array_len = n;
  return a;
}

leader_array clone_leader_array(leader_array const x) {
  u_int i;
  leader_array a = alloc_leader_array(x.leader_array_len);
  for (i = 0; i < a.leader_array_len; i++) {
    clone_leader(&a.leader_array_val[i], &x.leader_array_val[i]);
  }
  return a;
}

/* Clone a site definition */

site_def *clone_site_def(site_def const *site) {
  site_def *retval = new_site_def();
  assert(site->global_node_set.node_set_len == _get_maxnodes(site));
  *retval = *site;
  init_node_list(site->nodes.node_list_len, site->nodes.node_list_val,
                 &retval->nodes);
  retval->global_node_set = clone_node_set(site->global_node_set);
  retval->local_node_set = clone_node_set(site->local_node_set);
  retval->leaders = clone_leader_array(site->leaders);
  retval->cached_leaders = false;    // Invalidate cached leaders
  retval->dispatch_table = nullptr;  // Invalidate dispatch table
  assert(retval->global_node_set.node_set_len == _get_maxnodes(retval));
  IFDBG(D_NONE, FN; PTREXP(site); PTREXP(retval));
  return retval;
}

/* Initialize a site definition from array of string pointers */

void init_site_def(u_int n, node_address *names, site_def *site) {
  const site_def *latest_config;
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
  /* Inherit latest configuration's event horizon or fallback to default */
  latest_config = get_site_def();
  if (latest_config != nullptr) {
    site->event_horizon = latest_config->event_horizon;
  } else {
    site->event_horizon = EVENT_HORIZON_MIN;
  }
  assert(site->event_horizon);
}

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
    realloc_node_set(&site->global_node_set, _get_maxnodes(site));
    realloc_node_set(&site->local_node_set, _get_maxnodes(site));
  }
}

/* Return group id of site */
uint32_t get_group_id(site_def const *site) {
  if (site) {
    uint32_t group_id = site->start.group_id;
    assert(site->global_node_set.node_set_len == _get_maxnodes(site));
    IFDBG(D_NONE, FN; NDBG((unsigned long)group_id, lu););
    return group_id;
  } else {
    return null_id;
  }
}

static inline node_no _get_maxnodes(site_def const *site) {
  if (site) {
    return site->nodes.node_list_len;
  } else
    return 0;
}

/* Return maxnodes of site */
node_no get_maxnodes(site_def const *site) { return _get_maxnodes(site); }

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

node_no find_nodeno(site_def const *site, const char *address) {
  uint32_t i;
  G_TRACE("find_nodeno: Node to find: %s", address);
  for (i = 0; i < site->nodes.node_list_len; i++) {
    G_TRACE("find_nodeno: Node %d: %s", i,
            site->nodes.node_list_val[i].address);
    if (strcmp(site->nodes.node_list_val[i].address, address) == 0) return i;
  }
  return VOID_NODE_NO;
}

node_no get_prev_nodeno() { return _get_nodeno(_get_prev_site_def()); }

/* Find the maximum config number known on this node */
synode_no config_max_boot_key(gcs_snapshot const *gcs_snap) {
  int i;
  synode_no max = null_synode;

  /* Loop over all configs looking for max */
  for (i = (int)gcs_snap->cfg.configs_len - 1; i >= 0; i--) {
    config_ptr cp = gcs_snap->cfg.configs_val[i];
    if (cp && cp->boot_key.group_id == gcs_snap->log_start.group_id &&
        synode_gt(cp->boot_key, max)) {
      max = cp->boot_key;
    }
  }
  return max;
}

/* Import configs from snapshot */
void import_config(gcs_snapshot *gcs_snap) {
  int i;
  IFDBG(D_NONE, FN; SYCEXP(gcs_snap->log_start); SYCEXP(gcs_snap->log_end));
  for (i = (int)gcs_snap->cfg.configs_len - 1; i >= 0; i--) {
    config_ptr cp = gcs_snap->cfg.configs_val[i];
    if (cp) {
      /* We now have a valid pointer to a config.
         Unconditionally import if if we have no configs.
         If we have a config, import it if either the boot_key or
         start does not match the latest config we already have.
         This avoids import of duplicate configs.
         */
      if (!get_site_def() ||
          !synode_eq(cp->boot_key, get_site_def()->boot_key) ||
          !synode_eq(cp->start, get_site_def()->start)) {
        site_def *site = new_site_def();
        IFDBG(D_NONE, FN; SYCEXP(cp->start); SYCEXP(cp->boot_key));
        init_site_def(cp->nodes.node_list_len, cp->nodes.node_list_val, site);
        site->start = cp->start;
        site->boot_key = cp->boot_key;
        assert(cp->event_horizon);
        site->event_horizon = cp->event_horizon;
        copy_node_set(&cp->global_node_set, &site->global_node_set);
        site->max_active_leaders = cp->max_active_leaders;
        site->leaders = clone_leader_array(cp->leaders);
        site_install_action(site, app_type);
      }
    }
  }
}

extern synode_no executed_msg;

/* Return maximum config number, which is the config number of the last site */
static synode_no get_conf_max() {
  u_int i;
  for (i = 0; i < site_defs.count; i++) {
    site_def *site = site_defs.site_def_ptr_array_val[i];
    if (site) {
      return site->boot_key;
    }
  }
  return null_synode;
}

/* Export configs to snapshot */
gcs_snapshot *export_config() {
  u_int i;
  gcs_snapshot *gcs_snap =
      (gcs_snapshot *)xcom_calloc((size_t)1, sizeof(gcs_snapshot));
  gcs_snap->cfg.configs_val =
      (config_ptr *)xcom_calloc((size_t)site_defs.count, sizeof(config_ptr));
  gcs_snap->cfg.configs_len = site_defs.count;

  for (i = 0; i < site_defs.count; i++) {
    site_def *site = site_defs.site_def_ptr_array_val[i];
    if (site) {
      config_ptr cp = (config_ptr)xcom_calloc((size_t)1, sizeof(config));
      init_node_list(site->nodes.node_list_len, site->nodes.node_list_val,
                     &cp->nodes);
      cp->start = site->start;
      cp->boot_key = site->boot_key;
      cp->event_horizon = site->event_horizon;
      assert(cp->event_horizon);
      cp->global_node_set = clone_node_set(site->global_node_set);
      cp->max_active_leaders = site->max_active_leaders;
      cp->leaders = clone_leader_array(site->leaders);
      IFDBG(D_BUG, FN; SYCEXP(cp->start); SYCEXP(cp->boot_key));
      gcs_snap->cfg.configs_val[i] = cp;
    }
  }
  /* log_start is the last message that has actually been delivered and
  executed. During recovery, only messages > log_start will be delivered. */
  gcs_snap->log_start = get_last_delivered_msg();
  /* log_end is the first message that will not be delivered during recovery. */
  gcs_snap->log_end = get_conf_max(); /* Set log_end based on configs */
  set_log_end(gcs_snap); /* Possibly advance log_end based on max_synode */

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
  IFDBG(D_NONE, FN; SYCEXP(retval));
  return retval;
}

/* Track the minimum delivered message numbers based on incoming messages */
void update_delivered(site_def *s, node_no node, synode_no msgno) {
  if (node < s->nodes.node_list_len) {
    s->delivered_msg[node] = msgno;
    /* IFDBG(D_NONE, FN; SYCEXP(s->delivered_msg[node]); NDBG(node,u)); */
  }
}

/* The last shall be first and the first last.
The FIRST config in the snaphost has the highest message number, and
the LAST config has the lowest. */

/* Return boot_key of highest numbered config in snapshot */
synode_no get_highest_boot_key(gcs_snapshot *gcs_snap) {
  int i;
  synode_no retval = null_synode;
  IFDBG(D_NONE, FN; SYCEXP(gcs_snap->log_start); SYCEXP(gcs_snap->log_end));
  for (i = 0; i < (int)gcs_snap->cfg.configs_len; i++) {
    config_ptr cp = gcs_snap->cfg.configs_val[i];
    if (cp) {
      IFDBG(D_NONE, FN; SYCEXP(cp->start); SYCEXP(cp->boot_key));
      retval = cp->boot_key;
      break;
    }
  }
  assert(!synode_eq(retval, null_synode));
  return retval;
}
