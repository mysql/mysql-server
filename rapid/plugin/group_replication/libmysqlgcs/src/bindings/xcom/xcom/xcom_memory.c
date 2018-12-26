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

#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom_memory.h"
#include "xcom_proto_enum.h"

extern xcom_proto const my_xcom_version;

/**
   Recursive free of data structures allocated by XDR.
 */
void
my_xdr_free (xdrproc_t proc, char *objp)
{
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
  (*proc) (&x, objp, 0);
}

void xcom_xdr_free(xdrproc_t f, char *p)
{
  if(p){
    my_xdr_free(f,p);
    X_FREE(p);
  }
}

