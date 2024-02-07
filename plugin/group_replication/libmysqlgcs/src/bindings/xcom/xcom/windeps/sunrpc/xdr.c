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
  @file windeps/sunrpc/xdr.c
  Generic XDR routines implementation.

  These are the "generic" xdr routines used to serialize and de-serialize
  most common data items.  See xdr.h for more info on the interface to
  xdr.
*/

#include <libintl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#ifdef USE_IN_LIBIO
#include <wchar.h>
#endif

#include "xcom/xcom_vp_platform.h"

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE ((long)0)
#define XDR_TRUE ((long)1)
#define LASTUNSIGNED ((u_int)0 - 1)

/*
 * for unit alignment
 */
static const char xdr_zero[BYTES_PER_XDR_UNIT] = {0, 0, 0, 0};

/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void xdr_free(xdrproc_t proc, char *objp) {
  XDR x;

  x.x_op = XDR_FREE;
  (*proc)(&x, objp);
}

/*
 * XDR nothing
 */
bool_t xdr_void(void) { return TRUE; }
INTDEF(xdr_void)

/*
 * XDR integers
 */
bool_t xdr_int(XDR *xdrs, int *ip) {
#if INT_MAX < LONG_MAX
  long l;

  switch (xdrs->x_op) {
    case XDR_ENCODE:
      l = (long)*ip;
      return XDR_PUTLONG(xdrs, &l);

    case XDR_DECODE:
      if (!XDR_GETLONG(xdrs, &l)) {
        return FALSE;
      }
      *ip = (int)l;
    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
#elif INT_MAX == LONG_MAX
  return INTUSE(xdr_long)(xdrs, (long *)ip);
#elif INT_MAX == SHRT_MAX
  return INTUSE(xdr_short)(xdrs, (short *)ip);
#else
#error unexpected integer sizes in xdr_int()
#endif
}
INTDEF(xdr_int)

bool_t xdr_int32_t(XDR *xdrs, int32_t *ip) {
  switch (xdrs->x_op) {
    case XDR_ENCODE:
      return XDR_PUTINT32(xdrs, ip);
    case XDR_DECODE:
      return XDR_GETINT32(xdrs, ip);
    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_int32_t)

/*
 * XDR unsigned integers
 */
bool_t xdr_u_int(XDR *xdrs, u_int *up) {
#if UINT_MAX < ULONG_MAX
  long l;

  switch (xdrs->x_op) {
    case XDR_ENCODE:
      l = (u_long)*up;
      return XDR_PUTLONG(xdrs, &l);

    case XDR_DECODE:
      if (!XDR_GETLONG(xdrs, &l)) {
        return FALSE;
      }
      *up = (u_int)(u_long)l;
    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
#elif UINT_MAX == ULONG_MAX
  return INTUSE(xdr_u_long)(xdrs, (u_long *)up);
#elif UINT_MAX == USHRT_MAX
  return INTUSE(xdr_short)(xdrs, (short *)up);
#else
#error unexpected integer sizes in xdr_u_int()
#endif
}
INTDEF(xdr_u_int)

bool_t xdr_uint32_t(XDR *xdrs, uint32_t *ip) {
  switch (xdrs->x_op) {
    case XDR_ENCODE:
      return XDR_PUTINT32(xdrs, (int32_t *)ip);
    case XDR_DECODE:
      return XDR_GETINT32(xdrs, (int32_t *)ip);
    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_uint32_t)

/*
 * XDR long integers
 * The definition of xdr_long() is kept for backward
 * compatibility. Instead xdr_int() should be used.
 */
bool_t xdr_long(XDR *xdrs, long *lp) {
  if (xdrs->x_op == XDR_ENCODE &&
      (sizeof(int32_t) == sizeof(long) || (int32_t)*lp == *lp))
    return XDR_PUTLONG(xdrs, lp);

  if (xdrs->x_op == XDR_DECODE) return XDR_GETLONG(xdrs, lp);

  if (xdrs->x_op == XDR_FREE) return TRUE;

  return FALSE;
}
INTDEF(xdr_long)

/*
 * XDR unsigned long integers
 * The definition of xdr_u_long() is kept for backward
 * compatibility. Instead xdr_u_int() should be used.
 */
bool_t xdr_u_long(XDR *xdrs, u_long *ulp) {
  switch (xdrs->x_op) {
    case XDR_DECODE: {
      long int tmp;

      if (XDR_GETLONG(xdrs, &tmp) == FALSE) return FALSE;

      *ulp = (uint32_t)tmp;
      return TRUE;
    }

    case XDR_ENCODE:
      if (sizeof(uint32_t) != sizeof(u_long) && (uint32_t)*ulp != *ulp)
        return FALSE;

      return XDR_PUTLONG(xdrs, (long *)ulp);

    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_u_long)

/*
 * XDR hyper integers
 * same as xdr_u_hyper - open coded to save a proc call!
 */
bool_t xdr_hyper(XDR *xdrs, quad_t *llp) {
  long int t1, t2;

  if (xdrs->x_op == XDR_ENCODE) {
    t1 = (long)((*llp) >> 32);
    t2 = (long)(*llp);
    return (XDR_PUTLONG(xdrs, &t1) && XDR_PUTLONG(xdrs, &t2));
  }

  if (xdrs->x_op == XDR_DECODE) {
    if (!XDR_GETLONG(xdrs, &t1) || !XDR_GETLONG(xdrs, &t2)) return FALSE;
    *llp = ((quad_t)t1) << 32;
    *llp |= (uint32_t)t2;
    return TRUE;
  }

  if (xdrs->x_op == XDR_FREE) return TRUE;

  return FALSE;
}
INTDEF(xdr_hyper)

/*
 * XDR hyper integers
 * same as xdr_hyper - open coded to save a proc call!
 */
bool_t xdr_u_hyper(XDR *xdrs, u_quad_t *ullp) {
  long int t1, t2;

  if (xdrs->x_op == XDR_ENCODE) {
    t1 = (unsigned long)((*ullp) >> 32);
    t2 = (unsigned long)(*ullp);
    return (XDR_PUTLONG(xdrs, &t1) && XDR_PUTLONG(xdrs, &t2));
  }

  if (xdrs->x_op == XDR_DECODE) {
    if (!XDR_GETLONG(xdrs, &t1) || !XDR_GETLONG(xdrs, &t2)) return FALSE;
    *ullp = ((u_quad_t)t1) << 32;
    *ullp |= (uint32_t)t2;
    return TRUE;
  }

  if (xdrs->x_op == XDR_FREE) return TRUE;

  return FALSE;
}
INTDEF(xdr_u_hyper)

bool_t xdr_longlong_t(XDR *xdrs, quad_t *llp) {
  return INTUSE(xdr_hyper)(xdrs, llp);
}

bool_t xdr_u_longlong_t(XDR *xdrs, u_quad_t *ullp) {
  return INTUSE(xdr_u_hyper)(xdrs, ullp);
}

bool_t xdr_int64_t(XDR *xdrs, quad_t *llp) {
  return INTUSE(xdr_hyper)(xdrs, llp);
}

bool_t xdr_uint64_t(XDR *xdrs, u_quad_t *ullp) {
  return INTUSE(xdr_u_hyper)(xdrs, ullp);
}

/*
 * XDR short integers
 */
bool_t xdr_short(XDR *xdrs, short *sp) {
  long l;

  switch (xdrs->x_op) {
    case XDR_ENCODE:
      l = (long)*sp;
      return XDR_PUTLONG(xdrs, &l);

    case XDR_DECODE:
      if (!XDR_GETLONG(xdrs, &l)) {
        return FALSE;
      }
      *sp = (short)l;
      return TRUE;

    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_short)

/*
 * XDR unsigned short integers
 */
bool_t xdr_u_short(XDR *xdrs, u_short *usp) {
  long l;

  switch (xdrs->x_op) {
    case XDR_ENCODE:
      l = (u_long)*usp;
      return XDR_PUTLONG(xdrs, &l);

    case XDR_DECODE:
      if (!XDR_GETLONG(xdrs, &l)) {
        return FALSE;
      }
      *usp = (u_short)(u_long)l;
      return TRUE;

    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_u_short)

/*
 * XDR a char
 */
bool_t xdr_char(XDR *xdrs, char *cp) {
  int i;

  i = (*cp);
  if (!INTUSE(xdr_int)(xdrs, &i)) {
    return FALSE;
  }
  *cp = i;
  return TRUE;
}

/*
 * XDR an unsigned char
 */
bool_t xdr_u_char(XDR *xdrs, u_char *cp) {
  u_int u;

  u = (*cp);
  if (!INTUSE(xdr_u_int)(xdrs, &u)) {
    return FALSE;
  }
  *cp = u;
  return TRUE;
}

/*
 * XDR booleans
 */
bool_t xdr_bool(XDR *xdrs, bool_t *bp) {
  long lb;

  switch (xdrs->x_op) {
    case XDR_ENCODE:
      lb = *bp ? XDR_TRUE : XDR_FALSE;
      return XDR_PUTLONG(xdrs, &lb);

    case XDR_DECODE:
      if (!XDR_GETLONG(xdrs, &lb)) {
        return FALSE;
      }
      *bp = (lb == XDR_FALSE) ? FALSE : TRUE;
      return TRUE;

    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_bool)

/*
 * XDR enumerations
 */
bool_t xdr_enum(XDR *xdrs, enum_t *ep) {
  enum sizecheck { SIZEVAL }; /* used to find the size of an enum */

  /*
   * enums are treated as ints
   */
  if (sizeof(enum sizecheck) == 4) {
#if INT_MAX < LONG_MAX
    long l;

    switch (xdrs->x_op) {
      case XDR_ENCODE:
        l = *ep;
        return XDR_PUTLONG(xdrs, &l);

      case XDR_DECODE:
        if (!XDR_GETLONG(xdrs, &l)) {
          return FALSE;
        }
        *ep = l;
      case XDR_FREE:
        return TRUE;
    }
    return FALSE;
#else
    return INTUSE(xdr_long)(xdrs, (long *)ep);
#endif
  } else if (sizeof(enum sizecheck) == sizeof(short)) {
    return INTUSE(xdr_short)(xdrs, (short *)ep);
  } else {
    return FALSE;
  }
}
INTDEF(xdr_enum)

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t xdr_opaque(XDR *xdrs, caddr_t cp, u_int cnt) {
  u_int rndup;
  static char crud[BYTES_PER_XDR_UNIT];

  /*
   * if no data we are done
   */
  if (cnt == 0) return TRUE;

  /*
   * round byte count to full xdr units
   */
  rndup = cnt % BYTES_PER_XDR_UNIT;
  if (rndup > 0) rndup = BYTES_PER_XDR_UNIT - rndup;

  switch (xdrs->x_op) {
    case XDR_DECODE:
      if (!XDR_GETBYTES(xdrs, cp, cnt)) {
        return FALSE;
      }
      if (rndup == 0) return TRUE;
      return XDR_GETBYTES(xdrs, (caddr_t)crud, rndup);

    case XDR_ENCODE:
      if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
        return FALSE;
      }
      if (rndup == 0) return TRUE;
      return XDR_PUTBYTES(xdrs, xdr_zero, rndup);

    case XDR_FREE:
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_opaque)

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t xdr_bytes(xdrs, cpp, sizep, maxsize) XDR *xdrs;
char **cpp;
u_int *sizep;
u_int maxsize;
{
  char *sp = *cpp; /* sp is the actual string pointer */
  u_int nodesize;

  /*
   * first deal with the length since xdr bytes are counted
   */
  if (!INTUSE(xdr_u_int)(xdrs, sizep)) {
    return FALSE;
  }
  nodesize = *sizep;
  if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
    return FALSE;
  }

  /*
   * now deal with the actual bytes
   */
  switch (xdrs->x_op) {
    case XDR_DECODE:
      if (nodesize == 0) {
        return TRUE;
      }
      if (sp == NULL) {
        *cpp = sp = (char *)mem_alloc(nodesize);
      }
      if (sp == NULL) {
        (void)__fxprintf(NULL, "%s: %s", __func__, _("out of memory\n"));
        return FALSE;
      }
      /* fall into ... */

    case XDR_ENCODE:
      return INTUSE(xdr_opaque)(xdrs, sp, nodesize);

    case XDR_FREE:
      if (sp != NULL) {
        mem_free(sp, nodesize);
        *cpp = NULL;
      }
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_bytes)

/*
 * Implemented here due to commonality of the object.
 */
bool_t xdr_netobj(xdrs, np) XDR *xdrs;
struct netobj *np;
{
  return INTUSE(xdr_bytes)(xdrs, &np->n_bytes, &np->n_len, MAX_NETOBJ_SZ);
}
INTDEF(xdr_netobj)

/*
 * XDR a discriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t xdr_union(xdrs, dscmp, unp, choices, dfault) XDR *xdrs;
enum_t *dscmp;                     /* enum to decide which arm to work on */
char *unp;                         /* the union itself */
const struct xdr_discrim *choices; /* [value, xdr proc] for each arm */
xdrproc_t dfault;                  /* default xdr routine */
{
  enum_t dscm;

  /*
   * we deal with the discriminator;  it's an enum
   */
  if (!INTUSE(xdr_enum)(xdrs, dscmp)) {
    return FALSE;
  }
  dscm = *dscmp;

  /*
   * search choices for a value that matches the discriminator.
   * if we find one, execute the xdr routine for that value.
   */
  for (; choices->proc != NULL_xdrproc_t; choices++) {
    if (choices->value == dscm)
      return (*(choices->proc))(xdrs, unp, LASTUNSIGNED);
  }

  /*
   * no match - execute the default xdr routine if there is one
   */
  return ((dfault == NULL_xdrproc_t) ? FALSE
                                     : (*dfault)(xdrs, unp, LASTUNSIGNED));
}
INTDEF(xdr_union)

/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */

/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t xdr_string(xdrs, cpp, maxsize) XDR *xdrs;
char **cpp;
u_int maxsize;
{
  char *sp = *cpp; /* sp is the actual string pointer */
  u_int size;
  u_int nodesize;

  /*
   * first deal with the length since xdr strings are counted-strings
   */
  switch (xdrs->x_op) {
    case XDR_FREE:
      if (sp == NULL) {
        return TRUE; /* already free */
      }
      /* fall through... */
    case XDR_ENCODE:
      if (sp == NULL) return FALSE;
      size = (u_int)strlen(sp);
      break;
    case XDR_DECODE:
      break;
  }
  if (!INTUSE(xdr_u_int)(xdrs, &size)) {
    return FALSE;
  }
  if (size > maxsize) {
    return FALSE;
  }
  nodesize = size + 1;
  if (nodesize == 0) {
    /* This means an overflow.  It a bug in the caller which
       provided a too large maxsize but nevertheless catch it
       here.  */
    return FALSE;
  }

  /*
   * now deal with the actual bytes
   */
  switch (xdrs->x_op) {
    case XDR_DECODE:
      if (sp == NULL) *cpp = sp = (char *)mem_alloc(nodesize);
      if (sp == NULL) {
        (void)__fxprintf(NULL, "%s: %s", __func__, _("out of memory\n"));
        return FALSE;
      }
      sp[size] = 0;
      /* fall into ... */

    case XDR_ENCODE:
      return INTUSE(xdr_opaque)(xdrs, sp, size);

    case XDR_FREE:
      mem_free(sp, nodesize);
      *cpp = NULL;
      return TRUE;
  }
  return FALSE;
}
INTDEF(xdr_string)

/*
 * Wrapper for xdr_string that can be called directly from
 * routines like clnt_call
 */
bool_t xdr_wrapstring(xdrs, cpp) XDR *xdrs;
char **cpp;
{
  if (INTUSE(xdr_string)(xdrs, cpp, LASTUNSIGNED)) {
    return TRUE;
  }
  return FALSE;
}
