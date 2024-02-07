#ifndef SQL_GIS_GEOMETRY_EXTRACTION_H_INCLUDED
#define SQL_GIS_GEOMETRY_EXTRACTION_H_INCLUDED

// Copyright (c) 2021, 2024, Oracle and/or its affiliates.
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

#include "my_sys.h"  // my_error
#include "sql/dd/cache/dictionary_client.h"
#include "sql/gis/geometries.h"
#include "sql/gis/wkb.h"
#include "sql/sql_class.h"  // THD

#include <algorithm>
#include <memory>
/// @file
///
/// This file contains a few convenience functions for working with Geometries,
/// to avoid boilerplate and mishandling of Geometries.

/// Type used to differentiate the three cases that can happen when parsing a
/// geometry.
enum class ResultType { Error, NullValue, Value };

/// Type used to handle both the result of the decoding of a geometry and the
/// geometry in the case of success.
class GeometryExtractionResult {
 private:
  const ResultType m_resultType;
  std::unique_ptr<gis::Geometry> m_value;
  const dd::Spatial_reference_system *m_srs = nullptr;

 public:
  ResultType GetResultType() const { return m_resultType; }
  const dd::Spatial_reference_system *GetSrs() const {
    assert(m_resultType == ResultType::Value);
    return m_srs;
  }
  std::unique_ptr<gis::Geometry> GetValue() {
    assert(m_resultType == ResultType::Value);
    return std::move(m_value);
  }
  explicit GeometryExtractionResult(ResultType resultType)
      : m_resultType(resultType) {
    if (resultType == ResultType::Value) {
      throw ResultType::Error;
    }
  }
  explicit GeometryExtractionResult(std::unique_ptr<gis::Geometry> geometry,
                                    const dd::Spatial_reference_system *srs)
      : m_resultType(ResultType::Value),
        m_value(std::move(geometry)),
        m_srs(srs) {}
};

/// ExtractGeometry takes an Item or a Field, attempts to parse a geometry out
/// of it and returns a value combining the result of the parsing process with
/// the geometry in case it is a success.
///
/// @param[in] fieldOrItem The Field or Item we want a geometry from.
/// @param[in] thd THD* to report errors on
/// @param[in] func_name C-string to report errors as.
/// @returns GeometryExtractionResult which holds a result and an optional
/// Geometry

template <typename FieldOrItem>
GeometryExtractionResult ExtractGeometry(FieldOrItem *fieldOrItem, THD *thd,
                                         const char *func_name) {
  String backing_arg_wkb;
  if (fieldOrItem->result_type() != STRING_RESULT) {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name);
    return GeometryExtractionResult(ResultType::Error);
  }
  if (fieldOrItem->is_null()) {
    return GeometryExtractionResult(ResultType::NullValue);
  }
  String *arg_wkb = fieldOrItem->val_str(&backing_arg_wkb);
  if (thd->is_error()) {
    return GeometryExtractionResult(ResultType::Error);
  }
  if (nullptr == arg_wkb) {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name);
    return GeometryExtractionResult(ResultType::Error);
  }

  std::unique_ptr<dd::cache::Dictionary_client::Auto_releaser> releaser(
      new dd::cache::Dictionary_client::Auto_releaser(
          current_thd->dd_client()));
  const dd::Spatial_reference_system *srs = nullptr;
  std::unique_ptr<gis::Geometry> geo;
  bool result = gis::parse_geometry(thd, func_name, arg_wkb, &srs, &geo);

  if (result == true) {
    return GeometryExtractionResult(ResultType::Error);
  } else {
    return GeometryExtractionResult(std::move(geo), srs);
  }
}

#endif  // SQL_GIS_GEOMETRY_EXTRACTION_H_INCLUDED
