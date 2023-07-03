/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

  Testshapes to use for testing set operations (union, difference,
  symdifference, intersection).
*/

#ifndef UNITTEST_GUNIT_GIS_SETOPS_TESTSHAPES_H_INCLUDED
#define UNITTEST_GUNIT_GIS_SETOPS_TESTSHAPES_H_INCLUDED

template <typename T>
typename T::Multipoint simple_mpt() {
  typename T::Multipoint mpt;
  mpt.push_back(typename T::Point(0.0, 0.0));
  return mpt;
}

template <typename T>
typename T::Linestring simple_ls() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.0, 0.0));
  ls.push_back(typename T::Point(0.1, 0.0));
  return ls;
}

template <typename T>
typename T::Linestring vertical_ls() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.1, 0.0));
  ls.push_back(typename T::Point(0.1, 0.1));
  return ls;
}

template <typename T>
typename T::Linestring simple_ls_2() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.0, 0.1));
  ls.push_back(typename T::Point(0.1, 0.1));
  return ls;
}

template <typename T>
typename T::Linestring offset_simple_ls() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.05, 0.0));
  ls.push_back(typename T::Point(0.15, 0.0));
  return ls;
}

template <typename T>
typename T::Linestring diagonal_ls() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.0, 0.0));
  ls.push_back(typename T::Point(0.1, 0.1));
  return ls;
}

template <typename T>
typename T::Linestring ls_crossing_base_py() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.0, -0.05));
  ls.push_back(typename T::Point(0.0, 0.12));
  return ls;
}

template <typename T>
typename T::Multilinestring ls_crossing_base_py_difference() {
  typename T::Multilinestring mls;
  typename T::Linestring ls1;
  typename T::Linestring ls2;
  ls1.push_back(typename T::Point(0.0, -0.05));
  ls1.push_back(typename T::Point(0.0, 0.0));
  ls2.push_back(typename T::Point(0.0, 0.1));
  ls2.push_back(typename T::Point(0.0, 0.12));
  mls.push_back(ls1);
  mls.push_back(ls2);
  return mls;
}

template <typename T>
typename T::Linestring ls_overlapping_base_py() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.05, 0.05));
  ls.push_back(typename T::Point(-0.05, -0.05));
  return ls;
}

template <typename T>
typename T::Linestring ls_overlapping_base_py_difference() {
  typename T::Linestring ls;
  ls.push_back(typename T::Point(0.0, 0.0));
  ls.push_back(typename T::Point(-0.05, -0.05));
  return ls;
}

template <typename T>
typename T::Multilinestring simple_mls() {
  typename T::Linestring ls = simple_ls<T>();
  typename T::Multilinestring mls;
  mls.push_back(ls);
  return mls;
}

template <typename T>
typename T::Multilinestring diagonal_mls() {
  typename T::Linestring ls = diagonal_ls<T>();
  typename T::Multilinestring mls;
  mls.push_back(ls);
  return mls;
}

template <typename T>
typename T::Polygon base_py() {
  typename T::Polygon py;
  typename T::Linearring exterior;
  exterior.push_back(typename T::Point(0.0, 0.0));
  exterior.push_back(typename T::Point(0.1, 0.0));
  exterior.push_back(typename T::Point(0.1, 0.1));
  exterior.push_back(typename T::Point(0.0, 0.1));
  exterior.push_back(typename T::Point(0.0, 0.0));
  py.push_back(exterior);
  return py;
}

template <typename T>
typename T::Polygon overlapping_py() {
  typename T::Polygon py;
  typename T::Linearring exterior;
  exterior.push_back(typename T::Point(0.1, 0.0));
  exterior.push_back(typename T::Point(0.2, 0.0));
  exterior.push_back(typename T::Point(0.2, 0.1));
  exterior.push_back(typename T::Point(0.1, 0.1));
  exterior.push_back(typename T::Point(0.1, 0.0));
  py.push_back(exterior);
  return py;
}

template <typename T>
typename T::Polygon disjoint_py() {
  typename T::Polygon py;
  typename T::Linearring exterior;
  exterior.push_back(typename T::Point(0.2, -0.2));
  exterior.push_back(typename T::Point(0.2, -0.8));
  exterior.push_back(typename T::Point(0.8, -0.8));
  exterior.push_back(typename T::Point(0.8, -0.2));
  exterior.push_back(typename T::Point(0.2, -0.2));
  py.push_back(exterior);
  return py;
}

template <typename T>
typename T::Polygon base_union_overlapping_py() {
  typename T::Polygon py;
  typename T::Linearring exterior;
  exterior.push_back(typename T::Point(0.0, 0.0));
  exterior.push_back(typename T::Point(0.1, 0.0));
  exterior.push_back(typename T::Point(0.2, 0.0));
  exterior.push_back(typename T::Point(0.2, 0.1));
  exterior.push_back(typename T::Point(0.1, 0.1));
  exterior.push_back(typename T::Point(0.0, 0.1));
  exterior.push_back(typename T::Point(0.0, 0.0));
  py.push_back(exterior);
  return py;
}

template <typename T>
typename T::Multipolygon simple_mpy() {
  typename T::Polygon py = base_py<T>();
  typename T::Multipolygon mpy;
  mpy.push_back(py);
  return mpy;
}

#endif  // UNITTEST_GUNIT_GIS_SETOPS_TESTSHAPES_H_INCLUDED
