// Boost.Bimap
//
// Copyright (c) 2025 Joaquin M Lopez Munoz
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_BIMAP_DETAIL_MPL_TO_MP11_LIST_HPP
#define BOOST_BIMAP_DETAIL_MPL_TO_MP11_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/mp11/list.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/next.hpp>

/** \struct boost::bimaps::detail::mpl_to_mp11_list

\brief Converts a MPL sequence to a Mp11 list

\code
using mp11_list = mpl_to_mp11_list< mpl_sequence >;
\endcode
                                                                        **/

#ifndef BOOST_BIMAP_DOXYGEN_WILL_NOT_PROCESS_THE_FOLLOWING_LINES

namespace boost {
namespace bimaps {
namespace detail {

template< typename First, typename Last, typename... Ts >
struct mpl_to_mp11_list_impl: mpl_to_mp11_list_impl
<
    typename mpl::next<First>::type, Last,
    Ts..., typename mpl::deref<First>::type
> {};

template< typename Last, typename... Ts >
struct mpl_to_mp11_list_impl< Last, Last, Ts... >
{
    using type = mp11::mp_list< Ts... >;
};

template< typename TypeList >
using mpl_to_mp11_list=typename mpl_to_mp11_list_impl
<
    typename mpl::begin<TypeList>::type,
    typename mpl::end<TypeList>::type
>::type;

} // namespace detail
} // namespace bimaps
} // namespace boost

#endif // BOOST_BIMAP_DOXYGEN_WILL_NOT_PROCESS_THE_FOLLOWING_LINES

#endif // BOOST_BIMAP_DETAIL_MPL_TO_MP11_LIST_HPP
