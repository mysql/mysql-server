/*****************************************************************************
Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*****************************************************************************/

/** @file gis0geo.h
 The r-tree define from MyISAM
 *******************************************************/

#ifndef _gis0geo_h
#define _gis0geo_h

#include "page0types.h"
#include "sql/gis/rtree_support.h"

constexpr uint32_t SPLEN = 8;

namespace dd {
class Spatial_reference_system;
}

/* Rtree split node structure. */
struct rtr_split_node_t {
  double square;  /* square of the mbr.*/
  int n_node;     /* which group in.*/
  uchar *key;     /* key. */
  double *coords; /* mbr. */
};

/** Inline function for reserving coords */
inline static double *reserve_coords(double **d_buffer, /*!< in/out: buffer. */
                                     int n_dim)         /*!< in: dimensions. */
{
  double *coords = *d_buffer;
  (*d_buffer) += n_dim * 2;
  return coords;
}

/** Split rtree nodes.
 Return which group the first rec is in.  */
int split_rtree_node(
    rtr_split_node_t *node,                   /*!< in: split nodes.*/
    int n_entries,                            /*!< in: entries number.*/
    int all_size,                             /*!< in: total key's size.*/
    int key_size,                             /*!< in: key's size.*/
    int min_size,                             /*!< in: minimal group size.*/
    int size1,                                /*!< in: size of group.*/
    int size2,                                /*!< in: initial group sizes */
    double **d_buffer,                        /*!< in/out: buffer.*/
    int n_dim,                                /*!< in: dimensions. */
    uchar *first_rec,                         /*!< in: the first rec. */
    const dd::Spatial_reference_system *srs); /*!< in: SRS of R-tree */

/** Compares two keys a and b depending on mode
 mode can contain these flags:
    PAGE_CUR_INTERSECT   a intersects b
    PAGE_CUR_CONTAIN     a contains   b
    PAGE_CUR_DISJOINT    a disjoint   b
    PAGE_CUR_WITHIN      a within     b
    PAGE_CUR_MBR_EQUAL   All coordinates of MBRs are equal
 @param[in]     mode    compare method
 @param[in]     a       first key
 @param[in]     a_len   first key len
 @param[in]     b       second key
 @param[in]     b_len   second_key_len
 @param[in]     srs     Spatial reference system of R-tree
 @retval true if the predicate is true, otherwise false. */
bool rtree_key_cmp(page_cur_mode_t mode, const uchar *a, int a_len,
                   const uchar *b, int b_len,
                   const dd::Spatial_reference_system *srs);
#endif
