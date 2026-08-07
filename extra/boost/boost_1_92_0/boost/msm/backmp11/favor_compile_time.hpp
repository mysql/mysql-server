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

#ifndef BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_HPP
#define BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_HPP

#include <typeindex>
#include <unordered_map>
#include <vector>

#include <boost/msm/front/completion_event.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/state_visitor.hpp>
#include <boost/msm/backmp11/detail/transition_table.hpp>
#include <boost/msm/backmp11/event_traits.hpp>

// Macro to generate the dispatch_table for a specific state machine type.
#define BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(SM_TYPE)                     \
    namespace boost::msm::backmp11::detail                                     \
    {                                                                          \
    template <>                                                                \
    const typename compile_policy_impl<                                        \
        favor_compile_time>::template dispatch_table<SM_TYPE, any_event>&      \
    compile_policy_impl<favor_compile_time>::dispatch_table<                   \
        SM_TYPE, any_event>::instance()                                        \
    {                                                                          \
        static dispatch_table table;                                           \
        return table;                                                          \
    }                                                                          \
    } // boost::msm::backmp11::detail

namespace boost:: msm::backmp11
{

using any_event = std::any;

namespace detail
{

template <>
struct compile_policy_impl<favor_compile_time>
{
    // Type-punned init cell value to suppress redundant template instantiations.
    using generic_cell = void(*)();

// GCC cannot recognize the parameter pack in the array's initializer.
#if !defined(__GNUC__) || defined(__clang__)
    // Convert a list with integral constants of the same value_type to a value array
    // (non-constexpr version required due to typeid).
    template <typename List>
    struct value_array;
    template <typename... Ts>
    struct value_array<mp11::mp_list<Ts...>>
    {
        using value_type =
            typename mp11::mp_front<mp11::mp_list<Ts...>>::value_type;
        const value_type value[sizeof...(Ts)]{Ts{}.value...};
    };
#endif

    template <typename Event>
    static any_event normalize_event(const Event& event)
    {
        return any_event{event};
    }

    constexpr static const any_event& normalize_event(const any_event& event)
    {
        return event;
    }

    template<typename StateMachine>
    static bool is_end_interrupt_event(const StateMachine& sm, const any_event& event)
    {
        using event_set = generate_event_set<
            typename StateMachine::front_end_t::transition_table>;
        bool result{false};
        mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, event_set>>(
            [&sm, &event, &result](auto event_identity)
            {
                using Event = typename decltype(event_identity)::type;
                using Flag = EndInterruptFlag<Event>;
                if (event.type() == typeid(Event))
                {
                    result = sm.template is_flag_active<Flag>();
                }
            });
        return result;
    }

    // Dispatch table for event deferral checks.
    // Converts any_events and calls 'is_event_deferred' on states.
    class is_event_deferred_dispatch_table
    {
      public:
        template <typename State, typename Fsm>
        static bool dispatch(const State& state, const any_event& event,
                             const Fsm& fsm)
        {
            static const is_event_deferred_dispatch_table table{state, fsm};
            auto it = table.m_cells.find(event.type());
            if (it != table.m_cells.end())
            {
                using real_cell =
                    bool (*)(const State&, const any_event&, const Fsm&);
                auto cell = reinterpret_cast<real_cell>(it->second);
                return (*cell)(state, event, fsm);
            }
            return false;
        }

      private:
        template <typename State, typename Fsm>
        is_event_deferred_dispatch_table(const State&, const Fsm&)
        {
            using deferred_events = to_mp_list_t<typename State::deferred_events>;
            using deferred_event_identities =
                mp11::mp_transform<mp11::mp_identity, deferred_events>;
            mp11::mp_for_each<deferred_event_identities>(
                [this](auto event_identity)
                {
                    using Event = typename decltype(event_identity)::type;
                    m_cells[to_type_index<Event>()] =
                        reinterpret_cast<generic_cell>(&convert_and_execute<State, Event, Fsm>);
                });
        }

        template <typename State, typename Event, typename Fsm>
        static bool convert_and_execute(const State& state,
                                        const any_event& event, const Fsm& fsm)
        {
            return state.is_event_deferred(*std::any_cast<Event>(&event), fsm);
        }

        std::unordered_map<std::type_index, generic_cell> m_cells;
    };

    class is_event_deferred_visitor : public is_event_deferred_visitor_base
    {
      public:
        is_event_deferred_visitor(const any_event& event)
            : m_event(event)
        {
        }

        template <typename State, typename Fsm>
        void operator()(const State& state, const Fsm& fsm)
        {
            using table = is_event_deferred_dispatch_table;
            m_result |= table::dispatch(state, m_event, fsm);
        }

      private:
        const any_event& m_event;
    };

    template <typename StateMachine>
    static bool is_event_deferred(const StateMachine& sm,
                                  const any_event& event)
    {
        using visitor_t = is_event_deferred_visitor;
        using state_visitor =
            event_deferral_visitor<const StateMachine, 
                                   visitor_t,
                                   visitor_t::template predicate>;
        if constexpr (state_visitor::needs_traversal::value)
        {
            visitor_t visitor{event};
            state_visitor::visit(sm, visitor);
            return visitor.result();
        }
        return false;
    }

    template <typename StateMachine>
    static bool try_defer_event(StateMachine& sm, const any_event& event)
    {
        using visitor_t = is_event_deferred_visitor;
        using state_visitor =
            event_deferral_visitor<const StateMachine, 
                                    visitor_t,
                                    visitor_t::template predicate>;
        if constexpr (state_visitor::needs_traversal::value)
        {
            if (is_event_deferred(sm, event))
            {
                sm.defer_event(event);
                return true;
            }
        }
        return false;
    }

    // Convert an event to a type index.
    template<class Event>
    static std::type_index to_type_index()
    {
        return std::type_index{typeid(Event)};
    }


    // Class used to build a chain of transitions for a given event and state.
    // Allows transition conflicts.
    class transition_chain
    {
      public:
        template <typename StateMachine>
        process_result process(StateMachine& sm, uint8_t region_id,
                               any_event const& event,
                               process_result result) const
        {
            using cell_t = process_result (*)(StateMachine&, int, any_event const&);
            for (const generic_cell cell : m_transition_cells)
            {
                result |= reinterpret_cast<cell_t>(cell)(sm, region_id, event);
                if (any(result & consumed_or_deferred))
                {
                    // If a guard rejected previously, ensure this bit is not present.
                    return result & consumed_or_deferred;
                }
            }
            // At this point result can be HANDLED_FALSE or HANDLED_GUARD_REJECT.
            return result;
        }

        template <typename StateMachine>
        void add_transition_cell(process_result (*cell)(StateMachine&,
                                                        uint8_t /*region_id*/,
                                                        any_event const&))
        {
            m_transition_cells.emplace_back(
                reinterpret_cast<generic_cell>(cell));
        }

      private:
        std::vector<generic_cell> m_transition_cells;
    };

    // Class used to build a chain of sm-internal transitions for a given event and state.
    // Allows transition conflicts.
    class internal_transition_chain
    {
      public:
        template<typename StateMachine>
        process_result process(StateMachine& sm, any_event const& event) const
        {
            using cell_t = process_result (*)(StateMachine&, any_event const&);
            process_result result = process_result::discarded;
            for (const generic_cell cell : m_transition_cells)
            {
                result |= reinterpret_cast<cell_t>(cell)(sm, event);
                if (any(result & consumed_or_deferred))
                {
                    // If a guard rejected previously, ensure this bit is not present.
                    return result & consumed_or_deferred;
                }
            }
            // At this point result can be HANDLED_FALSE or HANDLED_GUARD_REJECT.
            return result;
        }

        template <typename StateMachine>
        void add_transition_cell(process_result (*cell)(StateMachine&,
                                                        any_event const&))
        {
            m_transition_cells.emplace_back(
                reinterpret_cast<generic_cell>(cell));
        }

      private:
        std::vector<generic_cell> m_transition_cells;
    };

    // Generates a singleton runtime lookup table that maps current state
    // to a function that makes the SM take its transition on the given
    // Event type.
    template<class StateMachine, typename Event>
    class dispatch_table;
    template<class StateMachine>
    class dispatch_table<StateMachine, any_event>
    {
      public:
        // Dispatch an event.
        static process_result dispatch(StateMachine& sm, uint8_t region_id,
                                       const any_event& event)
        {
            const auto state_id = sm.m_active_state_ids[region_id];
            const dispatch_table& self = instance();
            return self.m_state_dispatch_tables[state_id].dispatch(sm, region_id, event);
        }

        // Dispatch an event to the SM's internal table.
        static process_result internal_dispatch(StateMachine& sm, const any_event& event)
        {
            if constexpr (has_internal_transitions::value)
            {
                const dispatch_table& self = instance();
                return self.m_internal_dispatch_table.dispatch(sm, event);
            }
            return process_result::discarded;
        }

      private:
        using state_set = typename StateMachine::internal::state_set;
        using submachines = mp11::mp_copy_if<state_set, has_state_machine_tag>;
        
        // Value used to initialize a cell of the dispatch table.
        using cell_t = process_result (*)(StateMachine&, uint8_t /*region_id*/,
                                          any_event const&);
        struct init_cell_value
        {
            std::type_index event_type_index;
            size_t state_id;
            cell_t cell;
        };
        template<typename Event, size_t index, cell_t cell>
        struct init_cell_constant
        {
            using value_type = init_cell_value;
            const value_type value = {typeid(Event), index, cell};
        };

        template<typename Event, typename Transition>
        static process_result convert_event_and_process(
            StateMachine& sm, uint8_t region_id, const any_event& event)
        {
            return Transition::process(sm, region_id, *std::any_cast<Event>(&event));
        }

        template <typename Transition>
        struct get_init_cell_constant_impl
        {
            using type = init_cell_constant<
                typename Transition::transition_event,
                StateMachine::template get_state_id<typename Transition::current_state_type>(),
                convert_event_and_process<typename Transition::transition_event, Transition>
                >;
        };
        template<typename Transition>
        using get_init_cell_constant = typename get_init_cell_constant_impl<Transition>::type;

        using has_internal_transitions =
            mp11::mp_not<mp11::mp_empty<internal_transition_table<StateMachine>>>;
        using has_transitions =
            mp11::mp_not<mp11::mp_empty<transition_table<StateMachine>>>;

        // Dispatch table for one state.
        class state_dispatch_table
        {
          public:
            // Initialize the call to the composite state's process_event function.
            template<typename CompositeState>
            void init_composite_state()
            {
                m_call_process_event = &call_process_event<CompositeState>;
            }

            // Add a cell to the dispatch table.
            void add_transition_cell(const init_cell_value& value)
            {
                transition_chain& chain = m_transition_chains[value.event_type_index];
                chain.add_transition_cell(value.cell);
            }

            // Dispatch an event.
            process_result dispatch(StateMachine& sm, uint8_t region_id, const any_event& event) const
            {
                process_result result = process_result::discarded;
                if (m_call_process_event)
                {
                    result = m_call_process_event(sm, event);
                    if (any(result & consumed_or_deferred))
                    {
                        return result;
                    }
                }
                auto it = m_transition_chains.find(event.type());
                if (it != m_transition_chains.end())
                {
                    result = (it->second.process)(sm, region_id, event, result);
                }
                return result;
            }

          private:
            template <typename Submachine>
            static process_result call_process_event(StateMachine& sm, const any_event& event)
            {
                return sm.template get_state<Submachine&>()
                    .process_event_observed(event, process_info::submachine_call);
            }

            std::unordered_map<std::type_index, transition_chain> m_transition_chains;
            // Optional method in case the state is a composite.
            process_result (*m_call_process_event)(StateMachine&, const any_event&){nullptr};
        };

        // Value used to initialize a cell of the sm-internal dispatch table.
        using internal_cell_t = process_result (*)(StateMachine&, any_event const&);
        struct init_internal_cell_value
        {
            std::type_index event_type_index;
            internal_cell_t cell;
        };
        template<typename Event, internal_cell_t cell>
        struct init_internal_cell_constant
        {
            using value_type = init_internal_cell_value;
            const value_type value = {typeid(Event), cell};
        };

        template<typename Event, typename Transition>
        static process_result convert_event_and_process(
            StateMachine& sm, const any_event& event)
        {
            return Transition::process(sm, *std::any_cast<Event>(&event));
        }

        template <typename Transition>
        struct get_internal_init_cell_constant_impl
        {
            using type = init_internal_cell_constant<
                typename Transition::transition_event,
                convert_event_and_process<typename Transition::transition_event,
                                          Transition>>;
        };
        template <typename Transition>
        using get_internal_init_cell_constant =
            typename get_internal_init_cell_constant_impl<Transition>::type;

        // Dispatch table for sm-internal transitions.
        class internal_dispatch_table
        {
          public:
            // Add a cell to the dispatch table.
            void add_transition_cell(const init_internal_cell_value& value)
            {
                internal_transition_chain& chain =
                    m_transition_chains[value.event_type_index];
                chain.add_transition_cell(value.cell);
            }

            // Dispatch an event.
            process_result dispatch(StateMachine& sm, const any_event& event) const
            {
                process_result result = process_result::discarded;
                auto it = m_transition_chains.find(event.type());
                if (it != m_transition_chains.end())
                {
                    result = (it->second.process)(sm, event);
                }
                return result;
            }

          private:
            std::unordered_map<std::type_index, internal_transition_chain>
                m_transition_chains;
        };

        dispatch_table()
        {
            // Initialize the state dispatch tables.
            using submachines = mp11::mp_copy_if<state_set, has_state_machine_tag>;
            mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, submachines>>(
                [this](auto state_identity)
                {
                    using Submachine = typename decltype(state_identity)::type;
                    static constexpr auto state_id =
                        StateMachine::template get_state_id<Submachine>();
                    m_state_dispatch_tables[state_id]
                        .template init_composite_state<Submachine>();
                });
            if constexpr (has_transitions::value)
            {
                using init_cell_constants = mp11::mp_transform<
                    get_init_cell_constant,
                    transition_table<StateMachine>>;
#if defined(__GNUC__) && !defined(__clang__)
                mp11::mp_for_each<init_cell_constants>(
                    [this](auto constant)
                    {
                        m_state_dispatch_tables[constant.value.state_id].add_transition_cell(constant.value);
                    });
#else
                value_array<init_cell_constants> value_array;
                for (const init_cell_value& value: value_array.value)
                {
                    m_state_dispatch_tables[value.state_id].add_transition_cell(value);
                }
#endif
            }

            // Initialize the sm-internal dispatch table.
            if constexpr (has_internal_transitions::value)
            {
                using init_internal_cell_constants = mp11::mp_transform<
                    get_internal_init_cell_constant,
                    internal_transition_table<StateMachine>>;
#if defined(__GNUC__) && !defined(__clang__)
                mp11::mp_for_each<init_internal_cell_constants>(
                    [this](auto constant)
                    {
                        m_internal_dispatch_table.add_transition_cell(constant.value);
                    });
#else
                value_array<init_internal_cell_constants> value_array;
                for (const init_internal_cell_value& value: value_array.value)
                {
                    m_internal_dispatch_table.add_transition_cell(value);
                }
#endif
            }
        }

        // The singleton instance.
        static const dispatch_table& instance();

        // We need one dispatch table per state.
        state_dispatch_table m_state_dispatch_tables[mp11::mp_size<state_set>::value];
        internal_dispatch_table m_internal_dispatch_table;
    };
};

#ifndef BOOST_MSM_BACKMP11_MANUAL_GENERATION

template<class StateMachine>
const typename compile_policy_impl<favor_compile_time>::template
    dispatch_table<StateMachine, any_event>& 
compile_policy_impl<favor_compile_time>::dispatch_table<StateMachine, any_event>::instance()
{
    static dispatch_table table;
    return table;
}

#endif

} // namespace detail
} // namespace boost::msm::backmp11::detail

#endif //BOOST_MSM_BACKMP11_FAVOR_COMPILE_TIME_HPP
