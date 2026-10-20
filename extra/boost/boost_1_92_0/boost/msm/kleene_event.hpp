// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_KLEENE_EVENT_H
#define BOOST_MSM_KLEENE_EVENT_H

#include <boost/mpl/bool.hpp>
#include <type_traits>

namespace boost { namespace msm
{

// default: no event is a kleene event (kleene: matches any event in a transitions)
template <typename Event>
struct is_kleene_event : std::false_type {};

}} // boost::msm

#endif // BOOST_MSM_KLEENE_EVENT_H
