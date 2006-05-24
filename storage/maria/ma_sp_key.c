/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"

#ifdef HAVE_SPATIAL

#include "ma_sp_defs.h"

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

static void get_double(double *d, const byte *pos)
{
  float8get(*d, pos);
}

uint sp_make_key(register MARIA_HA *info, uint keynr, uchar *key,
		 const byte *record, my_off_t filepos)
{
  HA_KEYSEG *keyseg;
  MARIA_KEYDEF *keyinfo = &info->s->keyinfo[keynr];
  uint len = 0;
  byte *pos;
  uint dlen;
  uchar *dptr;
  double mbr[SPDIMS * 2];
  uint i;

  keyseg = &keyinfo->seg[-1];
  pos = (byte*)record + keyseg->start;

  dlen = _ma_calc_blob_length(keyseg->bit_start, pos);
  memcpy_fixed(&dptr, pos + keyseg->bit_start, sizeof(char*));
  if (!dptr)
  {
    my_errno= HA_ERR_NULL_IN_SPATIAL;
    return 0;
  }
  sp_mbr_from_wkb(dptr + 4, dlen - 4, SPDIMS, mbr);	/* SRID */

  for (i = 0, keyseg = keyinfo->seg; keyseg->type; keyseg++, i++)
  {
    uint length = keyseg->length;

    pos = ((byte*)mbr) + keyseg->start;
    if (keyseg->flag & HA_SWAP_KEY)
    {
#ifdef HAVE_ISNAN
      if (keyseg->type == HA_KEYTYPE_FLOAT)
      {
	float nr;
	float4get(nr, pos);
	if (isnan(nr))
	{
	  /* Replace NAN with zero */
 	  bzero(key, length);
	  key+= length;
	  continue;
	}
      }
      else if (keyseg->type == HA_KEYTYPE_DOUBLE)
      {
	double nr;
	get_double(&nr, pos);
	if (isnan(nr))
	{
 	  bzero(key, length);
	  key+= length;
	  continue;
	}
      }
#endif
      pos += length;
      while (length--)
      {
        *key++ = *--pos;
      }
    }
    else
    {
      memcpy((byte*)key, pos, length);
      key += keyseg->length;
    }
    len += keyseg->length;
  }
  _ma_dpointer(info, key, filepos);
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
			       uchar byte_order __attribute__((unused)),
			       double *mbr)
{
  double ord;
  double *mbr_end= mbr + n_dims * 2;

  while (mbr < mbr_end)
  {
    if ((*wkb) > end - 8)
      return -1;
    get_double(&ord, (const byte*) *wkb);
    (*wkb)+= 8;
    if (ord < *mbr)
      float8store((char*) mbr, ord);
    mbr++;
    if (ord > *mbr)
      float8store((char*) mbr, ord);
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

#endif /*HAVE_SPATIAL*/
