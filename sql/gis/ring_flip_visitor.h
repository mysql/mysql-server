#ifndef SQL_GIS_RING_FLIP_VISITOR_H_INCLUDED
#define SQL_GIS_RING_FLIP_VISITOR_H_INCLUDED

// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/geometry_visitor.h"

namespace gis {

/// A visitor that flips polygon rings so that exterior rings are in a
/// counter-clockwise direction and interior rings in a clockwise direction.
///
/// Invalid polygon rings are not guaranteed to be flipped to the correct
/// direction.
class Ring_flip_visitor : public Nop_visitor {
 private:
  /// Whether a ring with unknown direction has been encountered
  bool m_detected_unknown;

 public:
  Ring_flip_visitor() : m_detected_unknown(false) {}

  /// Check if the visitor has detected any invalid polygon rings during
  /// processing.
  ///
  /// Polygon rings which direction can't be determined are invalid. This is the
  /// only way this visitor detects invalid rings. Other invalid rings, e.g.,
  /// rings crossing themselves, are not necessarily detected.
  ///
  /// @retval true At least one invalid polygon ring.
  /// @retval false No invalid rings detected, but the geometry may still be
  /// invalid.
  bool invalid() const { return m_detected_unknown; }

  using Nop_visitor::visit_enter;
  bool visit_enter(Polygon *py) override;
  bool visit_enter(Multipolygon *py) override;

  bool visit_enter(Multipoint *mpt) override {
    return true;  // Don't descend into each point.
  }

  bool visit_enter(Multilinestring *mls) override {
    return true;  // Don't descend into each linestring.
  }
};

}  // namespace gis

#endif  // SQL_GIS_RING_FLIP_VISITOR_H_INCLUDED
