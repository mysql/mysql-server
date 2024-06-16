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
  @file windeps/sunrpc/xdr_ref.c
  These are the "non-trivial" xdr primitives used to serialize and
  de-serialize "pointers".  See xdr.h for more info on the interface to xdr.
*/

#include <libintl.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_IN_LIBIO
#include <libio/iolibio.h>
#include <wchar.h>
#endif

#define LASTUNSIGNED ((u_int)0 - 1)

/*
 * XDR an indirect pointer
 * xdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the  necessary storage is allocated.
 * size is the size of the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
bool_t xdr_reference(xdrs, pp, size, proc)
XDR *xdrs;
caddr_t *pp;    /* the pointer to work on */
u_int size;     /* size of the object pointed to */
xdrproc_t proc; /* xdr routine to handle the object */
{
  caddr_t loc = *pp;
  bool_t stat;

  if (loc == NULL) switch (xdrs->x_op) {
      case XDR_FREE:
        return TRUE;

      case XDR_DECODE:
        *pp = loc = (caddr_t)calloc(1, size);
        if (loc == NULL) {
          (void)__fxprintf(NULL, "%s: %s", __func__, _("out of memory\n"));
          return FALSE;
        }
        break;
      default:
        break;
    }

  stat = (*proc)(xdrs, loc, LASTUNSIGNED);

  if (xdrs->x_op == XDR_FREE) {
    mem_free(loc, size);
    *pp = NULL;
  }
  return stat;
}
INTDEF(xdr_reference)

/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialize
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
bool_t xdr_pointer(xdrs, objpp, obj_size, xdr_obj)
XDR *xdrs;
char **objpp;
u_int obj_size;
xdrproc_t xdr_obj;
{
  bool_t more_data;

  more_data = (*objpp != NULL);
  if (!INTUSE(xdr_bool)(xdrs, &more_data)) {
    return FALSE;
  }
  if (!more_data) {
    *objpp = NULL;
    return TRUE;
  }
  return INTUSE(xdr_reference)(xdrs, objpp, obj_size, xdr_obj);
}
