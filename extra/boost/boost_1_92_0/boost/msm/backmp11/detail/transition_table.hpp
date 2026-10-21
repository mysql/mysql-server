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

#ifndef BOOST_MSM_BACKMP11_DETAIL_TRANSITION_TABLE_HPP
#define BOOST_MSM_BACKMP11_DETAIL_TRANSITION_TABLE_HPP

#include <boost/assert.hpp>

#include <boost/msm/active_state_switching_policies.hpp>
#include <boost/msm/back/common_types.hpp>
#include <boost/msm/row_tags.hpp>

#include <boost/msm/backmp11/detail/common.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>

namespace boost::msm::front
{

struct Defer;

}

namespace boost::msm::backmp11::detail
{

// Adapter for the basic front-end.
constexpr process_result to_process_result(back::HandledEnum value)
{
    return static_cast<process_result>(value);
}

constexpr process_result to_process_result(process_result result)
{
    return result;
}

// Chain of priority tags for SFINAE handling:
// priority_tag_0 
//   ↓ (SFINAE fails?)
// priority_tag_1 (base of priority_tag_0)
//   ↓ (SFINAE fails?)
// ...
struct priority_tag_3 {};
struct priority_tag_2 : priority_tag_3 {};
struct priority_tag_1 : priority_tag_2 {};
struct priority_tag_0 : priority_tag_1 {};

template <typename Functor, typename Event, typename Fsm, typename Source,
          typename Target>
auto invoke_functor(priority_tag_0, Functor&&, const Event& event, Fsm& fsm,
                    Source& source, Target& target)
    -> decltype(Functor{}(event, fsm, source, target))
{
    return Functor{}(event, fsm, source, target);
}
template <typename Functor, typename Event, typename Fsm, typename Source,
          typename Target>
auto invoke_functor(priority_tag_1, Functor&&, const Event& event, Fsm& fsm, Source&,
                    Target&) -> decltype(Functor{}(event, fsm))
{
    return Functor{}(event, fsm);
}
template <typename Functor, typename Event, typename Fsm, typename Source,
          typename Target>
auto invoke_functor(priority_tag_2, Functor&&, const Event&, Fsm& fsm, Source&,
                    Target&) -> decltype(Functor{}(fsm))
{
    return Functor{}(fsm);
}
template <typename...>
inline constexpr bool invokable = false;
template <typename Functor, typename Event, typename Fsm, typename Source,
          typename Target>
auto invoke_functor(priority_tag_3, Functor&&, const Event&, Fsm&, Source&,
                    Target&)
{
    static_assert(
        invokable<Functor, Event, Fsm, Source, Target>,
        "Action/Guard must be invokable with one of these signatures: "
        "(event, fsm, source, target), (event, fsm), or (fsm)");
}

template <typename Row>
using get_Guard = typename Row::Guard;
template <typename Row>
struct has_Guard : mp11::mp_valid<get_Guard, Row> {};

template <typename Functor>
struct invoke_guard_functor
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    static bool execute(const Event& event, Fsm& fsm, Source& source,
                        Target& target)
    {
        return invoke_functor<Functor>(priority_tag_0{}, Functor{}, event, fsm,
                                       source, target);
    }
};
template <>
struct invoke_guard_functor<front::none>
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    static bool execute(const Event&, Fsm&, Source&, Target&)
    {
        return true;
    }
};

template <typename Row>
using get_Action = typename Row::Action;
template <typename Row>
struct has_Action : mp11::mp_valid<get_Action, Row> {};

template <typename Functor>
struct invoke_action_functor
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    static process_result execute(const Event& event, Fsm& fsm, Source& source,
                                  Target& target)
    {
        invoke_functor<Functor>(priority_tag_0{}, Functor{}, event, fsm, source,
                                target);
        return process_result::consumed;
    }
};
template <>
struct invoke_action_functor<front::none>
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    static process_result execute(const Event&, Fsm&, Source&, Target&)
    {
        return process_result::consumed;
    }
};
template <>
struct invoke_action_functor<front::Defer>
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    static process_result execute(const Event& event, Fsm& fsm, Source&,
                                  Target&)
    {
        fsm.defer_event(event);
        return process_result::deferred;
    }
};

template <typename StateMachine>
struct transition_table_impl
{
    using front_end_t = typename StateMachine::front_end_t;
    using derived_t = typename StateMachine::derived_t;
    using state_set = typename StateMachine::state_set;

    template<typename T>
    using get_active_state_switch_policy = typename T::active_state_switch_policy;
    using active_state_switching =
        boost::mp11::mp_eval_or<active_state_switch_after_exit,
                                get_active_state_switch_policy, front_end_t>;

    template <typename Row, bool HasGuard, typename Event, typename Source,
              typename Target>
    static bool call_guard_or_true(StateMachine& sm, const Event& event,
                                   Source& source, Target& target)
    {
        if constexpr (has_Guard<Row>::value)
        {
            return invoke_guard_functor<typename Row::Guard>::execute(
                event, sm.get_fsm_argument(), source, target);
        }
        else if constexpr (HasGuard)
        {
            return Row::guard_call(
                sm.get_fsm_argument(), event, source, target, sm.m_states);
        }
        else
        {
            return true;
        }
    }

    template <typename Row, bool HasAction, typename Event, typename Source,
              typename Target>
    static process_result call_action_or_true(StateMachine& sm,
                                              const Event& event,
                                              Source& source, Target& target)
    {
        if constexpr (has_Action<Row>::value)
        {
            return invoke_action_functor<typename Row::Action>::execute(
                event, sm.get_fsm_argument(), source, target);
        }
        else if constexpr (HasAction)
        {
            return to_process_result(Row::action_call(
                sm.get_fsm_argument(), event, source, target, sm.m_states));
        }
        else
        {
            return process_result::consumed;
        }
    }

    template <typename DirectWrapper>
    using get_state = typename DirectWrapper::state;

    template <typename FeTarget>
    using is_explicit_entry_point = mp11::mp_eval_if<
        mpl::is_sequence<FeTarget>,
        mp11::mp_true,
        has_explicit_entry_be_tag,
        FeTarget>;

    template <typename Row, typename Event, typename Target>
    static void call_entry(StateMachine& sm, const Event& event, Target& target)
    {
        using FeTarget = typename Row::Target;
        auto& fsm = sm.get_fsm_argument();

        if constexpr (is_explicit_entry_point<FeTarget>::value)
        {
            using targets = to_mp_list_t<FeTarget>;
            using states = mp11::mp_transform<get_state, targets>;
            target.template on_explicit_entry<states>(event, fsm);
        }
        else if constexpr (has_entry_pseudostate_be_tag<FeTarget>::value)
        {
            using targets = to_mp_list_t<FeTarget>;
            using states = mp11::mp_transform<get_state, targets>;
            target.template on_pseudo_entry<states>(event, fsm);
        }
        else
        {
            target.on_entry(event, fsm);
            if constexpr (has_exit_pseudostate_be_tag<Target>::value)
            {
                // Execute the second part of the compound transition.
                target.forward_event(*sm.m_root_sm, event);
            }
        }
    }

    // Template used to create transitions from rows in the transition table.
    template <typename Row, bool HasAction, bool HasGuard>
    struct transition
    {
        using transition_event = typename Row::Evt;
        using current_state_type =
            convert_source_state<derived_t, typename Row::Source>;
        using next_state_type =
            convert_target_state<derived_t, typename Row::Target>;

        template <typename Event>
        static process_result process(StateMachine& sm,
                                      uint8_t region_id,
                                      const Event& event)
        {
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template pre_process_transition<
                        typename Row::Source, typename Row::Evt,
                        typename Row::Target, typename Row::Action,
                        typename Row::Guard>(sm, region_id);
            }
            const auto result = process_impl(sm, region_id, event);
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template post_process_transition<
                        typename Row::Source, typename Row::Evt,
                        typename Row::Target, typename Row::Action,
                        typename Row::Guard>(sm, region_id, result);
            }
            return result;
        }

        template <typename Event>
        static process_result process_impl(StateMachine& sm,
                                           uint8_t region_id,
                                           const Event& event)
        {
            auto& state_id = sm.m_active_state_ids[region_id];
            [[maybe_unused]] constexpr auto current_state_id =
                StateMachine::template get_state_id<current_state_type>();
            BOOST_ASSERT(state_id == current_state_id);
            constexpr auto next_state_id =
                StateMachine::template get_state_id<next_state_type>();

            auto& source = sm.template get_state<current_state_type>();
            auto& target = sm.template get_state<next_state_type>();

            if (!call_guard_or_true<Row, HasGuard>(sm, event, source, target))
            {
                // guard rejected the event, we stay in the current one
                return process_result::rejected;
            }
            if constexpr (std::is_same_v<active_state_switching,
                                         active_state_switch_before_transition>)
            {
                state_id = next_state_id;
            }

            // first call the exit method of the current state
            source.on_exit(event, sm.get_fsm_argument());
            if constexpr (std::is_same_v<active_state_switching,
                                         active_state_switch_after_exit>)
            {
                state_id = next_state_id;
            }

            // then call the action method
            process_result res =
                call_action_or_true<Row, HasAction>(sm, event, source, target);
            if constexpr (std::is_same_v<active_state_switching,
                                         active_state_switch_after_transition_action>)
            {
                state_id = next_state_id;
            }

            // and finally the entry method of the new state
            call_entry<Row>(sm, event, target);
            if constexpr (std::is_same_v<active_state_switching,
                                         active_state_switch_after_entry>)
            {
                state_id = next_state_id;
            }

            // Give a chance to handle completion transitions.
            sm.on_state_entry_completed(target, region_id);

            return res;
        }
    };

    // Template used to create internal transitions
    // from rows in the transition table.
    template <typename Row, bool HasAction, bool HasGuard,
              typename State = typename Row::Source>
    struct internal_transition
    {
        using transition_event = typename Row::Evt;
        using current_state_type = State;

        template <typename Event>
        static process_result process(StateMachine& sm,
                                      uint8_t region_id,
                                      const Event& event)
        {
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template pre_process_transition<
                        typename Row::Source, typename Row::Evt,
                        typename front::none, typename Row::Action,
                        typename Row::Guard>(sm, region_id);
            }
            const auto result = process_impl(sm, region_id, event);
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template post_process_transition<
                        typename Row::Source, typename Row::Evt,
                        typename front::none, typename Row::Action,
                        typename Row::Guard>(sm, region_id, result);
            }
            return result;
        }

        template <typename Event>
        static process_result process_impl(StateMachine& sm,
                                           uint8_t region_id,
                                           const Event& event)
        {
            [[maybe_unused]] const auto state_id = sm.m_active_state_ids[region_id];
            BOOST_ASSERT(
                state_id ==
                StateMachine::template get_state_id<current_state_type>());

            auto& source = sm.template get_state<current_state_type>();
            auto& target = source;

            if (!call_guard_or_true<Row, HasGuard>(sm, event, source, target))
            {
                return process_result::rejected;
            }
            return call_action_or_true<Row, HasAction>(sm, event, source, target);
        }
    };

    // Template used to create sm-internal transitions
    // from rows in the transition table.
    template<typename Row, bool HasAction, bool HasGuard>
    struct internal_transition<Row, HasAction, HasGuard, StateMachine>
    {
        using transition_event = typename Row::Evt;

        template <typename Event>
        static process_result process(StateMachine& sm,
                                      const Event& event)
        {
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template pre_process_transition<typename Row::Evt,
                                                     typename Row::Action,
                                                     typename Row::Guard>(
                        sm);
            }
            const auto result = process_impl(sm, event);
            if constexpr (!std::is_same_v<typename StateMachine::observer_t,
                                          no_observer>)
            {
                sm.get_observer()
                    .template post_process_transition<typename Row::Evt,
                                                      typename Row::Action,
                                                      typename Row::Guard>(
                        sm, result);
            }
            return result;
        }
        
        template <typename Event>
        static process_result process_impl(StateMachine& sm,
                                           const Event& event)
        {
            auto& source = sm;
            auto& target = source;

            if (!call_guard_or_true<Row, HasGuard>(sm, event, source, target))
            {
                return process_result::rejected;
            }
            return call_action_or_true<Row, HasAction>(sm, event, source, target);
        }
    };

    // Helpers used to create back-end transitions
    // from front-end transitions.
    template <class Tag, class Row, class State>
    struct create_transition_impl;
    template <class Row, class State>
    struct create_transition_impl<g_row_tag, Row, State>
    {
        using type = transition<Row, false, true>;
    };
    template <class Row, class State>
    struct create_transition_impl<a_row_tag, Row, State>
    {
        using type = transition<Row, true, false>;
    };
    template <class Row, class State>
    struct create_transition_impl<_row_tag, Row, State>
    {
        using type = transition<Row, false, false>;
    };
    template <class Row, class State>
    struct create_transition_impl<row_tag, Row, State>
    {
        using type = transition<Row, true, true>;
    };
    template <class Row, class State>
    struct create_transition_impl<g_irow_tag, Row, State>
    {
        using type = internal_transition<Row, false, true>;
    };
    template <class Row, class State>
    struct create_transition_impl<a_irow_tag, Row, State>
    {
        using type = internal_transition<Row, true, false>;
    };
    template <class Row, class State>
    struct create_transition_impl<irow_tag, Row, State>
    {
        using type = internal_transition<Row, true, true>;
    };
    template <class Row, class State>
    struct create_transition_impl<_irow_tag, Row, State>
    {
        using type = internal_transition<Row, false, false>;
    };
    template <class Row, class State>
    struct create_transition_impl<sm_a_i_row_tag, Row, State>
    {
        using type = internal_transition<Row, true, false, State>;
    };
    template <class Row, class State>
    struct create_transition_impl<sm_g_i_row_tag, Row, State>
    {
        using type = internal_transition<Row, false, true, State>;
    };
    template <class Row, class State>
    struct create_transition_impl<sm_i_row_tag, Row, State>
    {
        using type = internal_transition<Row, true, true, State>;
    };
    template <class Row, class State>
    struct create_transition_impl<sm__i_row_tag, Row, State>
    {
        using type = internal_transition<Row, false, false, State>;
    };
    template <class Row, class State>
    using create_transition =
        typename create_transition_impl<typename Row::row_type_tag, Row,
                                        State>::type;

    // Transform the front-end's internal transition table
    // to a back-end transition table,
    // with reversed ordering for backward compatibility.
    using internal_transition_table = mp11::mp_reverse<
        mp11::mp_transform_q<
            mp11::mp_bind_back<create_transition, StateMachine>,
            to_mp_list_t<typename front_end_t::internal_transition_table>>>;

    // Expand the front-end's transition table(s)
    // to a single back-end transition table,
    // with reversed ordering for backward compatibility.
    // First transform the main transition table of the SM.
    using sm_transition_table = mp11::mp_transform_q<
        mp11::mp_bind_back<create_transition, StateMachine>,
        to_mp_list_t<typename front_end_t::transition_table>>;
    // Then transform and append the states' internal transition tables.
    template <typename State>
    using append_state_transition_tables_predicate = mp11::mp_or<
        is_composite<State>,
        mp11::mp_empty<to_mp_list_t<typename State::internal_transition_table>>>;
    template <typename TransitionTable, typename State>
    using append_internal_transition_table = mp11::mp_append<
        TransitionTable,
        mp11::mp_transform_q<
            mp11::mp_bind_back<create_transition, State>,
            to_mp_list_t<typename State::internal_transition_table>>>;
    using transition_table = mp11::mp_reverse<
        mp11::mp_fold<
            mp11::mp_remove_if<state_set, append_state_transition_tables_predicate>,
            sm_transition_table,
            append_internal_transition_table>>;

    // Completion transitions are handled separately per state.
    template <typename Transition>
    using has_completion_event =
        has_completion_event<typename Transition::transition_event>;
    using completion_transition_table =
        mp11::mp_copy_if<transition_table, has_completion_event>;
    static_assert(mp11::mp_empty<completion_transition_table>::value ||
                      StateMachine::has_event_pool,
                  "Completion transitions require an event pool");
    template <typename State, typename Table = completion_transition_table>
    struct completion_transitions_impl
    {
        template <typename Transition>
        using predicate = std::is_same<typename Transition::current_state_type, State>;

        using type = mp11::mp_copy_if<Table, predicate>;
    };
    template <typename State>
    struct completion_transitions_impl<State, mp11::mp_list<>>
    {
        using type = mp11::mp_list<>;
    };
    template <typename State>
    using completion_transitions = typename completion_transitions_impl<State>::type;
};

template <typename StateMachine>
using transition_table = 
    typename transition_table_impl<StateMachine>::transition_table;

template <typename StateMachine>
using internal_transition_table = 
    typename transition_table_impl<StateMachine>::internal_transition_table;

template <typename StateMachine, typename State>
using completion_transitions = 
    typename transition_table_impl<StateMachine>::template completion_transitions<State>;

template <typename StateMachine, typename State>
using has_completion_transitions =
    mp11::mp_not<mp11::mp_empty<completion_transitions<StateMachine, State>>>;

// Class used to execute a chain of transitions for a given event and state.
// Handles transition conflicts.
template <typename StateMachine, typename State, typename Transitions, typename Event>
struct transition_chain
{
    using current_state_type = State;
    using transition_event = Event;

    static process_result process(StateMachine& sm,
                                  uint8_t region_id,
                                  const Event& evt)
    {
        process_result result = process_result::discarded;
        mp_for_each_until<Transitions>(
            [&result, &sm, region_id, &evt](auto transition)
            {
                using Transition = decltype(transition);
                result |= Transition::process(sm, region_id, evt);
                if (any(result & consumed_or_deferred))
                {
                    // If a guard rejected previously, ensure this bit is not present.
                    result &= consumed_or_deferred;
                    return true;
                }
                return false;
            }
        );
        return result;
    }
};


} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_TRANSITION_TABLE_HPP
