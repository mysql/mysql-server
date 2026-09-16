/* Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

#ifndef SOCK_PROBE_H
#define SOCK_PROBE_H

#include "xcom/xcom_common.h"

#include "xcom/xcom_os_layer.h"

struct sock_probe;
typedef struct sock_probe sock_probe;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

/**
  Find the index of the local node within a node_list.

  This function scans the supplied XCom node list and identifies the first
  entry that matches the local node. It first applies the globally configured
  port matcher. If an explicit XCom identity is configured, matching is based
  on that configured endpoint; otherwise, the function falls back to matching
  the candidate addresses against the local interfaces visible to the current
  process.

  The function evaluates global XCom state, namely the installed port matcher,
  the configured XCom identity, and the configured network namespace manager.
  The caller does not need to switch to the configured network namespace before
  invoking this function; if a namespace is configured, the function enters and
  restores it internally before probing interfaces.

  @param[in] nodes The node list to scan.

  @return The index of the first entry classified as the local node, or
          VOID_NODE_NO if no entry matches, if no port matcher is installed, if
          the socket probe cannot be initialized, or if every candidate is
          rejected by parsing, port mismatch, configured-identity mismatch, or
          local-interface validation.
*/
node_no xcom_find_node_index(node_list *nodes);

/**
  Check whether a candidate endpoint identifies this local node.

  This function first applies the globally configured port matcher to the
  candidate port. If an explicit XCom identity is configured, matching is based
  on that configured endpoint; otherwise, the function falls back to resolving
  the candidate host/IP and comparing it against the local interfaces visible
  to the current process.

  The function evaluates global XCom state, namely the installed port matcher,
  the configured XCom identity, and the configured network namespace manager.
  The caller does not need to switch to the configured network namespace before
  invoking this function; if a namespace is configured, the function enters and
  restores it internally before probing interfaces.

  @param[in] name The candidate host or IP to classify.
  @param[in] port The candidate XCom port to classify.

  @retval 1 The candidate endpoint was classified as this local node.
  @retval 0 The candidate endpoint was not classified as this local node, or
            the local probe state could not be initialized.
*/
node_no xcom_mynode_match(const char *name, xcom_port port);

typedef int (*port_matcher)(xcom_port if_port);
void set_port_matcher(port_matcher x);
port_matcher get_port_matcher();

#endif
