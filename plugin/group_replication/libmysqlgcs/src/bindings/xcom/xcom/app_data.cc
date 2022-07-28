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

#include <assert.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom/app_data.h"
#include "xcom/checked_data.h"
#include "xcom/node_list.h"
#include "xcom/node_set.h"
#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/synode_no.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_vp_str.h"
#include "xcom/xdr_utils.h"
#include "xdr_gen/xcom_vp.h"

static app_data_list nextp(app_data_list l);

/**
   Debug a single app_data struct.
 */
/* purecov: begin deadcode */
static char *dbg_app_data_single(app_data_ptr a) {
  if (a) {
    GET_NEW_GOUT;
    STRLIT("app_data");
    PTREXP(a);
    SYCEXP(a->unique_id);
    NDBG(a->group_id, x);
    NDBG64(a->lsn);
    SYCEXP(a->app_key);
    NDBG(a->consensus, d);
    NDBG(a->log_it, d);
    NDBG(a->chosen, d);
    NDBG(a->recover, d);
    NDBG(a->expiry_time, f);
    STRLIT(cargo_type_to_str(a->body.c_t));
    STRLIT(" ");
    switch (a->body.c_t) {
      case xcom_set_group: {
        node_list *nodes = &a->body.app_u_u.nodes;
        COPY_AND_FREE_GOUT(dbg_list(nodes));
      } break;
      case unified_boot_type:
      case add_node_type:
      case remove_node_type:
      case force_config_type: {
        node_list *nodes = &a->body.app_u_u.nodes;
        COPY_AND_FREE_GOUT(dbg_list(nodes));
      } break;
      case xcom_boot_type: {
        node_list *nodes = &a->body.app_u_u.nodes;
        COPY_AND_FREE_GOUT(dbg_list(nodes));
      } break;
      case app_type:
        NDBG(a->body.app_u_u.data.data_len, u);
        break;
      case exit_type:
        break;
      case reset_type:
        break;
      case begin_trans:
        break;
      case prepared_trans:
        TIDCEXP(a->body.app_u_u.td.tid);
        NDBG(a->body.app_u_u.td.pc, u);
        STREXP(a->body.app_u_u.td.cluster_name);
        break;
      case abort_trans:
        TIDCEXP(a->body.app_u_u.td.tid);
        NDBG(a->body.app_u_u.td.pc, d);
        STREXP(a->body.app_u_u.td.cluster_name);
        break;
      case view_msg:
        COPY_AND_FREE_GOUT(dbg_node_set(a->body.app_u_u.present));
        break;
      case get_event_horizon_type:
        break;
      case set_event_horizon_type:
        NDBG(a->body.app_u_u.event_horizon, u);
        break;
      case set_max_leaders:
        NDBG(a->body.app_u_u.max_leaders, u);
        break;
      case set_leaders_type:
        for (u_int i = 0; i < a->body.app_u_u.leaders.leader_array_len; i++) {
          STREXP(a->body.app_u_u.leaders.leader_array_val[i].address);
          STREXP(" ");
        }
        break;
      case get_leaders_type:
        break;
      default:
        STRLIT("unknown type ");
        break;
    }
    PTREXP(a->next);
    RET_GOUT;
  }
  return nullptr;
}
/* purecov: end */
/* Clone app_data message list */

app_data_ptr clone_app_data(app_data_ptr a) {
  app_data_ptr retval = nullptr;
  app_data_list p = &retval; /* Initialize p with empty list */

  while (nullptr != a) {
    app_data_ptr clone = clone_app_data_single(a);
    follow(p, clone);
    a = a->next;
    p = nextp(p);
    if (clone == nullptr && retval != nullptr) {
      XCOM_XDR_FREE(xdr_app_data, retval);
      break;
    }
  }
  return retval;
}

/**
   Clone an app_data struct.
 */
app_data_ptr clone_app_data_single(app_data_ptr a) {
  char *str = nullptr;
  app_data_ptr p = nullptr;

  if (nullptr != a) {
    bool_t copied = FALSE;

    p = new_app_data();
    p->unique_id = a->unique_id;
    p->lsn = a->lsn;
    p->app_key = a->app_key;
    p->consensus = a->consensus;
    p->expiry_time = a->expiry_time;
    p->body.c_t = a->body.c_t;
    p->group_id = a->group_id;
    p->log_it = a->log_it;
    p->chosen = a->chosen;
    p->recover = a->recover;
    switch (a->body.c_t) {
      case xcom_set_group:
      case unified_boot_type:
      case add_node_type:
      case remove_node_type:
      case force_config_type:
      case xcom_boot_type: {
        p->body.app_u_u.nodes = clone_node_list(a->body.app_u_u.nodes);
      } break;
      case app_type:
        copied =
            copy_checked_data(&p->body.app_u_u.data, &a->body.app_u_u.data);
        if (!copied) {
          G_ERROR("Memory allocation failed.");
          free(p);
          return nullptr;
        }
        break;
#ifdef XCOM_TRANSACTIONS
      case begin_trans:
        break;
      case prepared_trans:
        p->body.app_u_u.tid = a->body.app_u_u.tid;
        break;
      case abort_trans:
        p->body.app_u_u.tid = a->body.app_u_u.tid;
        break;
#endif
      case view_msg:
        p->body.app_u_u.present = clone_node_set(a->body.app_u_u.present);
        break;
      case exit_type: /* purecov: deadcode */
      case enable_arbitrator:
      case disable_arbitrator:
      case x_terminate_and_exit:
        break;
      case get_event_horizon_type:
        break;
      case set_event_horizon_type:
        p->body.app_u_u.event_horizon = a->body.app_u_u.event_horizon;
        break;
      case set_max_leaders:
        p->body.app_u_u.max_leaders = a->body.app_u_u.max_leaders;
        break;
      case set_leaders_type:
        p->body.app_u_u.leaders = clone_leader_array(a->body.app_u_u.leaders);
        break;
      default: /* Should not happen */
        str = dbg_app_data(a);
        G_ERROR("%s", str);
        free(str);
        assert(("No such xcom type" && FALSE));
    }
    assert(p->next == nullptr);
  }
  return p;
}

size_t node_set_size(node_set ns) {
  return ns.node_set_len * sizeof(*ns.node_set_val);
}

size_t synode_no_array_size(synode_no_array sa) {
  return sa.synode_no_array_len * sizeof(*sa.synode_no_array_val);
}

/**
   Return size of an app_data.
   Used both for keeping track of the size of cached data, which is OK, as long
   as no one steals the payload, and to control the xcom automatic
   batching, which is more dubious, since there we should use the length of
   serialized data.
 */
size_t app_data_size(app_data const *a) {
  size_t size = sizeof(*a);
  if (a == nullptr) return 0;
  switch (a->body.c_t) {
    case xcom_set_group:
    case unified_boot_type:
    case add_node_type:
    case remove_node_type:
    case force_config_type:
    case app_type:
      size += a->body.app_u_u.data.data_len;
      break;
#ifdef XCOM_TRANSACTIONS
    case begin_trans:
      break;
    case prepared_trans:
      break;
    case abort_trans:
      break;
#endif
    case view_msg:
      size += node_set_size(a->body.app_u_u.present);
      break;
    case exit_type:
    case enable_arbitrator:
    case disable_arbitrator:
    case x_terminate_and_exit:
    case get_event_horizon_type:
    case set_event_horizon_type:
    case get_synode_app_data_type:
    case convert_into_local_server_type:
    case set_max_leaders:
    case set_leaders_type:
      break;
    default: /* Should not happen */
      DBGOUT_ASSERT(FALSE, STRLIT("No such cargo type "); NDBG(a->body.c_t, d));
  }
  return size;
}

/* app_data structs may be linked. This function returns the size of the whole
 * list */
size_t app_data_list_size(app_data const *a) {
  size_t size = 0;
  while (a) {
    size += app_data_size(a);
    a = a->next;
  }
  return (size);
}

/**
   Return next element in list of app_data.
 */
static app_data_list nextp(app_data_list l) { return (*l) ? &((*l)->next) : l; }

/**
   Constructor for app_data
 */
app_data_ptr new_app_data() {
  app_data_ptr retval = (app_data_ptr)xcom_calloc((size_t)1, sizeof(app_data));
  retval->expiry_time = 13.0;
  return retval;
}

app_data_ptr init_app_data(app_data_ptr retval) {
  memset(retval, 0, sizeof(app_data));
  retval->expiry_time = 13.0;
  return retval;
}

/* Debug list of app_data */
/* purecov: begin deadcode */
char *dbg_app_data(app_data_ptr a) {
  if (msg_count(a) > 100) {
    G_WARNING("Abnormally long message list %lu", msg_count(a));
  }
  {
    GET_NEW_GOUT;
    STRLIT("app_data ");
    PTREXP(a);
    NDBG(msg_count(a), lu);
    while (nullptr != a) {
      COPY_AND_FREE_GOUT(dbg_app_data_single(a));
      a = a->next;
    }
    RET_GOUT;
  }
}
/* purecov: end */

/* Replace target with copy of source list */

void _replace_app_data_list(app_data_list target, app_data_ptr source) {
  IFDBG(D_NONE, FN; PTREXP(target); PTREXP(source));
  XCOM_XDR_FREE(xdr_app_data, *target); /* Will remove the whole list */
  *target = clone_app_data(source);
}

/**
   Insert p after l.
 */
void follow(app_data_list l, app_data_ptr p) {
  IFDBG(D_NONE, FN; PTREXP(p));
  if (p) {
    if (p->next) {
      IFDBG(D_NONE, FN; STRLIT("unexpected next ");
            COPY_AND_FREE_GOUT(dbg_app_data(p)));
    }
    assert(p->next == nullptr);
    p->next = *l;
  }
  *l = p;
  assert(!p || p->next != p);
  IFDBG(D_NONE, FN; COPY_AND_FREE_GOUT(dbg_app_data(p)));
}

/* purecov: begin deadcode */
/**
   Count the number of messages in a list.
 */
unsigned long msg_count(app_data_ptr a) {
  unsigned long n = 0;
  while (a) {
    n++;
    a = a->next;
  }
  return n;
}

#ifdef XCOM_STANDALONE
/* Create a new app_data message from list of node:port */

app_data_ptr new_nodes(u_int n, node_address *names, cargo_type cargo) {
  app_data_ptr retval = new_app_data();
  retval->body.c_t = cargo;
  retval->log_it = TRUE;
  init_node_list(n, names, &retval->body.app_u_u.nodes);
  assert(retval);
  return retval;
}

/* Create a new app_data message from blob */

app_data_ptr new_data(u_int n, char *val, cons_type consensus) {
  u_int i = 0;
  app_data_ptr retval = new_app_data();
  retval->body.c_t = app_type;
  retval->body.app_u_u.data.data_len = n;
  retval->body.app_u_u.data.data_val =
      (char *)xcom_calloc((size_t)n, sizeof(char));
  for (i = 0; i < n; i++) {
    retval->body.app_u_u.data.data_val[i] = val[i];
  }
  retval->consensus = consensus;
  return retval;
}

/* Create a new reset message */

app_data_ptr new_reset(cargo_type type) {
  app_data_ptr retval = new_app_data();
  retval->app_key = null_synode;
  retval->body.c_t = type;
  retval->consensus = cons_majority;
  return retval;
}

/* Create a new exit message */

app_data_ptr new_exit() {
  app_data_ptr retval = new_app_data();
  retval->app_key = null_synode;
  retval->body.c_t = exit_type;
  retval->consensus = cons_majority;
  return retval;
}

/* purecov: end */
#endif
