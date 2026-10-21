// Copyright 2026 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_INTERCEPTOR_HPP
#define BOOST_MSM_BACKMP11_INTERCEPTOR_HPP

#include <cstddef>

#include <boost/msm/backmp11/common_types.hpp>

namespace boost::msm::backmp11
{

/// Default observer for state machine activities.
/// Defines no-op hooks for all observable activities.
class default_observer
{
  public:
    using process_result = backmp11::process_result;

    /// Hook called before a (sub-)machine processes an event.
    template <typename StateMachine, typename Event>
    constexpr void pre_process_event(const StateMachine&, const Event&) const
    {
    }

    /// Hook called after a (sub-)machine processes an event.
    template <typename StateMachine, typename Event>
    constexpr void post_process_event(const StateMachine&, const Event&,
                                      process_result) const
    {
    }

    /// Hook called before a (sub-)machine processes a transition.
    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    constexpr void pre_process_transition(const StateMachine&,
                                          size_t /*region_id*/) const
    {
    }

    /// Hook called after a (sub-)machine processes a transition.
    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    constexpr void post_process_transition(const StateMachine&,
                                           size_t /*region_id*/,
                                           process_result) const
    {
    }

    /// Hook called before a (sub-)machine processes a sm-internal transition.
    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    constexpr void pre_process_transition(const StateMachine&) const
    {
    }

    /// Hook called after a (sub-)machine processes a sm-internal transition.
    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    constexpr void post_process_transition(const StateMachine&,
                                           process_result) const
    {
    }
};

/// Wrapper for an observer reference whose lifetime is managed externally.
template <typename Observer>
class observer_ref
{
  public:
    observer_ref(Observer& observer) noexcept : m_observer(&observer) {}
 
    Observer& get() const noexcept
    {
        return *m_observer;
    }

    operator Observer&() const noexcept
    {
        return *m_observer;
    }

    /// @copydoc observer::pre_process_event
    template <typename StateMachine, typename Event>
    void pre_process_event(StateMachine& sm, const Event& event) const
    {
        m_observer->pre_process_event(sm, event);
    }

    /// @copydoc observer::post_process_event
    template <typename StateMachine, typename Event>
    void post_process_event(StateMachine& sm, const Event& event,
                                      process_result result) const
    {
        m_observer->post_process_event(sm, event, result);
    }

    /// @copydoc observer::pre_process_transition
    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void pre_process_transition(StateMachine& sm,
                                          size_t region_id) const
    {
        m_observer->template pre_process_transition<Source, Event, Target,
                                                    Action, Guard>(sm,
                                                                   region_id);
    }

    /// @copydoc observer::post_process_transition
    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void post_process_transition(StateMachine& sm, size_t region_id,
                                           process_result result) const
    {
        m_observer->template post_process_transition<Source, Event, Target,
                                                    Action, Guard>(
            sm, region_id, result);
    }

    /// @copydoc observer::pre_process_transition
    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void pre_process_transition(StateMachine& sm) const
    {
        m_observer->template pre_process_transition<Event, Action, Guard>(sm);
    }

    /// @copydoc observer::post_process_transition
    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void post_process_transition(StateMachine& sm,
                                           process_result result) const
    {
        m_observer->template post_process_transition<Event, Action, Guard>(
            sm, result);
    }

  private:
    Observer* m_observer;
};

} // namespace boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_INTERCEPTOR_HPP
