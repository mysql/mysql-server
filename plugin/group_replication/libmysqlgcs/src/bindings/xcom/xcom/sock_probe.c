/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32
#include <netdb.h>
#endif
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#ifdef _WIN32
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_win32.c"
#else
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_ix.c"
#endif

/*
  Get host name from host:port string.
  name should be a buffer at least MAXHOSTNAMELEN+1 bytes.
*/
void get_host_name(char *a, char *name) {
  if (!a || !name) return;
  {
    int i = 0;
    while (a[i] != 0 && a[i] != ':' && i <= MAXHOSTNAMELEN) {
      name[i] = a[i];
      i++;
    }
    name[i] = 0;
  }
}

/* compare two sockaddr */
static bool_t sockaddr_default_eq(sockaddr *x, sockaddr *y) {
  return 0 == memcmp(x, y, sizeof(*x));
}

/* return index of this machine in node list, or -1 if no match */

static port_matcher match_port;
void set_port_matcher(port_matcher x) { match_port = x; }

port_matcher get_port_matcher() { return match_port; }

node_no xcom_find_node_index(node_list *nodes) {
  node_no i;
  node_no retval = VOID_NODE_NO;
  char *name = NULL;
  struct addrinfo *addr = 0;
  struct addrinfo *saved_addr = 0;

  sock_probe *s = calloc((size_t)1, sizeof(sock_probe));

  if (init_sock_probe(s) < 0) {
    free(s);
    return retval;
  }

  /*
    On some platform, MAXHOSTNAMELEN is sysconf(_SC_HOST_NAME_MAX).
    so need to check if it is greater than 0.
  */
  if (MAXHOSTNAMELEN <= 0) return retval;

  name = (char *)calloc((size_t)1, (size_t)(MAXHOSTNAMELEN + 1));

  /* For each node in list */
  for (i = 0; i < nodes->node_list_len; i++) {
    /* See if port matches first */
    if (match_port) {
      if (!match_port(xcom_get_port(nodes->node_list_val[i].address))) continue;
    }
    /* Get host name from host:port string */
    get_host_name(nodes->node_list_val[i].address, name);
    /* Get addresses of host */

    checked_getaddrinfo(name, 0, 0, &addr);
    saved_addr = addr;
    MAY_DBG(FN; STREXP(name); PTREXP(addr));
    /* getaddrinfo returns linked list of addrinfo */
    while (addr) {
      int j;
      /* Match sockaddr of host with list of interfaces on this machine. Skip
       * disabled interfaces */
      for (j = 0; j < number_of_interfaces(s); j++) {
        sockaddr tmp = get_sockaddr(s, j);
        if (sockaddr_default_eq(addr->ai_addr, &tmp) && is_if_running(s, j)) {
          retval = i;
          if (saved_addr) freeaddrinfo(saved_addr);
          goto end_loop;
        }
      }
      addr = addr->ai_next;
    }
  }
/* Free resources and return result */
end_loop:
  free(name);
  delete_sock_probe(s);
  return retval;
}

node_no xcom_mynode_match(char *name, xcom_port port) {
  node_no retval = 0;
  struct addrinfo *addr = 0;
  struct addrinfo *saved_addr = 0;

  if (match_port && !match_port(port)) return 0;

  {
    sock_probe *s = calloc((size_t)1, sizeof(sock_probe));
    if (init_sock_probe(s) < 0) {
      free(s);
      return retval;
    }

    checked_getaddrinfo(name, 0, 0, &addr);
    saved_addr = addr;
    MAY_DBG(FN; STREXP(name); PTREXP(addr));
    /* getaddrinfo returns linked list of addrinfo */
    while (addr) {
      int j;
      /* Match sockaddr of host with list of interfaces on this machine. Skip
       * disabled interfaces */
      for (j = 0; j < number_of_interfaces(s); j++) {
        sockaddr tmp = get_sockaddr(s, j);
        if (sockaddr_default_eq(addr->ai_addr, &tmp) && is_if_running(s, j)) {
          retval = 1;
          goto end_loop;
        }
      }
      addr = addr->ai_next;
    }
  /* Free resources and return result */
  end_loop:
    if (saved_addr) freeaddrinfo(saved_addr);
    delete_sock_probe(s);
  }
  return retval;
}
