//=======================================================================
// Copyright (C) 2026 Arnaud Becheler
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#ifndef BOOST_GRAPH_LOUVAIN_CLUSTERING_HPP
#define BOOST_GRAPH_LOUVAIN_CLUSTERING_HPP

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/louvain_quality_functions.hpp>

#include <boost/property_map/property_map.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <boost/range/iterator_range.hpp>

#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/concept/assert.hpp>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/container_hash/hash.hpp>
#include <algorithm>
#include <type_traits>

namespace boost
{
namespace louvain_detail 
{

/// @brief Result of graph aggregation operation.
template <typename Graph, typename PartitionMap, typename WeightType>
struct aggregation_result
{
    Graph graph;
    PartitionMap partition;
    // coarsened vertex to original vertex indices
    std::vector<std::vector<std::size_t>> vertex_mapping;
};

/// @brief Aggregate graph by collapsing communities into super-nodes. 
/// @note Edges between communities are preserved with accumulated weights.
/// @tparam IndexMap A ReadablePropertyMap mapping vertex_descriptor to std::size_t
template <typename Graph, typename CommunityMap, typename WeightMap, typename IndexMap>
auto aggregate(
    const Graph& g, 
    const CommunityMap& communities, 
    const WeightMap& weight,
    const IndexMap& index_map
){
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    using vertex_iterator = typename graph_traits<Graph>::vertex_iterator;
    using edge_iterator = typename graph_traits<Graph>::edge_iterator;
    using community_type = typename property_traits<CommunityMap>::value_type;
    using weight_type = typename property_traits<WeightMap>::value_type;
    using edge_property_t = property<edge_weight_t, weight_type>;
    using aggregated_graph_t = adjacency_list<vecS, vecS, undirectedS, no_property, edge_property_t>;
    using agg_vertex_t = typename graph_traits<aggregated_graph_t>::vertex_descriptor; // always size_t since vecS
    using result_t = aggregation_result<aggregated_graph_t, vector_property_map<agg_vertex_t>, weight_type>;
    
    aggregated_graph_t new_g;
    vector_property_map<agg_vertex_t> new_community_map;
    
    // Map community labels to coarsened vertex
    boost::unordered_flat_map<community_type, agg_vertex_t> comm_to_vertex;

    // Collect unique communities and create super-nodes
    vertex_iterator vi, vi_end;
    for (tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
        community_type c = get(communities, *vi);
        if (comm_to_vertex.find(c) == comm_to_vertex.end()) {
            agg_vertex_t new_v = add_vertex(new_g);
            comm_to_vertex[c] = new_v;
            put(new_community_map, new_v, new_v);
        }
    }
    
    std::size_t n_communities = num_vertices(new_g);
    
    // Coarsened vertex to original vertex indices (via index_map)
    std::vector<std::vector<std::size_t>> vertex_to_originals(n_communities);

    // Record which original vertices belong to which super-node
    for (tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
        community_type c = get(communities, *vi);
        agg_vertex_t new_v = comm_to_vertex[c];
        vertex_to_originals[new_v].push_back(get(index_map, *vi));
    } 
    
    // Build edges with accumulated weights
    boost::unordered_flat_map<std::pair<agg_vertex_t, agg_vertex_t>, weight_type> temp_edge_weights;
    
    edge_iterator edge_it, edge_end;
    for (tie(edge_it, edge_end) = edges(g); edge_it != edge_end; ++edge_it) {
        vertex_descriptor u = source(*edge_it, g);
        vertex_descriptor v = target(*edge_it, g);
        
        community_type c_u = get(communities, u);
        community_type c_v = get(communities, v);
        
        agg_vertex_t new_u = comm_to_vertex[c_u];
        agg_vertex_t new_v = comm_to_vertex[c_v];
        
        weight_type w = get(weight, *edge_it);
        
        // vertices in same community = self loop on super node
        if (new_u == new_v) {
            auto edge_key = std::make_pair(new_u, new_u);
            temp_edge_weights[edge_key] += w;
        } else {
            auto edge_key = std::make_pair(std::min(new_u, new_v), std::max(new_u, new_v));
            temp_edge_weights[edge_key] += w;
        }
    }
    
    // Add edges to connect super-nodes
    for (const auto& kv : temp_edge_weights) {
        typename graph_traits<aggregated_graph_t>::edge_descriptor e;
        bool inserted;
        tie(e, inserted) = add_edge(kv.first.first, kv.first.second, kv.second, new_g);
        BOOST_ASSERT(inserted);
    }
    
    return result_t{std::move(new_g), std::move(new_community_map), std::move(vertex_to_originals)};
}

/// @brief Unfold a coarse partition back to the original vertices through hierarchy levels.
/// @param final_communities Coarsened vertex index to community label
/// @param levels Hierarchy: levels[i][coarsened_v] = {original vertex indices...}
/// @param n_original Number of vertices in the original graph
/// @return Vector mapping original vertex index to community label
inline std::vector<std::size_t> unfold(
    const std::vector<std::size_t>& final_communities,
    const std::vector<std::vector<std::vector<std::size_t>>>& levels,
    std::size_t n_original)
{
    BOOST_ASSERT(!levels.empty());

    std::vector<std::size_t> original_partition(n_original);
    
    for (std::size_t coarse_v = 0; coarse_v < final_communities.size(); ++coarse_v) {
        std::vector<std::size_t> current_nodes;
        current_nodes.push_back(coarse_v);
        
        // From coarse to fine
        for (auto level = levels.size(); level--; ) {
            std::vector<std::size_t> next_nodes;
            
            for (std::size_t node : current_nodes) {
                BOOST_ASSERT(node < levels[level].size());
                const auto& originals = levels[level][node];
                next_nodes.insert(next_nodes.end(), originals.begin(), originals.end());
            }
            
            current_nodes = std::move(next_nodes);
        }
        
        // Assign all original vertices to community
        for (std::size_t original_v : current_nodes) {
            original_partition[original_v] = final_communities[coarse_v];
        }
    }
    
    return original_partition;
}

// Create vector of all vertices.
template <typename Graph>
auto get_vertex_vector(const Graph& g)
{
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    using vertex_iterator = typename graph_traits<Graph>::vertex_iterator;
    
    std::vector<vertex_descriptor> vertices_vec;
    vertices_vec.reserve(num_vertices(g));
    vertex_iterator vi, vi_end;
    for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
        vertices_vec.push_back(*vi);
    }
    return vertices_vec;
}

/// @brief Fast version, requires the QualityFunction to implement GraphPartitionQualityFunctionIncrementalConcept
template <typename QualityFunction = newman_and_girvan, typename Graph, typename CommunityMap, typename WeightMap, typename URBG>
typename property_traits<WeightMap>::value_type
local_optimization_impl(
    const Graph& g, 
    CommunityMap& communities, 
    const WeightMap& w,
    URBG&& gen,
    QualityFunction f,
    typename property_traits<WeightMap>::value_type min_improvement_inner,
    std::true_type /* incremental */
)
{
    // Graph concept checks
    BOOST_CONCEPT_ASSERT((VertexListGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((IncidenceGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((GraphPartitionQualityFunctionConcept<QualityFunction, Graph, CommunityMap, WeightMap>));
    BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<CommunityMap, typename graph_traits<Graph>::vertex_descriptor>));
    BOOST_CONCEPT_ASSERT((ReadablePropertyMapConcept<WeightMap, typename graph_traits<Graph>::edge_descriptor>));

    // Louvain modularity is defined for undirected graphs only
    static_assert(
        std::is_convertible<typename graph_traits<Graph>::directed_category, undirected_tag>::value,
        "louvain_clustering requires an undirected graph"
    );

    using community_type = typename property_traits<CommunityMap>::value_type;
    using weight_type = typename property_traits<WeightMap>::value_type;
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    using vertex_iterator = typename graph_traits<Graph>::vertex_iterator;
    using out_edge_iterator = typename graph_traits<Graph>::out_edge_iterator;
    
    std::size_t n = num_vertices(g);
    
    // Use vertex_index map to support both integral (vecS) and non-integral (listS) vertex descriptors
    auto idx_map = get(vertex_index, g);
    
    // Storage vectors backing property maps: enables both vertex_descriptor and integer access
    std::vector<weight_type> k_vec(n, weight_type(0));
    std::vector<weight_type> in_vec(n, weight_type(0));
    std::vector<weight_type> tot_vec(n, weight_type(0));
    
    // Property map views with vertex_descriptor keys (through idx_map)
    auto k = make_iterator_property_map(k_vec.begin(), idx_map);
    auto in = make_iterator_property_map(in_vec.begin(), idx_map);
    auto tot = make_iterator_property_map(tot_vec.begin(), idx_map);
    weight_type m;
    
    // populates k[c], in[c], tot[c]
    weight_type Q = f.quality(g, communities, w, k, in, tot, m);
    weight_type Q_new = Q;
    std::size_t num_moves = 0;
    bool has_rolled_back = false;
    
    // Randomize vertex order once
    std::vector<vertex_descriptor> vertex_order = louvain_detail::get_vertex_vector(g);
    std::shuffle(vertex_order.begin(), vertex_order.end(), gen);
    
    // Pre-allocate neighbor buffers
    std::vector<weight_type> neigh_weight_vec(n, weight_type(0));
    auto neigh_weight = make_iterator_property_map(neigh_weight_vec.begin(), idx_map);
    std::vector<community_type> neigh_comm;
    neigh_comm.reserve(100);
    
    // Pre-allocate rollback buffers (only used if needed)
    std::vector<community_type> saved_partition;
    std::vector<weight_type> saved_in_vec;
    std::vector<weight_type> saved_tot_vec;
    
    do
    {
        Q = Q_new;
        num_moves = 0;        
        
        // Lazy save: only save state if we've previously needed to rollback
        if (has_rolled_back) {
            saved_partition.resize(num_vertices(g));
            saved_in_vec = in_vec;
            saved_tot_vec = tot_vec;
            vertex_iterator vi_save, vi_save_end;
            for (boost::tie(vi_save, vi_save_end) = vertices(g); vi_save != vi_save_end; ++vi_save) {
                saved_partition[get(vertex_index, g, *vi_save)] = get(communities, *vi_save);
            }
        }
        
        for(auto v : vertex_order)
        {
            community_type c_old = get(communities, v);
            weight_type k_v = get(k, v);
            weight_type w_selfloop = 0;
            
            // Single-pass neighbor aggregation using pre-allocated vector
            neigh_comm.clear();
            out_edge_iterator ei, ei_end;
            for (boost::tie(ei, ei_end) = out_edges(v, g); ei != ei_end; ++ei) {
                vertex_descriptor neighbor = target(*ei, g);
                weight_type edge_w = get(w, *ei);
                if (neighbor == v) {
                    w_selfloop += edge_w;
                } else {
                    community_type c_neighbor = get(communities, neighbor);
                    if (get(neigh_weight, c_neighbor) == weight_type(0)) {
                        neigh_comm.push_back(c_neighbor);
                    }
                    put(neigh_weight, c_neighbor, get(neigh_weight, c_neighbor) + edge_w);
                }
            }
            
            // Remove v from community
            weight_type k_v_in_old = get(neigh_weight, c_old);
            f.remove(in, tot, c_old, k_v, k_v_in_old, w_selfloop);
            
            // Find best community
            community_type c_best = c_old;
            weight_type best_gain = 0;
            
            for(community_type c_neighbor : neigh_comm)
            {
                weight_type k_v_in_neighbor = get(neigh_weight, c_neighbor);
                weight_type gain = f.gain(tot, m, c_neighbor, k_v_in_neighbor, k_v);
                
                if (gain > best_gain) {
                    best_gain = gain;
                    c_best = c_neighbor;
                }
            }
            
            // Insert v into best community
            if (c_best != c_old) {
                put(communities, v, c_best);
                num_moves++;
            }
            
            weight_type k_v_in_best = get(neigh_weight, c_best);
            f.insert(in, tot, c_best, k_v, k_v_in_best, w_selfloop);
            
            // Clear neighbor weights for next vertex
            for(community_type c : neigh_comm) {
                put(neigh_weight, c, weight_type(0));
            }
        }

        // Compute quality from incremental in/tot (no graph traversal)
        // Use integer-keyed views into the same underlying vectors
        {
            auto in_idx = make_iterator_property_map(in_vec.begin(), boost::typed_identity_property_map<std::size_t>());
            auto tot_idx = make_iterator_property_map(tot_vec.begin(), boost::typed_identity_property_map<std::size_t>());
            Q_new = f.quality(in_idx, tot_idx, m, n);
        }
        
        // Rollback if quality didn't improve after moving nodes
        // Prevent endless oscillations of vertices: algo gets one extra pass before giving up
        if (num_moves > 0 && Q_new <= Q) {
            if (has_rolled_back) {
                vertex_iterator vi_restore, vi_restore_end;
                for (boost::tie(vi_restore, vi_restore_end) = vertices(g); vi_restore != vi_restore_end; ++vi_restore) {
                    put(communities, *vi_restore, saved_partition[get(vertex_index, g, *vi_restore)]);
                }
                in_vec = saved_in_vec;
                tot_vec = saved_tot_vec;
                Q_new = Q;
                break;
            } else {
                has_rolled_back = true;
            }
        }
        
    } while (num_moves > 0 && (Q_new - Q) > min_improvement_inner);
    
    return Q_new;
}

/// @brief Slow version, requires the QualityFunction to implement GraphPartitionQualityFunctionConcept
template <typename QualityFunction = newman_and_girvan, typename Graph, typename CommunityMap, typename WeightMap, typename URBG>
typename property_traits<WeightMap>::value_type
local_optimization_impl(
    const Graph& g, 
    CommunityMap& communities, 
    const WeightMap& w,
    URBG&& gen,
    QualityFunction f,
    typename property_traits<WeightMap>::value_type min_improvement_inner,
    std::false_type /* non incremental */
)
{
    // Graph concept checks
    BOOST_CONCEPT_ASSERT((VertexListGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((IncidenceGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((GraphPartitionQualityFunctionConcept<QualityFunction, Graph, CommunityMap, WeightMap>));
    BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<CommunityMap, typename graph_traits<Graph>::vertex_descriptor>));
    BOOST_CONCEPT_ASSERT((ReadablePropertyMapConcept<WeightMap, typename graph_traits<Graph>::edge_descriptor>));

    static_assert(
        std::is_convertible<typename graph_traits<Graph>::directed_category, undirected_tag>::value,
        "louvain_clustering requires an undirected graph"
    );

    using community_type = typename property_traits<CommunityMap>::value_type;
    using weight_type = typename property_traits<WeightMap>::value_type;
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    using vertex_iterator = typename graph_traits<Graph>::vertex_iterator;
    using out_edge_iterator = typename graph_traits<Graph>::out_edge_iterator;

    // Randomize vertex order once
    std::vector<vertex_descriptor> vertex_order = louvain_detail::get_vertex_vector(g);
    std::shuffle(vertex_order.begin(), vertex_order.end(), gen);

    weight_type Q = f.quality(g, communities, w);
    weight_type Q_new = Q;
    std::size_t num_moves = 0;
    bool has_rolled_back = false;

    // Pre-allocate rollback buffer
    std::vector<community_type> saved_partition;

    do
    {
        Q = Q_new;
        num_moves = 0;

        // Lazy save: only save state if we've previously needed to rollback
        if (has_rolled_back) {
            saved_partition.resize(num_vertices(g));
            vertex_iterator vi_save, vi_save_end;
            for (boost::tie(vi_save, vi_save_end) = vertices(g); vi_save != vi_save_end; ++vi_save) {
                saved_partition[get(vertex_index, g, *vi_save)] = get(communities, *vi_save);
            }
        }

        for (auto v : vertex_order)
        {
            community_type c_old = get(communities, v);

            // Collect unique neighbor communities
            boost::unordered_flat_set<community_type> neigh_comms;
            out_edge_iterator ei, ei_end;
            for (boost::tie(ei, ei_end) = out_edges(v, g); ei != ei_end; ++ei) {
                vertex_descriptor neighbor = target(*ei, g);
                if (neighbor != v) {
                    neigh_comms.insert(get(communities, neighbor));
                }
            }

            // Try each neighbor community, keep best
            community_type c_best = c_old;
            weight_type best_Q = Q_new;

            for (community_type c_try : neigh_comms)
            {
                if (c_try == c_old)
                    continue;

                put(communities, v, c_try);
                weight_type Q_try = f.quality(g, communities, w);

                if (Q_try > best_Q)
                {
                    best_Q = Q_try;
                    c_best = c_try;
                }
            }

            // Commit best move or restore
            put(communities, v, c_best);
            if (c_best != c_old)
            {
                num_moves++;
                Q_new = best_Q;
            }
        }

        // Recompute quality (might differ from tracked Q_new due to interaction effects)
        Q_new = f.quality(g, communities, w);

        // Rollback if quality didn't improve after moving nodes
        if (num_moves > 0 && Q_new <= Q) {
            if (has_rolled_back) {
                vertex_iterator vi_restore, vi_restore_end;
                for (boost::tie(vi_restore, vi_restore_end) = vertices(g); vi_restore != vi_restore_end; ++vi_restore) {
                    put(communities, *vi_restore, saved_partition[get(vertex_index, g, *vi_restore)]);
                }
                Q_new = Q;
                break;
            } else {
                has_rolled_back = true;
            }
        }

    } while (num_moves > 0 && (Q_new - Q) > min_improvement_inner);

    return Q_new;
}

template <typename QualityFunction = newman_and_girvan, typename Graph, typename CommunityMap, typename WeightMap, typename URBG>
typename property_traits<WeightMap>::value_type
local_optimization(
    const Graph& g, 
    CommunityMap& communities, 
    const WeightMap& w,
    URBG&& gen,
    QualityFunction f = QualityFunction{},
    typename property_traits<WeightMap>::value_type min_improvement_inner = typename property_traits<WeightMap>::value_type(0.0)
){
    using is_incremental = louvain_detail::is_incremental_quality_function<QualityFunction, Graph, CommunityMap, WeightMap>;
    return local_optimization_impl(g, communities, w, std::forward<URBG>(gen), f, min_improvement_inner, is_incremental{});
}

} // namespace louvain_detail

/// @brief Find the best partition of the vertices of a graph conditionally to a quality function like modularity.
/// @return the modularity value of the best partition.
template <typename QualityFunction = newman_and_girvan, typename Graph, typename ComponentMap, typename WeightMap, typename URBG>
typename property_traits<WeightMap>::value_type
louvain_clustering(
    const Graph& g0,
    ComponentMap components,
    const WeightMap& w0,
    URBG&& gen,
    QualityFunction f = QualityFunction{},
    typename property_traits<WeightMap>::value_type min_improvement_inner = typename property_traits<WeightMap>::value_type(0.0),
    typename property_traits<WeightMap>::value_type min_improvement_outer = typename property_traits<WeightMap>::value_type(0.0)
){
    // Graph concept checks
    BOOST_CONCEPT_ASSERT((VertexListGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((IncidenceGraphConcept<Graph>));
    BOOST_CONCEPT_ASSERT((GraphPartitionQualityFunctionConcept<QualityFunction, Graph, ComponentMap, WeightMap>));
    BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<ComponentMap, typename graph_traits<Graph>::vertex_descriptor>));
    BOOST_CONCEPT_ASSERT((ReadablePropertyMapConcept<WeightMap, typename graph_traits<Graph>::edge_descriptor>));

    static_assert(
        std::is_convertible<typename graph_traits<Graph>::directed_category, undirected_tag>::value,
        "louvain_clustering requires an undirected graph"
    );

    using weight_type = typename property_traits<WeightMap>::value_type;
    using vertex_iterator = typename graph_traits<Graph>::vertex_iterator;
    
    auto idx = get(vertex_index, g0);
    std::size_t n = num_vertices(g0);
    
    // Initialize each vertex to its own community
    vertex_iterator vi, vi_end;
    for (boost::tie(vi, vi_end) = vertices(g0); vi != vi_end; ++vi) {
        put(components, *vi, *vi);
    }
    
    // Dispatch the local optimization using the incremental or non-incremental variant
    weight_type Q = louvain_detail::local_optimization(g0, components, w0, gen, f, min_improvement_inner);
    
    // Build index-based partition from community map for aggregation
    std::vector<std::size_t> vertex_index_to_community(n);
    for (boost::tie(vi, vi_end) = vertices(g0); vi != vi_end; ++vi) {
        vertex_index_to_community[get(idx, *vi)] = get(idx, get(components, *vi));
    }
    
    using level_t = std::vector<std::vector<std::size_t>>;
    std::vector<level_t> levels;
    
    auto partition_idx_map = make_iterator_property_map(vertex_index_to_community.begin(), idx);
    auto coarse = louvain_detail::aggregate(g0, partition_idx_map, w0, idx);
    
    std::size_t prev_n_vertices = n;
    while (true) {
        // Check convergence: graph didn't get smaller (no communities merged)
        std::size_t n_communities = num_vertices(coarse.graph);
        
        if (n_communities >= prev_n_vertices || n_communities == 1) {
            break;
        }
        
        levels.push_back(std::move(coarse.vertex_mapping));
        weight_type Q_old = Q;

        // Dispatch the local optimization using the incremental or non-incremental variant
        weight_type Q_agg = louvain_detail::local_optimization(coarse.graph, coarse.partition, get(edge_weight, coarse.graph), gen, f, min_improvement_inner);
        
        // Build communities vector from coarsened graph (vecS/vecS to indices = descriptors)
        std::size_t n_coarse = num_vertices(coarse.graph);
        std::vector<std::size_t> coarse_communities(n_coarse);
        for (std::size_t v = 0; v < n_coarse; ++v) {
            coarse_communities[v] = get(coarse.partition, v);
        }

        vertex_index_to_community = louvain_detail::unfold(coarse_communities, levels, n);        
        Q = Q_agg;

        // Stop if quality did not improve
        if (Q - Q_old <= min_improvement_outer) {
            break;
        }
        
        prev_n_vertices = n_communities;
        coarse = louvain_detail::aggregate(coarse.graph, 
                                           coarse.partition,
                                           get(edge_weight, coarse.graph),
                                           get(vertex_index, coarse.graph));
    }

    boost::unordered_flat_map<std::size_t, std::size_t> comm_label;
    std::size_t next_label = 0;
    
    for (boost::tie(vi, vi_end) = vertices(g0); vi != vi_end; ++vi) {
        auto community = vertex_index_to_community[get(idx, *vi)]; // can be arbitrary and uncontiguous
        auto it = comm_label.find(community);
        if (it == comm_label.end()) {
            it = comm_label.emplace(community, next_label++).first; // remap labels to 0, 1 ... i 
        }
        put(components, *vi, it->second);
    }
    
    return Q;
}

} // namespace boost

#endif
