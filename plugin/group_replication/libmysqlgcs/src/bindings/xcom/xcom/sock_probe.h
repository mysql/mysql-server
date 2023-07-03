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

#ifndef SOCK_PROBE_H
#define SOCK_PROBE_H

#include "xcom/xcom_common.h"

#include "xcom/xcom_os_layer.h"

struct sock_probe;
typedef struct sock_probe sock_probe;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

node_no xcom_find_node_index(node_list *nodes);
node_no xcom_mynode_match(char *name, xcom_port port);

typedef int (*port_matcher)(xcom_port if_port);
void set_port_matcher(port_matcher x);
port_matcher get_port_matcher();

#endif
