/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#ifndef NODE_CONNECTION_H
#define NODE_CONNECTION_H

#include <stdlib.h>

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif

#include <openssl/ssl.h>

#endif

#include "xcom/network/include/network_provider.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_proto.h"
#include "xdr_gen/xcom_vp.h"

enum con_state { CON_NULL, CON_FD, CON_PROTO };
typedef enum con_state con_state;

struct connection_descriptor {
  int fd;
#ifndef XCOM_WITHOUT_OPENSSL
  SSL *ssl_fd;
#endif
  con_state connected_;
  unsigned int snd_tag;
  xcom_proto x_proto;
  enum_transport_protocol protocol_stack;
};

typedef struct connection_descriptor connection_descriptor;

#ifndef XCOM_WITHOUT_OPENSSL
static inline connection_descriptor *new_connection(int fd, SSL *ssl_fd) {
  connection_descriptor *c = (connection_descriptor *)xcom_calloc(
      (size_t)1, sizeof(connection_descriptor));
  c->fd = fd;
  c->ssl_fd = ssl_fd;
  c->connected_ = CON_NULL;
  return c;
}
#else
static inline connection_descriptor *new_connection(int fd) {
  connection_descriptor *c = (connection_descriptor *)xcom_calloc(
      (size_t)1, sizeof(connection_descriptor));
  c->fd = fd;
  c->connected_ = CON_NULL;
  return c;
}
#endif
static inline int is_connected(connection_descriptor *con) {
  return con->connected_ >= CON_FD;
}

static inline int proto_done(connection_descriptor *con) {
  return con->connected_ == CON_PROTO;
}

static inline void set_connected(connection_descriptor *con, con_state val) {
  con->connected_ = val;
}

static inline void set_protocol_stack(connection_descriptor *con,
                                      enum_transport_protocol val) {
  con->protocol_stack = val;
}

#endif /* NODE_CONNECTION_H */
