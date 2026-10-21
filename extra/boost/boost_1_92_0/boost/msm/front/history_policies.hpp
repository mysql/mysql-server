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

#ifndef BOOST_MSM_FRONT_HISTORY_POLICIES_H
#define BOOST_MSM_FRONT_HISTORY_POLICIES_H

namespace boost { namespace msm { namespace front
{

// Configurable history policies for state machine definitions
// (only used by backmp11).

/// No history (default).
struct no_history {};

/// Shallow history.
/// For deep history use this policy in all contained state machines.
template <typename... Events>
struct shallow_history {};

/// Shallow history for all entry events.
/// For deep history use this policy in all contained state machines.
struct always_shallow_history {};

}}} // boost::msm::front

#endif //BOOST_MSM_FRONT_HISTORY_POLICIES_H
