/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file windeps/sunrpc/xdr_sizeof.c
  General purpose routine to see how much space something will use
  when serialized using XDR.
*/

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/types.h>
#include <stdlib.h>

/* ARGSUSED */
static bool_t
x_putlong (XDR *xdrs, const long *longp)
{
  xdrs->x_handy += BYTES_PER_XDR_UNIT;
  return TRUE;
}

/* ARGSUSED */
static bool_t
x_putbytes (XDR *xdrs, const char *bp, u_int len)
{
  xdrs->x_handy += len;
  return TRUE;
}

static u_int
x_getpostn (const XDR *xdrs)
{
  return xdrs->x_handy;
}

/* ARGSUSED */
static bool_t
x_setpostn (XDR *xdrs, u_int len)
{
  /* This is not allowed */
  return FALSE;
}

static int32_t *
x_inline (XDR *xdrs, u_int len)
{
  if (len == 0)
    return NULL;
  if (xdrs->x_op != XDR_ENCODE)
    return NULL;
  if (len < (u_int) (long int) xdrs->x_base)
    {
      /* x_private was already allocated */
      xdrs->x_handy += len;
      return (int32_t *) xdrs->x_private;
    }
  else
    {
      /* Free the earlier space and allocate new area */
      free (xdrs->x_private);
      if ((xdrs->x_private = (caddr_t) malloc (len)) == NULL)
	{
	  xdrs->x_base = 0;
	  return NULL;
	}
      xdrs->x_base = (void *) (long) len;
      xdrs->x_handy += len;
      return (int32_t *) xdrs->x_private;
    }
}

static int
harmless (void)
{
  /* Always return FALSE/NULL, as the case may be */
  return 0;
}

static void
x_destroy (XDR *xdrs)
{
  xdrs->x_handy = 0;
  xdrs->x_base = 0;
  if (xdrs->x_private)
    {
      free (xdrs->x_private);
      xdrs->x_private = NULL;
    }
  return;
}

static bool_t
x_putint32 (XDR *xdrs, const int32_t *int32p)
{
  xdrs->x_handy += BYTES_PER_XDR_UNIT;
  return TRUE;
}

unsigned long
xdr_sizeof (xdrproc_t func, void *data)
{
  XDR x;
  struct xdr_ops ops;
  bool_t stat;
  /* to stop ANSI-C compiler from complaining */
  typedef bool_t (*dummyfunc1) (XDR *, long *);
  typedef bool_t (*dummyfunc2) (XDR *, caddr_t, u_int);
  typedef bool_t (*dummyfunc3) (XDR *, int32_t *);

  ops.x_putlong = x_putlong;
  ops.x_putbytes = x_putbytes;
  ops.x_inline = x_inline;
  ops.x_getpostn = x_getpostn;
  ops.x_setpostn = x_setpostn;
  ops.x_destroy = x_destroy;
  ops.x_putint32 = x_putint32;

  /* the other harmless ones */
  ops.x_getlong = (dummyfunc1) harmless;
  ops.x_getbytes = (dummyfunc2) harmless;
  ops.x_getint32 = (dummyfunc3) harmless;

  x.x_op = XDR_ENCODE;
  x.x_ops = &ops;
  x.x_handy = 0;
  x.x_private = (caddr_t) NULL;
  x.x_base = (caddr_t) 0;

  stat = func (&x, data);
  free (x.x_private);
  return stat == TRUE ? x.x_handy : 0;
}
