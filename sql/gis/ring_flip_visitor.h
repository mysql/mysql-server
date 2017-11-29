#ifndef SQL_GIS_RING_FLIP_VISITOR_H_INCLUDED
#define SQL_GIS_RING_FLIP_VISITOR_H_INCLUDED

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
