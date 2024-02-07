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

#include <assert.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom/x_platform.h"

#ifndef _WIN32
#include <strings.h>
#endif

#include "xcom/node_address.h"
#include "xcom/node_list.h"
#include "xcom/server_struct.h"
#include "xcom/site_def.h"
#include "xcom/task_debug.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xdr_gen/xcom_vp.h"

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

/* Clone a node list */

node_list clone_node_list(node_list list) {
  node_list retval;
  init_node_list(list.node_list_len, list.node_list_val, &retval);
  return retval;
}

int match_node(node_address const *n1, node_address const *n2, u_int with_uid) {
  char n1_ip[IP_MAX_SIZE], n2_ip[IP_MAX_SIZE];
  xcom_port n1_port, n2_port;
  int error_ipandport1, error_ipandport2;
  int retval;

  if (n1 == nullptr || n2 == nullptr) return 0;

  error_ipandport1 = get_ip_and_port(n1->address, n1_ip, &n1_port);
  error_ipandport2 = get_ip_and_port(n2->address, n2_ip, &n2_port);

  retval = (!error_ipandport1 && !error_ipandport2 && (n1_port == n2_port) &&
            strcmp(n1->address, n2->address) == 0);

  if (with_uid) {
    retval =
        retval && (n1->uuid.data.data_len == n2->uuid.data.data_len) &&
        (memcmp((void *)n1->uuid.data.data_val, (void *)n2->uuid.data.data_val,
                n1->uuid.data.data_len) == 0);
  }

  return retval;
}

int match_node_list(node_address const *n1, node_address const *n2, u_int len2,
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

/* Clone a blob */
blob clone_blob(blob const b) {
  blob retval = b;
  if (retval.data.data_len > 0) {
    retval.data.data_val = (char *)calloc((size_t)1, (size_t)b.data.data_len);
    memcpy(retval.data.data_val, b.data.data_val, (size_t)retval.data.data_len);
  } else {
    retval.data.data_val = nullptr;
  }
  return retval;
}

/* purecov: begin deadcode */
blob *clone_blob_ptr(blob const *b) {
  blob *retval = (blob *)calloc((size_t)1, sizeof(blob));
  *retval = clone_blob(*b);
  return retval;
}
/* purecov: end */

static void clone_node_address(node_address *target,
                               node_address const *source) {
  *target = *source; /* Copy everything */
  /* Now clone what should not be shared */
  target->address = strdup(source->address);
  target->uuid = clone_blob(source->uuid);
}

/* Add nodes to node list, avoid duplicate entries */
void add_node_list(u_int n, node_address *names, node_list *nodes) {
  /* Find new nodes */
  if (n && names) {
    u_int added = added_nodes(n, names, nodes);

    if (added) {
      node_address *np = nullptr;
      u_int i;

      /* Expand node list and add new nodes */
      nodes->node_list_val = (node_address *)realloc(
          nodes->node_list_val,
          (added + nodes->node_list_len) * sizeof(node_address));
      np = &nodes->node_list_val[nodes->node_list_len];
      for (i = 0; i < n; i++) {
        /* 			IFDBG(D_NONE, FN; STREXP(names[i])); */
        if (!exists(&names[i], nodes, FALSE)) {
          clone_node_address(np, &names[i]);
          np++;
          /* Update length here so next iteration will check for duplicates
             against newly added node */
          nodes->node_list_len++;
          ADD_DBG(D_BASE, add_event(EVENT_DUMP_PAD, string_arg("adding node"));
                  add_event(EVENT_DUMP_PAD, uint_arg(nodes->node_list_len));
                  add_event(EVENT_DUMP_PAD, string_arg("node_list"));
                  add_event(EVENT_DUMP_PAD, void_arg(nodes));
                  /* add_event(EVENT_DUMP_PAD,
                     uint_arg(chksum_node_list(nodes))); */
          );
        }
      }
    }
  }
}

/* Remove nodes from node list, ignore missing nodes */
void remove_node_list(u_int n, node_address *names, node_list *nodes) {
  node_address *np = nullptr;
  u_int i;
  u_int new_len = nodes->node_list_len;

  np = nodes->node_list_val;
  for (i = 0; i < nodes->node_list_len; i++) {
    if (match_node_list(&nodes->node_list_val[i], names, n, FALSE)) {
      free(nodes->node_list_val[i].address);
      nodes->node_list_val[i].address = nullptr;
      free(nodes->node_list_val[i].uuid.data.data_val);
      nodes->node_list_val[i].uuid.data.data_val = nullptr;
      new_len--;
      ADD_DBG(
          D_BASE, add_event(EVENT_DUMP_PAD, string_arg("removing node"));
          add_event(EVENT_DUMP_PAD, uint_arg(i));
          add_event(EVENT_DUMP_PAD, string_arg("node_list"));
          add_event(EVENT_DUMP_PAD, void_arg(nodes));
          /* add_event(EVENT_DUMP_PAD, uint_arg(chksum_node_list(nodes))); */
      );
    } else {
      *np = nodes->node_list_val[i];
      np++;
    }
  }
  nodes->node_list_len = new_len;
}

/* Initialize a node list from array of string pointers */

void init_node_list(u_int n, node_address *names, node_list *nodes) {
  nodes->node_list_len = 0;
  nodes->node_list_val = nullptr;
  add_node_list(n, names, nodes);
}

node_list *empty_node_list() {
  return (node_list *)xcom_calloc((size_t)1, sizeof(node_list));
}
/* purecov: end */

node_address *init_single_node_address(node_address *na, char const *name,
                                       uint32_t services) {
  na->address = strdup(name);
  init_proto_range(&(na->proto));
  na->services = services;
  assert(na->uuid.data.data_len == 0 && na->uuid.data.data_val == nullptr);
  return na;
}

node_address *init_node_address(node_address *na, u_int n,
                                char const *names[]) {
  u_int i;
  for (i = 0; i < n; i++) {
    init_single_node_address(&na[i], names[i]);
  }
  return na;
}

/* Create node addresses with default roles from array of names */
node_address *new_node_address(u_int n, char const *names[]) {
  node_address *na =
      (node_address *)xcom_calloc((size_t)n, sizeof(node_address));
  return init_node_address(na, n, names);
}

/* Create node addresses with default roles from array of names and uuids */
node_address *new_node_address_uuid(u_int n, char const *names[], blob uuid[]) {
  u_int i;

  node_address *na =
      (node_address *)xcom_calloc((size_t)n, sizeof(node_address));
  init_node_address(na, n, names);

  for (i = 0; i < n; i++) {
    na[i].uuid.data.data_len = uuid[i].data.data_len;
    na[i].uuid.data.data_val =
        (char *)calloc(uuid[i].data.data_len, sizeof(char));
    memcpy(na[i].uuid.data.data_val, uuid[i].data.data_val,
           uuid[i].data.data_len);
  }

  return na;
}

void delete_node_address(u_int n, node_address *na) {
  u_int i;
  for (i = 0; i < n; i++) {
    free(na[i].address);
    na[i].address = nullptr;
    free(na[i].uuid.data.data_val);
    na[i].uuid.data.data_val = nullptr;
  }
  free(na);
  na = nullptr;
}

/* Fowler-Noll-Vo type multiplicative hash */
static uint32_t fnv_hash(unsigned char *buf, size_t length, uint32_t sum) {
  size_t i = 0;
  for (i = 0; i < length; i++) {
    sum = sum * (uint32_t)0x01000193 ^ (uint32_t)buf[i];
  }
  return sum;
}

uint32_t chksum_node_list(node_list const *nodes) {
  u_int i;
  uint32_t sum = 0x811c9dc5;
  for (i = 0; i < nodes->node_list_len; i++) {
    sum = fnv_hash((unsigned char *)nodes->node_list_val[i].address,
                   strlen(nodes->node_list_val[i].address), sum);
  }
  return sum;
}
