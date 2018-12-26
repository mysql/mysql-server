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

#ifndef RETRY_H
#define RETRY_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/result.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_os.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XCOM_HAVE_OPENSSL

static inline int can_retry(int err) {
  if (is_ssl_err(err))
    return from_ssl_err(err) == SSL_ERROR_WANT_WRITE ||
           from_ssl_err(err) == SSL_ERROR_WANT_READ;
  else
    return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
           from_errno(err) == SOCK_EWOULDBLOCK;
}

static inline int can_retry_read(int err) {
  if (is_ssl_err(err))
    return from_ssl_err(err) == SSL_ERROR_WANT_READ;
  else
    return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
           from_errno(err) == SOCK_EWOULDBLOCK;
}

static inline int can_retry_write(int err) {
  if (is_ssl_err(err))
    return from_ssl_err(err) == SSL_ERROR_WANT_WRITE;
  else
    return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
           from_errno(err) == SOCK_EWOULDBLOCK;
}
#else
static inline int can_retry(int err) {
  return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
         from_errno(err) == SOCK_EWOULDBLOCK;
}

static inline int can_retry_read(int err) {
  return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
         from_errno(err) == SOCK_EWOULDBLOCK;
}

static inline int can_retry_write(int err) {
  return from_errno(err) == SOCK_EAGAIN || from_errno(err) == SOCK_EINTR ||
         from_errno(err) == SOCK_EWOULDBLOCK;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
