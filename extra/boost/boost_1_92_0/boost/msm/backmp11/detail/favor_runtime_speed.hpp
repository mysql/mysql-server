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

#ifndef BOOST_MSM_BACKMP11_DETAIL_FAVOR_RUNTIME_SPEED_HPP
#define BOOST_MSM_BACKMP11_DETAIL_FAVOR_RUNTIME_SPEED_HPP

#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/state_visitor.hpp>
#include <boost/msm/backmp11/detail/transition_table.hpp>
#include <boost/msm/backmp11/event_traits.hpp>

namespace boost::msm::backmp11
{

namespace detail
{

template <typename Policy>
struct compile_policy_impl<
    Policy,
    std::enable_if_t<std::is_base_of_v<favor_runtime_speed, Policy>>>
{
    template <typename Event>
    constexpr static const Event& normalize_event(const Event& event)
    {
        return event;
    }

    template <typename StateMachine, typename Event>
    static bool is_end_interrupt_event(StateMachine& sm, const Event&)
    {
        return sm.template is_flag_active<EndInterruptFlag<Event>>();
    }

    template <typename Event>
    class is_event_deferred_visitor : public is_event_deferred_visitor_base
    {
      public:
        template <typename State>
        using predicate2 = has_deferred_event<State, Event>;

        is_event_deferred_visitor(const Event& event) : m_event(event)
        {
        }

        template <typename State, typename Fsm>
        void operator()(const State& state, Fsm& fsm)
        {
            m_result |= state.is_event_deferred(m_event, fsm);
        }

      private:
        const Event& m_event;
    };

    template <typename StateMachine, typename Event>
    static constexpr bool needs_event_deferral_check()
    {
        // Instantiate the templates for checking lazily,
        // optimize for the no deferred events case.
        using base_visit_set =
            recursive_visit_set<const StateMachine,
                                is_event_deferred_visitor_base::predicate>;
        // First check:
        // We have have deferring states.
        if constexpr (base_visit_set::needs_traversal::value)
        {
            using visitor_t = is_event_deferred_visitor<Event>;
            using minimal_visit_set =
                recursive_visit_set<const StateMachine,
                                    is_event_deferred_visitor_base::predicate,
                                    visitor_t::template predicate2>;
            // Second check:
            // We have deferring states that defer this event.
            if constexpr (minimal_visit_set::needs_traversal::value)
            {
                return true;
            }
        }
        return false;
    }

    template <typename StateMachine, typename Event>
    static bool is_event_deferred(const StateMachine& sm, const Event& event)
    {
        if constexpr (needs_event_deferral_check<StateMachine, Event>())
        {
            using visitor_t = is_event_deferred_visitor<Event>;
            using state_visitor =
                event_deferral_visitor<const StateMachine, visitor_t,
                                       visitor_t::template predicate,
                                       visitor_t::template predicate2>;
            visitor_t visitor{event};
            state_visitor::visit(sm, visitor);
            return visitor.result();
        }
        return false;
    }

    template <typename StateMachine, typename Event>
    static bool try_defer_event(StateMachine& sm, const Event& event)
    {
        if constexpr (needs_event_deferral_check<StateMachine, Event>())
        {
            if (is_event_deferred(sm, event))
            {
                sm.defer_event(event);
                return true;
            }
        }
        return false;
    }

    // Generates a singleton runtime lookup table that maps current state
    // to a function that makes the SM take its transition on the given
    // Event type.
    template <class StateMachine, typename Event>
    class dispatch_table
    {
      public:
        // Dispatch an event.
        static process_result dispatch(StateMachine& sm, uint8_t region_id,
                                       const Event& event)
        {
            if constexpr (has_transitions::value ||
                          has_forward_transitions::value)
            {
                using table = dispatch_impl<typename Policy::dispatch_strategy>;
                return table::dispatch(sm, region_id, event);
            }
            return process_result::discarded;
        }

        // Dispatch an event to the SM's internal table.
        static process_result internal_dispatch(StateMachine& sm,
                                                const Event& event)
        {
            if constexpr (has_internal_transitions::value)
            {
                return internal_dispatch_impl::transition::process(sm, event);
            }
            return process_result::discarded;
        }

      private:
        // All dispatch tables are friend with each other to check recursively
        // whether forward transitions are required.
        template <class, typename>
        friend class dispatch_table;

        // Compute the maximum state value in the sm so we know how big
        // to make the tables.
        using state_set = typename StateMachine::state_set;
        static constexpr auto max_state = mp11::mp_size<state_set>::value;

        // Filter the transition tables by event.
        template <typename Transition,
                  bool IsKleeneEvent = is_kleene_event<
                      typename Transition::transition_event>::value>
        struct transition_event_predicate_impl
        {
            using type = std::is_base_of<typename Transition::transition_event, Event>;
        };
        template <typename Transition>
        struct transition_event_predicate_impl<Transition, /*IsKleeneEvent=*/true>
        {
            using type = mp11::mp_true;
        };
        template <typename Transition>
        using transition_event_predicate = 
            typename transition_event_predicate_impl<Transition>::type;
        
        using filtered_internal_transition_table = mp11::mp_copy_if<
                internal_transition_table<StateMachine>,
                transition_event_predicate>;
        using has_internal_transitions =
            mp11::mp_not<mp11::mp_empty<filtered_internal_transition_table>>;

        using filtered_transition_table = mp11::mp_copy_if<
                transition_table<StateMachine>,
                transition_event_predicate>;
        using has_transitions = mp11::mp_not<mp11::mp_empty<filtered_transition_table>>;

        // All submachines that could process the event or need to
        // forward it to sub-submachines require a pseudo-transition
        // for forwarding.
        // This metafunction recursively walks through submachines and
        // uses the same metafunctions for checking which are 
        // anyways needed for dispatch table creation.
        template <typename Submachine>
        struct needs_forward_transition_impl
        {
            template <typename Subsubmachine>
            using needs_forward_transition =
                typename needs_forward_transition_impl<Subsubmachine>::type;

            using type = mp11::mp_or<
                typename dispatch_table<Submachine, Event>::has_transitions,
                typename dispatch_table<Submachine, Event>::has_internal_transitions,
                mp11::mp_any_of<
                    typename Submachine::internal::submachines,
                    needs_forward_transition>>;
        };
        template <typename Submachine>
        using needs_forward_transition =
            typename needs_forward_transition_impl<Submachine>::type;

        using submachines = mp11::mp_copy_if<state_set, is_composite>;
        using submachines_with_forward_transitions =
            mp11::mp_copy_if<submachines, needs_forward_transition>;
        using has_forward_transitions =
            mp11::mp_not<mp11::mp_empty<submachines_with_forward_transitions>>;

        class dispatch_base
        {
            // Build a map with key=state/value=[matching_transitions]
            // from the filtered transition table.
            template <typename M, typename Key, typename Value>
            using push_map_value = mp11::mp_push_back<
                mp11::mp_second<mp11::mp_map_find<M, Key>>,
                Value>;
            template <
                typename Map,
                typename Transition,
                bool FirstEntry = !mp11::mp_map_contains<
                    Map, typename Transition::current_state_type>::value>
            struct map_updater_impl;
            template <typename Map, typename Transition>
            struct map_updater_impl<Map, Transition, /*FirstEntry=*/false>
            {
                using type = mp11::mp_map_replace<
                    Map,
                    mp11::mp_list<
                        typename Transition::current_state_type,
                        push_map_value<
                            Map,
                            typename Transition::current_state_type,
                            Transition>>>;
            };
            template <typename Map, typename Transition>
            struct map_updater_impl<Map, Transition, /*FirstEntry=*/true>
            {
                using type = mp11::mp_map_replace<
                    Map,
                    mp11::mp_list<
                        typename Transition::current_state_type,
                        mp11::mp_list<Transition>>>;
            };
            template <typename Map, typename Transition>
            using map_updater = typename map_updater_impl<Map, Transition>::type;

            // Pseudo-transition used to forward event processing to a submachine.
            template <typename Submachine>
            struct forward_transition
            {
                using current_state_type = Submachine;
                using transition_event = Event;

                static process_result process(StateMachine& sm,
                                              uint8_t region_id,
                                              Event const& event)
                {
                    [[maybe_unused]] const auto state_id = sm.m_active_state_ids[region_id];
                    BOOST_ASSERT(state_id == StateMachine::template get_state_id<Submachine>());
                    constexpr process_info info =
                        process_info::submachine_call;
                    process_result result =
                        sm.template get_state<Submachine>()
                            .process_event_observed(event, info);
                    return result;
                }
            };

            template <typename Submachine>
            using initial_map_item = mp11::mp_list<
                Submachine,
                mp11::mp_list<forward_transition<Submachine>>>;

            // Merge each list of transitions into a chain if needed.
            template <typename State, typename Transitions>
            struct merge_transitions_impl;
            template <typename State, typename Transition>
            struct merge_transitions_impl<State, mp11::mp_list<Transition>>
            {
                using type = Transition;
            };
            template <typename State, typename... Transitions>
            struct merge_transitions_impl<State, mp11::mp_list<Transitions...>>
            {
                using type = transition_chain<
                    StateMachine, State, mp11::mp_list<Transitions...>, Event>;
            };
            template <typename StateAndTransitions>
            using merge_transitions = typename merge_transitions_impl<
                mp11::mp_first<StateAndTransitions>,
                mp11::mp_second<StateAndTransitions>>::type;

            using initial_map_items =
                mp11::mp_transform<initial_map_item,
                                   submachines_with_forward_transitions>;
            using filtered_transitions_by_state_map =
                mp11::mp_fold<filtered_transition_table,
                              initial_map_items,
                              map_updater>;

          public:
            using merged_transitions =
                mp11::mp_transform<merge_transitions,
                                   filtered_transitions_by_state_map>;
        };

        template <typename Strategy, typename NotExplicit = void>
        class dispatch_impl;

        template <typename NotExplicit>
        class dispatch_impl<dispatch_strategy::flat_fold, NotExplicit>
            : public dispatch_base
        {
           using base = dispatch_base;
          public:
            static inline process_result dispatch(StateMachine& sm, uint8_t region_id,
                                           const Event& event)
            {
                const auto state_id = sm.m_active_state_ids[region_id];
                process_result result = process_result::discarded;
                mp11::mp_for_each<typename base::merged_transitions>(
                    [&sm, region_id, &event, state_id, &result](auto transition)
                    {
                        using Transition = decltype(transition);
                        using SourceState =
                            typename Transition::current_state_type;
                        constexpr auto source_state_id =
                            StateMachine::template get_state_id<SourceState>();
                        if (state_id == source_state_id)
                        {
                            result = Transition::process(sm, region_id, event);
                        }
                    });
                return result;
            }
        };

        template <typename NotExplicit>
        class dispatch_impl<dispatch_strategy::function_pointer_array, NotExplicit>
            : public dispatch_base
        {
            using base = dispatch_base;
            using cell_t = process_result (*)(StateMachine&, uint8_t /*region_id*/,
                                              Event const&);

          public:
            static inline process_result dispatch(
                StateMachine& sm, uint8_t region_id, const Event& event)
            {
                const auto state_id = sm.m_active_state_ids[region_id];
                constexpr auto& cells = m_cells;
                const cell_t cell = cells[state_id];
                if (cell)
                {
                    return cell(sm, region_id, event);
                }
                return process_result::discarded;
            }

          private:
            struct cell_table
            {
                cell_t data[max_state]{};
                constexpr cell_t operator[](size_t i) const { return data[i]; }
            };

            // Build the cell table by traversing merged_transitions exactly once.
            // Each transition knows its own state_id, so we just assign directly.
            template <typename... Transitions>
            static constexpr cell_table make_cells_from(mp11::mp_list<Transitions...>)
            {
                cell_table table{};
                // Fold expression: one assignment per transition, no searching.
                ((table.data[
                    StateMachine::template get_state_id<
                        typename Transitions::current_state_type>()] =
                        &Transitions::process), ...);
                return table;
            }

            static constexpr cell_table m_cells =
                make_cells_from(typename base::merged_transitions{});
        };

        // "Dispatch" for a specific event (sm-internal).
        // A SM's internal transition table gets transformed into one
        // transition (chain), it can be executed immediately.
        class internal_dispatch_impl
        {
          private:
            // Class used to execute a chain of sm-internal transitions for a given event.
            // Handles transition conflicts.
            template <typename Transitions>
            struct internal_transition_chain
            {
                using transition_event = Event;

                static process_result process(StateMachine& sm, Event const& evt)
                {
                    process_result result = process_result::discarded;
                    mp_for_each_until<Transitions>(
                        [&result, &sm, &evt](auto transition)
                        {
                            using Transition = decltype(transition);
                            result |= Transition::process(sm, evt);
                            if (any(result & consumed_or_deferred))
                            {
                                // If a guard rejected previously,
                                // ensure this bit is not present.
                                result &= consumed_or_deferred;
                                return true;
                            }
                            return false;
                        }
                    );
                    return result;
                }
            };

            // Helpers for row processing
            template <typename Transitions>
            struct transition_impl
            {
                using type = internal_transition_chain<Transitions>;
            };
            template <typename Transition>
            struct transition_impl<mp11::mp_list<Transition>>
            {
                using type = Transition;
            };
            template <typename Transition>
            using transition_event_predicate = transition_event_predicate<Transition>;
            using filtered_transitions =
                mp11::mp_copy_if<internal_transition_table<StateMachine>,
                                 transition_event_predicate>;

          public:
            using transition = typename transition_impl<filtered_transitions>::type;
        };
    };
};

} // detail
} // boost::msm::backmp11


#endif // BOOST_MSM_BACKMP11_DETAIL_FAVOR_RUNTIME_SPEED_HPP
