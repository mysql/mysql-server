#ifndef SQL_GIS_IS_VALID_H_INCLUDED
#define SQL_GIS_IS_VALID_H_INCLUDED

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
/// This file declares the interface to calculate if a geometry is valid

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/geometries.h"

namespace gis {

/// Decides if a geometry is valid.
///
/// The SRS must match the SRS referenced by the geometry, this is the caller's
/// responsibility.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g The geometry
/// @param[in] func_name Function name used in error reporting.
/// @param[out] is_valid The validity of the geometry
///
/// @retval false No error occured
/// @retval true An error has occured, the error has been reported with
/// my_error().
bool is_valid(const dd::Spatial_reference_system *srs, const Geometry *g,
              const char *func_name, bool *is_valid) noexcept;

}  // namespace gis

#endif  // SQL_GIS_IS_VALID_H_INCLUDED
