// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_EVENT_TRAITS_HPP
#define BOOST_MSM_BACKMP11_EVENT_TRAITS_HPP

#include <boost/msm/kleene_event.hpp>
#include <any>

namespace boost::msm
{

// add this way in this namespace specializations for events which you want to use as kleene
// requirement: a copy-constructor matching the events which will be converted to this kleene
template <>
struct is_kleene_event<std::any> : std::true_type {};

} // namespace boost::msm

#endif //BOOST_MSM_EVENT_TRAITS_HPP
