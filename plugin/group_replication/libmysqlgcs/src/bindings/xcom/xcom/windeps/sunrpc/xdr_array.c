/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

/**
  @file windeps/sunrpc/xdr_array.c
  Generic XDR routines implementation.
  These are the "non-trivial" xdr primitives used to serialize and
  de-serialize arrays.  See xdr.h for more info on the interface to xdr.
*/

#include <libintl.h>
#include <limits.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_IN_LIBIO
#include <wchar.h>
#endif

#define LASTUNSIGNED ((u_int)0 - 1)

/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t xdr_array(xdrs, addrp, sizep, maxsize, elsize, elproc) XDR *xdrs;
caddr_t *addrp;   /* array pointer */
u_int *sizep;     /* number of elements */
u_int maxsize;    /* max numberof elements */
u_int elsize;     /* size in bytes of each element */
xdrproc_t elproc; /* xdr routine to handle each element */
{
  u_int i;
  caddr_t target = *addrp;
  u_int c; /* the actual element count */
  bool_t stat = TRUE;

  /* like strings, arrays are really counted arrays */
  if (!INTUSE(xdr_u_int)(xdrs, sizep)) {
    return FALSE;
  }
  c = *sizep;
  /*
   * XXX: Let the overflow possibly happen with XDR_FREE because mem_free()
   * doesn't actually use its second argument anyway.
   */
  if ((c > maxsize || c > UINT_MAX / elsize) && (xdrs->x_op != XDR_FREE)) {
    return FALSE;
  }

  /*
   * if we are deserializing, we may need to allocate an array.
   * We also save time by checking for a null array if we are freeing.
   */
  if (target == NULL) switch (xdrs->x_op) {
      case XDR_DECODE:
        if (c == 0) return TRUE;
        *addrp = target = calloc(c, elsize);
        if (target == NULL) {
          (void)__fxprintf(NULL, "%s: %s", __func__, _("out of memory\n"));
          return FALSE;
        }
        break;

      case XDR_FREE:
        return TRUE;
      default:
        break;
    }

  /*
   * now we xdr each element of array
   */
  for (i = 0; (i < c) && stat; i++) {
    stat = (*elproc)(xdrs, target, LASTUNSIGNED);
    target += elsize;
  }

  /*
   * the array may need freeing
   */
  if (xdrs->x_op == XDR_FREE) {
    mem_free(*addrp, c * elsize);
    *addrp = NULL;
  }
  return stat;
}
INTDEF(xdr_array)

/*
 * xdr_vector():
 *
 * XDR a fixed length array. Unlike variable-length arrays,
 * the storage of fixed length arrays is static and unfreeable.
 * > basep: base of the array
 * > size: size of the array
 * > elemsize: size of each element
 * > xdr_elem: routine to XDR each element
 */
bool_t xdr_vector(xdrs, basep, nelem, elemsize, xdr_elem) XDR *xdrs;
char *basep;
u_int nelem;
u_int elemsize;
xdrproc_t xdr_elem;
{
  u_int i;
  char *elptr;

  elptr = basep;
  for (i = 0; i < nelem; i++) {
    if (!(*xdr_elem)(xdrs, elptr, LASTUNSIGNED)) {
      return FALSE;
    }
    elptr += elemsize;
  }
  return TRUE;
}
