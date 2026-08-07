// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_EDGE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_EDGE_HPP

#include <boost/core/ignore_unused.hpp>
#include <boost/geometry/algorithms/detail/overlay/approximately_equals.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segment_point.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/node_util.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/select_toi_by_incoming.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
#include <boost/geometry/io/wkt/wkt.hpp>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename Point>
struct edge_and_side
{
    turn_operation_id toi{0};
    Point point{};
    int side{0};
};

template
<
    bool Reverse1,
    bool Reverse2,
    overlay_type OverlayType,
    typename Geometry1,
    typename Geometry2,
    typename Turns,
    typename Clusters,
    typename Strategy
>
struct edge_selector
{
private:
    static constexpr operation_type target_operation = operation_from_overlay<OverlayType>::value;
    using point_type = typename Turns::value_type::point_type;
    using edge_type = edge_and_side<point_type>;
    using edges_type = std::vector<edge_type>;

    // Use the coordinate type, but if it is too small (e.g. std::int16), use a double
    using coor_type = typename geometry::select_most_precise
        <
            geometry::coordinate_type_t<point_type>,
            double
        >::type;

    // Walks over a ring to get the point after the turn.
    // The turn can be located at the very end of a segment.
    // Therefore it can be the first point on the next segment.
    template <typename Operation>
    point_type walk_to_point_after_turn(Operation const& op, point_type const& turn_point) const
    {
        static const coor_type tolerance
            = common_approximately_equals_epsilon_multiplier<coor_type>::value();
        int offset = 1;
        point_type point;
        do
        {
            geometry::copy_segment_point<Reverse1, Reverse2>(m_geometry1, m_geometry2,
                op.seg_id, offset, point);
            ++offset;
        } while (approximately_equals(point, turn_point, tolerance) && offset < 10);
        return point;
    }

    // Compares and returns true for the left most operation.
    // p1 is the point before the current turn.
    // p2 is the current turn.
    // So (p1, p2) together define the direction of the segment.
    bool select_collinear_target_edge(edge_type const& a, edge_type const& b) const
    {
        auto const& turn_a = m_turns[a.toi.turn_index];
        auto const& turn_b = m_turns[b.toi.turn_index];
        auto const& op_a = turn_a.operations[a.toi.operation_index];
        auto const& op_b = turn_b.operations[b.toi.operation_index];

        auto const target_a = get_node_id(m_turns, op_a.enriched.travels_to_ip_index);
        auto const target_b = get_node_id(m_turns, op_b.enriched.travels_to_ip_index);

        auto const& other_op_a = turn_a.operations[1 - a.toi.operation_index];
        auto const& other_op_b = turn_b.operations[1 - b.toi.operation_index];

        if (other_op_a.enriched.travels_to_ip_index == -1)
        {
            return true;
        }
        if (other_op_b.enriched.travels_to_ip_index == -1)
        {
            return false;
        }

        auto const other_target_a = get_node_id(m_turns, other_op_a.enriched.travels_to_ip_index);
        auto const other_target_b = get_node_id(m_turns, other_op_b.enriched.travels_to_ip_index);

        if (target_b == other_target_a || target_b == other_target_b)
        {
            // The second edge goes via one of the targets of the first
            return false;
        }
        if (target_a == other_target_a || target_a == other_target_b)
        {
            // Vice versa
            return true;
        }

        return true;
    }

    void report(const char* caption, edges_type const& edges,
        point_type const& p1, point_type const& p2) const
    {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << " *** Sorted edges " << caption
        << " from " << geometry::wkt(p1) << " to " << geometry::wkt(p2)
        << std::endl;
        for (auto const& item : edges)
        {
            auto const& op = m_turns[item.toi.turn_index].operations[item.toi.operation_index];
            std::cout << "  -> " << item.toi
                << " to " << op.enriched.travels_to_ip_index
                << " side: " << item.side
                << std::endl;
        }
#endif
    }

    turn_operation_id select_by_side(edges_type& edges, point_type const& p1, point_type const& p2) const
    {
        // Select point and calculate side for each edge
        auto const side_strategy = m_intersection_strategy.side();
        for (auto& edge : edges)
        {
            auto const& op = m_turns[edge.toi.turn_index].operations[edge.toi.operation_index];
            edge.point = walk_to_point_after_turn(op, p2);
            edge.side = side_strategy.apply(p1, p2, edge.point);
        }

        // Sort by side (with respect to segment [p1..p2]) (TEMPORARY: and then by toi)
        // Right = -1 will come first. Left = 1 will come last.
        // This works for both union and intersection operations, because it should always
        // take the right turn (even in uu in buffer/union).
        std::sort(edges.begin(), edges.end(), [](auto const& a, auto const& b)
        {
            return std::tie(a.side, a.toi) < std::tie(b.side, b.toi);
        });

        report("by side", edges, p1, p2);

        if (edges.size() == 1 || (edges.size() > 1 && edges.front().side != edges[1].side))
        {
            return edges.front().toi;
        }

        if (edges.front().side != edges.back().side)
        {
            // Remove all edges with different side than the first
            auto it = std::find_if(edges.begin() + 1, edges.end(), [&](auto const& item)
            {
                return item.side != edges.front().side;
            });
            edges.erase(it, edges.end());
        }

        if (edges.front().side == 0)
        {
            // Select for collinearity (it makes no sense to sort on mutual side)
            auto compare = [&](edge_type const& a, edge_type const& b) -> bool
            {
                return select_collinear_target_edge(a, b);
            };
            std::sort(edges.begin(), edges.end(), compare);
            return edges.front().toi;
        }

        // Phase 2, sort by mutual side, of the edges having the front edge's side.
        auto compare_one_side = [&](auto const& a, auto const& b) -> bool
        {
            // Calculating one side is enough. Either both are 0, or they are opposite.
            int const side = side_strategy.apply(p2, a.point, b.point);
            return side == 1;
        };

        std::sort(edges.begin(), edges.end(), compare_one_side);

        report("by mutual side", edges, p1, p2);

        return edges.front().toi;
    }

public:

    edge_selector(Geometry1 const& m_geometry1, Geometry2 const& m_geometry2,
        Turns const& m_turns, Clusters const& clusters,
        Strategy const& strategy)
        : m_geometry1(m_geometry1)
        , m_geometry2(m_geometry2)
        , m_turns(m_turns)
        , m_clusters(clusters)
        , m_intersection_strategy(strategy)
    {}

    // Select one operation which is the leftmost or rightmost operation.
    // p1 is the point before the current turn.
    // p2 is the current turn.
    // So (p1, p2) together define the direction of the segment.
    turn_operation_id select_target_edge(set_of_tois const& turn_operation_ids,
            point_type const& p1, point_type const& p2) const
    {
        if (turn_operation_ids.empty())
        {
            return {};
        }
        if (turn_operation_ids.size() == 1)
        {
            return *turn_operation_ids.begin();
        }

        edges_type edges;
        edges.reserve(turn_operation_ids.size());
        for (auto const& toi : turn_operation_ids)
        {
            edges.emplace_back(edge_type{toi});
        }

        // Verification function for clusters: if it is clustered, all should come from one cluster.
        auto assert_one_cluster = [&]() -> bool
        {
            auto const& turn0 = m_turns[edges[0].toi.turn_index];
            auto const cluster_id = turn0.cluster_id;
            for (auto const& toi : turn_operation_ids)
            {
                auto const& turn = m_turns[toi.turn_index];
                if (turn.cluster_id != cluster_id)
                {
                    return false;
                }
            }
            return true;
        };

        boost::ignore_unused(assert_one_cluster);

        // It often happens there are just two collinear edges.
        // If they travel to the same target, take either.
        if (edges.size() == 2)
        {
            auto const& turn0 = m_turns[edges[0].toi.turn_index];
            auto const& turn1 = m_turns[edges[1].toi.turn_index];
            auto const& op0 = turn0.operations[edges[0].toi.operation_index];
            auto const& op1 = turn1.operations[edges[1].toi.operation_index];
            if (op0.operation == operation_continue
                && op1.operation == operation_continue
                && op0.enriched.travels_to_ip_index == op1.enriched.travels_to_ip_index)
            {
                return edges.front().toi;
            }

            if (target_operation == operation_union
                && turn0.is_clustered()
                && op0.operation == operation_union
                && op1.operation == operation_union
                && op0.enriched.rank == op1.enriched.rank)
            {
                // Because it is clustered, and all operations come from the same cluster,
                // the rank can be used, which is more efficient.
                BOOST_GEOMETRY_ASSERT(assert_one_cluster());

                turn_operation_id result;
                if (select_toi_for_union(result, op0, op1, edges[0].toi, edges[1].toi, m_turns))
                {
                    return result;
                }

                bool const better = is_better_collinear_for_union(
                        op0, op1, edges.front().toi, edges.back().toi);
                return better ? edges.front().toi : edges.back().toi;
            }
        }

        return select_by_side(edges, p1, p2);
    }

private:
    Geometry1 const& m_geometry1;
    Geometry2 const& m_geometry2;
    Turns const& m_turns;
    Clusters const& m_clusters;
    Strategy const& m_intersection_strategy;
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_EDGE_HPP
