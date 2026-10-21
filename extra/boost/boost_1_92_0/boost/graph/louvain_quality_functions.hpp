//=======================================================================
// Copyright 2026 Becheler Code Labs for C++ Alliance
// Authors: Arnaud Becheler
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#ifndef BOOST_GRAPH_LOUVAIN_QUALITY_FUNCTIONS_HPP
#define BOOST_GRAPH_LOUVAIN_QUALITY_FUNCTIONS_HPP

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/concept/assert.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/type_traits/make_void.hpp>
#include <boost/core/ignore_unused.hpp>
#include <utility>

namespace boost 
{

namespace centrality_detail {

    /// @brief Detect if vertex_descriptor is integral (vecS) or pointer-like (listS/setS)
    template <typename Graph>
    struct uses_vector_storage : std::is_integral<typename graph_traits<Graph>::vertex_descriptor> {};
    
    /// @brief Detect if type is hashable, but naive, for BGL integral types are hashable
    template <typename T>
    struct is_hashable : std::is_integral<T> {};
    
    /// @brief Vertex property map selector
    template <typename Graph, typename ValueType, bool IsIntegral = uses_vector_storage<Graph>::value>
    struct vertex_pmap_selector;

    /// @brief vecS specialization uses vector: get(k,v) O(1), put(k, v, val) O(1), space O(V) pre-allocated array
    template <typename Graph, typename ValueType>
    struct vertex_pmap_selector<Graph, ValueType, true> {
        using type = vector_property_map<ValueType>;
    };

    /// @brief listS/setS specialization uses flat_map for safety: get(k,v) O(1) average, put(k, v, val) O(1) average, space O(V)
    template <typename Graph, typename ValueType>
    struct vertex_pmap_selector<Graph, ValueType, false> {
        using type = associative_property_map<boost::unordered_flat_map<typename graph_traits<Graph>::vertex_descriptor, ValueType>>;
    };
    
    /// @brief Community storage selector: picks optimal container based on hashability
    template <typename CommunityType, typename ValueType, bool IsHashable = is_hashable<CommunityType>::value>
    struct community_storage_selector;
    
    /// @brief Hashable types use unordered_flat_map: get/put O(1) average, better cache locality
    template <typename CommunityType, typename ValueType>
    struct community_storage_selector<CommunityType, ValueType, true> {
        using type = boost::unordered_flat_map<CommunityType, ValueType>;
    };
    
    /// @brief Non-hashable types use flat_map: get/put O(1) average
    template <typename CommunityType, typename ValueType>
    struct community_storage_selector<CommunityType, ValueType, false> {
        using type = boost::unordered_flat_map<CommunityType, ValueType>;
    };
}

/// @brief Quality Function concept for graph partition quality metrics (e.g., Louvain algorithm)
/// @see Campigotto, R., Céspedes, P. C., & Guillaume, J. L. (2014). A generalized and adaptive method for community detection.
template<class QualityFunction, class Graph, class CommunityMap, class WeightMap>
struct GraphPartitionQualityFunctionConcept
{
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    using weight_type = typename property_traits<WeightMap>::value_type;
    using community_type = typename property_traits<CommunityMap>::value_type;

    Graph g;
    CommunityMap cmap;
    WeightMap wmap;

    void constraints()
    {
        // Full computation from graph traversal
        QualityFunction f;
        weight_type q1 = f.quality(g, cmap, wmap);
        boost::ignore_unused(q1);
    }
};

/// @brief Quality Function concept for incremental versions of graph partition quality metrics (e.g., used in Louvain algorithm)
/// @see Campigotto, R., Céspedes, P. C., & Guillaume, J. L. (2014). A generalized and adaptive method for community detection.
template <typename QualityFunction, typename Graph, typename CommunityMap, typename WeightMap>
struct GraphPartitionQualityFunctionIncrementalConcept : GraphPartitionQualityFunctionConcept<QualityFunction, Graph, CommunityMap, WeightMap>
{
    using weight_type = typename property_traits<WeightMap>::value_type;
    using community_type = typename property_traits<CommunityMap>::value_type;
    using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
    
    void constraints()
    {
        GraphPartitionQualityFunctionConcept<QualityFunction, Graph, CommunityMap, WeightMap>::constraints();

        QualityFunction f;

        // Full computation from graph traversal, user-provided maps
        weight_type q2 = f.quality(g, cmap, wmap, k, in, tot, m);
        
        // Fast computation from pre-maintained community maps (no traversal)
        weight_type q3 = f.quality(in, tot, m, num_communities);

        // Incremental update: remove vertex from its community
        f.remove(in, tot, comm, weight_val, weight_val, weight_val);
        
        // Incremental update: insert vertex into a community
        f.insert(in, tot, comm, weight_val, weight_val, weight_val);
        
        // Incremental update: Modularity gain of moving a vertex to a target community
        weight_type gain = f.gain(tot, m, comm, weight_val, weight_val);
        boost::ignore_unused(q2);
        boost::ignore_unused(q3);
        boost::ignore_unused(gain);
    }
        
    Graph g;
    CommunityMap cmap;
    WeightMap wmap;
    vector_property_map<weight_type> k;
    associative_property_map<boost::unordered_flat_map<community_type, weight_type>> in;
    associative_property_map<boost::unordered_flat_map<community_type, weight_type>> tot;
    weight_type m;
    weight_type weight_val;
    community_type comm;
    std::size_t num_communities;
};

namespace louvain_detail {

/// @brief Type trait to detect if a quality function supports incremental updates.
/// Uses SFINAE to check for the existence of f.gain(...).
template<class QualityFunction, class Graph, class CommunityMap, class WeightMap, typename=void>
struct is_incremental_quality_function : std::false_type{};

template<class QualityFunction, class Graph, class CommunityMap, class WeightMap>
struct is_incremental_quality_function<
    QualityFunction, 
    Graph, 
    CommunityMap, 
    WeightMap,
    boost::void_t<decltype(
        std::declval<QualityFunction>().gain(
            std::declval<
                associative_property_map<
                    boost::unordered_flat_map<
                        typename property_traits<CommunityMap>::value_type,
                        typename property_traits<WeightMap>::value_type
                    >
                >
            >(),
            std::declval<typename property_traits<WeightMap>::value_type>(),
            std::declval<typename property_traits<CommunityMap>::value_type>(),
            std::declval<typename property_traits<WeightMap>::value_type>(),
            std::declval<typename property_traits<WeightMap>::value_type>()
        )
    )>
> : std::true_type{};

} // namespace louvain_detail

// Modularity: Q = sum_c [ (L_c/m) - (k_c/2m)^2 ]
// L_c = internal edge weight for community c
// k_c = sum of degrees in community c
// m = total edge weight / 2
struct newman_and_girvan
{

    /// @brief Traverse the graph to compute partition quality with user-provided property maps
    template <class Graph, class CommunityMap, class WeightMap, class VertexDegreeMap, class CommunityInMap, class CommunityTotMap>
    inline typename property_traits<WeightMap>::value_type
    quality(
        const Graph& g, 
        const CommunityMap& communities, 
        const WeightMap& weights,
        VertexDegreeMap k,
        CommunityInMap in,
        CommunityTotMap tot,
        typename property_traits<WeightMap>::value_type& m
    )
    {
        using community_type = typename property_traits<CommunityMap>::value_type;
        using weight_type = typename property_traits<WeightMap>::value_type;
        using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
        using edge_iterator = typename graph_traits<Graph>::edge_iterator;

        m = weight_type(0);
        
        // Collect all communities and initialize maps
        boost::unordered_flat_set<community_type> communities_set;
        typename graph_traits<Graph>::vertex_iterator vi, vi_end;
        for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi) {
            // init user provided maps
            put(k, *vi, weight_type(0));
            community_type c = get(communities, *vi);
            if (communities_set.insert(c).second) {
                // First time seeing this community
                put(in, c, weight_type(0));
                put(tot, c, weight_type(0));
            }
        }

        for(const auto&c : communities_set){
            // init user provided maps
            put(in, c, weight_type(0));
            put(tot, c, weight_type(0));
        }

        edge_iterator ei, ei_end;
        for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
        {
            vertex_descriptor src = source(*ei, g);
            vertex_descriptor trg = target(*ei, g);
            weight_type w = get(weights, *ei);
            community_type c_src = get(communities, src);
            community_type c_trg = get(communities, trg);
            
            if (src == trg) {
                // Self-loop counts twice (once per endpoint)
                put(k, src, get(k, src) + 2 * w);
                put(tot, c_src, get(tot, c_src) + 2 * w);
                put(in, c_src, get(in, c_src) + 2 * w);
                m += 2 * w;
            } else {
                // Regular edge
                put(k, src, get(k, src) + w);
                put(k, trg, get(k, trg) + w);
                put(tot, c_src, get(tot, c_src) + w);
                put(tot, c_trg, get(tot, c_trg) + w);
                m += 2 * w;
                
                if (c_src == c_trg) {
                    put(in, c_src, get(in, c_src) + 2 * w);
                }
            }
        }

        // m = (sum of all degrees) / 2
        m /= weight_type(2);

        weight_type two_m = weight_type(2) * m;
        
        // Empty graphs have zero modularity
        if (two_m == weight_type(0)){
            return weight_type(0);  
        }

        // Q formula: sum_c [ (2*L_c - K_c^2/(2m)) ] / (2m)
        weight_type Q(0);
        for (const auto& c : communities_set)
        {
            weight_type K_c = get(tot, c);
            weight_type two_L_c = get(in, c);
            Q += two_L_c - (K_c * K_c) / two_m;
        }
        Q /= two_m;
        return Q;
    }
    
    /// @brief Traverse the graph to compute partition quality with internally allocated property maps
    template <typename Graph, typename CommunityMap, typename WeightMap>
    inline typename property_traits<WeightMap>::value_type
    quality(const Graph& g, const CommunityMap& communities, const WeightMap& weights)
    {
        using community_type = typename property_traits<CommunityMap>::value_type;
        using weight_type = typename property_traits<WeightMap>::value_type;
        using vertex_descriptor = typename graph_traits<Graph>::vertex_descriptor;
        using community_storage_t = typename centrality_detail::community_storage_selector<community_type, weight_type>::type;

        // Use unordered_flat_map for vertex degree storage
        // (associative_property_map's default ctor wraps a null pointer, so
        //  we must create the underlying storage first)
        boost::unordered_flat_map<vertex_descriptor, weight_type> k_storage;
        auto k = make_assoc_property_map(k_storage);
        community_storage_t in_map;
        community_storage_t tot_map;
        auto in = make_assoc_property_map(in_map);
        auto tot = make_assoc_property_map(tot_map);
        weight_type m;
        
        return quality(g, communities, weights, k, in, tot, m);
    }

        
    /**
     * Compute modularity from incrementally maintained maps:
     * Faster than quality() as it doesn't traverse the graph.
     * @param in Property map: community -> internal edge weights
     * @param tot Property map: community -> total edge weights  
     * @param m Total edge weight (half sum of all degrees)
     * @param num_communities Number of communities to check
     * @return Modularity Q
     */
    template<typename CommunityInMap, typename CommunityTotMap, typename WeightType>
    inline WeightType quality(CommunityInMap in, CommunityTotMap tot, WeightType m, std::size_t num_communities) {
        if (m == WeightType(0)) {
            return WeightType(0);
        }
        
        WeightType Q = 0;
        WeightType two_m = WeightType(2) * m;
        
        for (std::size_t c = 0; c < num_communities; ++c) {
            WeightType K_c = get(tot, c);
            if (K_c > WeightType(0)) {
                WeightType two_L_c = get(in, c);
                Q += two_L_c - (K_c * K_c) / two_m;
            }
        }
        
        return Q / two_m;
    }

    /**
     * Remove vertex from community.
     * @param in Property map: community to internal edge weights
     * @param tot Property map: community to total edge weights
     * @param old_comm Community to remove from
     * @param k_v vertex total degree
     * @param k_v_in_old Sum of edge weights from vertex to vertices in old_comm
     * @param w_selfloop Self-loop weight (default 0)
     */
    template<typename CommunityInMap, typename CommunityTotMap, typename CommunityType, typename WeightType>
    static inline void remove(
        CommunityInMap in,
        CommunityTotMap tot,
        CommunityType old_comm,
        WeightType k_v,
        WeightType k_v_in_old,
        WeightType w_selfloop = WeightType(0)
    )
    {
        put(in, old_comm, get(in, old_comm) - (2 * k_v_in_old + w_selfloop));
        put(tot, old_comm, get(tot, old_comm) - k_v);
    }
    
    /**
     * Insert node into community.
     * @param in Property map: community to internal edge weights
     * @param tot Property map: community to total edge weights
     * @param new_comm Community to insert into
     * @param k_v Node's total degree
     * @param k_v_in_new Sum of edge weights from node to vertices in new_comm
     * @param w_selfloop Self-loop weight (default 0)
     */
    template<typename CommunityInMap, typename CommunityTotMap, typename CommunityType, typename WeightType>
    inline void insert(
        CommunityInMap in,
        CommunityTotMap tot,
        CommunityType new_comm,
        WeightType k_v,
        WeightType k_v_in_new,
        WeightType w_selfloop = WeightType(0)
    )
    {
        put(in, new_comm, get(in, new_comm) + (2 * k_v_in_new + w_selfloop));
        put(tot, new_comm, get(tot, new_comm) + k_v);
    }
    
    /**
     * Compute modularity gain of moving node to target community.
     * @param tot Property map: community to total edge weights
     * @param m Total edge weight (half sum of all degrees)
     * @param target_comm Community to evaluate
     * @param k_v_in_target Sum of edge weights from node to vertices in target_comm
     * @param k_v Node's total degree
     * @return Modularity gain
     */
    template<typename CommunityTotMap, typename CommunityType, typename WeightType>
    inline WeightType gain(
        CommunityTotMap tot,
        WeightType m,
        CommunityType target_comm,
        WeightType k_v_in_target,
        WeightType k_v
    ) {
        // Empty graph check
        if (m == WeightType(0)) {
            return WeightType(0);
        }
        
        // Gain = k_v_in_target - tot[target] * k_v / (2m)
        WeightType tot_target = get(tot, target_comm);
        return k_v_in_target - (tot_target * k_v) / (WeightType(2) * m);
    }

}; // newman_and_girvan

} // namespace boost

#endif // BOOST_GRAPH_LOUVAIN_QUALITY_FUNCTIONS_HPP
