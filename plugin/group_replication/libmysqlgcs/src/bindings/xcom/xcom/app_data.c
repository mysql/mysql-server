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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/app_data.h"

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xdr_utils.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

define_xdr_funcs(synode_no) define_xdr_funcs(app_data_ptr)

    static app_data_list nextp(app_data_list l);
static unsigned long msg_count(app_data_ptr a);

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
    NDBG64(a->lsn);
    SYCEXP(a->app_key);
    NDBG(a->group_id, x);
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
      case xcom_recover: {
        u_int i = 0;
        synode_no_array *list = &a->body.app_u_u.rep.msg_list;
        SYCEXP(a->body.app_u_u.rep.vers);
        NDBG(list->synode_no_array_len, u);
        for (i = 0; i < list->synode_no_array_len; i++) {
          SYCEXP(list->synode_no_array_val[i]);
        }
      } break;
      case app_type:
        NDBG(a->body.app_u_u.data.data_len, u);
        break;
      case query_type:
        break;
      case query_next_log:
        break;
      case exit_type:
        break;
      case reset_type:
        break;
      case begin_trans:
        break;
      case prepared_trans:
        TIDCEXP(a->body.app_u_u.td.tid);
        NDBG(a->body.app_u_u.td.pc, d);
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
      default:
        STRLIT("unknown type ");
        break;
    }
    PTREXP(a->next);
    RET_GOUT;
  }
  return NULL;
}
/* purecov: end */
/* {{{ Clone app_data message list */

app_data_ptr clone_app_data(app_data_ptr a) {
  app_data_ptr retval = 0;
  app_data_list p = &retval; /* Initialize p with empty list */

  while (0 != a) {
    follow(p, clone_app_data_single(a));
    a = a->next;
    p = nextp(p);
  }
  return retval;
}

/**
   Clone an app_data struct.
 */
app_data_ptr clone_app_data_single(app_data_ptr a) {
  char *str = NULL;
  app_data_ptr p = 0;
  if (0 != a) {
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
      case xcom_recover:
        /* purecov: begin deadcode */
        p->body.app_u_u.rep.vers = a->body.app_u_u.rep.vers;
        p->body.app_u_u.rep.msg_list =
            clone_synode_no_array(a->body.app_u_u.rep.msg_list);
        break;
      /* purecov: end */
      case app_type:
        p->body.app_u_u.data.data_val =
            calloc((size_t)a->body.app_u_u.data.data_len, sizeof(char));
        if (p->body.app_u_u.data.data_val == NULL) {
          p->body.app_u_u.data.data_len = 0;
          G_ERROR("Memory allocation failed.");
          break;
        }
        p->body.app_u_u.data.data_len = a->body.app_u_u.data.data_len;
        memcpy(p->body.app_u_u.data.data_val, a->body.app_u_u.data.data_val,
               (size_t)a->body.app_u_u.data.data_len);
        break;
      case query_type:
        break;
      case query_next_log:
        break;
      case reset_type:
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
      case enable_arbitrator:
      case disable_arbitrator:
      case x_terminate_and_exit:
        break;
      default: /* Should not happen */
        str = dbg_app_data(a);
        G_ERROR("%s", str);
        free(str);
        assert(("No such xcom type" && FALSE));
    }
    assert(p->next == 0);
  }
  return p;
}

size_t node_set_size(node_set ns) {
  return ns.node_set_len * sizeof(*ns.node_set_val);
}

static size_t node_list_size(node_list nodes) {
  return sizeof(node_list) + nodes.node_list_len * sizeof(*nodes.node_list_val);
}

size_t synode_no_array_size(synode_no_array sa) {
  return sa.synode_no_array_len * sizeof(*sa.synode_no_array_val);
}

/**
   Return size of an app_data.
 */
size_t app_data_size(app_data const *a) {
  size_t size = sizeof(*a);
  if (a == 0) return 0;
  switch (a->body.c_t) {
    case xcom_set_group:
    case unified_boot_type:
    case add_node_type:
    case remove_node_type:
    case force_config_type:
    case xcom_boot_type:
      /* purecov: begin deadcode */
      size += node_list_size(a->body.app_u_u.nodes);
      break;
    /* purecov: end */
    case xcom_recover:
      /* purecov: begin deadcode */
      size += synode_no_array_size(a->body.app_u_u.rep.msg_list);
      break;
    /* purecov: end */
    case app_type:
      size += a->body.app_u_u.data.data_len;
      break;
    case query_type:
      break;
    case query_next_log:
      break;
    case reset_type:
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
    case enable_arbitrator:
    case disable_arbitrator:
    case x_terminate_and_exit:
      break;
    default: /* Should not happen */
      assert(("No such xcom type" && FALSE));
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
  app_data_ptr retval = calloc((size_t)1, sizeof(app_data));
  retval->expiry_time = 13.0;
  return retval;
}

app_data_ptr init_app_data(app_data_ptr retval) {
  memset(retval, 0, sizeof(app_data));
  retval->expiry_time = 13.0;
  return retval;
}

/* {{{ Debug list of app_data */
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
    while (0 != a) {
      COPY_AND_FREE_GOUT(dbg_app_data_single(a));
      a = a->next;
    }
    RET_GOUT;
  }
}
/* purecov: end */
/* }}} */

/* {{{ Replace target with copy of source list */

void _replace_app_data_list(app_data_list target, app_data_ptr source) {
  MAY_DBG(FN; PTREXP(target); PTREXP(source));
  XCOM_XDR_FREE(xdr_app_data, *target); /* Will remove the whole list */
  *target = clone_app_data(source);
}

/* }}} */

/**
   Insert p after l.
 */
void follow(app_data_list l, app_data_ptr p) {
  MAY_DBG(FN; PTREXP(p));
  if (p) {
    if (p->next) {
      MAY_DBG(FN; STRLIT("unexpected next ");
              COPY_AND_FREE_GOUT(dbg_app_data(p)));
    }
    assert(p->next == 0);
    p->next = *l;
  }
  *l = p;
  assert(!p || p->next != p);
  MAY_DBG(FN; COPY_AND_FREE_GOUT(dbg_app_data(p)));
}
/* purecov: begin deadcode */
/**
   Count the number of messages in a list.
 */
static unsigned long msg_count(app_data_ptr a) {
  unsigned long n = 0;
  while (a) {
    n++;
    a = a->next;
  }
  return n;
}

/* {{{ Message constructors */

/**
   Sort an array of app_data pointers.
   TODO: Maybe replace with Dewar's improved heap sort?
   Quicksort is not optimal here, since the log is typically
   already almost or completely sorted.
 */
void sort_app_data(app_data_ptr x[], int n) {
#define insert_sort_gt(a, b) synode_gt(a->app_key, b->app_key)
  insert_sort(app_data_ptr, x, n);
#undef insert_sort_gt
}

/* {{{ Create a new app_data message from list of node:port */

app_data_ptr new_nodes(u_int n, node_address *names, cargo_type cargo) {
  app_data_ptr retval = new_app_data();
  retval->body.c_t = cargo;
  retval->log_it = TRUE;
  init_node_list(n, names, &retval->body.app_u_u.nodes);
  assert(retval);
  return retval;
}

/* }}} */

/* {{{ Create a new app_data message from blob */

app_data_ptr new_data(u_int n, char *val, cons_type consensus) {
  u_int i = 0;
  app_data_ptr retval = new_app_data();
  retval->body.c_t = app_type;
  retval->body.app_u_u.data.data_len = n;
  retval->body.app_u_u.data.data_val = calloc((size_t)n, sizeof(char));
  for (i = 0; i < n; i++) {
    retval->body.app_u_u.data.data_val[i] = val[i];
  }
  retval->consensus = consensus;
  return retval;
}

/* }}} */

/* {{{ Create a new reset message */

app_data_ptr new_reset(cargo_type type) {
  app_data_ptr retval = new_app_data();
  retval->app_key = null_synode;
  retval->body.c_t = type;
  retval->consensus = cons_majority;
  return retval;
}

/* {{{ Create a new exit message */

app_data_ptr new_exit() {
  app_data_ptr retval = new_app_data();
  retval->app_key = null_synode;
  retval->body.c_t = exit_type;
  retval->consensus = cons_majority;
  return retval;
}

  /* }}} */

  /* {{{ app_data_list functions */

#if 0 /* UNUSED */
/**
   Return last element of app_data list.
 */
static app_data_ptr last(app_data_ptr p)
{
  app_data_ptr retval = p;
  while(p){
    retval = p;
    p = p->next;
  }
  return retval;
}
#endif

#if 0
/**
   Append p at the end of list l.
 */
static void appendp(app_data_list l, app_data_ptr p)
{
  while(*l) l = nextp(l);
  follow(l,p);
}
#endif

#if 0
/**
   Remove first element of list l, if any.
 */
static app_data_ptr removep(app_data_list l)
{
  app_data_ptr retval = *l;
  if(*l){
    *l = retval->next;
    retval->next = 0;
  }
  return retval;
}
#endif
/* purecov: end */

/* }}} */

/* }}} */
