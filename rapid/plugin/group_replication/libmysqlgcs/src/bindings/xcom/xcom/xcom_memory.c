/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"

#include <rpc/rpc.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_proto_enum.h"

extern xcom_proto const my_xcom_version;

/**
   Recursive free of data structures allocated by XDR.
 */
void my_xdr_free(xdrproc_t proc, char *objp) {
  XDR x;
  x.x_public = (caddr_t)&my_xcom_version;
  x.x_op = XDR_FREE;

  /*
    Mac OSX changed the xdrproc_t prototype to take
    three parameters instead of two.

    The argument is that it has the potential to break
    the ABI due to compiler optimizations.

    The recommended value for the third parameter is
    0 for those that are not making use of it (which
    is the case). This will keep this code cross-platform
    and cross-version compatible.
  */
  (*proc)(&x, objp, 0);
}

void xcom_xdr_free(xdrproc_t f, char *p) {
  if (p) {
    my_xdr_free(f, p);
    X_FREE(p);
  }
}
