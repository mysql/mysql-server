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

/// Returns true.
///
/// @warning Despite the name, this function does not compute the intersection
/// relationship. It always returns true.
///
/// For both MBRs, the coordinates of the MBR's minimum corners must be smaller
/// than or equal to the corresponding coordinates of the maximum corner.
///
/// @param[in] srs Ignored.
/// @param[in] a Ignored.
/// @param[in] b Ignored.
///
/// @return Always returns true.
bool mbr_intersect_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                       rtr_mbr_t* b);

/// Returns false.
///
/// @warning Despite the name, this function does not compute the disjoint
/// relationship. It always returns false.
///
/// For both MBRs, the coordinates of the MBR's minimum corners must be smaller
/// than or equal to the corresponding coordinates of the maximum corner.
///
/// @param[in] srs Ignored.
/// @param[in] a Ignored.
/// @param[in] b Ignored.
///
/// @return Always returns false.
bool mbr_disjoint_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                      rtr_mbr_t* b);

/// Checks if one MBR is covered by another MBR.
///
/// @warning Despite the name, this function computes the covered_by relation,
/// not within.
///
/// @note If the minimum corner coordinates are larger than the corresponding
/// coordinates of the maximum corner, and if not all a and b coordinates are
/// the same, the function returns the inverse result, i.e., return true if a is
/// not covered by b.
///
/// @param[in] srs Spatial reference system.
/// @param[in] a The first MBR.
/// @param[in] b The second MBR.
///
/// @retval true MBR a is within MBR b.
/// @retval false MBR a isn't within MBR b.
bool mbr_within_cmp(const dd::Spatial_reference_system* srs, rtr_mbr_t* a,
                    rtr_mbr_t* b);

/// Expands an MBR to also cover another MBR.
///
/// @note The function takes a dimension parameter, but currently only supports
/// 2d MBRs.
///
/// MBR format: a[0] = xmin, a[1] = xmax, a[2] = ymin, a[3] = ymax. Same for b.
///
/// @param[in] srs Spatial reference system.
/// @param[in,out] a The first MBR, where the joined result will be.
/// @param[in] b The second MBR.
/// @param[in] n_dim Number of dimensions. Must be 2.
void mbr_join(const dd::Spatial_reference_system* srs, double* a,
              const double* b, int n_dim);

/// Computes the combined area of two MBRs.
///
/// The MBRs may overlap.
///
/// @note The function takes a dimension parameter, but currently only supports
/// 2d MBRs.
///
/// @param[in] srs Spatial reference system.
/// @param[in] a The first MBR.
/// @param[in] b The second MBR.
/// @param[in] n_dim Number of dimensions. Must be 2.
///
/// @return The area of MBR a expanded by MBR b.
double mbr_join_area(const dd::Spatial_reference_system* srs, const double* a,
                     const double* b, int n_dim);

/// Computes the area of an MBR.
///
/// @note The function takes a dimension parameter, but currently only supports
/// 2d MBRs.
///
/// @param[in] srs Spatial reference system.
/// @param[in] a The MBR.
/// @param[in] n_dim Number of dimensions. Must be 2.
///
/// @return Are of the MBR.
double compute_area(const dd::Spatial_reference_system* srs, const double* a,
                    int n_dim);

/// Computes the MBR of a geometry.
///
/// If the geometry is empty, a box that covers the entire domain is returned.
///
/// The geometry is expected to be on the storage format (SRID + WKB). The
/// caller is expected to provide an output buffer that is large enough.
///
/// @note The function takes a dimension parameter, but currently only supports
/// 2d MBRs.
///
/// The SRID of the SRS parameter must match the SRID stored in the first four
/// bytes of the geometry string.
///
/// @param[in] srs Spatial reference system.
/// @param[in] store The geometry.
/// @param[in] size Number of bytes in the geometry string.
/// @param[in] n_dims Number of dimensions. Must be 2.
/// @param[out] mbr The computed MBR.
/// @param[out] srid SRID of the geometry
///
/// @retval 0 The geometry is valid.
/// @retval -1 The geometry is invalid.
int get_mbr_from_store(const dd::Spatial_reference_system* srs, uchar* store,
                       uint size, uint n_dims, double* mbr, gis::srid_t* srid);

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
