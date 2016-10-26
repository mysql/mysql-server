/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SOCK_PROBE_H
#define SOCK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xcom_os_layer.h"

struct sock_probe;
typedef struct sock_probe sock_probe;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr_in6 sockaddr_in6;
typedef struct in_addr in_addr;
typedef struct sockaddr sockaddr;

void get_host_name(char *a, char *name);
bool_t sockaddr_default_eq(sockaddr *x, sockaddr *y);
node_no xcom_find_node_index(node_list *nodes);
node_no	xcom_mynode_match(char *name, xcom_port port);

#ifdef __cplusplus
}
#endif

#endif

