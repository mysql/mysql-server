// Copyright 2026 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_DETAIL_COMMON_HPP
#define BOOST_MSM_BACKMP11_DETAIL_COMMON_HPP

#include <boost/msm/backmp11/common_types.hpp>

namespace boost::msm::backmp11::detail
{

class process_guard
{
  public:
    explicit process_guard(machine_state& machine_state)
        : m_machine_state(machine_state)
    {
        m_machine_state = machine_state::processing;
    }

    ~process_guard()
    {
        m_machine_state = machine_state::idle;
    }

  private:
    machine_state& m_machine_state;
};

// Additional info required for event processing.
enum class process_info
{
    direct_call,
    submachine_call,
    event_pool
};

// Bitmask for process result checks.
constexpr process_result consumed_or_deferred =
    process_result::consumed | process_result::deferred;

constexpr bool any(process_result result)
{
    return result != process_result::discarded;
}

template <typename Policy, typename = void>
struct compile_policy_impl;

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_COMMON_HPP
