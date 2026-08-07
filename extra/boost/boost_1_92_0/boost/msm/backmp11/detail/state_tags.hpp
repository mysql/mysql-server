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

#ifndef BOOST_MSM_BACKMP11_DETAIL_STATE_TAGS_HPP
#define BOOST_MSM_BACKMP11_DETAIL_STATE_TAGS_HPP

#include <boost/mp11.hpp>

#include <boost/msm/front/detail/state_tags.hpp>

namespace boost::msm::backmp11::detail
{

// States
struct state_machine_tag {};
template <typename T>
using has_state_machine_tag = std::is_same<typename T::internal::tag, state_machine_tag>;
template <typename T>
using is_composite = mp11::mp_or<
    std::is_same<typename T::internal::tag, msm::front::detail::composite_state_tag>,
    has_state_machine_tag<T>
    >;

// Pseudostates
struct explicit_entry_be_tag {};
template <typename T>
using has_explicit_entry_be_tag = std::is_same<typename T::internal::tag, explicit_entry_be_tag>;

struct entry_pseudostate_be_tag {};
template <typename T>
using has_entry_pseudostate_be_tag = std::is_same<typename T::internal::tag, entry_pseudostate_be_tag>;

struct exit_pseudostate_be_tag {};
template <typename T>
using has_exit_pseudostate_be_tag = std::is_same<typename T::internal::tag, exit_pseudostate_be_tag>;

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_STATE_TAGS_HPP
