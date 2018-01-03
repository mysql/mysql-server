/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
   
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

#include <math.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/sp_defs.h"

static int sp_add_point_to_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                             uchar byte_order, double *mbr);
static int sp_get_point_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                           uchar byte_order, double *mbr);
static int sp_get_linestring_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                                uchar byte_order, double *mbr);
static int sp_get_polygon_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                             uchar byte_order, double *mbr);
static int sp_get_geometry_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                              double *mbr, int top);
static int sp_mbr_from_wkb(uchar (*wkb), uint size, uint n_dims, double *mbr);

uint sp_make_key(MI_INFO *info, uint keynr, uchar *key,
		 const uchar *record, my_off_t filepos)
{
  HA_KEYSEG *keyseg;
  MI_KEYDEF *keyinfo = &info->s->keyinfo[keynr];
  uint len = 0;
  uchar *pos;
  uint dlen;
  uchar *dptr;
  double mbr[SPDIMS * 2];
  uint i;
  
  keyseg = &keyinfo->seg[-1];
  pos = (uchar*)record + keyseg->start;
  
  dlen = _mi_calc_blob_length(keyseg->bit_start, pos);
  memcpy(&dptr, pos + keyseg->bit_start, sizeof(char*));
  if (!dptr)
  {
    set_my_errno(HA_ERR_NULL_IN_SPATIAL);
    return 0;
  }
  sp_mbr_from_wkb(dptr + 4, dlen - 4, SPDIMS, mbr);	/* SRID */
  
  for (i = 0, keyseg = keyinfo->seg; keyseg->type; keyseg++, i++)
  {
    uint length = keyseg->length, start= keyseg->start;
    double val;

    DBUG_ASSERT(length == sizeof(double));
    DBUG_ASSERT(!(start % sizeof(double)));
    DBUG_ASSERT(start < sizeof(mbr));
    DBUG_ASSERT(keyseg->type == HA_KEYTYPE_DOUBLE);
    
    val= mbr[start / sizeof (double)];
    if (isnan(val))
    {
      memset(key, 0, length);
      key+= length;
      len+= length;
      continue;
    }

    if (keyseg->flag & HA_SWAP_KEY)
    {
      uchar buf[sizeof(double)];

      float8store(buf, val);
      pos= &buf[length];
      while (pos > buf)
        *key++ = *--pos;
    }
    else
    {
      float8store((uchar *)key, val);
      key += length;
    }
    len+= length;
  }
  _mi_dpointer(info, key, filepos);
  return len;
}

/*
Calculate minimal bounding rectangle (mbr) of the spatial object
stored in "well-known binary representation" (wkb) format.
*/
static int sp_mbr_from_wkb(uchar *wkb, uint size, uint n_dims, double *mbr)
{
  uint i;

  for (i=0; i < n_dims; ++i)
  {
    mbr[i * 2] = DBL_MAX;
    mbr[i * 2 + 1] = -DBL_MAX;
  }

  return sp_get_geometry_mbr(&wkb, wkb + size, n_dims, mbr, 1);
}

/*
  Add one point stored in wkb to mbr
*/

static int sp_add_point_to_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
			       uchar byte_order MY_ATTRIBUTE((unused)),
			       double *mbr)
{
  double ord;
  double *mbr_end= mbr + n_dims * 2;

  while (mbr < mbr_end)
  {
    if ((*wkb) > end - 8)
      return -1;
    float8get(&ord, (const uchar*) *wkb);
    (*wkb)+= 8;
    if (ord < *mbr)
      *mbr= ord;
    mbr++;
    if (ord > *mbr)
      *mbr= ord;
    mbr++;
  }
  return 0;
}


static int sp_get_point_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                           uchar byte_order, double *mbr)
{
  return sp_add_point_to_mbr(wkb, end, n_dims, byte_order, mbr);
}


static int sp_get_linestring_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                                  uchar byte_order, double *mbr)
{
  uint n_points;

  n_points = uint4korr(*wkb);
  (*wkb) += 4;
  for (; n_points > 0; --n_points)
  {
    /* Add next point to mbr */
    if (sp_add_point_to_mbr(wkb, end, n_dims, byte_order, mbr))
      return -1;
  }
  return 0;
}


static int sp_get_polygon_mbr(uchar *(*wkb), uchar *end, uint n_dims, 
                               uchar byte_order, double *mbr)
{
  uint n_linear_rings;
  uint n_points;

  n_linear_rings = uint4korr((*wkb));
  (*wkb) += 4;

  for (; n_linear_rings > 0; --n_linear_rings)
  {
    n_points = uint4korr((*wkb));
    (*wkb) += 4;
    for (; n_points > 0; --n_points)
    {
      /* Add next point to mbr */
      if (sp_add_point_to_mbr(wkb, end, n_dims, byte_order, mbr))
        return -1;
    }
  }
  return 0;
}

static int sp_get_geometry_mbr(uchar *(*wkb), uchar *end, uint n_dims,
                              double *mbr, int top)
{
  int res;
  uchar byte_order;
  uint wkb_type;

  byte_order = *(*wkb);
  ++(*wkb);

  wkb_type = uint4korr((*wkb));
  (*wkb) += 4;

  switch ((enum wkbType) wkb_type)
  {
    case wkbPoint:
      res = sp_get_point_mbr(wkb, end, n_dims, byte_order, mbr);
      break;
    case wkbLineString:
      res = sp_get_linestring_mbr(wkb, end, n_dims, byte_order, mbr);
      break;
    case wkbPolygon:
      res = sp_get_polygon_mbr(wkb, end, n_dims, byte_order, mbr);
      break;
    case wkbMultiPoint:
    {
      uint n_items;
      n_items = uint4korr((*wkb));
      (*wkb) += 4;
      for (; n_items > 0; --n_items)
      {
        byte_order = *(*wkb);
        ++(*wkb);
        (*wkb) += 4;
        if (sp_get_point_mbr(wkb, end, n_dims, byte_order, mbr))
          return -1;
      }
      res = 0;
      break;
    }
    case wkbMultiLineString:
    {
      uint n_items;
      n_items = uint4korr((*wkb));
      (*wkb) += 4;
      for (; n_items > 0; --n_items)
      {
        byte_order = *(*wkb);
        ++(*wkb);
        (*wkb) += 4;
        if (sp_get_linestring_mbr(wkb, end, n_dims, byte_order, mbr))
          return -1;
      }
      res = 0;
      break;
    }
    case wkbMultiPolygon:
    {
      uint n_items;
      n_items = uint4korr((*wkb));
      (*wkb) += 4;
      for (; n_items > 0; --n_items)
      {
        byte_order = *(*wkb);
        ++(*wkb);
        (*wkb) += 4;
        if (sp_get_polygon_mbr(wkb, end, n_dims, byte_order, mbr))
          return -1;
      }
      res = 0;
      break;
    }
    case wkbGeometryCollection:
    {
      uint n_items;

      if (!top)
        return -1;

      n_items = uint4korr((*wkb));
      (*wkb) += 4;
      for (; n_items > 0; --n_items)
      {
        if (sp_get_geometry_mbr(wkb, end, n_dims, mbr, 0))
          return -1;
      }
      res = 0;
      break;
    }
    default:
      res = -1;
  }
  return res;
}
