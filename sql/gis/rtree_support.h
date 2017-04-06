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

#ifndef RTREE_H_INCLUDED
#define RTREE_H_INCLUDED

/* In memory representation of a minimum bounding rectangle */
typedef struct rtr_mbr
{
  double xmin;                                 /*!< minimum on x */
  double xmax;                                 /*!< maximum on x */
  double ymin;                                 /*!< minimum on y */
  double ymax;                                 /*!< maximum on y */
} rtr_mbr_t;

/*
  Whether MBR 'a' contains 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' contains 'b', else false.
*/
bool mbr_contain_cmp(rtr_mbr_t* a,rtr_mbr_t* b, std::uint32_t srid);

/*
  Whether MBR 'a' equals to 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' equals 'b', else false.
*/
bool mbr_equal_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid);

/*
  Whether MBR 'a' intersects 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' intersects 'b', else false.
*/
bool mbr_intersect_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid);

/*
  Whether MBR 'a' and 'b' disjoint

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' and 'b' are disjoint, else false.
*/
bool mbr_disjoint_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid);

/*
  Whether MBR 'a' within 'b'

  @param a    The first MBR.
  @param b    The second MBR.
  @param srid SRID value.

  @return true if 'a' is within 'b', else false.
*/
bool mbr_within_cmp(rtr_mbr_t* a, rtr_mbr_t* b, std::uint32_t srid);

/**
  Join 2 MBR's of dimensions n_dim.

  @param          a     The first MBR, where the joined result will be.
  @param          b     The second MBR.
  @param[in, out] n_dim Number of dimensions.
  @param          srid  SRID value.
*/
void mbr_join(double* a, const double* b, int n_dim, std::uint32_t srid);

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
                     std::uint32_t srid);

/**
  Computes the area of MBR of dimension n_dim.

  @param a     MBR.
  @param n_dim Number of dimensions.
  @param srid  SRID value.

  @return calculated MBR area
*/
double compute_area(const double* a, int n_dim, std::uint32_t srid);

/**
  Calculate Minimal Bounding Rectangle (MBR) of the spatial object
  stored in in geometry storage format (WKB+SRID).

  @param         store  Pointer to storage format of WKB+SRID, where point is
                        stored.
  @param         size   Size of WKB.
  @param         n_dims Number of dimensions.
  @param[in,out] mbr    MBR, which must be of length n_dims * 2.

  @return 0 if the geometry is valid, otherwise -1.
*/
int get_mbr_from_store(uchar* store, uint size, uint n_dims, double* mbr);

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
double rtree_area_increase(const uchar* mbr_a, const uchar* mbr_b,
                           int mbr_len, double* ab_area, std::uint32_t srid);

/**
  Calculates overlapping area

  @param mbr_a   First MBR.
  @param mbr_b   Second MBR.
  @param mbr_len MBR length.
  @param srid    SRID value.

  @return overlapping area
*/
double rtree_area_overlapping(const uchar* mbr_a, const uchar* mbr_b,
                              int mbr_len, std::uint32_t srid);

#endif // RTREE_H_INCLUDED
