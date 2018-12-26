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

#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/bitset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

/* purecov: begin deadcode */
node_set bit_set_to_node_set(bit_set *set, u_int n) {
  node_set new_set;
  alloc_node_set(&new_set, n);
  {
    u_int i;
    MAY_DBG(FN; STRLIT("bit_set_to_node_set "); dbg_bitset(set, n););
    for (i = 0; i < n; i++) {
      new_set.node_set_val[i] = BIT_ISSET(i, set);
    }
  }
  return new_set;
}

void dump_node_set(node_set set) {
  u_int i;
  GET_GOUT;
  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
  NDBG(set.node_set_len, u);
  PTREXP(set.node_set_val);
  for (i = 0; i < set.node_set_len; i++) {
    NPUT(set.node_set_val[i], d);
  }
  PRINT_GOUT;
  FREE_GOUT;
}
/* purecov: end */

node_set *alloc_node_set(node_set *set, u_int n) {
  set->node_set_val = calloc((size_t)n, sizeof(bool_t));
  set->node_set_len = n;
  return set;
}

node_set *realloc_node_set(node_set *set, u_int n) {
  u_int old_n = set->node_set_len;
  bool_t *old_p = set->node_set_val;
  u_int i;

  set->node_set_val = realloc(old_p, n * sizeof(bool_t));
  set->node_set_len = n;
  for (i = old_n; i < n; i++) {
    set->node_set_val[i] = 0;
  }
  return set;
}

/* {{{ Copy node set. Reallocate if mismatch */

void copy_node_set(node_set const *from, node_set *to) {
  if (from->node_set_len > 0) {
    u_int i;
    if (to->node_set_val == 0 || from->node_set_len != to->node_set_len) {
      init_node_set(to, from->node_set_len);
    }
    for (i = 0; i < from->node_set_len; i++) {
      to->node_set_val[i] = from->node_set_val[i];
    }
  }
}

/* }}} */

/* {{{ Initialize node set. Free first if necessary */

node_set *init_node_set(node_set *set, u_int n) {
  if (set) {
    free_node_set(set);
    alloc_node_set(set, n);
  }
  return set;
}

/* {{{ Free node set contents */

void free_node_set(node_set *set) {
  if (set) {
    if (set->node_set_val != 0) X_FREE(set->node_set_val);
    set->node_set_len = 0;
  }
}

/* }}} */

/* {{{ Clone set. Used when sending messages */

node_set clone_node_set(node_set set) {
  node_set new_set;
  new_set.node_set_len = 0;
  new_set.node_set_val = 0;
  copy_node_set(&set, &new_set);
  return new_set;
}

/* }}} */

/**
   Debug a node set.
 */
/* purecov: begin deadcode */
char *_dbg_node_set(node_set set, const char *name) {
  u_int i;
  GET_NEW_GOUT;
  STRLIT(name);
  NDBG(set.node_set_len, u);
  PTREXP(set.node_set_val);
  for (i = 0; i < set.node_set_len; i++) {
    NPUT(set.node_set_val[i], d);
  }
  RET_GOUT;
}
/* purecov: end */
/* {{{ Add all nodes */

node_set *set_node_set(node_set *set) {
  u_int i;
  for (i = 0; set && i < set->node_set_len; i++) {
    set->node_set_val[i] = TRUE;
  }
  return set;
}

/* }}} */

/* {{{ Reset a node set */
/* purecov: begin deadcode */
node_set *reset_node_set(node_set *set) {
  u_int i;
  for (i = 0; set && i < set->node_set_len; i++) {
    set->node_set_val[i] = FALSE;
  }
  return set;
}
/* }}} */

/**
   Debug a node set with G_MESSAGE.
 */
void _g_dbg_node_set(node_set set, const char *name MY_ATTRIBUTE((unused))) {
  u_int n = 2 * set.node_set_len + 1;
  char *s = calloc((size_t)n, (size_t)1);
  u_int i;
  for (i = 0; i < set.node_set_len; i++) {
    s[i * 2] = set.node_set_val[i] ? '1' : '0';
    s[i * 2 + 1] = ' ';
  }
  s[n - 1] = 0;
  G_INFO("%s : Node set %s", name, s);
  free(s);
}

/* {{{ Count number of nodes in set */

u_int node_count(node_set set) {
  u_int count = 0;
  u_int i;
  for (i = 0; i < set.node_set_len; i++) {
    if (set.node_set_val[i]) count++;
  }
  return count;
}

/* }}} */

/* {{{ Return true if empty node set */

bool_t is_empty_node_set(node_set set) {
  u_int i;
  for (i = 0; i < set.node_set_len; i++) {
    if (set.node_set_val[i]) return FALSE;
  }
  return TRUE;
}

/* }}} */

/* {{{ Return true if full node set */

bool_t is_full_node_set(node_set set) {
  u_int i;
  for (i = 0; i < set.node_set_len; i++) {
    if (!set.node_set_val[i]) return FALSE;
  }
  return TRUE;
}

/* }}} */

/* {{{ Return true if equal node sets */

bool_t equal_node_set(node_set x, node_set y) {
  u_int i;
  if (x.node_set_len != y.node_set_len) return FALSE;
  for (i = 0; i < x.node_set_len; i++) {
    if (x.node_set_val[i] != y.node_set_val[i]) return FALSE;
  }
  return TRUE;
}
/* purecov: end */
/* }}} */

/* {{{ Return true if node i is in set */

bool_t is_set(node_set set, node_no i) {
  if (i < set.node_set_len) {
    return set.node_set_val[i];
  } else {
    return FALSE;
  }
}

/* }}} */

/* {{{ Add node to set */
/* purecov: begin deadcode */
void add_node(node_set set, node_no node) {
  if (node < set.node_set_len) {
    set.node_set_val[node] = TRUE;
  }
}

/* }}} */

/* {{{ Remove node from set */

void remove_node(node_set set, node_no node) {
  if (node < set.node_set_len) {
    set.node_set_val[node] = FALSE;
  }
}

/* }}} */

/* {{{ AND operation, return result in x */

void and_node_set(node_set *x, node_set const *y) {
  u_int i;
  for (i = 0; i < x->node_set_len && i < y->node_set_len; i++) {
    x->node_set_val[i] = x->node_set_val[i] && y->node_set_val[i];
  }
}

/* }}} */

/* {{{ OR operation, return result in x */
void or_node_set(node_set *x, node_set const *y) {
  u_int i;
  for (i = 0; i < x->node_set_len && i < y->node_set_len; i++) {
    x->node_set_val[i] = x->node_set_val[i] || y->node_set_val[i];
  }
}

/* }}} */

/* {{{ XOR operation, return result in x */
void xor_node_set(node_set *x, node_set const *y) {
  u_int i;
  for (i = 0; i < x->node_set_len && i < y->node_set_len; i++) {
    x->node_set_val[i] =
        x->node_set_val[i] ^ y->node_set_val[i]; /* Beware of the bitwise xor */
  }
}

/* }}} */

/* {{{ NOT operation, return result in x */
void not_node_set(node_set *x, node_set const *y) {
  u_int i;
  for (i = 0; i < x->node_set_len && i < y->node_set_len; i++) {
    x->node_set_val[i] = (y->node_set_val[i] == TRUE ? FALSE : TRUE);
  }
}
/* purecov: end */
/* }}} */
