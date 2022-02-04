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

#ifndef XCOM_MEMORY_H
#define XCOM_MEMORY_H

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <stdlib.h>

#define X_FREE(x) \
  {               \
    free(x);      \
    x = 0;        \
  }
#define XCOM_XDR_FREE(proc, ptr)                   \
  {                                                \
    xcom_xdr_free((xdrproc_t)proc, (char *)(ptr)); \
    (ptr) = 0;                                     \
  }

void xcom_xdr_free(xdrproc_t f, char *p);

extern int oom_abort;

static inline void *xcom_malloc(size_t size) {
  void *retval = malloc(size);
  if (retval == nullptr) {
    oom_abort = 1;
  }
  return retval;
}

static inline void *xcom_calloc(size_t nmemb, size_t size) {
  void *retval = calloc(nmemb, size);
  if (retval == nullptr) {
    oom_abort = 1;
  }
  return retval;
}

#endif
