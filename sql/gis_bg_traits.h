#ifndef GIS_BG_TRAITS_INCLUDED
#define GIS_BG_TRAITS_INCLUDED

/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/


/* This file defines all boost geometry traits. */

#include <boost/static_assert.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/interior_type.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/multi/core/tags.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/concept/requires.hpp>

#include <boost/geometry/geometries/concepts/point_concept.hpp>
#include <boost/geometry/geometries/concepts/linestring_concept.hpp>
#include <boost/geometry/geometries/concepts/polygon_concept.hpp>

#include "spatial.h"

/**
  Helper class for spherical, spherical_equaltorial(i.e. geography)
  coordinate systems. Every Geometry subclass will need one.
*/
class Gis_point_spherical: public Gis_point
{
public:
  explicit Gis_point_spherical(bool is_bg_adapter= true)
    :Gis_point(is_bg_adapter)
  {
  }

  /// @brief Default constructor, no initialization.
  Gis_point_spherical(const void *ptr, size_t nbytes,
                      const Flags_t &flags, srid_t srid)
    :Gis_point(ptr, nbytes, flags, srid)
  {
  }
};


class Gis_multi_point_spherical: public Gis_wkb_vector<Gis_point_spherical>
{
  typedef Gis_wkb_vector<Gis_point_spherical> base_type;
public:
  /**** Boost Geometry Adapter Interface ******/
  explicit Gis_multi_point_spherical(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_multipoint, 0),
               default_srid, is_bg_adapter)
  {}

  Gis_multi_point_spherical(const void *ptr, size_t nbytes,
                            const Flags_t &flags, srid_t srid)
    :base_type(ptr, nbytes, flags, srid, true)
  {
    set_geotype(wkb_multipoint);
  }
};


class Gis_line_string_spherical : public Gis_wkb_vector<Gis_point_spherical>
{
  typedef Gis_wkb_vector<Gis_point_spherical> base_type;
public:
  explicit Gis_line_string_spherical(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_linestring, 0), default_srid, is_bg_adapter)
  {}

  Gis_line_string_spherical(const void *wkb, size_t len,
                            const Flags_t &flags, srid_t srid)
    :base_type(wkb, len, flags, srid, true)
  {
    set_geotype(wkb_linestring);
  }
};


class Gis_polygon_ring_spherical : public Gis_wkb_vector<Gis_point_spherical>
{
  typedef Gis_wkb_vector<Gis_point_spherical> base_type;
public:
  Gis_polygon_ring_spherical(const void *wkb, size_t nbytes,
                             const Flags_t &flags, srid_t srid)
    :base_type(wkb, nbytes, flags, srid, true)
  {
    set_geotype(wkb_linestring);
  }

  Gis_polygon_ring_spherical()
    :base_type(NULL, 0, Flags_t(Geometry::wkb_linestring, 0),
               default_srid, true)
  {}
};


/*
  It's OK to derive from Gis_polygon because Gis_polygon doesn't derive from
  Gis_wkb_vector<T>, and when its rings are accessed, the right ring type will
  be used and hence the right point type will be used via iterator types.
*/
class Gis_polygon_spherical : public Gis_polygon
{
  typedef Gis_polygon base_type;
public:
  typedef Gis_polygon_ring_spherical ring_type;
  typedef Gis_wkb_vector<ring_type> inner_container_type;

  Gis_polygon_spherical(const void *wkb, size_t nbytes,
                        const Flags_t &flags, srid_t srid)
    :base_type(wkb, nbytes, flags, srid)
  {
  }

  explicit Gis_polygon_spherical(bool isbgadapter= true) :base_type(isbgadapter)
  {
  }

  /*
    We have to define them here because the ring_type isn't the same.
    The Gis_polygon_ring_spherical has nothing extra than Gis_polygon_ring,
    so we can do so.
   */
  ring_type &outer() const
  {
    return *(reinterpret_cast<ring_type *>(&base_type::outer()));
  }

  inner_container_type &inners() const
  {
    return *(reinterpret_cast<inner_container_type *>(&base_type::inners()));
  }
};


class Gis_multi_line_string_spherical :
  public Gis_wkb_vector<Gis_line_string_spherical>
{
  typedef Gis_wkb_vector<Gis_line_string_spherical> base_type;
public:
  explicit Gis_multi_line_string_spherical(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_multilinestring, 0),
               default_srid, is_bg_adapter)
  {}

  Gis_multi_line_string_spherical(const void *ptr, size_t nbytes,
                                  const Flags_t &flags, srid_t srid)
    :base_type(ptr, nbytes, flags, srid, true)
  {
    set_geotype(wkb_multilinestring);
  }
};


class Gis_multi_polygon_spherical : public Gis_wkb_vector<Gis_polygon_spherical>
{
  typedef Gis_wkb_vector<Gis_polygon_spherical> base_type;
public:
  explicit Gis_multi_polygon_spherical(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_multipolygon, 0), default_srid, is_bg_adapter)
  {}

  Gis_multi_polygon_spherical(const void *ptr, size_t nbytes,
                              const Flags_t &flags, srid_t srid)
    :base_type(ptr, nbytes, flags, srid, true)
  {
    set_geotype(wkb_multipolygon);
  }
};


// Boost Geometry traits.
namespace boost { namespace geometry
{
namespace traits
{
template<>
struct tag<Gis_point>
{
  typedef boost::geometry::point_tag type;
};

template<>
struct coordinate_type<Gis_point>
{
  typedef double type;
};

template<>
struct coordinate_system<Gis_point>
{
  typedef boost::geometry::cs::cartesian type;
};

template<>
struct dimension<Gis_point>
  : boost::mpl::int_<GEOM_DIM>
{};

template<std::size_t Dimension>
struct access<Gis_point, Dimension>
{
  static inline double get(
    Gis_point const& p)
  {
    return p.get<Dimension>();
  }

  static inline void set(
    Gis_point &p,
    double const& value)
  {
    p.set<Dimension>(value);
  }
};


template<>
struct tag<Gis_point_spherical>
{
  typedef boost::geometry::point_tag type;
};

template<>
struct coordinate_type<Gis_point_spherical>
{
  typedef double type;
};

template<>
struct coordinate_system<Gis_point_spherical>
{
  typedef boost::geometry::cs::spherical_equatorial<
    boost::geometry::degree> type;
};

template<>
struct dimension<Gis_point_spherical>
  : boost::mpl::int_<GEOM_DIM>
{};

template<std::size_t Dimension>
struct access<Gis_point_spherical, Dimension>
{
  static inline double get(
    Gis_point_spherical const& p)
  {
    return p.get<Dimension>();
  }

  static inline void set(
    Gis_point_spherical &p,
    double const& value)
  {
    p.set<Dimension>(value);
  }
};
////////////////////////////////// LINESTRING ////////////////////////////
template<>
struct tag<Gis_line_string>
{
  typedef boost::geometry::linestring_tag type;
};
template<>
struct tag<Gis_line_string_spherical>
{
  typedef boost::geometry::linestring_tag type;
};


////////////////////////////////// POLYGON //////////////////////////////////


template<>
struct tag
<
  Gis_polygon
>
{
  typedef boost::geometry::polygon_tag type;
};


template<>
struct ring_const_type
<
  Gis_polygon
>
{
  typedef Gis_polygon::ring_type const& type;
};


template<>
struct ring_mutable_type
<
  Gis_polygon
>
{
  typedef Gis_polygon::ring_type& type;
};

template<>
struct interior_const_type
<
  Gis_polygon
>
{
  typedef Gis_polygon::inner_container_type const& type;
};


template<>
struct interior_mutable_type
<
  Gis_polygon
>
{
  typedef Gis_polygon::inner_container_type& type;
};

template<>
struct exterior_ring
<
  Gis_polygon
>
{
  typedef Gis_polygon polygon_type;

  static inline polygon_type::ring_type& get(polygon_type& p)
  {
    return p.outer();
  }

  static inline polygon_type::ring_type const& get(
          polygon_type const& p)
  {
    return p.outer();
  }
};

template<>
struct interior_rings
<
  Gis_polygon
>
{
  typedef Gis_polygon polygon_type;

  static inline polygon_type::inner_container_type& get(
          polygon_type& p)
  {
    return p.inners();
  }

  static inline polygon_type::inner_container_type const& get(
          polygon_type const& p)
  {
    return p.inners();
  }
};

template<>
struct tag
<
  Gis_polygon_spherical
>
{
  typedef boost::geometry::polygon_tag type;
};


template<>
struct ring_const_type
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical::ring_type const& type;
};


template<>
struct ring_mutable_type
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical::ring_type& type;
};

template<>
struct interior_const_type
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical::inner_container_type const& type;
};


template<>
struct interior_mutable_type
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical::inner_container_type& type;
};

template<>
struct exterior_ring
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical polygon_type;

  static inline polygon_type::ring_type& get(polygon_type& p)
  {
    return p.outer();
  }

  static inline polygon_type::ring_type const& get(
          polygon_type const& p)
  {
    return p.outer();
  }
};

template<>
struct interior_rings
<
  Gis_polygon_spherical
>
{
  typedef Gis_polygon_spherical polygon_type;

  static inline polygon_type::inner_container_type& get(
          polygon_type& p)
  {
    return p.inners();
  }

  static inline polygon_type::inner_container_type const& get(
          polygon_type const& p)
  {
    return p.inners();
  }
};
////////////////////////////////// RING //////////////////////////////////
template<>
struct point_order<Gis_polygon_ring>
{
  static const order_selector value = counterclockwise;
};

template<>
struct closure<Gis_polygon_ring>
{
  static const closure_selector value = closed;
};

template<>
struct tag<Gis_polygon_ring>
{
  typedef boost::geometry::ring_tag type;
};


template<>
struct point_order<Gis_polygon_ring_spherical>
{
  static const order_selector value = counterclockwise;
};

template<>
struct closure<Gis_polygon_ring_spherical>
{
  static const closure_selector value = closed;
};

template<>
struct tag<Gis_polygon_ring_spherical>
{
  typedef boost::geometry::ring_tag type;
};


////////////////////////////////// MULTI GEOMETRIES /////////////////////////


/////////////////////////////////// multi linestring types /////////////////////
template<>
struct tag< Gis_multi_line_string>
{
  typedef boost::geometry::multi_linestring_tag type;
};

template<>
struct tag< Gis_multi_line_string_spherical>
{
  typedef boost::geometry::multi_linestring_tag type;
};


/////////////////////////////////// multi point types /////////////////////


template<>
struct tag< Gis_multi_point>
{
  typedef boost::geometry::multi_point_tag type;
};

template<>
struct tag< Gis_multi_point_spherical>
{
  typedef boost::geometry::multi_point_tag type;
};


/////////////////////////////////// multi polygon types /////////////////////
template<>
struct tag< Gis_multi_polygon>
{
  typedef boost::geometry::multi_polygon_tag type;
};

template<>
struct tag< Gis_multi_polygon_spherical>
{
  typedef boost::geometry::multi_polygon_tag type;
};


} // namespace traits

} // namespace geometry
} // namespace boost


#endif // !GIS_BG_TRAITS_INCLUDED
