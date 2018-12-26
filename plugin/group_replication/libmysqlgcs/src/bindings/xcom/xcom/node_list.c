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

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"

#ifndef _WIN32
#include <strings.h>
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_address.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

extern xcom_port xcom_get_port(char *a);

/**
   Debug a node list.
 */
/* purecov: begin deadcode */
char *dbg_list(node_list const *nodes) {
  u_int i;
  GET_NEW_GOUT;
  PTREXP(nodes);
  NDBG(nodes->node_list_len, u);
  PTREXP(nodes->node_list_val);
  for (i = 0; i < nodes->node_list_len; i++) {
    COPY_AND_FREE_GOUT(dbg_node_address(nodes->node_list_val[i]));
  }
  RET_GOUT;
}
/* purecov: end */

/* {{{ Clone a node list */

node_list clone_node_list(node_list list) {
  node_list retval;
  init_node_list(list.node_list_len, list.node_list_val, &retval);
  return retval;
}

/* }}} */

/* OHKFIX Do something more intelligent than strcmp */
int match_node(node_address *n1, node_address *n2, u_int with_uid) {
  int retval = (n1 != 0 && n2 != 0 &&
                (xcom_get_port(n1->address) == xcom_get_port(n2->address)) &&
                strcmp(n1->address, n2->address) == 0);

  if (with_uid) {
    int retval_with_uid = (n1->uuid.data.data_len == n2->uuid.data.data_len);
    if (retval_with_uid) {
      u_int i = 0;
      for (; i < n1->uuid.data.data_len && retval_with_uid; i++) {
        retval_with_uid &=
            !(n1->uuid.data.data_val[i] ^ n2->uuid.data.data_val[i]);
      }
    }
    retval &= retval_with_uid;
  }

  return retval;
}

int match_node_list(node_address *n1, node_address *n2, u_int len2,
                    u_int with_uid) {
  u_int i;
  for (i = 0; i < len2; i++) {
    if (match_node(n2 + i, n1, with_uid)) return 1;
  }
  return 0;
}

static int exists(node_address *name, node_list const *nodes, u_int with_uid) {
  return match_node_list(name, nodes->node_list_val, nodes->node_list_len,
                         with_uid);
}

int node_exists(node_address *name, node_list const *nodes) {
  return exists(name, nodes, FALSE);
}

int node_exists_with_uid(node_address *name, node_list const *nodes) {
  return exists(name, nodes, TRUE);
}

static u_int added_nodes(u_int n, node_address *names, node_list *nodes) {
  u_int i;
  u_int added = n;
  if (nodes->node_list_val) {
    for (i = 0; i < n; i++) {
      if (exists(&names[i], nodes, FALSE)) {
        added--;
      }
    }
  }
  return added;
}

static void init_proto_range(x_proto_range *r) {
  r->min_proto = my_min_xcom_version;
  r->max_proto = my_xcom_version;
}

/* Add nodes to node list, avoid duplicate entries */
void add_node_list(u_int n, node_address *names, node_list *nodes) {
  /* Find new nodes */
  if (n && names) {
    u_int added = added_nodes(n, names, nodes);

    if (added) {
      node_address *np = 0;
      u_int i;

      /* Expand node list and add new nodes */
      nodes->node_list_val =
          realloc(nodes->node_list_val,
                  (added + nodes->node_list_len) * sizeof(node_address));
      np = &nodes->node_list_val[nodes->node_list_len];
      for (i = 0; i < n; i++) {
        /* 			DBGOUT(FN; STREXP(names[i])); */
        if (!exists(&names[i], nodes, FALSE)) {
          np->address = strdup(names[i].address);
          np->uuid.data.data_len = names[i].uuid.data.data_len;
          if (np->uuid.data.data_len) {
            np->uuid.data.data_val =
                calloc((size_t)1, (size_t)np->uuid.data.data_len);
            memcpy(np->uuid.data.data_val, names[i].uuid.data.data_val,
                   (size_t)np->uuid.data.data_len);
          } else {
            np->uuid.data.data_val = 0;
          }
          np->proto = names[i].proto;
          np++;
          /* Update length here so next iteration will check for duplicates
             against newly a
              dded node
                                               */
          nodes->node_list_len++;
        }
      }
    }
  }
}

/* Remove nodes from node list, ignore missing nodes */
void remove_node_list(u_int n, node_address *names, node_list *nodes) {
  node_address *np = 0;
  u_int i;
  u_int new_len = nodes->node_list_len;

  np = nodes->node_list_val;
  for (i = 0; i < nodes->node_list_len; i++) {
    if (match_node_list(&nodes->node_list_val[i], names, n, FALSE)) {
      free(nodes->node_list_val[i].address);
      nodes->node_list_val[i].address = 0;
      free(nodes->node_list_val[i].uuid.data.data_val);
      nodes->node_list_val[i].uuid.data.data_val = 0;
      new_len--;
    } else {
      *np = nodes->node_list_val[i];
      np++;
    }
  }
  nodes->node_list_len = new_len;
}

/* {{{ Initialize a node list from array of string pointers */

void init_node_list(u_int n, node_address *names, node_list *nodes) {
  nodes->node_list_len = 0;
  nodes->node_list_val = 0;
  add_node_list(n, names, nodes);
}

node_list *empty_node_list() { return calloc((size_t)1, sizeof(node_list)); }

/* }}} */

node_address *init_single_node_address(node_address *na, char *name) {
  na->address = strdup(name);
  init_proto_range(&(na->proto));
  assert(na->uuid.data.data_len == 0 && na->uuid.data.data_val == 0);
  return na;
}

node_address *init_node_address(node_address *na, u_int n, char *names[]) {
  u_int i;
  for (i = 0; i < n; i++) {
    init_single_node_address(&na[i], names[i]);
  }
  return na;
}

node_address *new_node_address(u_int n, char *names[]) {
  node_address *na = calloc((size_t)n, sizeof(node_address));
  return init_node_address(na, n, names);
}

node_address *new_node_address_uuid(u_int n, char *names[], blob uuids[]) {
  u_int i = 0;

  node_address *na = calloc((size_t)n, sizeof(node_address));
  init_node_address(na, n, names);

  for (; i < n; i++) {
    na[i].uuid.data.data_len = uuids[i].data.data_len;
    na[i].uuid.data.data_val = calloc(uuids[i].data.data_len, sizeof(char));
    na[i].uuid.data.data_val =
        strncpy(na[i].uuid.data.data_val, uuids[i].data.data_val,
                uuids[i].data.data_len);
  }

  return na;
}

void delete_node_address(u_int n, node_address *na) {
  u_int i;
  for (i = 0; i < n; i++) {
    free(na[i].address);
    na[i].address = 0;
    free(na[i].uuid.data.data_val);
    na[i].uuid.data.data_val = 0;
  }
  free(na);
  na = 0;
}
