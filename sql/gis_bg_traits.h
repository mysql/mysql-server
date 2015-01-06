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

// Helper class for spherical, spherical_equaltorial(i.e. geography)
// coordinate systems. Every Geometry subclass will need one, we for now
// only need two.
class Gis_point_spherical: public Gis_point
{
public:
  typedef Gis_point_spherical point_type;
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

  Gis_point_spherical(const Gis_point_spherical &pt) :Gis_point(pt)
  {
  }

  const Gis_point_spherical &operator=(const Gis_point_spherical &rhs)
  {
    Gis_point::operator=(rhs);
    return rhs;
  }
};


class Gis_multi_point_spherical: public Gis_multi_point
{
public:
  typedef Gis_point_spherical point_type;
  explicit Gis_multi_point_spherical(bool is_bg_adapter= true)
    :Gis_multi_point(is_bg_adapter)
  {
  }


  /// @brief Default constructor, no initialization.
  Gis_multi_point_spherical(const void *ptr, size_t nbytes,
                            const Flags_t &flags, srid_t srid)
    :Gis_multi_point(ptr, nbytes, flags, srid)
  {
  }


  Gis_multi_point_spherical(const Gis_multi_point_spherical &pt)
    :Gis_multi_point(pt)
  {
  }
};


class Gis_line_string_spherical : public Gis_line_string
{
  typedef Gis_line_string base;
  typedef Gis_line_string_spherical self;
public:
  typedef Gis_point_spherical point_type;
  explicit Gis_line_string_spherical(bool is_bg_adapter= true)
    :base(is_bg_adapter)
  {}

  Gis_line_string_spherical(const void *wkb, size_t len,
                            const Flags_t &flags, srid_t srid)
    :base(wkb, len, flags, srid)
  {
  }

  Gis_line_string_spherical(const self &ls) :base(ls)
  {}
};


class Gis_polygon_ring_spherical : public Gis_polygon_ring
{
  typedef Gis_polygon_ring_spherical self;
  typedef Gis_polygon_ring base;
public:
  typedef Gis_point_spherical point_type;
  Gis_polygon_ring_spherical(const void *wkb, size_t nbytes,
                             const Flags_t &flags, srid_t srid)
    :base(wkb, nbytes, flags, srid)
  {
  }

  Gis_polygon_ring_spherical(const self &r) :base(r)
  {}

  Gis_polygon_ring_spherical() :base() {}
};


class Gis_polygon_spherical : public Gis_polygon
{
  typedef Gis_polygon_spherical self;
  typedef Gis_polygon base;
public:
  typedef Gis_point_spherical point_type;
  typedef Gis_polygon_ring_spherical ring_type;
  typedef Gis_wkb_vector<ring_type> inner_container_type;
  Gis_polygon_spherical(const void *wkb, size_t nbytes,
                        const Flags_t &flags, srid_t srid)
    :base(wkb, nbytes, flags, srid)
  {
  }

  explicit Gis_polygon_spherical(bool isbgadapter= true) :base(isbgadapter)
  {
  }

  Gis_polygon_spherical(const self &r) :base(r)
  {
  }

  const self &operator=(const self &rhs)
  {
    base::operator=(rhs);
    return rhs;
  }

  /*
    We have to define them here because the ring_type isn't the same.
    The Gis_polygon_ring_spherical has nothing extra than Gis_polygon_ring,
    so we can do so.
   */
  ring_type &outer() const
  {
    return *(reinterpret_cast<ring_type *>(&base::outer()));
  }

  inner_container_type &inners() const
  {
    return *(reinterpret_cast<inner_container_type *>(&base::inners()));
  }
};


class Gis_multi_line_string_spherical : public Gis_multi_line_string
{
  typedef Gis_multi_line_string_spherical self;
  typedef Gis_multi_line_string base;
public:
  typedef Gis_point_spherical point_type;
  explicit Gis_multi_line_string_spherical(bool is_bg_adapter= true)
    :base(is_bg_adapter)
  {}

  Gis_multi_line_string_spherical(const void *ptr, size_t nbytes,
                                  const Flags_t &flags, srid_t srid)
    :base(ptr, nbytes, flags, srid)
  {
  }

  Gis_multi_line_string_spherical(const self &mls) :base(mls)
  {}
};


class Gis_multi_polygon_spherical : public Gis_multi_polygon
{
  typedef Gis_multi_polygon base;
public:
  typedef Gis_point_spherical point_type;
  explicit Gis_multi_polygon_spherical(bool is_bg_adapter= true)
    :base(is_bg_adapter)
  {}

  Gis_multi_polygon_spherical(const void *ptr, size_t nbytes,
                              const Flags_t &flags, srid_t srid)
    :base(ptr, nbytes, flags, srid)
  {
  }

  Gis_multi_polygon_spherical(const Gis_multi_polygon_spherical &mpl) :base(mpl)
  {}
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

}} // namespace boost::geometry


#endif // !GIS_BG_TRAITS_INCLUDED
