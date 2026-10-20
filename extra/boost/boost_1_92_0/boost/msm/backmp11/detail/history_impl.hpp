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

#ifndef BOOST_MSM_BACKMP11_DETAIL_HISTORY_IMPL_HPP
#define BOOST_MSM_BACKMP11_DETAIL_HISTORY_IMPL_HPP

#include <boost/msm/backmp11/common_types.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/front/history_policies.hpp>

namespace boost::msm::backmp11::detail
{

template <typename History, typename InitialStateIds>
class history_impl;

template <typename InitialStateIds>
class history_impl<front::no_history, InitialStateIds>
{
  public:
    template <typename StateMachine, typename Event>
    static void on_entry(StateMachine& sm, const Event&)
    {
        sm.m_active_state_ids = value_array<InitialStateIds>;
        if constexpr (StateMachine::has_event_pool)
        {
            sm.get_event_pool().events.clear();
        }
    }

    template <typename StateMachine, typename Visitor>
    static void on_entry(StateMachine& sm, Visitor&& visitor)
    {
        mp11::mp_for_each<InitialStateIds>(
            [&sm, &visitor](auto state_id)
            {
                auto& state = std::get<decltype(state_id)::value>(sm.m_states);
                visitor(state);
            });
    }
};

template <typename InitialStateIds>
class history_impl<front::always_shallow_history, InitialStateIds>
{
  public:
    template <typename StateMachine, typename Event>
    static void on_entry(StateMachine&, const Event&)
    {
    }

    template <typename StateMachine, typename Visitor>
    static void on_entry(StateMachine& sm, Visitor&& visitor)
    {
        sm.template visit<visit_mode::active_non_recursive>(visitor);
    }
};

template <typename... Events, typename InitialStateIds>
class history_impl<front::shallow_history<Events...>, InitialStateIds>
    : public history_impl<front::always_shallow_history, InitialStateIds>
{
    using events = mp11::mp_list<Events...>;

  public:
    template <typename StateMachine, typename Event>
    static void on_entry(StateMachine& sm, const Event&)
    {
        if constexpr (!mp11::mp_contains<events, Event>::value)
        {
            sm.m_active_state_ids = value_array<InitialStateIds>;
            if constexpr (StateMachine::has_event_pool)
            {
                sm.get_event_pool().events.clear();
            }
        }
    }

    template <typename StateMachine, typename Visitor>
    static void on_entry(StateMachine& sm, Visitor&& visitor)
    {
        sm.template visit<visit_mode::active_non_recursive>(visitor);
    }
};

} // boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_DETAIL_HISTORY_IMPL_HPP
