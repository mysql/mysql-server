// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TURN_OPERATION_ID_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TURN_OPERATION_ID_HPP

#include <cstddef>
#include <ostream>
#include <tuple>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct turn_operation_id
{
    std::size_t turn_index{0};
    int operation_index{0};

    bool operator<(turn_operation_id const& other) const
    {
        return std::tie(turn_index, operation_index) < std::tie(other.turn_index, other.operation_index);
    }

    bool operator==(turn_operation_id const& other) const
    {
        return std::tie(turn_index, operation_index) == std::tie(other.turn_index, other.operation_index);
    }

    bool operator!=(turn_operation_id const& other) const
    {
        return ! operator==(other);
    }

    friend std::ostream& operator<<(std::ostream& os, turn_operation_id const& toi)
    {
        os << toi.turn_index << "[" << toi.operation_index << "]";
        return os;
    }
};    

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TURN_OPERATION_ID_HPP
