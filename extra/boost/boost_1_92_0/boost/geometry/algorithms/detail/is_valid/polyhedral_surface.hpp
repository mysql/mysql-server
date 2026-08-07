// Boost.Geometry

// Copyright (c) 2025 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_POLYHEDRAL_SURFACE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_POLYHEDRAL_SURFACE_HPP

#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/detail/is_valid/interface.hpp>
#include <boost/geometry/algorithms/detail/is_valid/polygon.hpp>
#include <boost/geometry/algorithms/detail/calculate_point_order.hpp>
#include <boost/geometry/algorithms/distance.hpp> //needed for rtree
#include <boost/geometry/index/predicates.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/iterators/point_reverse_iterator.hpp>

#include <vector>
#include <stack>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace is_valid
{

struct polygon_frontier_type
{
    polygon_frontier_type(std::size_t size)
        : m_stack(), m_visited(size, false)
    {}

    void push(std::size_t index)
    {
        if (!m_visited[index])
        {
            m_stack.push(index);
            m_visited[index] = true;
        }
    }
    bool empty()
    {
        return m_stack.empty();
    }
    std::size_t top()
    {
        return m_stack.top();
    }
    void pop()
    {
        m_stack.pop();
    }
private :
    std::stack<std::size_t> m_stack;
    std::vector<bool> m_visited;
};

// Implementation of the polygon segment intersection algorithm
// This function checks if a segment (s1, s2) intersects with a 2D polygon in 3D space.
// proj1 and proj2 are the indices of the coordinate axes used to determine the sign of three points
// of the polygon after projection.
// It returns:
// - 0 if the segment does not intersect the polygon
// - 1 if the segment intersects the polygon in the interior
// - 2 if the segment intersects the polygon at a vertex
// - 3 if the segment intersects the polygon at an edge
// Reference: R.J.Segura, F.R.Feito - An algorithm for determining intersection segment-polygon in
// 3D, Computers & Graphics 1998
template <typename Point, typename Polygon, typename Strategy>
int polygon_segment_intersection(Point const& s1,
                                 Point const& s2,
                                 Polygon const& polygon,
                                 int proj1,
                                 int proj2,
                                 Strategy const& strategy)
{
    BOOST_GEOMETRY_ASSERT(geometry::dimension<Point>::value == 3);
    BOOST_GEOMETRY_ASSERT(geometry::dimension<Polygon>::value == 3);

    auto const pit_a = points_begin(polygon);
    auto const& p_a = *pit_a;

    int cut = 0;

    for (auto pit = boost::next(pit_a); pit != boost::prior(points_end(polygon)); ++pit)
    {
        auto const& p_b = *pit;
        auto const& p_c = *boost::next(pit);

        int const sign = strategy.side().apply(p_a, p_b, p_c, proj1, proj2);

        int const sign1 = strategy.side3().apply(s1, p_a, s2, p_b);
        int const sign2 = strategy.side3().apply(s1, p_c, p_b, s2);
        int const sign3 = strategy.side3().apply(s1, p_a, p_c, s2);

        // Insects at p_b or p_a or p_c -> vertex
        if ((sign1 == 0 && sign2 == 0) || (sign1 == 0 && sign3 == 0) || (sign2 == 0 && sign3 == 0))
        {
            return 2;
        }

        // Intersects at edge p_a, p_c -> internal edge
        if (sign1 == 0 && sign2 == sign3) cut += sign;

        // Intersects at edge p_b, p_c -> external edge
        if (sign2 == 0 && sign1 == sign3) return 3;

        // Intersects at edge p_a, p_b -> internal edge
        if (sign3 == 0 && sign1 == sign2) cut += sign;

        // Intersects inside p_a, p_b, p_c -> inside
        if (sign1 == sign2 && sign2 == sign3) cut += 2 * sign;
    }

    return (cut == 2) ? 1 : 0;
}

template <typename PolyhedralSurface>
struct valid_intersection
{
    valid_intersection(std::size_t num_of_polygons)
        : m_polygon_projections(num_of_polygons)
    {}

    using coordinate_type = typename coordinate_type<PolyhedralSurface>::type;
    using polygon_3d = typename boost::range_value<PolyhedralSurface>::type;
    using ring_3d = typename boost::geometry::ring_type<polygon_3d>::type;
    using point_3d = typename boost::geometry::point_type<ring_3d>::type;
    using segment_3d = typename boost::geometry::model::segment<point_3d>;
    using box_3d = typename boost::geometry::model::box<point_3d>;

    using point_2d = boost::geometry::model::point
        <coordinate_type, 2, boost::geometry::cs::cartesian>;
    using linestring_2d = typename boost::geometry::model::linestring<point_2d>;
    using polygon_2d = typename std::conditional
        <
            boost::geometry::point_order<polygon_3d>::value == boost::geometry::counterclockwise,
            typename boost::geometry::model::polygon<point_2d, true>,
            typename boost::geometry::model::polygon<point_2d, false>
        >::type;
    using ring_2d = typename boost::geometry::ring_type<polygon_2d>::type;

private:

    enum class intersection_type
    {
        none,
        valid,
        invalid
    };

    struct polygon_projection
    {
        polygon_2d projected_polygon;
        int p1_idx;
        int p2_idx;
        int p3_idx;
        int proj1;
        int proj2;
        int dir;
    };

    template <typename Strategy>
    static std::tuple<int, int, int> affinely_independent(point_3d const& p1,
                                                          point_3d const& p2,
                                                          point_3d const& p3,
                                                          Strategy const& strategy)
    {
        int side = strategy.side().apply(p1, p2, p3, 0, 1);
        if (side != 0) return {0, 1, side};

        side = strategy.side().apply(p1, p2, p3, 0, 2);
        if (side != 0) return {0, 2, side};

        side = strategy.side().apply(p1, p2, p3, 1, 2);
        if (side != 0) return {1, 2, side};

        return {-1, -1, 0}; // All points are collinear
    }

    template <typename Strategy>
    static std::tuple<int, int, int> find_non_collinear_points(polygon_3d const& polygon,
                                                               int& p1_index,
                                                               int& p2_index,
                                                               int& p3_index,
                                                               Strategy const& strategy)
    {
        for (auto pit1 = points_begin(polygon); pit1 != points_end(polygon); ++pit1)
        {
            for (auto pit2 = boost::next(pit1); pit2 != points_end(polygon); ++pit2)
            {
                for (auto pit3 = boost::next(pit2); pit3 != points_end(polygon); ++pit3)
                {
                    // A set of 3 independent points is sufficient to construct a plane.
                    auto res = affinely_independent(*pit1, *pit2, *pit3, strategy);
                    if (std::get<0>(res) != -1)
                    {
                        p1_index = std::distance(points_begin(polygon), pit1);
                        p2_index = std::distance(points_begin(polygon), pit2);
                        p3_index = std::distance(points_begin(polygon), pit3);
                        return res;
                    }
                }
            }
        }
        return {-1, -1, 0}; // All points are collinear
    }

    template <typename VisitPolicy>
    static bool has_correct_intersection_orientation(polygon_2d const& polygon,
                                                     point_2d const& segment_point1,
                                                     point_2d const& segment_point2,
                                                     VisitPolicy& visitor)
    {
        if (boost::geometry::equals(segment_point1, segment_point2))
        {
            visitor.template apply<failure_duplicate_points>();
            return false;
        }

        boost::geometry::closing_iterator<ring_2d> it(boost::geometry::exterior_ring(polygon));
        auto begin = it;
        boost::geometry::closing_iterator<ring_2d> end(boost::geometry::exterior_ring(polygon), true);

        for (; it != end - 1; ++it)
        {
            if (boost::geometry::equals(*it, segment_point1))
            {
                auto prev = (it == begin) ? end - 1 : boost::prior(it);

                if (boost::geometry::equals(segment_point2, *prev))
                {
                    // valid intersection and consistent orientation
                    return true;
                }

                auto next = (it == end) ? begin : boost::next(it);

                if (boost::geometry::equals(segment_point2, *next))
                {
                    // valid intersection but inconsistent orientation
                    visitor.template apply<failure_inconsistent_orientation>();
                    return false;
                }
            }
        }

        visitor.template apply<failure_invalid_intersection>();

        return false;
    }

    point_2d project_point_to_coordinate_plane(point_3d const& point, int proj1, int proj2)
    {
        if (proj1 == 1 && proj2 == 2) return point_2d{geometry::get<1>(point), geometry::get<2>(point)};
        if (proj1 == 0 && proj2 == 2) return point_2d{geometry::get<0>(point), geometry::get<2>(point)};
        if (proj1 == 0 && proj2 == 1) return point_2d{geometry::get<0>(point), geometry::get<1>(point)};
        return point_2d{};
    }

    // Check if a segment intersects a polygon in 3D space
    template <typename VisitPolicy, typename Strategy>
    intersection_type check_polygon_segment_intersection(point_3d const& segment_point1,
                                                         point_3d const& segment_point2,
                                                         polygon_3d const& polygon,
                                                         std::size_t const polygon_index,
                                                         VisitPolicy& visitor,
                                                         Strategy const& strategy)
    {
        auto const& projection = m_polygon_projections[polygon_index];
        auto const& projected_polygon = projection.projected_polygon;

        auto const& p1 = polygon.outer()[projection.p1_idx];
        auto const& p2 = polygon.outer()[projection.p2_idx];
        auto const& p3 = polygon.outer()[projection.p3_idx];

        int proj1 = projection.proj1;
        int proj2 = projection.proj2;
        int dir = projection.dir;

        // Check if the segment intersects the plane of the polygon
        auto sign1 = strategy.side3().apply(p1, p2, p3, segment_point1);
        auto sign2 = strategy.side3().apply(p1, p2, p3, segment_point2);

        if ((sign1 > 0 && sign2 > 0) || (sign1 < 0 && sign2 < 0))
        {
            return intersection_type::none; // No intersection, this is valid
        }

        // Check if the segment is on the same plane as the polygon
        if (sign1 == 0 && sign2 == 0)
        {
            auto const& projected_p1 = project_point_to_coordinate_plane(segment_point1, proj1, proj2);
            auto const& projected_p2 = project_point_to_coordinate_plane(segment_point2, proj1, proj2);

            linestring_2d projected_segment = {projected_p1, projected_p2};

            // Check if the projected segment intersects the polygon
            std::tuple
            <
                boost::geometry::model::multi_point<point_2d>,
                boost::geometry::model::multi_linestring<linestring_2d>> intersections;
            boost::geometry::intersection(projected_segment, projected_polygon, intersections);

            auto const& points = std::get<0>(intersections);
            auto const& linestrings = std::get<1>(intersections);

            if (boost::geometry::is_empty(points) && boost::geometry::is_empty(linestrings))
            {
                return intersection_type::none; // No intersection, this is valid
            }

            if (!points.empty())
            {
                if (points.size() == 1 &&
                    (boost::geometry::equals(points.front(), projected_p1) ||
                    boost::geometry::equals(points.front(), projected_p2)))
                {
                    return intersection_type::none; // Valid empty intersection
                }
                visitor.template apply<failure_invalid_intersection>();
                return intersection_type::invalid; // Invalid intersection
            }

            if (linestrings.size() == 1)
            {
                if (boost::geometry::equals(linestrings.front(), projected_segment))
                {
                    auto const& pp1 = dir < 0 ? projected_p2 : projected_p1;
                    auto const& pp2 = dir < 0 ? projected_p1 : projected_p2;
                    if (!has_correct_intersection_orientation(projected_polygon, pp1, pp2, visitor))
                    {
                        return intersection_type::invalid;
                    }
                    return intersection_type::valid; // The only case of valid non empty intersection
                }
            }
            visitor.template apply<failure_invalid_intersection>();
            return intersection_type::invalid;
        }

        int inter = polygon_segment_intersection(segment_point1, segment_point2, polygon, proj1,
            proj2, strategy);

        if ((inter == 1 || inter == 3) || (inter == 2 && sign1 !=0 && sign2 != 0))
        {
            visitor.template apply<failure_invalid_intersection>();
            return intersection_type::invalid;
        }

        return intersection_type::none;
    }

    // Helper function to check if any segment of a ring intersects a polygon
    template <typename VisitPolicy, typename Strategy>
    intersection_type check_polygon_polygon_intersection(polygon_3d const& polygon1,
                                                         polygon_3d const& polygon2,
                                                         std::size_t const& index2,
                                                         VisitPolicy& visitor,
                                                         Strategy const& strategy)
    {
        bool valid_intersection = false;
        auto const& ring1 = boost::geometry::exterior_ring(polygon1);

        for (auto it = boost::begin(ring1); it + 1 != boost::end(ring1); ++it)
        {
            auto result = check_polygon_segment_intersection(*it, *(it + 1), polygon2, index2,
                visitor, strategy);

            if (result == intersection_type::invalid)
            {
                return intersection_type::invalid; // Found invalid intersection
            }

            if (result == intersection_type::valid)
            {
                valid_intersection = true; // Found a valid intersection
            }
        }
        // Either no intersection or valid intersection
        return valid_intersection ? intersection_type::valid : intersection_type::none;
    }

 public:

    template <typename VisitPolicy, typename Strategy>
    intersection_type apply(polygon_3d const& polygon1,
                            polygon_3d const& polygon2,
                            std::size_t const& index1,
                            std::size_t const& index2,
                            VisitPolicy& visitor,
                            Strategy const& strategy)
    {
        auto intersection1 = check_polygon_polygon_intersection(polygon1, polygon2, index2,
            visitor, strategy);

        if (intersection1 == intersection_type::invalid)
        {
            return intersection_type::invalid; // Found invalid intersection
        }

        auto intersection2 = check_polygon_polygon_intersection(polygon2, polygon1, index1,
            visitor, strategy);

        if (intersection2 == intersection_type::invalid)
        {
            return intersection_type::invalid; // Found invalid intersection
        }

        if (intersection1 == intersection_type::none && intersection2 == intersection_type::none)
        {
            return intersection_type::none; // No intersection found
        }

        return intersection_type::valid;
    }

    template <typename VisitPolicy, typename Strategy>
    intersection_type initialize(polygon_3d const& polygon,
                                 std::size_t const polygon_index,
                                 VisitPolicy& visitor,
                                 Strategy const& strategy)
    {
        // TODO: Support interior rings
        ring_3d const& ring = boost::geometry::exterior_ring(polygon);

        if (boost::size(ring) < detail::minimum_ring_size<ring_3d>::value)
        {
            visitor.template apply<failure_few_points_on_face>();
            return intersection_type::invalid;
        }

        // Get three non-collinear points from the polygon to define the plane
        int p1_idx = 0;
        int p2_idx = 0;
        int p3_idx = 0;

        auto res = find_non_collinear_points(polygon, p1_idx, p2_idx, p3_idx, strategy);

        if (std::get<0>(res) == -1)
        {
            visitor.template apply<failure_collinear_points_on_face>();
            return intersection_type::invalid;
        }

        int proj1 = std::get<0>(res);
        int proj2 = std::get<1>(res);
        int dir = std::get<2>(res);

        auto const& p1 = polygon.outer()[p1_idx];
        auto const& p2 = polygon.outer()[p2_idx];
        auto const& p3 = polygon.outer()[p3_idx];

        // Check for non-coplanar points
        for (auto pit = points_begin(polygon); pit != points_end(polygon); ++pit)
        {
            if (strategy.side3().apply(p1, p2, p3, *pit) != 0)
            {
                visitor.template apply<failure_non_coplanar_points_on_face>();
                return intersection_type::invalid;
            }
        }

        // Project the polygon
        polygon_2d projected_polygon;

        auto add_projected_points = [&](auto begin_it, auto end_it)
        {
            for (auto it = begin_it; it != end_it; ++it)
            {
                projected_polygon.outer().emplace_back(
                    project_point_to_coordinate_plane(*it, proj1, proj2));
            }
        };

        if (dir > 0) {
            add_projected_points(points_begin(ring), points_end(ring));
        } else {
            add_projected_points(points_rbegin(ring), points_rend(ring));
        }

        // Check if the projected polygon is valid
        if (!resolve_dynamic::is_valid<polygon_2d>::apply(projected_polygon, visitor, strategy))
        {
            return intersection_type::invalid;
        }

        // Cache the projection for future use
        m_polygon_projections[polygon_index] = polygon_projection{std::move(projected_polygon),
            p1_idx, p2_idx, p3_idx, proj1, proj2, dir};

        return intersection_type::valid;
    }

private:
    std::vector<polygon_projection> m_polygon_projections;
};

template <typename PolyhedralSurface>
class is_valid_polyhedral_surface
{
public:
    template <typename VisitPolicy, typename Strategy>
    static inline bool apply(PolyhedralSurface const& surface,
                             VisitPolicy& visitor,
                             Strategy const& strategy)
    {
        boost::ignore_unused(strategy);

        valid_intersection<PolyhedralSurface> valid_intersection(surface.size());

        // Check for colinear/coplanar points and compute projections
        for (std::size_t i = 0; i < surface.size(); ++i)
        {
            auto intersection = valid_intersection.initialize(surface[i], i, visitor, strategy);

            using intersection_type = decltype(intersection);

            if (intersection == intersection_type::invalid)
            {
                return false;
            }
        }

        // Create an rtree to use it for polygon pair intersections
        using box_3d = typename boost::geometry::detail::is_valid::valid_intersection
            <PolyhedralSurface>::box_3d;
        using value = std::pair<box_3d, std::size_t>;

        boost::geometry::index::rtree<value, boost::geometry::index::rstar<4>> rtree;
        std::vector<std::unique_ptr<box_3d>> boxes(surface.size());

        for (std::size_t i = 0; i < surface.size(); ++i)
        {
            auto b = boost::geometry::return_envelope<box_3d>(surface[i], strategy);
            boxes[i] = std::make_unique<box_3d>(b);
            rtree.insert(std::make_pair(b, i)); // use balancing algorithm
        }

        // We check pairs of polygons for having a valid intersection.
        // We iterate in a BFS manner to check connectivity of the surface.

        auto it1 = boost::begin(surface);

        polygon_frontier_type polygon_frontier(surface.size());
        polygon_frontier.push(0);

        std::vector<std::size_t> remaining_polygons(surface.size());
        std::vector<std::size_t> positions(surface.size());

        for (std::size_t i = 0; i < surface.size(); ++i)
        {
            remaining_polygons[i] = i;
            positions[i] = i;
        }

        int remaining_count = surface.size();

        while (!polygon_frontier.empty())
        {
            auto index1 = polygon_frontier.top();
            polygon_frontier.pop();

            it1 = boost::begin(surface) + index1;

            // Remove index from remaining polygons and update positions
            auto moving_index = remaining_polygons[remaining_count - 1];
            remaining_polygons[positions[index1]] = moving_index;
            positions[moving_index] = positions[index1];

            // Query all polygons that intersect with the current polygon.
            // We use a buffer to avoid precision issues with the rtree query.
            std::vector<value> intersecting_boxes;
            auto const& b_query = *boxes[index1];
            auto const& b_query_buf = boost::geometry::return_buffer<box_3d>(b_query, 0.0001);

            rtree.query(boost::geometry::index::intersects(b_query_buf),
                        std::back_inserter(intersecting_boxes));

            for (int i = 0; i < remaining_count - 1; ++i)
            {
                auto index2 = remaining_polygons[i];

                // Skip if index2 is not in the intersecting boxes
                if (std::find_if(intersecting_boxes.begin(), intersecting_boxes.end(),
                    [index2](const value& v) { return v.second == index2; }
                ) == intersecting_boxes.end())
                {
                    continue;
                }

                auto intersection = valid_intersection.apply(*it1, surface[index2], index1, index2,
                    visitor, strategy);

                using intersection_type = decltype(intersection);

                if (intersection == intersection_type::invalid)
                {
                    return false; //  Invalid intersection found (including inconsistent orientation)
                }
                else if (intersection == intersection_type::valid)
                {
                    polygon_frontier.push(index2);
                }
            }
            remaining_count--;
        }

        if (remaining_count > 0)
        {
            visitor.template apply<failure_disconnected_surface>();
            return false; // Not all polygons were visited, hence disconnected
        }

        return true;
    }
};

}} // namespace detail::is_valid
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{


// Reference (for validity of Polyhedral Surfaces): OGC
// OpenGIS® Implementation Standard for Geographic
// information - Simple feature access - Part 1: Common
// architecture: 6.1.12
template <typename PolyhedralSurface, bool AllowEmptyMultiGeometries>
struct is_valid
    <
        PolyhedralSurface, polyhedral_surface_tag, AllowEmptyMultiGeometries
    > : detail::is_valid::is_valid_polyhedral_surface<PolyhedralSurface>
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_POLYHEDRAL_SURFACE_HPP
