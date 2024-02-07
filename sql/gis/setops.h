/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2.0,
    as published by the Free Software Foundation.

    This program is designed to work with certain software (including
    but not limited to OpenSSL) that is licensed under separate terms,
    as designated in a particular file or component or in included license
    documentation.  The authors of MySQL hereby grant you an additional
    permission to link the program and your derivative works with the
    separately licensed software that they have either included with
    the program or referenced in the documentation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
/**
    @file

    This file declares the interface of set GIS operations. These are
    set operations that compute a geometry given two input geometries.
*/

#ifndef SQL_GIS_SETOPS_H_INCLUDED
#define SQL_GIS_SETOPS_H_INCLUDED

#include <memory>  // std::unique_ptr

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/geometries.h"

namespace gis {

/// Finds the difference between two geometries.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g1 Input geometry.
/// @param[in] g2 Input geometry.
/// @param[in] func_name Function name used in error reporting.
/// @param[out] result Result.
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().
bool difference(const dd::Spatial_reference_system *srs, const Geometry *g1,
                const Geometry *g2, const char *func_name,
                std::unique_ptr<Geometry> *result) noexcept;

/// Finds the symdifference between two geometries.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g1 Input geometry.
/// @param[in] g2 Input geometry.
/// @param[in] func_name Function name used in error reporting.
/// @param[out] result Result.
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().
bool symdifference(const dd::Spatial_reference_system *srs, const Geometry *g1,
                   const Geometry *g2, const char *func_name,
                   std::unique_ptr<Geometry> *result) noexcept;

/// Finds the union of two geometries.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g1 Input geometry.
/// @param[in] g2 Input geometry.
/// @param[in] func_name Function name used in error reporting.
/// @param[out] result Result. Invalid if `result_null`.
/// @param[out] result_null Whether result is `NULL` instead of `result`.
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().
bool union_(const dd::Spatial_reference_system *srs, const Geometry *g1,
            const Geometry *g2, const char *func_name,
            std::unique_ptr<Geometry> *result, bool *result_null) noexcept;

/// Finds the intersection between two geometries.
///
/// @param[in] srs The spatial reference system.
/// @param[in] g1 Input geometry.
/// @param[in] g2 Input geometry.
/// @param[in] func_name Function name used in error reporting.
/// @param[out] result Result.
///
/// @retval false Success.
/// @retval true An error has occurred. The error has been reported with
/// my_error().
bool intersection(const dd::Spatial_reference_system *srs, const Geometry *g1,
                  const Geometry *g2, const char *func_name,
                  std::unique_ptr<Geometry> *result) noexcept;

}  // namespace gis

#endif  // SQL_GIS_SETOPS_H_INCLUDED
