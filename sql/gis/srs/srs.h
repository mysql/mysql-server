#ifndef SQL_GIS_SRS_SRS_H_INCLUDED
#define SQL_GIS_SRS_SRS_H_INCLUDED

// Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include <cmath>
#include <cstdint>

#include "my_dbug.h"
#include "sql/gis/srid.h"

namespace gis {
namespace srs {

/// Spatial reference system type.
enum class Srs_type : std::uint8_t { UNKNOWN = 0, PROJECTED, GEOGRAPHIC };

/// Projection method. Values are EPSG codes.
enum class Projection_type : std::uint32_t {
  UNKNOWN = 0,
  POPULAR_VISUALISATION_PSEUDO_MERCATOR = 1024,
  LAMBERT_AZIMUTHAL_EQUAL_AREA_SPHERICAL = 1027,
  EQUIDISTANT_CYLINDRICAL = 1028,
  EQUIDISTANT_CYLINDRICAL_SPHERICAL = 1029,
  KROVAK_NORTH_ORIENTATED = 1041,
  KROVAK_MODIFIED = 1042,
  KROVAK_MODIFIED_NORTH_ORIENTATED = 1043,
  LAMBERT_CONIC_CONFORMAL_2SP_MICHIGAN = 1051,
  COLOMBIA_URBAN = 1052,
  LAMBERT_CONIC_CONFORMAL_1SP = 9801,
  LAMBERT_CONIC_CONFORMAL_2SP = 9802,
  LAMBERT_CONIC_CONFORMAL_2SP_BELGIUM = 9803,
  MERCATOR_VARIANT_A = 9804,
  MERCATOR_VARIANT_B = 9805,
  CASSINI_SOLDNER = 9806,
  TRANSVERSE_MERCATOR = 9807,
  TRANSVERSE_MERCATOR_SOUTH_ORIENTATED = 9808,
  OBLIQUE_STEREOGRAPHIC = 9809,
  POLAR_STEREOGRAPHIC_VARIANT_A = 9810,
  NEW_ZEALAND_MAP_GRID = 9811,
  HOTINE_OBLIQUE_MERCATOR_VARIANT_A = 9812,
  LABORDE_OBLIQUE_MERCATOR = 9813,
  HOTINE_OBLIQUE_MERCATOR_VARIANT_B = 9815,
  TUNISIA_MINING_GRID = 9816,
  LAMBERT_CONIC_NEAR_CONFORMAL = 9817,
  AMERICAN_POLYCONIC = 9818,
  KROVAK = 9819,
  LAMBERT_AZIMUTHAL_EQUAL_AREA = 9820,
  ALBERS_EQUAL_AREA = 9822,
  TRANSVERSE_MERCATOR_ZONED_GRID_SYSTEM = 9824,
  LAMBERT_CONIC_CONFORMAL_WEST_ORIENTATED = 9826,
  BONNE_SOUTH_ORIENTATED = 9828,
  POLAR_STEREOGRAPHIC_VARIANT_B = 9829,
  POLAR_STEREOGRAPHIC_VARIANT_C = 9830,
  GUAM_PROJECTION = 9831,
  MODIFIED_AZIMUTHAL_EQUIDISTANT = 9832,
  HYPERBOLIC_CASSINI_SOLDNER = 9833,
  LAMBERT_CYLINDRICAL_EQUAL_AREA_SPHERICAL = 9834,
  LAMBERT_CYLINDRICAL_EQUAL_AREA = 9835
};

/// Coordinate axis direction.
enum class Axis_direction : std::uint8_t {
  UNSPECIFIED = 0,
  NORTH,
  SOUTH,
  EAST,
  WEST,
  OTHER
};

/// Superclass for all spatial reference systems.
class Spatial_reference_system {
 public:
  virtual ~Spatial_reference_system() {}

  /**
    Get the type of spatial refrence system: projected, geometric,
    etc.

    @return SRS type
  */
  virtual Srs_type srs_type() = 0;

  /**
    Clone the object.

    @return A new Spatial_reference_system object
  */
  virtual Spatial_reference_system *clone() = 0;

  /**
    Retrieve the axis direction of the spatial
    reference system.

    @param axis axis number, zero indexed
    @return Axis direction
  */
  virtual Axis_direction axis_direction(const int axis) const = 0;

  /**
    Retrieve the angular unit relative to radians.

    @return Conversion factor.
   */
  virtual double angular_unit() const = 0;

  /**
    Retrieve the prime meridian relative to Greenwich.

    The prime meridian is returned in the angular unit of the
    SRS. Positive numbers are East of Greenwich.

    @see angular_unit

    @return Prime meridian.
   */
  virtual double prime_meridian() const = 0;
};

namespace wkt_parser {
struct Geographic_cs;
}  // namespace wkt_parser

/// A geographic (longitude-latitude) spatial reference system.
class Geographic_srs : public Spatial_reference_system {
 private:
  /// Semi-major axis of ellipsoid
  double m_semi_major_axis;
  /// Inverse flattening of ellipsoid
  double m_inverse_flattening;
  /// Bursa Wolf transformation parameters used to transform to WGS84.
  double m_towgs84[7];
  /// Longitude of the prime meridian relative to the Greenwich
  /// Meridian (measured in m_angular_unit). Positive values are East
  /// of Greenwich.
  double m_prime_meridian;
  /// Conversion factor for the angular unit relative to radians.
  double m_angular_unit;
  /// Direction of x and y axis, respectively.
  Axis_direction m_axes[2];

 public:
  Geographic_srs()
      : m_semi_major_axis(NAN),
        m_inverse_flattening(NAN),
        m_prime_meridian(NAN),
        m_angular_unit(NAN) {
    for (double &d : m_towgs84) d = NAN;
    for (Axis_direction &d : m_axes) d = Axis_direction::UNSPECIFIED;
  }

  virtual Srs_type srs_type() override { return Srs_type::GEOGRAPHIC; }

  virtual Spatial_reference_system *clone() override {
    return new Geographic_srs(*this);
  }

  /**
    Initialize from parse tree.

    @param[in] srid Spatial reference system ID to use when reporting errors
    @param[in] g Parser output

    @retval true An error has occurred. The error has been flagged.
    @retval false Success
  */
  virtual bool init(srid_t srid, wkt_parser::Geographic_cs *g);

  /**
    Check if this SRS has valid Bursa Wolf parameters.

    @retval true Transformation parameters are specified
    @retval false Transformation parameters are not specified
  */
  bool has_towgs84() {
    // Either none or all parameters are specified.
    return !std::isnan(m_towgs84[0]);
  }

  /**
    Check if this SRS has valid axis definitions.

    @retval true Axes are specified
    @retval false Axes are not specified
  */
  bool has_axes() {
    // Either none or both axes are specified.
    DBUG_ASSERT((m_axes[0] == Axis_direction::UNSPECIFIED) ==
                (m_axes[1] == Axis_direction::UNSPECIFIED));

    return m_axes[0] != Axis_direction::UNSPECIFIED;
  }

  Axis_direction axis_direction(const int axis) const override {
    DBUG_ASSERT(axis >= 0 && axis <= 1);
    return m_axes[axis];
  }

  double semi_major_axis() const { return m_semi_major_axis; }

  double inverse_flattening() const { return m_inverse_flattening; }

  double angular_unit() const override { return m_angular_unit; }

  double prime_meridian() const override { return m_prime_meridian; }
};

namespace wkt_parser {
struct Projected_cs;
}  // namespace wkt_parser

/// A projected spatial reference system.
class Projected_srs : public Spatial_reference_system {
 private:
  /// The geographic SRS this SRS is projected from.
  Geographic_srs m_geographic_srs;
  /// Converson factor for the linar unit relative to meters.
  double m_linear_unit;
  /// Direction of x and y axis, respectively.
  Axis_direction m_axes[2];

 public:
  Projected_srs() : m_linear_unit(NAN) {
    for (Axis_direction &d : m_axes) d = Axis_direction::UNSPECIFIED;
  }

  virtual Srs_type srs_type() override { return Srs_type::PROJECTED; }

  /**
    Initialize from parse tree.

    @param[in] srid Spatial reference system ID to use when reporting errors
    @param[in] p Parser output

    @retval true An error has occurred. The error has been flagged.
    @retval false Success
  */
  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p);

  /**
    Get the map projection method.

    @return Projection type
  */
  virtual Projection_type projection_type() = 0;

  Axis_direction axis_direction(const int axis) const override {
    DBUG_ASSERT(axis >= 0 && axis <= 1);
    return m_axes[axis];
  }

  double angular_unit() const override {
    return m_geographic_srs.angular_unit();
  }

  double prime_meridian() const override {
    return m_geographic_srs.prime_meridian();
  }
};

/// A projected SRS of an unknown projection type.
///
/// This SRS can be used as any other projected SRS, but since the
/// projection type is unkown, geometries in this SRS can't be
/// transformed to other SRSs.
class Unknown_projected_srs : public Projected_srs {
 public:
  virtual Spatial_reference_system *clone() override {
    return new Unknown_projected_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::UNKNOWN;
  }
};

/// A Popular Visualisation Pseudo Mercator projection (EPSG 1024).
class Popular_visualisation_pseudo_mercator_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Popular_visualisation_pseudo_mercator_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Popular_visualisation_pseudo_mercator_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::POPULAR_VISUALISATION_PSEUDO_MERCATOR;
  }
};

/// A Lambert Azimuthal Equal Area (Spherical) projection (EPSG 1027).
class Lambert_azimuthal_equal_area_spherical_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_azimuthal_equal_area_spherical_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_azimuthal_equal_area_spherical_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_AZIMUTHAL_EQUAL_AREA_SPHERICAL;
  }
};

/// An Equidistant Cylindrical projection (EPSG 1028).
class Equidistant_cylindrical_srs : public Projected_srs {
 private:
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Equidistant_cylindrical_srs()
      : m_standard_parallel_1(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Equidistant_cylindrical_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::EQUIDISTANT_CYLINDRICAL;
  }
};

/// An Equidistant Cylindrical (Spherical) projection (EPSG 1029).
class Equidistant_cylindrical_spherical_srs : public Projected_srs {
 private:
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Equidistant_cylindrical_spherical_srs()
      : m_standard_parallel_1(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Equidistant_cylindrical_spherical_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::EQUIDISTANT_CYLINDRICAL_SPHERICAL;
  }
};

/// A Krovak (North Orientated) projection (EPSG 1041).
class Krovak_north_orientated_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_center;
  /// The rotation applied to spherical coordinates, measured on the
  /// conformal sphere in the plane of the meridian of origin (EPSG
  /// 1036).
  double m_azimuth;
  /// Latitude of the parallel on which the projection is based. This
  /// latitude is not geographic, but is defined on the conformal
  /// sphere AFTER its rotation to obtain the oblique aspect of the
  /// projection (EPSG 8818).
  double m_pseudo_standard_parallel_1;
  /// The factor by which the map grid is reduced or enlarged at the
  /// pseudo-standard parallel (EPSG 8819).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Krovak_north_orientated_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_pseudo_standard_parallel_1(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Krovak_north_orientated_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::KROVAK_NORTH_ORIENTATED;
  }
};

/// A Krovak Modified projection (EPSG 1042).
class Krovak_modified_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_center;
  /// The rotation applied to spherical coordinates, measured on the
  /// conformal sphere in the plane of the meridian of origin (EPSG
  /// 1036).
  double m_azimuth;
  /// Latitude of the parallel on which the projection is based. This
  /// latitude is not geographic, but is defined on the conformal
  /// sphere AFTER its rotation to obtain the oblique aspect of the
  /// projection (EPSG 8818).
  double m_pseudo_standard_parallel_1;
  /// The factor by which the map grid is reduced or enlarged at the
  /// pseudo-standard parallel (EPSG 8819).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;
  /// The first ordinate of the evaluation point (EPSG 8617).
  double m_evaluation_point_ordinate_1;
  /// The second ordinate of the evaluation point(EPSG 8618).
  double m_evaluation_point_ordinate_2;
  /// Coefficient C1 used in polynomial transformation (EPSG 1026).
  double m_c1;
  /// Coefficient C2 used in polynomial transformation (EPSG 1027).
  double m_c2;
  /// Coefficient C3 used in polynomial transformation (EPSG 1028).
  double m_c3;
  /// Coefficient C4 used in polynomial transformation (EPSG 1029).
  double m_c4;
  /// Coefficient C5 used in polynomial transformation (EPSG 1030).
  double m_c5;
  /// Coefficient C6 used in polynomial transformation (EPSG 1031).
  double m_c6;
  /// Coefficient C7 used in polynomial transformation (EPSG 1032).
  double m_c7;
  /// Coefficient C8 used in polynomial transformation (EPSG 1033).
  double m_c8;
  /// Coefficient C9 used in polynomial transformation (EPSG 1034).
  double m_c9;
  /// Coefficient C10 used in polynomial transformation (EPSG 1035).
  double m_c10;

 public:
  Krovak_modified_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_pseudo_standard_parallel_1(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN),
        m_evaluation_point_ordinate_1(NAN),
        m_evaluation_point_ordinate_2(NAN),
        m_c1(NAN),
        m_c2(NAN),
        m_c3(NAN),
        m_c4(NAN),
        m_c5(NAN),
        m_c6(NAN),
        m_c7(NAN),
        m_c8(NAN),
        m_c9(NAN),
        m_c10(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Krovak_modified_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::KROVAK_MODIFIED;
  }
};

/// A Krovak Modified (North Orientated) projection (EPSG 1043).
class Krovak_modified_north_orientated_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_center;
  /// The rotation applied to spherical coordinates, measured on the
  /// conformal sphere in the plane of the meridian of origin (EPSG
  /// 1036).
  double m_azimuth;
  /// Latitude of the parallel on which the projection is based. This
  /// latitude is not geographic, but is defined on the conformal
  /// sphere AFTER its rotation to obtain the oblique aspect of the
  /// projection (EPSG 8818).
  double m_pseudo_standard_parallel_1;
  /// The factor by which the map grid is reduced or enlarged at the
  /// pseudo-standard parallel (EPSG 8819).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;
  /// The first ordinate of the evaluation point (EPSG 8617).
  double m_evaluation_point_ordinate_1;
  /// The second ordinate of the evaluation point(EPSG 8618).
  double m_evaluation_point_ordinate_2;
  /// Coefficient C1 used in polynomial transformation (EPSG 1026).
  double m_c1;
  /// Coefficient C2 used in polynomial transformation (EPSG 1027).
  double m_c2;
  /// Coefficient C3 used in polynomial transformation (EPSG 1028).
  double m_c3;
  /// Coefficient C4 used in polynomial transformation (EPSG 1029).
  double m_c4;
  /// Coefficient C5 used in polynomial transformation (EPSG 1030).
  double m_c5;
  /// Coefficient C6 used in polynomial transformation (EPSG 1031).
  double m_c6;
  /// Coefficient C7 used in polynomial transformation (EPSG 1032).
  double m_c7;
  /// Coefficient C8 used in polynomial transformation (EPSG 1033).
  double m_c8;
  /// Coefficient C9 used in polynomial transformation (EPSG 1034).
  double m_c9;
  /// Coefficient C10 used in polynomial transformation (EPSG 1035).
  double m_c10;

 public:
  Krovak_modified_north_orientated_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_pseudo_standard_parallel_1(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN),
        m_evaluation_point_ordinate_1(NAN),
        m_evaluation_point_ordinate_2(NAN),
        m_c1(NAN),
        m_c2(NAN),
        m_c3(NAN),
        m_c4(NAN),
        m_c5(NAN),
        m_c6(NAN),
        m_c7(NAN),
        m_c8(NAN),
        m_c9(NAN),
        m_c10(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Krovak_modified_north_orientated_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::KROVAK_MODIFIED_NORTH_ORIENTATED;
  }
};

/// A Lambert Conic Conformal (2SP Michigan) projection (EPSG 1051).
class Lambert_conic_conformal_2sp_michigan_srs : public Projected_srs {
 private:
  /// Latitude of the false origin, at which the false easting and
  /// northing is defined (EPSG 8821).
  double m_latitude_of_origin;
  /// Longitude (central meridian) of the false origin, at which the
  /// false easting and northing is defined (EPSG 8822).
  double m_longitude_of_origin;
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Latitude of the second parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8824).
  double m_standard_parallel_2;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;
  /// Ellipsoid scaling factor (EPSG 1038).
  double m_ellipsoid_scale_factor;

 public:
  Lambert_conic_conformal_2sp_michigan_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_standard_parallel_1(NAN),
        m_standard_parallel_2(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN),
        m_ellipsoid_scale_factor(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_conformal_2sp_michigan_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_CONFORMAL_2SP_MICHIGAN;
  }
};

/// A Colombia Urban projection(EPSG 1052).
class Colombia_urban_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;
  /// The height of the projection plane at its origin (EPSG 1039).
  double m_projection_plane_height_at_origin;

 public:
  Colombia_urban_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN),
        m_projection_plane_height_at_origin(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Colombia_urban_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::COLOMBIA_URBAN;
  }
};

/// A Lambert Conic Conformal (1SP) projection, alias Lambert Conic
/// Conformal or LCC (EPSG 9801).
class Lambert_conic_conformal_1sp_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_conic_conformal_1sp_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_conformal_1sp_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_CONFORMAL_1SP;
  }
};

/// A Lambert Conic Conformal (2SP) projection, alias Lambert Conic
/// Conformal or LCC (EPSG 9802).
class Lambert_conic_conformal_2sp_srs : public Projected_srs {
 private:
  /// Latitude of the false origin, at which the false easting and
  /// northing is defined (EPSG 8821).
  double m_latitude_of_origin;
  /// Longitude (central meridian) of the false origin, at which the
  /// false easting and northing is defined (EPSG 8822).
  double m_longitude_of_origin;
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Latitude of the second parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8824).
  double m_standard_parallel_2;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;

 public:
  Lambert_conic_conformal_2sp_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_standard_parallel_1(NAN),
        m_standard_parallel_2(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_conformal_2sp_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_CONFORMAL_2SP;
  }
};

/// A Lambert Conic Conformal (2SP Belgium) projection (EPSG 9803).
class Lambert_conic_conformal_2sp_belgium_srs : public Projected_srs {
 private:
  /// Latitude of the false origin, at which the false easting and
  /// northing is defined (EPSG 8821).
  double m_latitude_of_origin;
  /// Longitude (central meridian) of the false origin, at which the
  /// false easting and northing is defined (EPSG 8822).
  double m_longitude_of_origin;
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Latitude of the second parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8824).
  double m_standard_parallel_2;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;

 public:
  Lambert_conic_conformal_2sp_belgium_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_standard_parallel_1(NAN),
        m_standard_parallel_2(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_conformal_2sp_belgium_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_CONFORMAL_2SP_BELGIUM;
  }
};

/// A Mercator (variant A) projection, alias Mercator (EPSG 9804).
class Mercator_variant_a_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Mercator_variant_a_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Mercator_variant_a_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::MERCATOR_VARIANT_A;
  }
};

/// A Mercator (variant B) projection, alias Mercator (EPSG 9805).
class Mercator_variant_b_srs : public Projected_srs {
 private:
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Mercator_variant_b_srs()
      : m_standard_parallel_1(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Mercator_variant_b_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::MERCATOR_VARIANT_B;
  }
};

/// A Cassini-Soldner projection, alias Cassini (EPSG 9806).
class Cassini_soldner_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Cassini_soldner_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Cassini_soldner_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::CASSINI_SOLDNER;
  }
};

/// A Transverse Mercator projection, alias Gauss-Boaga, Gauss-Krüger
/// or TM (EPSG 9807).
class Transverse_mercator_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Transverse_mercator_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Transverse_mercator_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::TRANSVERSE_MERCATOR;
  }
};

/// A Transverse Mercator (South Orientated) projection, alias
/// Gauss-Conform (EPSG 9808).
class Transverse_mercator_south_orientated_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Transverse_mercator_south_orientated_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Transverse_mercator_south_orientated_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::TRANSVERSE_MERCATOR_SOUTH_ORIENTATED;
  }
};

/// An Oblique stereographic projection, alias Double stereographic
/// (EPSG 9809).
class Oblique_stereographic_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Oblique_stereographic_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Oblique_stereographic_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::OBLIQUE_STEREOGRAPHIC;
  }
};

/// A Polar Stereographic (variant A) projection (EPSG 9810).
class Polar_stereographic_variant_a_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Polar_stereographic_variant_a_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Polar_stereographic_variant_a_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::POLAR_STEREOGRAPHIC_VARIANT_A;
  }
};

/// A New Zealand Map Grid projection (EPSG 9811).
class New_zealand_map_grid_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  New_zealand_map_grid_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new New_zealand_map_grid_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::NEW_ZEALAND_MAP_GRID;
  }
};

/// A Hotine Oblique Mercator (variant A) projection, alias Rectified
/// skew orthomorphic (EPSG 9812).
class Hotine_oblique_mercator_variant_a_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// Longitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8812).
  double m_longitude_of_center;
  /// Direction east of north of the great circle which is the central
  /// line (EPSG 8813).
  double m_azimuth;
  /// Angle at the natural origin through which the natural SRS is
  /// rotated to make the projection north axis parallel with true
  /// north (EPSG 8814).
  double m_rectified_grid_angle;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8815).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Hotine_oblique_mercator_variant_a_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_rectified_grid_angle(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Hotine_oblique_mercator_variant_a_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::HOTINE_OBLIQUE_MERCATOR_VARIANT_A;
  }
};

/// A Laborde Oblique Mercator projection (EPSG 9813).
class Laborde_oblique_mercator_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// Longitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8812).
  double m_longitude_of_center;
  /// Direction east of north of the great circle which is the central
  /// line (EPSG 8813).
  double m_azimuth;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8815).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Laborde_oblique_mercator_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Laborde_oblique_mercator_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LABORDE_OBLIQUE_MERCATOR;
  }
};

/// A Hotine Oblique Mercator (variant B) projection, alias Rectified
/// skew orthomorphic (EPSG 9815).
class Hotine_oblique_mercator_variant_b_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// Longitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8812).
  double m_longitude_of_center;
  /// Direction east of north of the great circle which is the central
  /// line (EPSG 8813).
  double m_azimuth;
  /// Angle at the natural origin through which the natural SRS is
  /// rotated to make the projection north axis parallel with true
  /// north (EPSG 8814).
  double m_rectified_grid_angle;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8815).
  double m_scale_factor;
  /// Easting value assigned to the projection center (EPSG 8816).
  double m_false_easting;
  /// Northing value assigned to the projection center (EPSG 8817).
  double m_false_northing;

 public:
  Hotine_oblique_mercator_variant_b_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_rectified_grid_angle(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Hotine_oblique_mercator_variant_b_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::HOTINE_OBLIQUE_MERCATOR_VARIANT_B;
  }
};

/// A Tunisia Mining Grid projection (EPSG 9816).
class Tunisia_mining_grid_srs : public Projected_srs {
 private:
  /// Latitude of the false origin, at which the false easting and
  /// northing is defined (EPSG 8821).
  double m_latitude_of_origin;
  /// Longitude (central meridian) of the false origin, at which the
  /// false easting and northing is defined (EPSG 8822).
  double m_longitude_of_origin;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;

 public:
  Tunisia_mining_grid_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Tunisia_mining_grid_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::TUNISIA_MINING_GRID;
  }
};

/// A Lambert Conic Near-Conformal projection (EPSG 9817).
class Lambert_conic_near_conformal_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_conic_near_conformal_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_near_conformal_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_NEAR_CONFORMAL;
  }
};

/// An American Polyconic projection, alias Polyconic (EPSG 9818).
class American_polyconic_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  American_polyconic_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new American_polyconic_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::AMERICAN_POLYCONIC;
  }
};

/// A Krovak projection (EPSG 9819).
class Krovak_srs : public Projected_srs {
 private:
  /// Latitude of the point at which the azimuth of the central line
  /// is defined (EPSG 8811).
  double m_latitude_of_center;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_center;
  /// The rotation applied to spherical coordinates, measured on the
  /// conformal sphere in the plane of the meridian of origin (EPSG
  /// 1036).
  double m_azimuth;
  /// Latitude of the parallel on which the projection is based. This
  /// latitude is not geographic, but is defined on the conformal
  /// sphere AFTER its rotation to obtain the oblique aspect of the
  /// projection (EPSG 8818).
  double m_pseudo_standard_parallel_1;
  /// The factor by which the map grid is reduced or enlarged at the
  /// pseudo-standard parallel (EPSG 8819).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Krovak_srs()
      : m_latitude_of_center(NAN),
        m_longitude_of_center(NAN),
        m_azimuth(NAN),
        m_pseudo_standard_parallel_1(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Krovak_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::KROVAK;
  }
};

/// A Lambert Azimuthal Equal Area projection, alias Lambert Equal
/// Area or LAEA (EPSG 9820).
class Lambert_azimuthal_equal_area_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_azimuthal_equal_area_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_azimuthal_equal_area_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_AZIMUTHAL_EQUAL_AREA;
  }
};

/// An Albers Equal Area projection, alias Albers (EPSG 9822).
class Albers_equal_area_srs : public Projected_srs {
 private:
  /// Latitude of the false origin, at which the false easting and
  /// northing is defined (EPSG 8821).
  double m_latitude_of_origin;
  /// Longitude (central meridian) of the false origin, at which the
  /// false easting and northing is defined (EPSG 8822).
  double m_longitude_of_origin;
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Latitude of the second parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8824).
  double m_standard_parallel_2;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;

 public:
  Albers_equal_area_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_standard_parallel_1(NAN),
        m_standard_parallel_2(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Albers_equal_area_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::ALBERS_EQUAL_AREA;
  }
};

/// A Transverse Mercator Zoned Grid System projection (EPSG 9824).
class Transverse_mercator_zoned_grid_system_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// The longitude of the western limit of the first zone (EPSG
  /// 8830).
  double m_initial_longitude;
  /// The longitude width of a zone (EPSG 8831).
  double m_zone_width;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Transverse_mercator_zoned_grid_system_srs()
      : m_latitude_of_origin(NAN),
        m_initial_longitude(NAN),
        m_zone_width(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Transverse_mercator_zoned_grid_system_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::TRANSVERSE_MERCATOR_ZONED_GRID_SYSTEM;
  }
};

/// A Lambert Conic Conformal (West Orientated) projection (EPSG 9826).
class Lambert_conic_conformal_west_orientated_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Multiplier for reducing a distance obtained from a map to the
  /// actual distance on the datum of the map (EPSG 8805).
  double m_scale_factor;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_conic_conformal_west_orientated_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_scale_factor(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_conic_conformal_west_orientated_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CONIC_CONFORMAL_WEST_ORIENTATED;
  }
};

/// A Bonne (South Orientated) projection (EPSG 9828).
class Bonne_south_orientated_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Bonne_south_orientated_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Bonne_south_orientated_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::BONNE_SOUTH_ORIENTATED;
  }
};

/// A Polar Stereographic (variant B) projection (EPSG 9829).
class Polar_stereographic_variant_b_srs : public Projected_srs {
 private:
  /// The parallel on which the scale factor is defined to be unity
  /// (EPSG 8832).
  double m_standard_parallel;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Polar_stereographic_variant_b_srs()
      : m_standard_parallel(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Polar_stereographic_variant_b_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::POLAR_STEREOGRAPHIC_VARIANT_B;
  }
};

/// A Polar Stereographic (variant C) projection (EPSG 9830).
class Polar_stereographic_variant_c_srs : public Projected_srs {
 private:
  /// The parallel on which the scale factor is defined to be unity
  /// (EPSG 8832).
  double m_standard_parallel;
  /// The meridian along which the northing axis increments and also
  /// across which parallels of latitude increment towards the north
  /// pole (EPSG 8833).
  double m_longitude_of_origin;
  /// Easting value assigned to the false origin (EPSG 8826).
  double m_false_easting;
  /// Northing value assigned to the false origin (EPSG 8827).
  double m_false_northing;

 public:
  Polar_stereographic_variant_c_srs()
      : m_standard_parallel(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Polar_stereographic_variant_c_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::POLAR_STEREOGRAPHIC_VARIANT_C;
  }
};

/// A Guam Projection projection (EPSG 9831).
class Guam_projection_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Guam_projection_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Guam_projection_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::GUAM_PROJECTION;
  }
};

/// A Modified Azimuthal Equidistant projection (EPSG 9832).
class Modified_azimuthal_equidistant_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Modified_azimuthal_equidistant_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Modified_azimuthal_equidistant_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::MODIFIED_AZIMUTHAL_EQUIDISTANT;
  }
};

/// A Hyperbolic Cassini-Soldner projection (EPSG 9833).
class Hyperbolic_cassini_soldner_srs : public Projected_srs {
 private:
  /// Latitude chosen as origin of y-coordinates (EPSG 8801).
  double m_latitude_of_origin;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Hyperbolic_cassini_soldner_srs()
      : m_latitude_of_origin(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Hyperbolic_cassini_soldner_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::HYPERBOLIC_CASSINI_SOLDNER;
  }
};

/// A Lambert Cylindrical Equal Area (Spherical) projection (EPSG
/// 9834).
class Lambert_cylindrical_equal_area_spherical_srs : public Projected_srs {
 private:
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_cylindrical_equal_area_spherical_srs()
      : m_standard_parallel_1(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_cylindrical_equal_area_spherical_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CYLINDRICAL_EQUAL_AREA_SPHERICAL;
  }
};

/// A Lambert Cylindrical Equal Area projection (EPSG 9835).
class Lambert_cylindrical_equal_area_srs : public Projected_srs {
 private:
  /// Latitude of the first parallel of intersection between the cone
  /// and the ellipsoid (EPSG 8823).
  double m_standard_parallel_1;
  /// Longitude chosen as origin of x-coordinates (central meridian)
  /// (EPSG 8802).
  double m_longitude_of_origin;
  /// Value added to x-coordinates (EPSG 8806).
  double m_false_easting;
  /// Value added to y-coordinates (EPSG 8807).
  double m_false_northing;

 public:
  Lambert_cylindrical_equal_area_srs()
      : m_standard_parallel_1(NAN),
        m_longitude_of_origin(NAN),
        m_false_easting(NAN),
        m_false_northing(NAN) {}

  virtual Spatial_reference_system *clone() override {
    return new Lambert_cylindrical_equal_area_srs(*this);
  }

  virtual bool init(srid_t srid, wkt_parser::Projected_cs *p) override;

  virtual Projection_type projection_type() override {
    return Projection_type::LAMBERT_CYLINDRICAL_EQUAL_AREA;
  }
};

/**
  Parse an SRS definition WKT string.

  The parser understands WKT as defined by the \<horz cs\>
  specification in OGC 01-009.

  If the string is successfully parsed, a new SRS object will be
  allocated on the heap. The caller is responsible for deleting it.

  If an error occurs, no object is allocated.

  @param[in] srid Spatial reference system ID to use when reporting errors
  @param[in] begin Start of WKT string in UTF-8
  @param[in] end End of WKT string
  @param[out] result Spatial reference system

  @retval true An error has occurred
  @retval false Success
*/
bool parse_wkt(srid_t srid, const char *begin, const char *end,
               Spatial_reference_system **result);

}  // namespace srs
}  // namespace gis

#endif  // SQL_GIS_SRS_SRS_H_INCLUDED
