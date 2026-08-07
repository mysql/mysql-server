 // Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_CLUSTERED_COUNTS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_CLUSTERED_COUNTS_HPP

#include <boost/geometry/algorithms/detail/position_code.hpp>
#include <boost/geometry/algorithms/detail/overlay/approximately_equals.hpp>
#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segment_point.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_distance_measure.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>

#include <set>
#include <map>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// Indicating if the segment is incoming (to cluster) or outgoing (from cluster)
enum class connection_type { unknown = -1, incoming = 0, outgoing = 1 };

// A turn contains four connections to a cluster:
// For both operations one incoming and one outgoing connection.
// They are stored in a map, with the (segment id, connection type) as key.
struct connection_key
{
    segment_identifier seg_id;
    connection_type connection{connection_type::unknown};

    bool operator<(connection_key const& rhs) const
    {
        return std::tie(seg_id, connection) < std::tie(rhs.seg_id, rhs.connection);
    }
};

// Properties of a connection in a property map
template <typename Point>
struct connection_properties
{
    // Assigned at construction time
    int position_code{0};
    Point point{};
    Point opposite_point{};
    bool is_shifted{false};

    // Assigned later
    std::size_t zone_count_left{0};
    std::size_t zone_count_right{0};
    std::size_t rank{0};
};

// Convenience structure to store connections in a vector
template <typename Point>
struct connection_item
{
    connection_key key{};
    connection_properties<Point> properties{};
};

template <overlay_type OverlayType>
struct is_corresponding_connection
{
    static inline bool apply(connection_key const& left, connection_key const& right)
    {
        return left.seg_id.source_index == right.seg_id.source_index;
    }
};

template <>
struct is_corresponding_connection<overlay_buffer>
{
    static inline bool apply(connection_key const& left, connection_key const& right)
    {
        // For buffer, the source_index is always the same.
        // It needs to check where the incoming seg_id is outgoing.
        return left.seg_id == right.seg_id;
    }
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
struct clustered_count_handler
{
    using point_type = typename Turns::value_type::point_type;
    using connection_map_type = std::map<connection_key, connection_properties<point_type>>;
    using ct_type = typename geometry::select_most_precise
        <
            geometry::coordinate_type_t<point_type>,
            double
        >::type;

    clustered_count_handler(Geometry1 const& m_geometry1, Geometry2 const& m_geometry2,
        Turns& m_turns, Clusters& clusters,
        Strategy const& strategy)
        : m_geometry1(m_geometry1)
        , m_geometry2(m_geometry2)
        , m_turns(m_turns)
        , m_clusters(clusters)
        , m_intersection_strategy(strategy)
        , m_side_strategy(m_intersection_strategy.side())
    {}

    // Walks over a ring to get the point after the turn.
    // The turn can be located at the very end of a segment.
    // Therefore it can be the first point on the next segment.
    template <typename Operation>
    bool get_segment_points(Operation const& op, point_type const& point_turn,
        point_type& point_from, point_type& point_to)
    {
        // Use the coordinate type, but if it is too small (e.g. std::int16), use a double
        static const ct_type tolerance
            = common_approximately_equals_epsilon_multiplier<ct_type>::value();

        // For a defensive check.
        constexpr int max_iterations = 10;

        int from_offset = 0;
        do
        {
            geometry::copy_segment_point<Reverse1, Reverse2>(m_geometry1, m_geometry2,
                op.seg_id, from_offset--, point_from);
        } while (approximately_equals(point_from, point_turn, tolerance) && from_offset > -max_iterations);

        int to_offset = 1;
        do
        {
            geometry::copy_segment_point<Reverse1, Reverse2>(m_geometry1, m_geometry2,
                op.seg_id, to_offset++, point_to);
        } while (approximately_equals(point_to, point_turn, tolerance) && to_offset < max_iterations);

        return from_offset < -1 || to_offset > 2;
    }

    void get_connection_map(cluster_info const& cluster, point_type const& point_turn,
        connection_map_type& connection_map, point_type& point_origin)
    {
        auto const get_position_code = [&](point_type const& point)
        {
            return detail::get_position_code(point_origin, point_turn, point, m_side_strategy);
        };

        auto insert = [&connection_map](auto const& op,  connection_type conn,
                    auto const& point, int position_code, auto const& opposite_point, bool is_shifted)
        {
            connection_key const key{op.seg_id, conn};
            connection_properties<point_type> properties{position_code, point, opposite_point, is_shifted};
            connection_map.insert({key, properties});
        };

        // Add them to the set, which keeps them unique on (seg_id,from/to)
        bool first = true;
        for (std::size_t index : cluster.turn_indices)
        {
            auto const& turn = m_turns[index];
            for (auto const& op : turn.operations)
            {
                point_type point_from, point_to;
                bool const is_shifted = get_segment_points(op, point_turn, point_from, point_to);

                if (first)
                {
                    // One of the incoming points is the origin. For the algorithm,
                    // it does not matter which one.
                    first = false;
                    point_origin = point_from;
                }

                // Insert the four connections. Insert all operations (even if they are blocked).
                insert(op, connection_type::incoming, point_from, get_position_code(point_from), point_to, is_shifted);
                insert(op, connection_type::outgoing, point_to, get_position_code(point_to), point_from, is_shifted);
            }
        }
    }

    void sort(point_type const& point_turn, std::vector<connection_item<point_type>>& item_vector)
    {
        auto compare_by_connection = [](auto const& left, auto const& right)
        {
            // Reversing it gives only one failure in ticket_9942 (difference)...
            return left.key.connection > right.key.connection;
        };

        // Compare by side, then by connection.
        // Left-side (1) goes before right-side (-1).
        // Outgoing (1) goes before incoming (0).
        auto compare_by_side = [&](auto const& left, auto const& right)
        {
            int const side_left = m_side_strategy.apply(point_turn, right.properties.point, left.properties.point);
            int const side_right = m_side_strategy.apply(point_turn, left.properties.point, right.properties.point);

            if (side_right == side_left)
            {
                return compare_by_connection(left, right);
            }
            return side_left < side_right;
        };

        std::sort(item_vector.begin(), item_vector.end(),
            [&](auto const& left, auto const& right)
            {
                if (left.properties.position_code == right.properties.position_code)
                {
                    if (left.properties.position_code == 1 || left.properties.position_code == 3)
                    {
                        // For collinear cases, side is be the same.
                        return compare_by_connection(left, right);
                    }
                    return compare_by_side(left, right);
                }
                return left.properties.position_code < right.properties.position_code;
            });
    }

    // Assign ranks, counter clockwise from the first incoming segment.
    void assign_ranks(point_type const& point_turn,
        std::vector<connection_item<point_type>>& item_vector)
    {
        std::size_t rank = 0;
        item_vector.front().properties.rank = 0;
        for (std::size_t i = 0; i + 1 < item_vector.size(); i++)
        {
            auto const& previous = item_vector[i];
            auto& item = item_vector[i + 1];
            if (item.properties.position_code != previous.properties.position_code)
            {
                item.properties.rank = ++rank;
                continue;
            }

            if (item.properties.position_code == 1 || item.properties.position_code == 3)
            {
                // Collinear cases always get the same rank.
                item.properties.rank = rank;
                continue;
            }

            // If it is collinear, it gets the same rank.
            // In other cases the side should be 1 (left) because the connections
            // are sorted counter clockwise.
            int const side = m_side_strategy.apply(point_turn, previous.properties.point,
                item.properties.point);
            item.properties.rank = side == 0 ? rank : ++rank;
        }
    }

    auto get_zone_counts(std::vector<connection_item<point_type>> const& item_vector,
        std::size_t rank_size)
    {
        std::size_t const vector_size = item_vector.size();
        auto get_next_item = [&vector_size](std::size_t counter)
        {
            return (counter + 1) % vector_size;
        };

        auto get_next_zone = [&rank_size](std::size_t counter)
        {
            return (counter + 1) % rank_size;
        };

        // Each segment occurs twice, once as from, once as to.
        // As soon as it comes in, increase the zone count, until it goes out.
        std::vector<std::size_t> zone_counts(rank_size, 0);
        for (std::size_t i = 0; i < item_vector.size(); i++)
        {
            auto const& item = item_vector[i];
            if (item.key.connection != connection_type::incoming)
            {
                continue;
            }

            // Walk ahead, cyclic, to find the next item with the same seg_id.
            // The iteration is a defensive check.
            std::size_t end_rank = item.properties.rank;
            for (std::size_t j = get_next_item(i), iteration = 0; ; j = get_next_item(j), iteration++)
            {
                if (iteration > vector_size)
                {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                    std::cerr << " *** ERROR: infinite loop in cluster" << std::endl;
#endif                    
                    return zone_counts;
                }
                auto const& next = item_vector[j];
                end_rank = next.properties.rank;

                if (next.key.connection == connection_type::outgoing
                    && is_corresponding_connection<OverlayType>::apply(item.key, next.key))
                {
                    // Found the corresponding outgoing segment for this incoming segment.
                    break;
                }
            }

            // Assign the ring count to the zone_counts in the rank range.
            for (std::size_t r = item.properties.rank; r != end_rank; r = get_next_zone(r))
            {
                zone_counts[r]++;
            }
        }

        return zone_counts;
    }

    void assign_zone_counts(std::vector<connection_item<point_type>>& item_vector,
        std::vector<std::size_t> const& zone_counts, std::size_t rank_size)
    {
        // The main goal is to get the number of polygons in the zone_counts.
        // The zone_counts on the right side of the seg_ids.
        for (auto& item : item_vector)
        {
            std::size_t const zone_right =
                item.key.connection == connection_type::incoming
                ? item.properties.rank
                : (item.properties.rank + rank_size - 1) % rank_size;

            std::size_t const zone_left =
                item.key.connection == connection_type::incoming
                ? (item.properties.rank + rank_size - 1) % rank_size
                : item.properties.rank;

            item.properties.zone_count_left = zone_counts[zone_left];
            item.properties.zone_count_right = zone_counts[zone_right];
        }
    }

    std::size_t get_open_count(std::vector<std::size_t> const& zone_counts, std::size_t rank_size)
    {
        std::size_t result = 0;
        for (std::size_t i = 0; i < rank_size; i++)
        {
            if (zone_counts[i] == 0)
            {
                result++;
            }
        }
        return result;
    }

    // Get the number of spikes in a cluster, and mark them as spikes.
    void handle_spikes(cluster_info& cluster, std::vector<connection_item<point_type>>& item_vector)
    {
        for (std::size_t i = 0; i < item_vector.size(); i++)
        {
            auto const next_i = (i + 1) % item_vector.size();
            if (item_vector[i].key.connection == item_vector[next_i].key.connection)
            {
                // The connection should be different
                continue;
            }
            auto& current = item_vector[i].properties;
            auto& next = item_vector[next_i].properties;
            if (current.rank != next.rank
                || current.zone_count_left != 1 || current.zone_count_right != 1
                || next.zone_count_left != 1 || next.zone_count_right != 1)
            {
                // The rank should be the same
                // It should have one zone on either side
                continue;
            }

            if (current.is_shifted || next.is_shifted) {
                // The opposite point is shifted. Therefore a spike measurement
                // cannot be done.
                continue;
            }

            // Precise measurement, not from the turn, but over the whole intersecting segment.
            // If it is positive (on the left side), it is a spike.
            auto const dm = get_distance_measure(current.opposite_point, current.point, next.point,
                m_intersection_strategy);
            if (dm.measure <= 0)
            {
                continue;
            }

            // There is a small measurable difference.
            // Make the cluster open and adapt the counts.
            cluster.open_count++;
            current.zone_count_left = 0;
            next.zone_count_right = 0;
        }
    }

    void assign_turn_operations(cluster_info const& cluster,
        connection_map_type const& connection_map)
    {
        // Assign the items, per seg_id, back to the outgoing turn operations.
        for (std::size_t index : cluster.turn_indices)
        {
            auto& turn = m_turns[index];
            for (int i = 0; i < 2; i++)
            {
                auto& op = turn.operations[i];
                connection_key const key{op.seg_id, connection_type::outgoing};
                auto const it = connection_map.find(key);
                if (it != connection_map.end())
                {
                    op.enriched.count_left = it->second.zone_count_left;
                    op.enriched.count_right = it->second.zone_count_right;
                    op.enriched.rank = it->second.rank;
                }
            }
        }
    }

    // Currently necessary for some failing cases in buffer only, where due to floating point
    // precision the i/u turns get unexpected counts for left/right.
    // rt_w10, rt_w11, rt_w14, rt_w15
    // The original sides are measured over the two whole intersecting segments.
    // The sides in clusters are measured w.r.t. the turn point, which is the point of the first cluster.
    // This can differ.
    // It should be possible to fix it in another way.
    void change_reversed_operations(signed_size_type const cluster_id, cluster_info const& cluster,
            point_type const& point_turn, point_type const& point_origin)
    {
        std::set<std::size_t> reversed_indices;
        for (std::size_t index : cluster.turn_indices)
        {
            auto const& turn = m_turns[index];
            if (! turn.combination(operation_union, operation_intersection))
            {
                continue;
            }
            int const union_index = turn.operations[0].operation == operation_union ? 0 : 1;
            auto const& op_u = turn.operations[union_index];
            auto const& op_i = turn.operations[1 - union_index];
            if (op_u.enriched.count_left > 0 && op_i.enriched.count_left == 0)
            {
                reversed_indices.insert(index);
            }
        }

        if (reversed_indices.empty())
        {
            return;
        }

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        std::cout << " *** REVERSED OPERATIONS in cluster: " << cluster_id
            << " cluster size: " << cluster.turn_indices.size()
            << " reversed: " << reversed_indices.size()
            << std::endl;
#endif
        for (std::size_t index : cluster.turn_indices)
        {
            auto& turn = m_turns[index];
            auto& op0 = turn.operations[0];
            auto& op1 = turn.operations[1];

            bool const is_same_target = op0.enriched.travels_to_ip_index == op1.enriched.travels_to_ip_index;
            if (is_same_target && reversed_indices.find(index) != reversed_indices.end())
            {
                // Best choice: i/u are nearly collinear, so we can let them continue.
                op0.operation = operation_continue;
                op1.operation = operation_continue;

                // Also adapt the left/right-counts, both should get the minimum of both.
                op0.enriched.count_left = (std::min)(op0.enriched.count_left, op1.enriched.count_left);
                op1.enriched.count_left = op0.enriched.count_left;
                op0.enriched.count_right = (std::min)(op0.enriched.count_right, op1.enriched.count_right);
                op1.enriched.count_right = op0.enriched.count_right;
            }
        }
    }

    template <typename Visitor>
    void apply(signed_size_type const cluster_id, cluster_info& cluster, Visitor& visitor)
    {
        if (cluster.turn_indices.empty())
        {
            // Defensive check.
            return;
        }

        point_type const& point_turn = m_turns[*cluster.turn_indices.begin()].point;
        point_type point_origin;
        connection_map_type connection_map;
        get_connection_map(cluster, point_turn, connection_map, point_origin);

        // Sort the items by position code, and if equal, by side.
        // For this they are copied into a vector.
        std::vector<connection_item<point_type>> item_vector;
        for (auto const& key_value : connection_map)
        {
            connection_item<point_type> item;
            item.key = key_value.first;
            item.properties = key_value.second;
            item_vector.push_back(std::move(item));
        }

        sort(point_turn, item_vector);
        assign_ranks(point_turn, item_vector);

        auto const rank_size = item_vector.back().properties.rank + 1;
        auto const zone_counts = get_zone_counts(item_vector, rank_size);

        assign_zone_counts(item_vector, zone_counts, rank_size);

        cluster.open_count = get_open_count(zone_counts, rank_size);
        if (cluster.open_count == 0)
        {
            handle_spikes(cluster, item_vector);
        }

        // Assign the updated properties back to the connection map
        for (auto const& item : item_vector)
        {
            connection_map[item.key] = item.properties;
        }

        assign_turn_operations(cluster, connection_map);
        change_reversed_operations(cluster_id, cluster, point_turn, point_origin);

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        // List the connections
        std::cout << "Cluster " << cluster_id << " size: " << cluster.turn_indices.size() << std::endl;
        for (auto const& item : item_vector)
        {
            std::cout << "  " << item.key.seg_id
                << " " << (item.key.connection == connection_type::incoming ? " in" : "out")
                << " " << item.properties.position_code
                << " " << item.properties.rank
                << " " << item.properties.zone_count_left
                << " " << item.properties.zone_count_right
                << std::endl;
        }
#endif

        visitor.visit_cluster_connections(cluster_id, m_turns, cluster, item_vector);
    }

    template <typename Visitor>
    void apply(Visitor& visitor)
    {
        for (auto& key_value : m_clusters)
        {
            auto& cluster = key_value.second;
            if (cluster.turn_indices.empty())
            {
                continue;
            }

            apply(key_value.first, cluster, visitor);
        }
    }

private:
    Geometry1 const& m_geometry1;
    Geometry2 const& m_geometry2;
    Turns& m_turns;
    Clusters& m_clusters;
    Strategy const& m_intersection_strategy;
    decltype(m_intersection_strategy.side()) m_side_strategy;
};

}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_CLUSTER_INFO_HPP
