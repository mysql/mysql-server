#ifndef GIS__GEOMETRIES_CS_H_INCLUDED
#define GIS__GEOMETRIES_CS_H_INCLUDED

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

#include "geometries.h"
#include "malloc_allocator.h"

namespace gis {

/// A Cartesian 2d point.
class Cartesian_point : public Point {
 public:
  Cartesian_point() = default;
  Cartesian_point(double x, double y) : Point(x, y) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
};

/// A geographic (ellipsoidal) 2d point.
class Geographic_point : public Point {
 public:
  Geographic_point() = default;
  Geographic_point(double x, double y) : Point(x, y) {}
  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
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
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Cartesian_linestring()
      : m_points(Malloc_allocator<Cartesian_point>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kCartesian;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(Point &&pt) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  virtual void flip() override;

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Point &operator[](std::size_t i) override { return m_points[i]; }
  const Point &operator[](std::size_t i) const override { return m_points[i]; }
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
  typedef decltype(m_points)::iterator iterator;
  typedef decltype(m_points)::const_iterator const_iterator;

  Geographic_linestring()
      : m_points(Malloc_allocator<Geographic_point>(
            key_memory_Geometry_objects_data)) {}

  Coordinate_system coordinate_system() const override {
    return Coordinate_system::kGeographic;
  }
  bool accept(Geometry_visitor *v) override;
  void push_back(Point &&pt) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }
  virtual void flip() override;

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }

  Point &operator[](std::size_t i) override { return m_points[i]; }
  const Point &operator[](std::size_t i) const override { return m_points[i]; }
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
  void push_back(gis::Point &&pt) override {
    Cartesian_linestring::push_back(std::forward<Point &&>(pt));
  }
  bool empty() const override { return Cartesian_linestring::empty(); }
  std::size_t size() const override { return Cartesian_linestring::size(); }
  void flip() override { Cartesian_linestring::flip(); }

  Point &operator[](std::size_t i) override {
    return Cartesian_linestring::operator[](i);
  }
  const Point &operator[](std::size_t i) const override {
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
  void push_back(gis::Point &&pt) override {
    Geographic_linestring::push_back(std::forward<Point &&>(pt));
  }
  bool empty() const override { return Geographic_linestring::empty(); }
  std::size_t size() const override { return Geographic_linestring::size(); }
  void flip() override { Geographic_linestring::flip(); }

  Point &operator[](std::size_t i) override {
    return Geographic_linestring::operator[](i);
  }
  const Point &operator[](std::size_t i) const override {
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
  void push_back(Linearring &&lr) override;
  bool empty() const override;

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
  void push_back(Linearring &&lr) override;
  bool empty() const override;

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
  Cartesian_geometrycollection(Cartesian_geometrycollection &gc);
  Cartesian_geometrycollection(Cartesian_geometrycollection &&gc) noexcept
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {
    m_geometries = std::move(gc.m_geometries);
  }
  ~Cartesian_geometrycollection() {
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_geometries.size(); }

  iterator begin() noexcept { return m_geometries.begin(); }
  const_iterator begin() const noexcept { return m_geometries.begin(); }

  iterator end() noexcept { return m_geometries.end(); }
  const_iterator end() const noexcept { return m_geometries.end(); }
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
  Geographic_geometrycollection(Geographic_geometrycollection &gc);
  Geographic_geometrycollection(Geographic_geometrycollection &&gc) noexcept
      : m_geometries(
            Malloc_allocator<Geometry *>(key_memory_Geometry_objects_data)) {
    m_geometries = std::move(gc.m_geometries);
  }
  ~Geographic_geometrycollection() {
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_geometries.size(); }

  iterator begin() noexcept { return m_geometries.begin(); }
  const_iterator begin() const noexcept { return m_geometries.begin(); }

  iterator end() noexcept { return m_geometries.end(); }
  const_iterator end() const noexcept { return m_geometries.end(); }
};

/// A Cartesian 2d multipoint.
class Cartesian_multipoint : public Multipoint {
 private:
  /// List of points in the collection.
  std::vector<Cartesian_point, Malloc_allocator<Cartesian_point>> m_points;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }
};

/// A geographic (ellipsoidal) 2d multipoint.
class Geographic_multipoint : public Multipoint {
 private:
  /// List of points in the collection.
  std::vector<Geographic_point, Malloc_allocator<Geographic_point>> m_points;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_points.size(); }

  iterator begin() noexcept { return m_points.begin(); }
  const_iterator begin() const noexcept { return m_points.begin(); }

  iterator end() noexcept { return m_points.end(); }
  const_iterator end() const noexcept { return m_points.end(); }
};

/// A Cartesian 2d multilinestring.
class Cartesian_multilinestring : public Multilinestring {
 private:
  /// List of linestrings in the collection.
  std::vector<Cartesian_linestring, Malloc_allocator<Cartesian_linestring>>
      m_linestrings;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_linestrings.size(); }

  iterator begin() noexcept { return m_linestrings.begin(); }
  const_iterator begin() const noexcept { return m_linestrings.begin(); }

  iterator end() noexcept { return m_linestrings.end(); }
  const_iterator end() const noexcept { return m_linestrings.end(); }
};

/// A geographic (ellipsoidal) 2d multilinestring.
class Geographic_multilinestring : public Multilinestring {
 private:
  /// List of linestrings in the collection.
  std::vector<Geographic_linestring, Malloc_allocator<Geographic_linestring>>
      m_linestrings;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_linestrings.size(); }

  iterator begin() noexcept { return m_linestrings.begin(); }
  const_iterator begin() const noexcept { return m_linestrings.begin(); }

  iterator end() noexcept { return m_linestrings.end(); }
  const_iterator end() const noexcept { return m_linestrings.end(); }
};

/// A Cartesian 2d multipolygon.
class Cartesian_multipolygon : public Multipolygon {
 private:
  /// List of polygons in the collection.
  std::vector<Cartesian_polygon, Malloc_allocator<Cartesian_polygon>>
      m_polygons;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_polygons.size(); }

  iterator begin() noexcept { return m_polygons.begin(); }
  const_iterator begin() const noexcept { return m_polygons.begin(); }

  iterator end() noexcept { return m_polygons.end(); }
  const_iterator end() const noexcept { return m_polygons.end(); }
};

/// A geographic (ellipsoidal) 2d multipolygon.
class Geographic_multipolygon : public Multipolygon {
 private:
  /// List of polygons in the collection.
  std::vector<Geographic_polygon, Malloc_allocator<Geographic_polygon>>
      m_polygons;

 public:
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
  void push_back(Geometry &&g) override;
  bool empty() const override;
  std::size_t size() const override { return m_polygons.size(); }

  iterator begin() noexcept { return m_polygons.begin(); }
  const_iterator begin() const noexcept { return m_polygons.begin(); }

  iterator end() noexcept { return m_polygons.end(); }
  const_iterator end() const noexcept { return m_polygons.end(); }
};

}  // namespace gis

#endif  // GIS__GEOMETRIES_CS_H_INCLUDED
