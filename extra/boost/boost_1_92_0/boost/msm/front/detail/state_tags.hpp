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

#ifndef BOOST_MSM_FRONT_DETAIL_STATE_TAGS_HPP
#define BOOST_MSM_FRONT_DETAIL_STATE_TAGS_HPP

#include <boost/type_traits/is_same.hpp>

namespace boost { namespace msm { namespace front { namespace detail
{

// States
struct state_tag {};
struct composite_state_tag {};

// Pseudostates
struct explicit_entry_tag {};
template <typename T>
using has_explicit_entry_tag = boost::is_same<typename T::internal::tag, explicit_entry_tag>;

struct entry_pseudostate_tag {};
template <typename T>
using has_entry_pseudostate_tag = boost::is_same<typename T::internal::tag, entry_pseudostate_tag>;

struct exit_pseudostate_tag {};
template <typename T>
using has_exit_pseudostate_tag = boost::is_same<typename T::internal::tag, exit_pseudostate_tag>;

}}}} // namespace boost::msm::front::detail

#endif // BOOST_MSM_FRONT_DETAIL_STATE_TAGS_HPP
