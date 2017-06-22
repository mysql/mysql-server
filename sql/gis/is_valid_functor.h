#ifndef SQL_GIS_IS_VALID_FUNCTOR_H_INCLUDED
#define SQL_GIS_IS_VALID_FUNCTOR_H_INCLUDED

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

#include <boost/geometry.hpp>

#include "sql/gis/functor.h"

namespace gis {

class Is_valid : public Unary_functor<bool> {
 private:
  boost::geometry::strategy::intersection::geographic_segments<>
      m_geographic_ll_la_aa_strategy;

 public:
  Is_valid(double semi_major, double semi_minor);
  virtual bool operator()(const Geometry &g) const;

  bool eval(const Cartesian_point &g) const;
  bool eval(const Cartesian_linestring &g) const;
  bool eval(const Cartesian_polygon &g) const;
  bool eval(const Cartesian_multipoint &g) const;
  bool eval(const Cartesian_multipolygon &g) const;
  bool eval(const Cartesian_multilinestring &g) const;
  bool eval(const Cartesian_geometrycollection &g) const;
  bool eval(const Geographic_point &g) const;
  bool eval(const Geographic_linestring &g) const;
  bool eval(const Geographic_polygon &g) const;
  bool eval(const Geographic_multipoint &g) const;
  bool eval(const Geographic_multipolygon &g) const;
  bool eval(const Geographic_multilinestring &g) const;
  bool eval(const Geographic_geometrycollection &g) const;
};
}
#endif  // SQL_GIS_IS_VALID_FUNCTOR_H_INCLUDED
