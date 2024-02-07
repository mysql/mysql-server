// Copyright (c) 2020, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file
///
/// This file declares the interface of various utility functions for
/// spatial operations (union, intersection, difference, symdifference).
/// The functions may throw exceptions.

#ifndef SQL_GIS_SO_UTILS_H_INCLUDED
#define SQL_GIS_SO_UTILS_H_INCLUDED

#include <memory>  // std::unique_ptr

#include "sql/gis/geometries.h"

namespace gis {

/// Removes all duplicates in a geometrycollection.
///
/// If the geometry is not a collection, the function does nothing.
/// Duplicates are removed in all levels, so for a geometrycollection,
/// the function is called for each member geometry as well.
///
/// @param[in] semi_major Semi-major axis of ellipsoid.
/// @param[in] semi_minor Semi-minor axis of ellipsoid.
/// @param[in, out] g The geometry to remove duplicates from.
void remove_duplicates(double semi_major, double semi_minor,
                       std::unique_ptr<Geometry> *g);

/// Narrow a geometry to its simplest form.
///
/// E.g. for a multipoint with only one point, the geometry is reduced to that
/// point. For an input geometry which cannot be narrowed further, the function
/// does nothing.
///
/// @param[in, out] g The geometry to possibly narrow.
void narrow_geometry(std::unique_ptr<Geometry> *g);

}  // namespace gis

#endif  // SQL_GIS_SO_UTILS_H_INCLUDED
