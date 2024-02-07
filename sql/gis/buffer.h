#ifndef SQL_GIS_BUFFER_H_INCLUDED
#define SQL_GIS_BUFFER_H_INCLUDED

// Copyright (c) 2021, 2024, Oracle and/or its affiliates.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License, version 2.0,
//  as published by the Free Software Foundation.
//
//  This program is designed to work with certain software (including
//  but not limited to OpenSSL) that is licensed under separate terms,
//  as designated in a particular file or component or in included license
//  documentation.  The authors of MySQL hereby grant you an additional
//  permission to link the program and your derivative works with the
//  separately licensed software that they have either included with
//  the program or referenced in the documentation.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License, version 2.0, for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

/// @file
///
/// This file declares the interface for calculating the buffer of a geometry.

#include <memory>                                   // std::unique_ptr
#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/buffer_strategies.h"              // buffer_strategies
#include "sql/gis/geometries.h"                     // gis::Geometry

namespace gis {

/// Invalid buffer strategies exception.
///
/// Thrown when a buffer strategy is given as argument and the geometry is not
/// compatible: e.g. point strategy with a Linestring. Also thrown when
/// distance is negative and geometry is not Multi-/Polygon or GC.
struct invalid_buffer_argument_exception : public std::exception {};

/// Invalid buffer result exception.
///
/// Thrown when the result from buffer call to Boost is invalid.
struct invalid_buffer_result_exception : public std::exception {};

/// Creates the buffer of a geometry
///
/// @param[in] srs The spatial reference system.
/// @param[in] g Input geometry.
/// @param[in] strategies Struct holding strategy arguments for buffer creation
/// @param[in] func_name Function name used in error reporting.
/// @param[out] result The geometry that is the buffer of input geometry g
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().

bool buffer(const dd::Spatial_reference_system *srs, const Geometry &g,
            const BufferStrategies &strategies, const char *func_name,
            std::unique_ptr<Geometry> *result) noexcept;

}  // namespace gis

#endif  // SQL_GIS_BUFFER_H_INCLUDED
