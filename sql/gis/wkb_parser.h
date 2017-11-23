#ifndef SQL_GIS_WKB_PARSER_H_INCLUDED
#define SQL_GIS_WKB_PARSER_H_INCLUDED

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
/// This file declares the interface of the WKB parser for geometries and the
/// parser for the internal geometry representation.

#include <memory>  // std::unique_ptr

#include "sql/gis/geometries.h"
#include "sql/gis/srid.h"

class THD;
class String;
namespace dd {
class Spatial_reference_system;
}

namespace gis {

/// Parses an SRID from a little-endian string.
///
/// @param[in] str The string.
/// @param[in] length Length of the string.
/// @param[out] srid The SRID read from the string.
///
/// @retval false Success.
/// @retval true Error. my_error() has not been called.
bool parse_srid(const char *str, std::size_t length, srid_t *srid);

///
/// Parses a geometry WKB string and constructs a geometry object.
///
/// The geometry is allocated on the heap and ownership is transferred to the
/// caller.
///
/// If ignore_axis_order is true, the string is assumed to be on the geometry
/// storage format, not general WKB. The storage format differs from ordinary
/// WKB by having a fixed x=longitude, y=latitude mapping.
///
/// @param srs The SRS of the geometry.
/// @param wkb The WKB string.
/// @param length Length of the WKB string.
/// @param ignore_axis_order Ignore SRS axis order and assume it's always
/// long-lat.
///
/// @return The geometry
std::unique_ptr<Geometry> parse_wkb(const dd::Spatial_reference_system *srs,
                                    const char *wkb, std::size_t length,
                                    bool ignore_axis_order = false);

/// Parses a little-endian geometry string (SRID + WKB).
///
/// The geometry is allocated on the heap and ownership is transferred to the
/// caller.
///
/// @param[in] thd Thread handle
/// @param[in] func_name The function name to use in error messages.
/// @param[in] str The geometry string.
/// @param[out] srs The spatial reference system of the geometry.
/// @param[out] geometry The geometry.
///
/// @retval false Success.
/// @retval true Error. my_error() has been called.
bool parse_geometry(THD *thd, const char *func_name, const String *str,
                    const dd::Spatial_reference_system **srs,
                    std::unique_ptr<Geometry> *geometry);

}  // namespace gis

#endif  // SQL_GIS_WKB_PARSER_H_INCLUDED
