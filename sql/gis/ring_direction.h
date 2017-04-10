#ifndef GIS__RING_DIRECTION_H_INCLUDED
#define GIS__RING_DIRECTION_H_INCLUDED

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

#include "dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "geometries.h"

namespace gis {

/// Computes the direction of a linear ring.
///
/// @param[in] srs The spatial reference system.
/// @param[in] lr Linear ring.
///
/// @return Ring direction, or kUnknown if it can't be determined.
Ring_direction ring_direction(const dd::Spatial_reference_system *srs,
                              const Linearring &lr) noexcept;

}  // namespace gis

#endif  // GIS__RING_DIRECTION_H_INCLUDED
