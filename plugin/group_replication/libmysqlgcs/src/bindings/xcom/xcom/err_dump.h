/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ERR_DUMP_H
#define ERR_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/gcs_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/result.h"

static inline void task_dump_err(int err) {
  if (err) {
#ifdef XCOM_HAVE_OPENSSL
    if (is_ssl_err(err)) {
      MAY_DBG(FN; NDBG(to_ssl_err(err), d));
    } else {
#endif
      MAY_DBG(FN; NDBG(to_errno(err), d); STREXP(strerror(err)));
#ifdef XCOM_HAVE_OPENSSL
    }
#endif
  }
}

#ifdef __cplusplus
}
#endif

#endif
