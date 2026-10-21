// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_GRAPH_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_GRAPH_HPP

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segments.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/is_operation_included.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/get_tois.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/node_util.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/select_edge.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/core/closure.hpp>

#include <boost/assert.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

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
struct traverse_graph
{
    static constexpr operation_type target_operation = operation_from_overlay<OverlayType>::value;
    static constexpr bool allow_closed = target_operation == operation_intersection;
    static constexpr bool is_buffer = OverlayType == overlay_buffer;

    using turn_type = typename boost::range_value<Turns>::type;
    using is_included = is_operation_included<target_operation>;
    using point_type = typename turn_type::point_type;
    using toi_set = std::set<turn_operation_id>;

    inline traverse_graph(Geometry1 const& geometry1, Geometry2 const& geometry2,
            Turns& turns, Clusters const& clusters,
            Strategy const& strategy)
        : m_edge_selector(geometry1, geometry2, turns, clusters, strategy)
        , m_geometry1(geometry1)
        , m_geometry2(geometry2)
        , m_turns(turns)
        , m_clusters(clusters)
        , m_strategy(strategy)
    {
    }

    template <typename Ring>
    void copy_segments(Ring& ring, turn_operation_id const& toi) const
    {
        auto const& op = m_turns[toi.turn_index].operations[toi.operation_index];
        auto const to_vertex_index = op.enriched.travels_to_vertex_index;
        if (op.seg_id.source_index == 0)
        {
            geometry::copy_segments<Reverse1>(m_geometry1,
                    op.seg_id, to_vertex_index,
                    m_strategy, ring);
        }
        else
        {
            geometry::copy_segments<Reverse2>(m_geometry2,
                    op.seg_id, to_vertex_index,
                    m_strategy, ring);
        }

    }

    template <typename Ring>
    void use_vertices(Ring& ring, turn_operation_id const& toi, bool is_round_trip = false) const
    {
        auto const& op = m_turns[toi.turn_index].operations[toi.operation_index];
        auto const to_vertex_index = op.enriched.travels_to_vertex_index;

        if (to_vertex_index < 0)
        {
            return;
        }

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << "At : " << toi << std::endl;
#endif


        if (op.seg_id.segment_index == to_vertex_index && ! is_round_trip)
        {
            auto const& next_turn = m_turns[op.enriched.travels_to_ip_index];

            bool allow = false;

            for (int j = 0; j < 2; j++)
            {
                auto const& next_op = next_turn.operations[j];
                if (next_op.seg_id == op.seg_id)
                {
                    // It is on the same segment. Determine if it is located before or after
                    if (next_op.fraction < op.fraction)
                    {
                        // It is before, so we can continue
                        allow = true;
                    }
                }
            }

            if (! allow)
            {
                return;
            }
        }

        copy_segments(ring, toi);
    }

    // Set the turn operation as visited.
    void set_visited(turn_operation_id const& toi)
    {
        // std::cout << "Set visited: " << toi << std::endl;
        m_visited_tois.insert(toi);

        // From the same cluster, set other operations with the same segment id,
        // going to the same target, as visited as well.
        auto const& turn = m_turns[toi.turn_index];
        if (! turn.is_clustered())
        {
            return;
        }
        auto cluster_it = m_clusters.find(turn.cluster_id);
        if (cluster_it == m_clusters.end())
        {
            return;
        }
        auto const& cluster = cluster_it->second;

        auto const& op = turn.operations[toi.operation_index];

        for (std::size_t turn_index : cluster.turn_indices)
        {
            if (turn_index == toi.turn_index)
            {
                continue;
            }
            auto const& other_turn = m_turns[turn_index];
            for (int j = 0; j < 2; j++)
            {
                auto const& other_op = other_turn.operations[j];
                if (other_op.enriched.travels_to_ip_index == op.enriched.travels_to_ip_index
                    && other_op.seg_id == op.seg_id)
                {
                    m_visited_tois.insert({turn_index, j});
                }
            }
        }
    }

    template <typename Ring>
    bool continue_traverse(Ring& ring,
            signed_size_type component_id,
            signed_size_type start_node_id,
            signed_size_type target_node_id)
    {
        signed_size_type current_node_id = target_node_id;

        std::size_t iteration_count = 0;

        // Keep traversing until it finds the start (successful finish), or it is stuck,
        // or it find an already visited node during traversal.
        // The iteration count is a defensive check to prevent endless loops and not iterate
        // more than times there are turns (this should not happen).
        while (iteration_count < m_turns.size())
        {
            auto const current_turn_indices = get_turn_indices_by_node_id(m_turns, m_clusters,
                    current_node_id, allow_closed);

            // Any valid node should always deliver at least one turn
            BOOST_ASSERT(! current_turn_indices.empty());

            auto const next_target_nodes = get_target_nodes<target_operation>(m_turns, m_clusters,
                    current_turn_indices, component_id);

            if (next_target_nodes.empty())
            {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                std::cout << "Stuck, start: " << start_node_id
                    << " stuck: " << current_node_id
                    << " (no targets) " << std::endl;
#endif
                return false;
            }

            auto const tois = get_tois<target_operation>(m_turns, m_clusters,
                    current_node_id, next_target_nodes);

            if (tois.empty())
            {
                return false;
            }

            auto const& turn_point = m_turns[*current_turn_indices.begin()].point;

            auto toi = *tois.begin();

            if (tois.size() > 1)
            {
                // Select the best target edge, using the last point of the ring and the turn point
                // for side calculations (if any).
                toi = m_edge_selector.select_target_edge(tois, range::back(ring), turn_point);
            }

            if (m_visited_tois.count(toi) > 0 || m_finished_tois.count(toi) > 0)
            {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                std::cout << "ALREADY visited, turn " << toi
                    << " in {" << current_node_id
                    << " -> size " << next_target_nodes.size() << "}" << std::endl;
#endif
                return false;
            }

            detail::overlay::append_no_collinear(ring, turn_point, m_strategy);

            set_visited(toi);
            use_vertices(ring, toi);

            auto const& selected_op = m_turns[toi.turn_index].operations[toi.operation_index];
            auto const next_target_node_id = get_node_id(m_turns,
                selected_op.enriched.travels_to_ip_index);
            if (next_target_node_id == start_node_id)
            {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                std::cout << "Finished at: " << next_target_node_id << std::endl;
#endif
                return true;
            }

            current_node_id = next_target_node_id;
            ++iteration_count;
        }
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << "Cancelled at: " << iteration_count << std::endl;
#endif
        return false;
    }

    template <typename Rings>
    void start_traverse(Rings& rings, point_type const& start_point,
            signed_size_type component_id,
            signed_size_type start_node_id,
            signed_size_type target_node_id)
    {
        // Select the first toi which is not yet visited and has the requested component.
        // If all tois are visited, not having the same component, it is not possible to continue,
        // and it returns an invalid toi.
        auto select_first_toi = [&](auto const& tois)
        {
            for (auto const& toi : tois)
            {
                if (m_finished_tois.count(toi) > 0)
                {
                    // Visited in the meantime
                    continue;
                }
                auto const& op = m_turns[toi.turn_index].operations[toi.operation_index];
                if (op.enriched.component_id != component_id)
                {
                    continue;
                }

                return toi;
            }
            return turn_operation_id{0, -1};
        };

        auto const toi = select_first_toi(get_tois<target_operation>(m_turns, m_clusters,
                start_node_id, target_node_id));
        if (toi.operation_index < 0)
        {
            return;
        }

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << "\n" << "-> Start traversing component " << component_id
            << " at: " << toi
            << " to " << target_node_id << std::endl;
#endif


        using ring_type = typename boost::range_value<Rings>::type;

        constexpr std::size_t min_size
                = core_detail::closure::minimum_ring_size
                        <
                            geometry::closure<ring_type>::value
                        >::value;

        ring_type ring;
        detail::overlay::append_no_collinear(ring, start_point, m_strategy);

        m_visited_tois.clear();
        set_visited(toi);

        bool const is_round_trip = start_node_id == target_node_id;
        use_vertices(ring, toi, is_round_trip);

        // Traverse the graph. If the target is at the start, it is a round trip,
        // and it is finished immediately.
        // The continuation could fail (no target nodes, or no target edges).
        bool const is_finished = is_round_trip
            || continue_traverse(ring, component_id, start_node_id, target_node_id);

        if (! is_finished)
        {
            return;
        }

        detail::overlay::append_no_collinear(ring, start_point, m_strategy);
        remove_spikes_at_closure(ring, m_strategy);
        fix_closure(ring, m_strategy);

        if (geometry::num_points(ring) >= min_size)
        {
        #if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
            std::cout << "Add ring: " << geometry::wkt(ring) << std::endl;
        #endif
            rings.push_back(std::move(ring));
        }
        m_finished_tois.insert(m_visited_tois.begin(), m_visited_tois.end());
    }

    void update_administration()
    {
        for (auto const& toi : m_finished_tois)
        {
            auto& op = m_turns[toi.turn_index].operations[toi.operation_index];
            op.enriched.is_traversed = true;
        }
    }

    template <typename Rings>
    void iterate(Rings& rings, std::size_t turn_index)
    {
        auto const& turn = m_turns[turn_index];
        if (turn.discarded)
        {
            return;
        }
        auto const source_node_id = get_node_id(m_turns, turn_index);
        auto const turn_indices = get_turn_indices_by_node_id(m_turns, m_clusters,
                source_node_id, allow_closed);

        for (int j = 0; j < 2; j++)
        {
            auto const& op = turn.operations[j];
            if (! op.enriched.startable || ! is_included::apply(op))
            {
                continue;
            }

            turn_operation_id const toi{turn_index, j};
            if (m_finished_tois.count(toi) > 0
                || ! is_target_operation<target_operation>(m_turns, toi))
            {
                continue;
            }

            auto const component_id = op.enriched.component_id;
            auto const target_nodes = get_target_nodes<target_operation>(m_turns, m_clusters,
                    turn_indices, component_id);

            for (auto const target_node_id : target_nodes)
            {
                auto const start = std::make_tuple(source_node_id, target_node_id, component_id);
                if (m_starts.count(start) > 0)
                {
                    // Don't repeat earlier or finished trials. This speeds up some cases by 1.5x
                    continue;
                }
                m_starts.insert(start);

    #if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                std::cout << "\n" << "Traversing component " << component_id
                    << " from " << source_node_id << " to " << target_node_id << std::endl;
    #endif
                start_traverse(rings, turn.point, component_id, source_node_id, target_node_id);
            }
        }
    }

    template <typename Rings>
    void iterate(Rings& rings)
    {
        for (std::size_t i = 0; i < m_turns.size(); i++)
        {
            iterate(rings, i);
        }

        update_administration();
    }

private:

    edge_selector
        <
            Reverse1, Reverse2, OverlayType,
            Geometry1, Geometry2,
            Turns, Clusters,
            Strategy
        > m_edge_selector;

    Geometry1 const& m_geometry1;
    Geometry2 const& m_geometry2;
    Turns& m_turns;
    Clusters const& m_clusters;
    Strategy const& m_strategy;

    // Visited turn operations on currenly traversed ring - they are either
    // inserted into the final set, or cleared before the next trial.
    toi_set m_visited_tois;

    // Visited turn operations after a ring is added
    toi_set m_finished_tois;

    // Keep track of started combinations (either finished, or stuck)
    std::set<std::tuple<signed_size_type, signed_size_type, signed_size_type>> m_starts;
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_GRAPH_HPP
