#ifndef GIS__LENGTH_FUNCTOR_H_INCLUDED
#define GIS__LENGTH_FUNCTOR_H_INCLUDED

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
/// This file declares the length functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::length() function.
///
/// @see gis::length

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>
#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"

namespace gis {

/// Length functor that calls Boost.Geometry with the correct parameter types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement length or other geographic functions. It should not be used
/// directly by other MySQL code.
class Length : public Unary_functor<double> {
 private:
  std::unique_ptr<boost::geometry::strategy::distance::andoyer<
      boost::geometry::srs::spheroid<double>>>
      m_geographic_strategy;

 public:
  Length(double major, double minor);
  double operator()(const Geometry *g1);

  double eval(const Geometry *g1) const;

  double eval(const Geographic_linestring *g1);
  double eval(const Cartesian_linestring *g1);

  double eval(const Geographic_multilinestring *g1);
  double eval(const Cartesian_multilinestring *g1);
};

}  // namespace gis

#endif  // GIS__LENGTH_FUNCTOR_H_INCLUDED
