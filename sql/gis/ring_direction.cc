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
/// This file implements the ring direction function.

#include "ring_direction.h"

#include "dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "geometries.h"
#include "my_sys.h"
#include "mysqld_error.h"

namespace gis {

static Ring_direction cartesian_ring_direction(const Linearring &lr) {
  // The ring must have at least four points.
  DBUG_ASSERT(lr.size() >= 4);
  // The ring must be closed (first and last point are equal).
  DBUG_ASSERT(lr[0].x() == lr[lr.size() - 1].x() &&
              lr[0].y() == lr[lr.size() - 1].y());

  double min_x = 0.0;
  double min_y = 0.0;
  std::size_t min_i = 0;

  // Find the coordinate with the lowest x and y value
  for (std::size_t i = 0; i < lr.size(); i++) {
    double x = lr[i].x();
    double y = lr[i].y();

    if (i == 0) {
      min_x = x;
      min_y = y;
      continue;
    }

    if (x < min_x) {
      min_x = x;
      min_y = y;
      min_i = i;
    } else if (x == min_x) {
      if (y < min_y) {
        min_y = y;
        min_i = i;
      }
    }
  }

  // Since rings are closed and we started at index 0, min_i can't be the last
  // point in the ring.
  DBUG_ASSERT(min_i < lr.size() - 1);

  // prev_i is the previous point from min_i, skipping duplicates.
  std::size_t prev_i = min_i == 0 ? lr.size() - 2 : min_i - 1;
  while (lr[prev_i].x() == lr[min_i].x() && lr[prev_i].y() == lr[min_i].y()) {
    if (prev_i == 0)
      prev_i = lr.size() - 2;
    else
      prev_i--;
    // If we get back to min_i, all the points in the ring are the same.
    if (prev_i == min_i) return Ring_direction::kUnknown;
  }

  // next_i is the next point from min_i, skipping duplicates.
  std::size_t next_i = min_i + 1;
  while (lr[next_i].x() == lr[min_i].x() && lr[next_i].y() == lr[min_i].y()) {
    if (next_i > lr.size() - 2)
      next_i = 0;
    else
      next_i++;
    // If we get back to min_i, all the points in the ring are the same.
    if (next_i == min_i) return Ring_direction::kUnknown;
  }

  // The triangle's area tells the direction.
  double x1 = lr[min_i].x() - lr[prev_i].x();
  double y1 = lr[min_i].y() - lr[prev_i].y();
  double x2 = lr[next_i].x() - lr[min_i].x();
  double y2 = lr[next_i].y() - lr[min_i].y();
  double sign = x1 * y2 - x2 * y1;

  if (sign < 0.0) return Ring_direction::kCW;
  if (sign > 0.0) return Ring_direction::kCCW;

  return Ring_direction::kUnknown;  // There's a spike in the ring.
}

Ring_direction ring_direction(const dd::Spatial_reference_system *srs,
                              const Linearring &lr) noexcept {
  DBUG_ASSERT(((srs == nullptr || srs->is_cartesian()) &&
               lr.coordinate_system() == Coordinate_system::kCartesian) ||
              (srs->is_geographic() &&
               lr.coordinate_system() == Coordinate_system::kGeographic));

  if (srs == nullptr || srs->is_cartesian()) {
    return cartesian_ring_direction(lr);
  } else {
    // Not implemented yet.
    return Ring_direction::kUnknown;
  }
}

}  // namespace gis
