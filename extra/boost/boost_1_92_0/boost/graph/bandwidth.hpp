// Copyright (c) Jeremy Siek 2001, Marc Wintermantel 2002
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GRAPH_BANDWIDTH_HPP
#define BOOST_GRAPH_BANDWIDTH_HPP

#include <algorithm> // for std::max
#include <boost/config.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/detail/numeric_traits.hpp>

namespace boost
{

template < typename Graph, typename VertexIndexMap >
typename graph_traits< Graph >::vertices_size_type ith_bandwidth(
    typename graph_traits< Graph >::vertex_descriptor i, const Graph& g,
    VertexIndexMap index)
{
    BOOST_USING_STD_MAX();
    using vertices_size_type = typename graph_traits< Graph >::vertices_size_type;
    vertices_size_type b = 0;
    typename graph_traits< Graph >::out_edge_iterator e, end;
    for (boost::tie(e, end) = out_edges(i, g); e != end; ++e)
    {
        vertices_size_type f_i = get(index, i);
        vertices_size_type f_j = get(index, target(*e, g));
        b = max BOOST_PREVENT_MACRO_SUBSTITUTION( b, f_i < f_j ? f_j - f_i : f_i - f_j );
    }
    return b;
}

template < typename Graph >
typename graph_traits< Graph >::vertices_size_type ith_bandwidth(
    typename graph_traits< Graph >::vertex_descriptor i, const Graph& g)
{
    return ith_bandwidth(i, g, get(vertex_index, g));
}

template < typename Graph, typename VertexIndexMap >
typename graph_traits< Graph >::vertices_size_type bandwidth(
    const Graph& g, VertexIndexMap index)
{
    BOOST_USING_STD_MAX();
    using vertices_size_type = typename graph_traits< Graph >::vertices_size_type;
    vertices_size_type b = 0;
    typename graph_traits< Graph >::edge_iterator i, end;
    for (boost::tie(i, end) = edges(g); i != end; ++i)
    {
        vertices_size_type f_i = get(index, source(*i, g));
        vertices_size_type f_j = get(index, target(*i, g));
        b = max BOOST_PREVENT_MACRO_SUBSTITUTION( b, f_i < f_j ? f_j - f_i : f_i - f_j );
    }
    return b;
}

template < typename Graph >
typename graph_traits< Graph >::vertices_size_type bandwidth(const Graph& g)
{
    return bandwidth(g, get(vertex_index, g));
}

template < typename Graph, typename VertexIndexMap >
typename graph_traits< Graph >::vertices_size_type edgesum(
    const Graph& g, VertexIndexMap index_map)
{
    typedef typename graph_traits< Graph >::vertices_size_type size_type;
    typedef
        typename detail::numeric_traits< size_type >::difference_type diff_t;
    size_type sum = 0;
    typename graph_traits< Graph >::edge_iterator i, end;
    for (boost::tie(i, end) = edges(g); i != end; ++i)
    {
        diff_t f_u = get(index_map, source(*i, g));
        diff_t f_v = get(index_map, target(*i, g));
        using namespace std; // to call abs() unqualified
        sum += abs(f_u - f_v);
    }
    return sum;
}

} // namespace boost

#endif // BOOST_GRAPH_BANDWIDTH_HPP
