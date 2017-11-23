#ifndef SQL_GIS_MBR_UTILS_H_INCLUDED
#define SQL_GIS_MBR_UTILS_H_INCLUDED

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
// Street, Suite 500, Boston, MA 02110-1335 USA.

/// @file
///
/// This file declares the interface of various utility functions for
/// geometrycollections. The functions may throw exceptions.

#include <boost/geometry.hpp>

#include "sql/gis/box.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"

namespace dd {
class Spatial_reference_system;
}  // namespace dd

namespace gis {

/// Checks if two MBRs are equal.
///
/// Empty boxes are considered equal.
///
/// @param[in] mbr1 First MBR.
/// @param[in] mbr2 Second MBR.
/// @retval true The MBRs are equal.
/// @retval false The MBRs are not equal.
bool mbrs_are_equal(Box const &mbr1, Box const &mbr2);

/// Checks if an MBR is empty.
///
/// By default, BG box coordinates are NaN. If a geometry is empty, its box will
/// have all NaN coordinates.
///
/// @param[in] mbr MBR to check.
/// @retval true The MBR is empty.
/// @retval false The MBR is not empty.
bool mbr_is_empty(Box const &mbr);

/// Checks if an MBR represents a point.
///
/// Boxes around points collapse so that min_corner == max_corner.
///
/// @param[in] mbr MBR to check.
/// @retval true The MBR is a point.
/// @retval false The MBR is not a point.
bool mbr_is_point(Box const &mbr);

/// Checks if an MBR represents a line.
///
/// Boxes around vertical and horizontal lines collapse so that either the
/// minimum and maximum X coordinate or Y coordinate are equal.
///
/// @param[in] mbr MBR to check.
/// @retval true The MBR is a line.
/// @retval false The MBR is not a line.
bool mbr_is_line(Box const &mbr);

/// Computes the envelope of a geometry.
///
/// The result may be a collapsed MBR.
///
/// @param[in] g The geometry.
/// @param[in] srs The spatial reference system of the geometry.
/// @param[out] mbr The envelope.
void box_envelope(const Geometry *g, const dd::Spatial_reference_system *srs,
                  Box *mbr);

}  // namespace gis

#endif  // SQL_GIS_MBR_UTILS_H_INCLUDED
