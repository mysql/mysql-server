/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/

#include <cmath>
#include <spatial.h>
#include "my_byteorder.h"
#include "my_inttypes.h"
#include "rtree_support.h"

/* Types of "well-known binary representation" (wkb) format. */
enum wkbType
{
  wkbPoint= 1,
  wkbLineString= 2,
  wkbPolygon= 3,
  wkbMultiPoint= 4,
  wkbMultiLineString= 5,
  wkbMultiPolygon= 6,
  wkbGeometryCollection= 7
};

/* Byte order of "well-known binary representation" (wkb) format. */
enum wkbByteOrder
{
  wkbXDR= 0,                                   /* Big Endian. */
  wkbNDR= 1                                    /* Little Endian. */
};

/*
  Whether MBR 'a' contains 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' contains 'b', else false.
*/
bool mbr_contain_cmp(rtr_mbr_t* a,rtr_mbr_t* b, std::uint32_t srid)
{
  return ((((b)->xmin >= (a)->xmin) && ((b)->xmax <= (a)->xmax)
         && ((b)->ymin >= (a)->ymin) && ((b)->ymax <= (a)->ymax)));
}

/*
  Whether MBR 'a' equals to 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' equals 'b', else false.
*/
bool mbr_equal_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid)
{
  return ((((b)->xmin == (a)->xmin) && ((b)->xmax == (a)->xmax))
         && (((b)->ymin == (a)->ymin) && ((b)->ymax == (a)->ymax)));
}

/*
  Whether MBR 'a' intersects 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' intersects 'b', else false.
*/
bool mbr_intersect_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid)
{
  return ((((b)->xmin <= (a)->xmax) || ((b)->xmax >= (a)->xmin))
         && (((b)->ymin <= (a)->ymax) || ((b)->ymax >= (a)->ymin)));
}

/*
  Whether MBR 'a' and 'b' disjoint

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' and 'b' are disjoint, else false.
*/
bool mbr_disjoint_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid)
{
  return !mbr_intersect_cmp(a, b, srid);
}

/*
  Whether MBR 'a' within 'b'

  @param a     The first MBR.
  @param b     The second MBR.
  @param srid  SRID value.

  @return true if 'a' is within 'b', else false.
*/
bool mbr_within_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid)
{
  return ((((b)->xmin <= (a)->xmin) && ((b)->xmax >= (a)->xmax))
         && (((b)->ymin <= (a)->ymin) && ((b)->ymax >= (a)->ymax)));
}

/* These definitions are for comparing 2 mbrs. */

/**
  Join 2 MBR's of dimensions n_dim.

  @param          a     The first MBR, where the joined result will be.
  @param          b     The second MBR.
  @param[in, out] n_dim Number of dimensions.
  @param          srid  SRID value.
*/
void mbr_join(double* a, const double* b, int n_dim, std::uint32_t srid)
{
  double* end= a + n_dim * 2;

  do
  {
    if (a[0] > b[0])
      a[0]= b[0];

    if (a[1] < b[1])
      a[1]= b[1];

    a+= 2;
    b+= 2;
 } while (a != end);
}

/**
  Computes the area of MBR which is the join of a and b. Both a and b are of
  dimensions n_dim.

  @param a     The first MBR.
  @param b     The second MBR.
  @param n_dim Number of dimensions.
  @param srid  SRID value.

  @return calculated MBR area of a join between 2 MBRs
*/
double mbr_join_area(const double* a, const double* b, int n_dim,
                       std::uint32_t srid)
{
  const double* end= a + n_dim * 2;
  double        area= 1.0;

  do
  {
    area*= std::max(a[1], b[1]) - std::min(a[0], b[0]);

    a+= 2;
    b+= 2;
  } while (a != end);

  /* Check for infinity or NaN, so we don't get NaN in calculations */
  if (std::isinf(area) || std::isnan(area))
    return DBL_MAX;

  return area;
}

/**
  Computes the area of MBR of dimension n_dim.

  @param a     MBR.
  @param n_dim Number of dimensions.
  @param srid  SRID value.

  @return calculated MBR area
*/
double compute_area(const double* a, int n_dim, std::uint32_t srid)
{
  const double* end= a + n_dim * 2;
  double        area= 1.0;

  do
  {
    area*= a[1] - a[0];
    a+= 2;
  } while (a != end);

  return area;
}

/**
  Add one point stored in WKB to a given MBR.

  @param         srid       Spatail reference system ID.
  @param         wkb        Pointer to WKB, where point is stored.
  @param         end        End of WKB.
  @param         n_dims     Number of dimensions.
  @param         byte_order Byte order.
  @param[in,out] mbr        MBR, which must be of length n_dims * 2.

  @return 0 if the point in wkb is valid, otherwise -1.
*/
static int rtree_add_point_to_mbr(std::uint32_t srid, uchar** wkb, uchar* end,
                                  uint n_dims, uchar byte_order, double* mbr)
{
  double  ord;
  double* mbr_end= mbr + n_dims * 2;

  while (mbr < mbr_end)
  {
    if ((*wkb) + sizeof(double) > end)
      return(-1);

    #ifdef WORDS_BIGENDIAN
      float8get(&ord, *wkb);
    #else
      doubleget(&ord, *wkb);
    #endif

    (*wkb)+= sizeof(double);

    if (ord < *mbr)
      *mbr= ord;

    mbr++;

    if (ord > *mbr)
      *mbr= ord;

    mbr++;
  }

  return(0);
}

/**
  Get MBR of point stored in WKB.

  @param         srid       Spatail reference system ID.
  @param         wkb        Pointer to WKB, where point is stored.
  @param         end        End of WKB.
  @param         n_dims     Number of dimensions.
  @param         byte_order Byte order.
  @param[in,out] mbr        MBR, which must be of length n_dims * 2.

  @return 0 if ok, otherwise -1.
*/
static int rtree_get_point_mbr(std::uint32_t srid, uchar** wkb, uchar* end,
                               uint n_dims, uchar byte_order, double* mbr)
{
  return rtree_add_point_to_mbr(srid, wkb, end, n_dims, byte_order, mbr);
}

/**
  Get MBR of linestring stored in WKB.

  @param         srid       Spatail reference system ID.
  @param         wkb        Pointer to WKB, where point is stored.
  @param         end        End of WKB.
  @param         n_dims     Number of dimensions.
  @param         byte_order Byte order.
  @param[in,out] mbr        MBR, which must be of length n_dims * 2.

  @return 0 if the linestring is valid, otherwise -1.
*/
static int rtree_get_linestring_mbr(std::uint32_t srid, uchar** wkb, uchar* end,
                                    uint n_dims, uchar byte_order, double* mbr)
{
  uint n_points;

  n_points= uint4korr(*wkb);
  (*wkb)+= 4;

  for (; n_points > 0; --n_points)
  {
     /* Add next point to mbr */
     if (rtree_add_point_to_mbr(srid, wkb, end, n_dims, byte_order, mbr))
       return(-1);
  }

  return(0);
}

/**
  Get MBR of polygon stored in WKB.

  @param         srid       Spatail reference system ID.
  @param         wkb        Pointer to WKB, where point is stored.
  @param         end        End of WKB.
  @param         n_dims     Number of dimensions.
  @param         byte_order Byte order.
  @param[in,out] mbr        MBR, which must be of length n_dims * 2.

  @return 0 if the polygon is valid, otherwise -1.
*/
static int rtree_get_polygon_mbr(std::uint32_t srid, uchar** wkb, uchar* end,
                                 uint n_dims, uchar byte_order, double* mbr)
{
  uint n_linear_rings;
  uint n_points;

  n_linear_rings= uint4korr((*wkb));
  (*wkb)+= 4;

  for (; n_linear_rings > 0; --n_linear_rings)
  {
    n_points= uint4korr((*wkb));
    (*wkb)+= 4;

    for (; n_points > 0; --n_points)
    {
            /* Add next point to mbr */
      if (rtree_add_point_to_mbr(srid, wkb, end, n_dims, byte_order, mbr))
        return(-1);
    }
  }

  return(0);
}

/**
  Get MBR of geometry stored in WKB (well-known binary representation).

  @param         srid   Spatail reference system ID.
  @param         wkb    Pointer to WKB, where point is stored.
  @param         end    End of WKB.
  @param         n_dims Number of dimensions.
  @param[in,out] mbr    MBR, which must be of length n_dims * 2.
  @param         top    If it's called by itself or not.

  @return 0 if the geometry is valid, otherwise -1.
*/
int rtree_get_geometry_mbr(std::uint32_t srid, uchar** wkb, uchar* end,
                           uint n_dims, double* mbr, int top)
{
  int   res;
  uchar byte_order= 2;
  uint  wkb_type= 0;
  uint  n_items;

  byte_order= *(*wkb);
  ++(*wkb);

  wkb_type= uint4korr((*wkb));
  (*wkb)+= 4;

  for (uint i= 0; i < n_dims; ++i)
  {
    mbr[i * 2]= DBL_MAX;
    mbr[i * 2 + 1]= -DBL_MAX;
  }

  switch ((enum wkbType) wkb_type)
  {
  case wkbPoint:
    res= rtree_get_point_mbr(srid, wkb, end, n_dims, byte_order, mbr);
    break;

  case wkbLineString:
    res= rtree_get_linestring_mbr(srid, wkb, end, n_dims, byte_order, mbr);
    break;

  case wkbPolygon:
    res= rtree_get_polygon_mbr(srid, wkb, end, n_dims, byte_order, mbr);
    break;

  case wkbMultiPoint:
    n_items= uint4korr((*wkb));
    (*wkb)+= 4;

    for (; n_items > 0; --n_items)
    {
      byte_order= *(*wkb);
      ++(*wkb);
      (*wkb)+= 4;
      if (rtree_get_point_mbr(srid, wkb, end, n_dims, byte_order, mbr))
        return(-1);
    }
    res= 0;
    break;

  case wkbMultiLineString:
    n_items= uint4korr((*wkb));
    (*wkb)+= 4;
    for (; n_items > 0; --n_items)
    {
      byte_order= *(*wkb);
      ++(*wkb);
      (*wkb)+= 4;
      if (rtree_get_linestring_mbr(srid, wkb, end, n_dims, byte_order, mbr))
        return(-1);
    }
    res= 0;
    break;

  case wkbMultiPolygon:
    n_items= uint4korr((*wkb));
    (*wkb)+= 4;
    for (; n_items > 0; --n_items)
    {
      byte_order= *(*wkb);
      ++(*wkb);
      (*wkb)+= 4;
      if (rtree_get_polygon_mbr(srid, wkb, end, n_dims, byte_order, mbr))
        return(-1);
    }
    res= 0;
    break;

  case wkbGeometryCollection:
    if (!top)
      return(-1);

    n_items= uint4korr((*wkb));
    (*wkb)+= 4;
    for (; n_items > 0; --n_items)
    {
      if (rtree_get_geometry_mbr(srid, wkb, end, n_dims, mbr, 0))
        return(-1);
    }
    res= 0;
    break;

  default:
    res= -1;
  }

  return(res);
}

/**
  Calculate Minimal Bounding Rectangle (MBR) of the spatial object
  stored in geometry storage format (WKB+SRID).

  @param         store  Pointer to storage format of WKB+SRID, where point
                        is stored.
  @param         size   Size of WKB.
  @param         n_dims Number of dimensions.
  @param[in,out] mbr    MBR, which must be of length n_dims * 2.

  @return 0 if the geometry is valid, otherwise -1.
*/
int get_mbr_from_store(uchar* store, uint size, uint n_dims, double* mbr)
{
  std::uint32_t srid= uint4korr(store);
  store+= SRID_SIZE;

  return rtree_get_geometry_mbr(srid, &store, store + size - SRID_SIZE,
                                n_dims, mbr, 1);
}

/**
  Calculates MBR_AREA(a+b) - MBR_AREA(a)
  Note: when 'a' and 'b' objects are far from each other,
  the area increase can be really big, so this function
  can return 'inf' as a result.

  @param      mbr_a   First MBR.
  @param      mbr_b   Second MBR.
  @param      mbr_len MBR length.
  @param[out] ab_area Total area.
  @param      srid    SRID value.

  @return Increased area
*/
double rtree_area_increase(const uchar* mbr_a, const uchar* mbr_b, int mbr_len,
                           double* ab_area, std::uint32_t srid)
{
  double a_area= 1.0;
  double loc_ab_area= 1.0;
  double amin, amax, bmin, bmax;
  int    key_len;
  int    keyseg_len;
  double data_round= 1.0;
  /*
    Since the mbr could be a point or a linestring, in this case, area of mbr is
    0. So, we define this macro for calculating the area increasing when we need to
    enlarge the mbr.
  */
  double line_mbr_weights= 0.001;

  keyseg_len= 2 * sizeof(double);

  for (key_len= mbr_len; key_len > 0; key_len-= keyseg_len)
  {
    double area;

    #ifdef WORDS_BIGENDIAN
      float8get(&amin, mbr_a);
      float8get(&bmin, mbr_b);
      float8get(&amax, mbr_a + sizeof(double));
      float8get(&bmax, mbr_b + sizeof(double));
    #else
      doubleget(&amin, mbr_a);
      doubleget(&bmin, mbr_b);
      doubleget(&amax, mbr_a + sizeof(double));
      doubleget(&bmax, mbr_b + sizeof(double));
    #endif

    area= amax - amin;
    if (area == 0)
      a_area*= line_mbr_weights;
    else
      a_area*= area;

    area= (double)std::max(amax, bmax) - (double)std::min(amin, bmin);

    if (area == 0)
      loc_ab_area*= line_mbr_weights;
    else
      loc_ab_area*= area;

    /* Value of amax or bmin can be so large that small difference
    are ignored. For example: 3.2884281489988079e+284 - 100 =
    3.2884281489988079e+284. This results some area difference
    are not detected */
    if (loc_ab_area == a_area) {
      if (bmin < amin || bmax > amax)
        data_round*= ((double)std::max(amax, bmax) - amax + (amin -
                      (double)std::min(amin, bmin)));
      else
        data_round*= area;
    }

    mbr_a+= keyseg_len;
    mbr_b+= keyseg_len;
  }

  *ab_area= loc_ab_area;

  if (loc_ab_area == a_area && data_round != 1.0)
    return(data_round);

  return(loc_ab_area - a_area);
}

/**
  Calculates overlapping area

  @param mbr_a   First MBR.
  @param mbr_b   Second MBR.
  @param mbr_len MBR length.
  @param srid    SRID value.

  @return overlapping area
*/
double rtree_area_overlapping(const uchar* mbr_a, const uchar* mbr_b,
                              int mbr_len, std::uint32_t srid)
{
  double area= 1.0;
  double amin;
  double amax;
  double bmin;
  double bmax;
  int    key_len;
  int    keyseg_len;

  keyseg_len= 2 * sizeof(double);

  for (key_len = mbr_len; key_len > 0; key_len -= keyseg_len)
  {
    #ifdef WORDS_BIGENDIAN
      float8get(&amin, mbr_a);
      float8get(&bmin, mbr_b);
      float8get(&amax, mbr_a + sizeof(double));
      float8get(&bmax, mbr_b + sizeof(double));
    #else
      doubleget(&amin, mbr_a);
      doubleget(&bmin, mbr_b);
      doubleget(&amax, mbr_a + sizeof(double));
      doubleget(&bmax, mbr_b + sizeof(double));
    #endif

    amin= std::max(amin, bmin);
    amax= std::min(amax, bmax);

    if (amin > amax)
      return(0);
    else
      area*= (amax - amin);

    mbr_a+= keyseg_len;
    mbr_b+= keyseg_len;
  }

  return(area);
}
