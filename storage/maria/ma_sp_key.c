/* Copyright (C) 2006 MySQL AB & Ramil Kalimullin

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"
#include "ma_blockrec.h"                        /* For ROW_FLAG_TRANSID */
#include "trnman.h"

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


/**
   Create spactial key
*/

MARIA_KEY *_ma_sp_make_key(MARIA_HA *info, MARIA_KEY *ret_key, uint keynr,
                           uchar *key, const uchar *record, my_off_t filepos,
                           ulonglong trid)
{
  HA_KEYSEG *keyseg;
  MARIA_KEYDEF *keyinfo = &info->s->keyinfo[keynr];
  uint len = 0;
  const uchar *pos;
  uint dlen;
  uchar *dptr;
  double mbr[SPDIMS * 2];
  uint i;
  DBUG_ENTER("_ma_sp_make_key");

  keyseg = &keyinfo->seg[-1];
  pos = record + keyseg->start;
  ret_key->data= key;

  dlen = _ma_calc_blob_length(keyseg->bit_start, pos);
  memcpy(&dptr, pos + keyseg->bit_start, sizeof(char*));
  if (!dptr)
  {
    my_errno= HA_ERR_NULL_IN_SPATIAL;
    DBUG_RETURN(0);
  }

  sp_mbr_from_wkb(dptr + 4, dlen - 4, SPDIMS, mbr);	/* SRID */

  for (i = 0, keyseg = keyinfo->seg; keyseg->type; keyseg++, i++)
  {
    uint length = keyseg->length, start= keyseg->start;
    double val;

    DBUG_ASSERT(length == 8);
    DBUG_ASSERT(!(start % 8));
    DBUG_ASSERT(start < sizeof(mbr));
    DBUG_ASSERT(keyseg->type == HA_KEYTYPE_DOUBLE);

    val= mbr[start / sizeof (double)];
#ifdef HAVE_ISNAN
    if (isnan(val))
    {
      bzero(key, length);
      key+= length;
      len+= length;
      continue;
    }
#endif

    if (keyseg->flag & HA_SWAP_KEY)
    {
      mi_float8store(key, val);
    }
    else
    {
      float8store((uchar *)key, val);
    }
    key += length;
    len+= length;
  }
  _ma_dpointer(info->s, key, filepos);
  ret_key->keyinfo= keyinfo;
  ret_key->data_length= len;
  ret_key->ref_length= info->s->rec_reflength;
  ret_key->flag= 0;
  if (_ma_have_versioning(info) && trid)
  {
    ret_key->ref_length+= transid_store_packed(info,
                                               key + ret_key->ref_length,
                                               trid);
  }
  DBUG_EXECUTE("key", _ma_print_key(DBUG_FILE, ret_key););
  DBUG_RETURN(ret_key);
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
    float8get(ord, (const uchar*) *wkb);
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

#endif /*HAVE_SPATIAL*/
