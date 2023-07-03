#ifndef SQL_GIS_GEOMETRIES_CS_H_INCLUDED
#define SQL_GIS_GEOMETRIES_CS_H_INCLUDED

// Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

/// @file
///
/// This file declares the coordinate system specific subclasses of
/// the geometry class hierarchy. The rest of the hierarchy is defined
/// in geometries.h.
///
/// For most of the server, including geometries.h should be
/// enough. This header is only needed if the code needs to access
/// coordinate system specific members.
///
/// @see geometries.h

#include <vector>

#include "sql/gis/geometries.h"
#include "sql/malloc_allocator.h"

namespace gis {

/// A Cartesian 2d point.
class Cartesian_point : public Point {
 public:
  Cartesian_point() = default;
  Cartesian_point(double x, double y) : Point(x, y) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  Cartesian_point *clone() const override { return new Cartesian_point(*this); }
};

/// A geographic (ellipsoidal) 2d point.
class Geographic_point : public Point {
 public:
  Geographic_point() = default;
  Geographic_point(double x, double y) : Point(x, y) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  Geographic_point *clone() const override {
    return new Geographic_point(*this);
  }
};

/// A Cartesian 2d linestring.
class Cartesian_linestring : public Linestring {
 protected:
  /// String of points constituting the linestring.
  ///
  /// The line starts in the first point, goes through all intermediate points,
  /// and ends in the last point.
  std::vector<Cartesian_point, Malloc_allocator<Cartesian_point>> m_points;

 public:
  typedef decltype(m_points)::value_type value_type;
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Cartesian_linestring()
      : m_points(Malloc_allocator<Cartesian_point>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(const Point &pt) override;
  void push_back(Point &&pt) override;
  void pop_front() override { m_points.erase(m_points.begin()); }
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  void resize(std::size_t count) { m_points.resize(count); }
  void clear() noexcept override { m_points.clear(); }

  Cartesian_linestring *clone() const override {
    return new Cartesian_linestring(*this);
  }

  Cartesian_point &back() override { return m_points.back(); }
  const Cartesian_point &back() const override { return m_points.back(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Cartesian_point &front() override { return m_points.front(); }
  const Cartesian_point &front() const override { return m_points.front(); }

  Cartesian_point &operator[](std::size_t i) override { return m_points[i]; }
  const Cartesian_point &operator[](std::size_t i) const override {
    return m_points[i];
  }
};

/// A geographic (ellipsoidal) 2d linestring.
///
/// The linestring follows the geodetic between each pair of points.
class Geographic_linestring : public Linestring {
 protected:
  /// String of points constituting the linestring.
  ///
  /// The line starts in the first point, goes through all intermediate points,
  /// and ends in the last point.
  std::vector<Geographic_point, Malloc_allocator<Geographic_point>> m_points;

 public:
  typedef decltype(m_points)::value_type value_type;
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Geographic_linestring()
      : m_points(Malloc_allocator<Geographic_point>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(const Point &pt) override;
  void push_back(Point &&pt) override;
  void pop_front() override { m_points.erase(m_points.begin()); }
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  void resize(std::size_t count) { m_points.resize(count); }
  void clear() noexcept override { m_points.clear(); }

  Geographic_linestring *clone() const override {
    return new Geographic_linestring(*this);
  }

  Geographic_point &back() override { return m_points.back(); }
  const Geographic_point &back() const override { return m_points.back(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Geographic_point &front() override { return m_points.front(); }
  const Geographic_point &front() const override { return m_points.front(); }

  Geographic_point &operator[](std::size_t i) override { return m_points[i]; }
  const Geographic_point &operator[](std::size_t i) const override {
    return m_points[i];
  }
};

/// A Cartesian 2d linear ring.
class Cartesian_linearring : public Cartesian_linestring, public Linearring {
 public:
  Geometry_type type() const override { return Linearring::type(); }
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override { return Cartesian_linestring::is_empty(); }
  void push_back(const gis::Point &pt) override {
    Cartesian_linestring::push_back(pt);
  }
  void push_back(gis::Point &&pt) override {
    Cartesian_linestring::push_back(std::forward<Point &&>(pt));
  }
  void pop_front() override { Cartesian_linestring::pop_front(); }
  bool empty() const override { return Cartesian_linestring::empty(); }
  std::size_t size() const override { return Cartesian_linestring::size(); }
  void clear() noexcept override { Cartesian_linestring::clear(); }

  /// This implementation of clone() uses a broader return type than
  /// other implementations. This is due to the restraint in some compilers,
  /// such as cl.exe, that overriding functions with ambiguous bases must have
  /// covariant return types.
  Cartesian_linestring *clone() const override {
    return new Cartesian_linearring(*this);
  }

  Cartesian_point &back() override { return Cartesian_linestring::back(); }
  const Cartesian_point &back() const override {
    return Cartesian_linestring::back();
  }

  Cartesian_point &front() override { return Cartesian_linestring::front(); }
  const Cartesian_point &front() const override {
    return Cartesian_linestring::front();
  }

  Cartesian_point &operator[](std::size_t i) override {
    return Cartesian_linestring::operator[](i);
  }
  const Cartesian_point &operator[](std::size_t i) const override {
    return Cartesian_linestring::operator[](i);
  }
};

/// A geographic (ellipsoidal) 2d linear ring.
class Geographic_linearring : public Geographic_linestring, public Linearring {
 public:
  Geometry_type type() const override { return Linearring::type(); }
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override { return Geographic_linestring::is_empty(); }
  void push_back(const gis::Point &pt) override {
    Geographic_linestring::push_back(pt);
  }
  void push_back(gis::Point &&pt) override {
    Geographic_linestring::push_back(std::forward<Point &&>(pt));
  }
  void pop_front() override { Geographic_linestring::pop_front(); }
  bool empty() const override { return Geographic_linestring::empty(); }
  std::size_t size() const override { return Geographic_linestring::size(); }
  void clear() noexcept override { Geographic_linestring::clear(); }

  /// This implementation of clone() uses a broader return type than
  /// other implementations. This is due to the restraint in some compilers,
  /// such as cl.exe, that overriding functions with ambiguous bases must have
  /// covariant return types.
  Geographic_linestring *clone() const override {
    return new Geographic_linearring(*this);
  }

  Geographic_point &back() override { return Geographic_linestring::back(); }
  const Geographic_point &back() const override {
    return Geographic_linestring::back();
  }

  Geographic_point &front() override { return Geographic_linestring::front(); }
  const Geographic_point &front() const override {
    return Geographic_linestring::front();
  }

  Geographic_point &operator[](std::size_t i) override {
    return Geographic_linestring::operator[](i);
  }
  const Geographic_point &operator[](std::size_t i) const override {
    return Geographic_linestring::operator[](i);
  }
};

/// A Cartesian 2d polygon.
class Cartesian_polygon : public Polygon {
 private:
  /// Exterior ring.
  Cartesian_linearring m_exterior_ring;

  /// Interior rings (holes).
  std::vector<Cartesian_linearring, Malloc_allocator<Cartesian_linearring>>
      m_interior_rings;

 public:
  Cartesian_polygon()
      : m_interior_rings(Malloc_allocator<Cartesian_linearring>(
            key_memory_Geometry_objects_data)) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(const Linearring &lr) override;
  void push_back(Linearring &&lr) override;
  bool empty() const override;

  Cartesian_polygon *clone() const override {
    return new Cartesian_polygon(*this);
  }

  /// Get list of interior rings.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The list of interior rings
  decltype(m_interior_rings) &interior_rings();

  /// Get list of interior rings.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The list of interior rings
  decltype(m_interior_rings) const &const_interior_rings() const;

  std::size_t size() const override;

  /// Get the exterior ring.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The exterior ring.
  Cartesian_linearring &cartesian_exterior_ring() const;
  Linearring &exterior_ring() override { return m_exterior_ring; }

  Linearring &interior_ring(std::size_t n) override;
};

/// A geographic (ellipsoidal) 2d polygon.
class Geographic_polygon : public Polygon {
 private:
  /// Exterior ring.
  Geographic_linearring m_exterior_ring;

  /// Interior rings (holes).
  std::vector<Geographic_linearring, Malloc_allocator<Geographic_linearring>>
      m_interior_rings;

 public:
  Geographic_polygon()
      : m_interior_rings(Malloc_allocator<Geographic_linearring>(
            key_memory_Geometry_objects_data)) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(const Linearring &lr) override;
  void push_back(Linearring &&lr) override;
  bool empty() const override;

  Geographic_polygon *clone() const override {
    return new Geographic_polygon(*this);
  }

  /// Get list of interior rings.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The list of interior rings
  decltype(m_interior_rings) &interior_rings();

  /// Get list of interior rings.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The list of interior rings
  decltype(m_interior_rings) const &const_interior_rings() const;

  std::size_t size() const override;

  /// Get the exterior ring.
  ///
  /// This function is used by the interface to Boost.Geometry.
  ///
  /// @return The exterior ring.
  Geographic_linearring &geographic_exterior_ring() const;
  Linearring &exterior_ring() override { return m_exterior_ring; }

  Linearring &interior_ring(std::size_t n) override;
};

/// A Cartesian 2d geometry collection.
class Cartesian_geometrycollection : public Geometrycollection {
 private:
  /// List of geometries in the collection.
  std::vector<Geometry *, Malloc_allocator<Geometry *>> m_geometries;

 public:
  typedef decltype(m_geometries)::iterator iterator;
  typedef decltype(m_geometries)::const_iterator const_iterator;

  Cartesian_geometrycollection()
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {}
  Cartesian_geometrycollection(const Cartesian_geometrycollection &gc);
  Cartesian_geometrycollection(Cartesian_geometrycollection &&gc) noexcept
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {
    m_geometries = std::move(gc.m_geometries);
  }
  ~Cartesian_geometrycollection() override {
    for (Geometry *g : m_geometries) {
      delete g;
    }
  }
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto g : m_geometries) {
      if (!g->is_empty()) return false;
    }
    return true;
  }
  void pop_front() override {
    delete *m_geometries.begin();
    m_geometries.erase(m_geometries.begin());
  }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_geometries.size(); }
  void resize(std::size_t count) override { m_geometries.resize(count); }
  void clear() noexcept override { m_geometries.clear(); }

  iterator begin() noexcept { return m_geometries.begin(); }
  const_iterator begin() const noexcept { return m_geometries.begin(); }

  iterator end() noexcept { return m_geometries.end(); }
  const_iterator end() const noexcept { return m_geometries.end(); }

  Geometry &front() override { return *m_geometries.front(); }
  const Geometry &front() const override { return *m_geometries.front(); }

  Geometry &operator[](std::size_t i) override { return *m_geometries[i]; }
  const Geometry &operator[](std::size_t i) const override {
    return *m_geometries[i];
  }
  Cartesian_geometrycollection *clone() const override {
    return new Cartesian_geometrycollection(*this);
  }
};

/// A geographic (ellipsoidal) 2d geometry collection.
class Geographic_geometrycollection : public Geometrycollection {
 private:
  /// List of geometries in the collection.
  std::vector<Geometry *, Malloc_allocator<Geometry *>> m_geometries;

 public:
  typedef decltype(m_geometries)::iterator iterator;
  typedef decltype(m_geometries)::const_iterator const_iterator;

  Geographic_geometrycollection()
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {}
  Geographic_geometrycollection(const Geographic_geometrycollection &gc);
  Geographic_geometrycollection(Geographic_geometrycollection &&gc) noexcept
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {
    m_geometries = std::move(gc.m_geometries);
  }
  ~Geographic_geometrycollection() override {
    for (Geometry *g : m_geometries) {
      delete g;
    }
  }
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto g : m_geometries) {
      if (!g->is_empty()) return false;
    }
    return true;
  }
  void pop_front() override {
    delete *m_geometries.begin();
    m_geometries.erase(m_geometries.begin());
  }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_geometries.size(); }
  void resize(std::size_t count) override { m_geometries.resize(count); }
  void clear() noexcept override { m_geometries.clear(); }

  iterator begin() noexcept { return m_geometries.begin(); }
  const_iterator begin() const noexcept { return m_geometries.begin(); }

  iterator end() noexcept { return m_geometries.end(); }
  const_iterator end() const noexcept { return m_geometries.end(); }

  Geometry &front() override { return *m_geometries.front(); }
  const Geometry &front() const override { return *m_geometries.front(); }

  Geometry &operator[](std::size_t i) override { return *m_geometries[i]; }
  const Geometry &operator[](std::size_t i) const override {
    return *m_geometries[i];
  }
  Geographic_geometrycollection *clone() const override {
    return new Geographic_geometrycollection(*this);
  }
};

/// A Cartesian 2d multipoint.
class Cartesian_multipoint : public Multipoint {
 private:
  /// List of points in the collection.
  std::vector<Cartesian_point, Malloc_allocator<Cartesian_point>> m_points;

 public:
  typedef decltype(m_points)::value_type value_type;
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Cartesian_multipoint()
      : m_points(Malloc_allocator<Cartesian_point>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &pt : m_points) {
      if (!pt.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_points.erase(m_points.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  void resize(std::size_t count) override { m_points.resize(count); }
  void clear() noexcept override { m_points.clear(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Cartesian_point &front() override { return m_points.front(); }
  const Cartesian_point &front() const override { return m_points.front(); }

  Cartesian_point &operator[](std::size_t i) override { return m_points[i]; }
  const Cartesian_point &operator[](std::size_t i) const override {
    return m_points[i];
  }
  Cartesian_multipoint *clone() const override {
    return new Cartesian_multipoint(*this);
  }
};

/// A geographic (ellipsoidal) 2d multipoint.
class Geographic_multipoint : public Multipoint {
 private:
  /// List of points in the collection.
  std::vector<Geographic_point, Malloc_allocator<Geographic_point>> m_points;

 public:
  typedef decltype(m_points)::value_type value_type;
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Geographic_multipoint()
      : m_points(Malloc_allocator<Geographic_point>(
            key_memory_Geometry_objects_data)) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &pt : m_points) {
      if (!pt.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_points.erase(m_points.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  void resize(std::size_t count) override { m_points.resize(count); }
  void clear() noexcept override { m_points.clear(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Geographic_point &front() override { return m_points.front(); }
  const Geographic_point &front() const override { return m_points.front(); }

  Geographic_point &operator[](std::size_t i) override { return m_points[i]; }
  const Geographic_point &operator[](std::size_t i) const override {
    return m_points[i];
  }
  Geographic_multipoint *clone() const override {
    return new Geographic_multipoint(*this);
  }
};

/// A Cartesian 2d multilinestring.
class Cartesian_multilinestring : public Multilinestring {
 private:
  /// List of linestrings in the collection.
  std::vector<Cartesian_linestring, Malloc_allocator<Cartesian_linestring>>
      m_linestrings;

 public:
  typedef decltype(m_linestrings)::value_type value_type;
  typedef decltype(m_linestrings)::iterator iterator;
  typedef decltype(m_linestrings)::const_iterator const_iterator;

  Cartesian_multilinestring()
      : m_linestrings(Malloc_allocator<Cartesian_linestring>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &ls : m_linestrings) {
      if (!ls.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_linestrings.erase(m_linestrings.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_linestrings.size(); }
  void resize(std::size_t count) override { m_linestrings.resize(count); }
  void clear() noexcept override { m_linestrings.clear(); }

  Cartesian_linestring &back() { return m_linestrings.back(); }
  const Cartesian_linestring &back() const { return m_linestrings.back(); }

  iterator begin() noexcept { return m_linestrings.begin(); }
  const_iterator begin() const noexcept { return m_linestrings.begin(); }

  iterator end() noexcept { return m_linestrings.end(); }
  const_iterator end() const noexcept { return m_linestrings.end(); }

  Cartesian_linestring &front() override { return m_linestrings.front(); }
  const Cartesian_linestring &front() const override {
    return m_linestrings.front();
  }

  Cartesian_linestring &operator[](std::size_t i) override {
    return m_linestrings[i];
  }
  const Geometry &operator[](std::size_t i) const override {
    return m_linestrings[i];
  }
  Cartesian_multilinestring *clone() const override {
    return new Cartesian_multilinestring(*this);
  }
};

/// A geographic (ellipsoidal) 2d multilinestring.
class Geographic_multilinestring : public Multilinestring {
 private:
  /// List of linestrings in the collection.
  std::vector<Geographic_linestring, Malloc_allocator<Geographic_linestring>>
      m_linestrings;

 public:
  typedef decltype(m_linestrings)::value_type value_type;
  typedef decltype(m_linestrings)::iterator iterator;
  typedef decltype(m_linestrings)::const_iterator const_iterator;

  Geographic_multilinestring()
      : m_linestrings(Malloc_allocator<Geographic_linestring>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &ls : m_linestrings) {
      if (!ls.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_linestrings.erase(m_linestrings.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_linestrings.size(); }
  void resize(std::size_t count) override { m_linestrings.resize(count); }
  void clear() noexcept override { m_linestrings.clear(); }

  Geographic_linestring &back() { return m_linestrings.back(); }
  const Geographic_linestring &back() const { return m_linestrings.back(); }

  iterator begin() noexcept { return m_linestrings.begin(); }
  const_iterator begin() const noexcept { return m_linestrings.begin(); }

  iterator end() noexcept { return m_linestrings.end(); }
  const_iterator end() const noexcept { return m_linestrings.end(); }

  Geographic_linestring &front() override { return m_linestrings.front(); }
  const Geographic_linestring &front() const override {
    return m_linestrings.front();
  }

  Geographic_linestring &operator[](std::size_t i) override {
    return m_linestrings[i];
  }
  const Geometry &operator[](std::size_t i) const override {
    return m_linestrings[i];
  }
  Geographic_multilinestring *clone() const override {
    return new Geographic_multilinestring(*this);
  }
};

/// A Cartesian 2d multipolygon.
class Cartesian_multipolygon : public Multipolygon {
 private:
  /// List of polygons in the collection.
  std::vector<Cartesian_polygon, Malloc_allocator<Cartesian_polygon>>
      m_polygons;

 public:
  typedef decltype(m_polygons)::value_type value_type;
  typedef decltype(m_polygons)::iterator iterator;
  typedef decltype(m_polygons)::const_iterator const_iterator;

  Cartesian_multipolygon()
      : m_polygons(Malloc_allocator<Cartesian_polygon>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &py : m_polygons) {
      if (!py.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_polygons.erase(m_polygons.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_polygons.size(); }
  void resize(std::size_t count) override { m_polygons.resize(count); }
  void clear() noexcept override { m_polygons.clear(); }

  iterator begin() noexcept { return m_polygons.begin(); }
  const_iterator begin() const noexcept { return m_polygons.begin(); }

  iterator end() noexcept { return m_polygons.end(); }
  const_iterator end() const noexcept { return m_polygons.end(); }

  Cartesian_polygon &front() override { return m_polygons.front(); }
  const Cartesian_polygon &front() const override { return m_polygons.front(); }

  Cartesian_polygon &operator[](std::size_t i) override {
    return m_polygons[i];
  }
  const Geometry &operator[](std::size_t i) const override {
    return m_polygons[i];
  }
  Cartesian_multipolygon *clone() const override {
    return new Cartesian_multipolygon(*this);
  }
};

/// A geographic (ellipsoidal) 2d multipolygon.
class Geographic_multipolygon : public Multipolygon {
 private:
  /// List of polygons in the collection.
  std::vector<Geographic_polygon, Malloc_allocator<Geographic_polygon>>
      m_polygons;

 public:
  typedef decltype(m_polygons)::value_type value_type;
  typedef decltype(m_polygons)::iterator iterator;
  typedef decltype(m_polygons)::const_iterator const_iterator;

  Geographic_multipolygon()
      : m_polygons(Malloc_allocator<Geographic_polygon>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  bool is_empty() const override {
    for (const auto &py : m_polygons) {
      if (!py.is_empty()) return false;
    }
    return true;
  }
  void pop_front() override { m_polygons.erase(m_polygons.begin()); }
  void push_back(const Geometry &g) override;
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_polygons.size(); }
  void resize(std::size_t count) override { m_polygons.resize(count); }
  void clear() noexcept override { m_polygons.clear(); }

  iterator begin() noexcept { return m_polygons.begin(); }
  const_iterator begin() const noexcept { return m_polygons.begin(); }

  iterator end() noexcept { return m_polygons.end(); }
  const_iterator end() const noexcept { return m_polygons.end(); }

  Geographic_polygon &front() override { return m_polygons.front(); }
  const Geographic_polygon &front() const override {
    return m_polygons.front();
  }

  Geographic_polygon &operator[](std::size_t i) override {
    return m_polygons[i];
  }
  const Geometry &operator[](std::size_t i) const override {
    return m_polygons[i];
  }
  Geographic_multipolygon *clone() const override {
    return new Geographic_multipolygon(*this);
  }
};

}  // namespace gis

#endif  // SQL_GIS_GEOMETRIES_CS_H_INCLUDED
