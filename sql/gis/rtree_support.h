#ifndef GIS__RTREE_SUPPORT_H_INCLUDED
#define GIS__RTREE_SUPPORT_H_INCLUDED

// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, 51 Franklin
// Street, Suite 500, Boston, MA 02110-1335 USA

/// @file
///
/// This file declares a set of functions that storage engines can call to do
/// geometrical operations.

#include "my_inttypes.h"  // uchar, uint
#include "sql/gis/srid.h"

namespace dd {
class Spatial_reference_system;
}

/// In memory representation of a minimum bounding rectangle
typedef struct rtr_mbr {
  /// minimum on x
  double xmin;
  /// maximum on x
  double xmax;
  /// minimum on y
  double ymin;
  /// maximum on y
  double ymax;
} rtr_mbr_t;

/// Fetches a copy of the dictionary entry for a spatial reference system.
///
/// Spatial reference dictionary cache objects have a limited lifetime,
/// typically until the end of a transaction. This function returns a clone of
/// the dictionary object so that it is valid also after the transaction has
/// ended. This is necessary since InnoDB may do index operations after the
/// transaction has ended.
///
/// @param[in] srid The spatial reference system ID to look up.
///
/// @return The spatial reference system dictionary entry, or nullptr.
dd::Spatial_reference_system* fetch_srs(gis::srid_t srid);

/// Checks if one MBR covers another MBR.
///
/// @warning Despite the name, this function computes the covers relation, not
/// contains.
///
/// For both MBRs, the coordinates of the MBR's minimum corners must be smaller
/// than or equal to the corresponding coordinates of the maximum corner.
///
/// @param[in] srs Spatial reference system.
/// @param[in] a The first MBR.
/// @param[in] b The second MBR.
///
/// @retval true MBR a contains MBR b.
/// @retval false MBR a doesn't contain MBR b.
bool mbr_contain_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                     rtr_mbr_t* b);

/// Checks if two MBRs are equal
///
/// For both MBRs, the coordinates of the MBR's minimum corners must be smaller
/// than or equal to the corresponding coordinates of the maximum corner.
///
/// @param[in] srs Spatial reference system.
/// @param[in] a The first MBR.
/// @param[in] b The second MBR.
///
/// @retval true The two MBRs are equal.
/// @retval false The two MBRs aren't equal.
bool mbr_equal_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                   rtr_mbr_t* b);

/// Whether MBR 'a' intersects 'b'
///
/// @param[in] srs Spatial reference system.
/// @param a    The first MBR.
/// @param b    The second MBR.
///
/// @return true if 'a' intersects 'b', else false.
bool mbr_intersect_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                       rtr_mbr_t* b);

/// Whether MBR 'a' and 'b' disjoint
///
/// @param[in] srs Spatial reference system.
/// @param a    The first MBR.
/// @param b    The second MBR.
///
/// @return true if 'a' and 'b' are disjoint, else false.
bool mbr_disjoint_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                      rtr_mbr_t* b);

/// Whether MBR 'a' within 'b'
///
/// @param[in] srs Spatial reference system.
/// @param a    The first MBR.
/// @param b    The second MBR.
///
/// @return true if 'a' is within 'b', else false.
bool mbr_within_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                    rtr_mbr_t* b);

/// Join 2 MBR's of dimensions n_dim.
///
/// @param[in] srs Spatial reference system.
/// @param          a     The first MBR, where the joined result will be.
/// @param          b     The second MBR.
/// @param[in, out] n_dim Number of dimensions.
void mbr_join(const dd::Spatial_reference_system* srs, double* a,
              const double* b, int n_dim);

/// Computes the area of MBR which is the join of a and b. Both a and b are of
/// dimensions n_dim.
///
/// @param[in] srs Spatial reference system.
/// @param a     The first MBR.
/// @param b     The second MBR.
/// @param n_dim Number of dimensions.
///
/// @return calculated MBR area of a join between 2 MBRs
double mbr_join_area(const dd::Spatial_reference_system* srs, const double* a,
                     const double* b, int n_dim);

/// Computes the area of MBR of dimension n_dim.
///
/// @param[in] srs Spatial reference system.
/// @param a     MBR.
/// @param n_dim Number of dimensions.
/// @param srid  SRID value.
///
/// @return calculated MBR area
double compute_area(const dd::Spatial_reference_system* srs, const double* a,
                    int n_dim);

/// Calculate Minimal Bounding Rectangle (MBR) of the spatial object
/// stored in in geometry storage format (WKB+SRID).
///
/// @param[in] srs Spatial reference system.
/// @param         store  Pointer to storage format of WKB+SRID, where point is
///                       stored.
/// @param         size   Size of WKB.
/// @param         n_dims Number of dimensions.
/// @param[in,out] mbr    MBR, which must be of length n_dims * 2.
/// @param[in,out] srid_ptr Pointer to spatial reference id to be retrieved
///
/// @return 0 if the geometry is valid, otherwise -1.
int get_mbr_from_store(const dd::Spatial_reference_system* srs, uchar* store,
                       uint size, uint n_dims, double* mbr,
                       gis::srid_t* srid_ptr);

/// Calculates MBR_AREA(a+b) - MBR_AREA(a)
/// Note: when 'a' and 'b' objects are far from each other,
/// the area increase can be really big, so this function
/// can return 'inf' as a result.
///
/// @param[in] srs Spatial reference system.
/// @param      mbr_a   First MBR.
/// @param      mbr_b   Second MBR.
/// @param      mbr_len MBR length.
/// @param[out] ab_area Total area.
///
/// @return Increased area
double rtree_area_increase(const dd::Spatial_reference_system* srs,
                           const uchar* mbr_a, const uchar* mbr_b, int mbr_len,
                           double* ab_area);

/// Calculates overlapping area
///
/// @param[in] srs Spatial reference system.
/// @param mbr_a   First MBR.
/// @param mbr_b   Second MBR.
/// @param mbr_len MBR length.
///
/// @return overlapping area
double rtree_area_overlapping(const dd::Spatial_reference_system* srs,
                              const uchar* mbr_a, const uchar* mbr_b,
                              int mbr_len);

#endif  // GIS__RTREE_SUPPORT_H_INCLUDED
