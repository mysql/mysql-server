/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "item_geofunc_internal.h"

// adaptors
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>


struct Rtree_value_maker
{
  typedef std::pair<BG_box, size_t> result_type;
  template<typename  T>
  result_type operator()(T const &v) const
  {
    BG_box box;
    make_bg_box(v.value(), &box);
    return result_type(box, v.index());
  }
};


struct Is_rtree_box_valid
{
  typedef std::pair<BG_box, size_t> Rtree_entry;
  bool operator()(Rtree_entry const& re) const
  {
    return is_box_valid(re.first);
  }
};


void
make_rtree(const BG_geometry_collection::Geometry_list &gl,
           Rtree_index *rtree)
{
  Rtree_index temp_rtree(gl | boost::adaptors::indexed() |
                         boost::adaptors::transformed(Rtree_value_maker()) |
                         boost::adaptors::filtered(Is_rtree_box_valid()));

  rtree->swap(temp_rtree);
}


/*
  A functor to make an rtree value entry from an array element of
  Boost.Geometry model type.
 */
struct Rtree_value_maker_bggeom
{
  typedef std::pair<BG_box, size_t> result_type;
  template<typename  T>
  result_type operator()(T const &v) const
  {
    BG_box box;
    boost::geometry::envelope(v.value(), box);

    return result_type(box, v.index());
  }
};


template <typename MultiGeometry>
void
make_rtree_bggeom(const MultiGeometry &mg,
                  Rtree_index *rtree)
{
  Rtree_index temp_rtree(mg | boost::adaptors::indexed() |
                         boost::adaptors::
                         transformed(Rtree_value_maker_bggeom()) |
                         boost::adaptors::filtered(Is_rtree_box_valid()));

  rtree->swap(temp_rtree);
}


// Explicit template instantiation
template
void make_rtree_bggeom<Gis_multi_line_string>(const Gis_multi_line_string&,
                                              Rtree_index*);
template
void make_rtree_bggeom<Gis_multi_point>(const Gis_multi_point&,
                                        Rtree_index*);

template
void make_rtree_bggeom<Gis_multi_polygon>(const Gis_multi_polygon&,
                                          Rtree_index *);
