#ifndef GIS__RING_FLIP_VISITOR_H_INCLUDED
#define GIS__RING_FLIP_VISITOR_H_INCLUDED

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
#include "geometry_visitor.h"
#include "ring_direction.h"

namespace gis {

/// A visitor that flips polygon rings so that exterior rings are in one
/// direction and interior rings in the other direction.
///
/// Invalid polygon rings are not guaranteed to be flipped to the correct
/// direction.
class Ring_flip_visitor : public Nop_visitor {
 private:
  /// Spatial reference system of the geometry
  const dd::Spatial_reference_system *m_srs;
  /// Direction of exterior rings.
  Ring_direction m_ring_direction;
  /// Whether a ring with unknown direction has been encountered
  bool m_detected_unknown;

 public:
  /// Construct a new ring flip visitor.
  ///
  /// @param srs The spatial reference system of the geometry.
  /// @param direction Direction of exterior rings.
  Ring_flip_visitor(const dd::Spatial_reference_system *srs,
                    const Ring_direction direction)
      : m_srs(srs), m_ring_direction(direction), m_detected_unknown(false) {
    DBUG_ASSERT(direction != Ring_direction::kUnknown);
  }

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
  bool visit_enter(Polygon *py) override {
    Linearring &exterior = py->exterior_ring();
    Ring_direction exterior_direction = ring_direction(m_srs, exterior);
    if (exterior_direction != m_ring_direction &&
        exterior_direction != Ring_direction::kUnknown) {
      exterior.flip();
    } else if (exterior_direction == Ring_direction::kUnknown) {
      m_detected_unknown = true;
    }

    DBUG_ASSERT(py->size() > 0);

    for (std::size_t i = 0; i < py->size() - 1; i++) {
      Linearring &interior = py->interior_ring(i);
      Ring_direction interior_direction = ring_direction(m_srs, interior);
      if (interior_direction == m_ring_direction)
        interior.flip();
      else if (interior_direction == Ring_direction::kUnknown)
        m_detected_unknown = true;
    }

    return true;  // Don't descend into each ring.
  }

  bool visit_enter(Multipoint *mpt) override {
    return true;  // Don't descend into each point.
  }

  bool visit_enter(Multilinestring *mls) override {
    return true;  // Don't descend into each linestring.
  }
};

}  // namespace gis

#endif  // GIS__RING_FLIP_VISITOR_H_INCLUDED
