#ifndef SQL_GIS_SRS_WKT_PARSER_H_INCLUDED
#define SQL_GIS_SRS_WKT_PARSER_H_INCLUDED

// Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include <string>
#include <vector>

#include <boost/variant/variant.hpp>

#include "sql/gis/srid.h"
#include "sql/gis/srs/srs.h"

namespace gis {
namespace srs {
namespace wkt_parser {

/// String type used in the parse tree
typedef std::string String;

struct Authority {
  bool valid;
  String name;
  String code;

  Authority() : valid(false) {}
};

struct Spheroid {
  String name;
  double semi_major_axis;
  double inverse_flattening;
  Authority authority;

  Spheroid() : semi_major_axis(0.0), inverse_flattening(0.0) {}
};

struct Towgs84 {
  bool valid;
  double dx;
  double dy;
  double dz;
  double ex;
  double ey;
  double ez;
  double ppm;

  Towgs84()
      : valid(false),
        dx(0.0),
        dy(0.0),
        dz(0.0),
        ex(0.0),
        ey(0.0),
        ez(0.0),
        ppm(0.0) {}
};

struct Datum {
  String name;
  Spheroid spheroid;
  Towgs84 towgs84;
  Authority authority;
};

struct Prime_meridian {
  String name;
  double longitude;
  Authority authority;

  Prime_meridian() : longitude(0.0) {}
};

struct Unit {
  String name;
  double conversion_factor;
  Authority authority;

  Unit() : conversion_factor(0.0) {}
};

struct Axis {
  String name;
  Axis_direction direction;

  Axis() : direction(Axis_direction::UNSPECIFIED) {}
};

struct Twin_axes {
  bool valid;
  Axis x;
  Axis y;

  Twin_axes() : valid(false) {}
};

struct Geographic_cs {
  String name;
  Datum datum;
  Prime_meridian prime_meridian;
  Unit angular_unit;
  Twin_axes axes;
  Authority authority;
};

struct Projection {
  String name;
  Authority authority;
};

struct Projection_parameter {
  String name;
  double value;
  Authority authority;

  Projection_parameter() : value(0.0) {}
};

typedef std::vector<Projection_parameter> Projection_parameters;

struct Projected_cs {
  String name;
  Geographic_cs geographic_cs;
  Projection projection;
  Projection_parameters parameters;
  Unit linear_unit;
  Twin_axes axes;
  Authority authority;
};

typedef boost::variant<Projected_cs, Geographic_cs> Coordinate_system;

/**
  Parse an SRS definition WKT string.

  The parser understands WKT as defined by the \<horz cs\>
  specification in OGC 01-009.

  @param[in] srid Spatial reference system ID to use when reporting errors
  @param[in] begin Start of WKT string in UTF-8
  @param[in] end End of WKT string in UTF-8
  @param[out] cs Coordinate system

  @retval true An error has occurred
  @retval false Success
*/
bool parse_wkt(srid_t srid, const char *begin, const char *end,
               Coordinate_system *cs);

}  // namespace wkt_parser
}  // namespace srs
}  // namespace gis

#endif  // SQL_GIS_SRS_WKT_PARSER_H_INCLUDED
