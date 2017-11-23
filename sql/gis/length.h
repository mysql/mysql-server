#ifndef GIS__LENGTH_H_INCLUDED
#define GIS__LENGTH_H_INCLUDED

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
/// This file declares the interface to calculate the length of linestrings and
/// multilinestrings.

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/geometries.h"

namespace gis {
/// Computes the length of linestrings and multilinestrings.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g1 linestring or multilinestring.
/// @param[out] distance The length of g1 in the SRS linear unit.
/// @param[out] null True if the return value is NULL.
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().
bool length(const dd::Spatial_reference_system *srs, const Geometry *g1,
            double *distance, bool *null) noexcept;

}  // namespace gis

#endif  // GIS__LENGTH_H_INCLUDED
