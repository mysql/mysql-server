/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include <string.h>

#include "xcom/result.h"
#include "xcom/task_debug.h"

static inline void task_dump_err(int err) {
  if (err) {
#ifndef XCOM_WITHOUT_OPENSSL
    if (is_ssl_err(err)) {
      IFDBG(D_BUG, FN; NDBG(from_ssl_err(err), d));
    } else {
#endif
      IFDBG(D_BUG, FN; NDBG(from_errno(err), d); STREXP(strerror(err)));
#ifndef XCOM_WITHOUT_OPENSSL
    }
#endif
  }
}

#endif
