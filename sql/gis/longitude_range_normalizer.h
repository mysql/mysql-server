#ifndef SQL_GIS_LONGITUDE_RANGE_NORMALIZER_H_INCLUDED
#define SQL_GIS_LONGITUDE_RANGE_NORMALIZER_H_INCLUDED

// Copyright (c) 2021, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
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
/// Implements a longitude normalizer that converts longitude coordinates
/// outside the range such that geometries wrap correctly across
/// the 180/-180 boundary on the globe.

#include <cmath>  // M_PI

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/geometry_visitor.h"

namespace gis {

class Longitude_range_normalizer : public Nop_visitor {
 private:
  const dd::Spatial_reference_system *m_srs;

 public:
  /// Construct a new longitude range normalizer.
  ///
  /// @param srs The spatial reference system of the geometry.
  explicit Longitude_range_normalizer(const dd::Spatial_reference_system *srs)
      : m_srs(srs) {}

  using Nop_visitor::visit;
  bool visit(Point *pt) override {
    double lon = pt->x() - m_srs->prime_meridian() * m_srs->angular_unit();
    if (!m_srs->positive_east()) lon *= -1.0;
    if (lon <= -M_PI) {
      // Longitude -180 or less, so we add 360 (2*pi): e.g. -182 + 360 = 178
      pt->x(lon + 2.0 * M_PI);
    } else if (lon > M_PI) {
      // Longitude over 180, so we subtract 360 (2*pi): e.g. 181 - 360 = -179
      pt->x(lon - 2.0 * M_PI);
    }
    return false;
  }
};

}  // namespace gis

#endif  // SQL_GIS_LONGITUDE_RANGE_NORMALIZER_H_INCLUDED
