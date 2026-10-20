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

#ifndef BOOST_MSM_BACKMP11_DETAIL_STATE_VISITOR_HPP
#define BOOST_MSM_BACKMP11_DETAIL_STATE_VISITOR_HPP

#include <boost/msm/backmp11/detail/common.hpp>
#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>

namespace boost::msm::backmp11::detail
{

constexpr bool has_flag(visit_mode value, visit_mode flag)
{
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

// Helper to filter with multiple predicates consecutively.
// Uses template unrolling to enable re-usage of template instances.
template <typename List, template <typename> typename... Predicates>
struct copy_if_impl;
template <typename List, 
          template <typename> typename Predicate,
          template <typename> typename... Predicates>
struct copy_if_impl<List, Predicate, Predicates...>
{
    using subset = mp11::mp_copy_if<List, Predicate>;
    using type = typename copy_if_impl<subset, Predicates...>::type;
};
template <typename List>
struct copy_if_impl<List>
{
    using type = List;
};
template <typename List, template <typename> typename... Predicates>
using copy_if = typename copy_if_impl<List, Predicates...>::type;


template <typename StateMachine,
          bool Recursive,
          template <typename> typename... Predicates>
class state_visitor_base_impl;
template <typename StateMachine, 
          template <typename> typename... Predicates>
class state_visitor_base_impl<
    StateMachine,
    /*Recursive=*/false,
    Predicates...>
{
  public:
    using states_to_visit =
        copy_if<typename StateMachine::internal::state_set, Predicates...>;
    using states_to_traverse = states_to_visit;

    using needs_traversal = mp11::mp_not<mp11::mp_empty<states_to_visit>>;

    template <visit_mode Mode, typename State, typename Visitor>
    static void accept(StateMachine& sm, Visitor& visitor)
    {
        auto& state = sm.template get_state<State>();
        visitor(state);
    }
};

// Set with traversal info for recursive visits.
// Uses template unrolling to enable re-usage of template instances.
template <typename StateMachine,
          template <typename> typename... Predicates>
struct recursive_visit_set;
template <typename StateMachine>
struct recursive_visit_set<StateMachine>
{
    using states_to_visit = typename StateMachine::internal::state_set;
    using submachines_to_traverse =
        typename StateMachine::internal::submachines;
    using states_to_traverse = states_to_visit;

    using needs_traversal = mp11::mp_true;
};
template <typename StateMachine, 
          template <typename> typename Predicate>
struct recursive_visit_set<StateMachine, Predicate>
{
    using states_to_visit =
        copy_if<typename StateMachine::internal::state_set, Predicate>;

    template <typename Submachine>
    using submachine_needs_traversal = typename recursive_visit_set<
        Submachine, Predicate>::needs_traversal;
    using submachines_to_traverse =
        mp11::mp_copy_if<typename StateMachine::internal::submachines,
                         submachine_needs_traversal>;

    using states_to_traverse =
        mp11::mp_set_union<states_to_visit, submachines_to_traverse>;

    using needs_traversal = mp11::mp_not<mp11::mp_empty<states_to_traverse>>;
};
template <typename StateMachine, 
          template <typename> typename FirstPredicate,
          template <typename> typename Predicate>
struct recursive_visit_set<StateMachine, FirstPredicate, Predicate>
    : recursive_visit_set<StateMachine, FirstPredicate>
{
    using base = recursive_visit_set<StateMachine, FirstPredicate>;

    using states_to_visit =
        copy_if<typename base::states_to_visit, Predicate>;

    template <typename Submachine>
    using submachine_needs_traversal = typename recursive_visit_set<
        Submachine, Predicate>::needs_traversal;
    using submachines_to_traverse =
        mp11::mp_copy_if<typename base::submachines_to_traverse,
                         submachine_needs_traversal>;

    using states_to_traverse = mp11::mp_set_union<states_to_visit, submachines_to_traverse>;

    using needs_traversal = mp11::mp_not<mp11::mp_empty<states_to_traverse>>;
};

template <visit_mode Mode, template <typename> typename... Predicates,
          typename StateMachine, typename Visitor>
void visit_if(StateMachine& sm, Visitor&& visitor);

template <typename StateMachine,
          template <typename> typename... Predicates>
class state_visitor_base_impl<
    StateMachine,
    /*Recursive=*/true,
    Predicates...>
{
    using visit_set = recursive_visit_set<StateMachine, Predicates...>;
  public:
    using needs_traversal = typename visit_set::needs_traversal;
    using states_to_traverse = typename visit_set::states_to_traverse;

    template <visit_mode Mode, typename State, typename Visitor>
    static void accept(StateMachine& sm, Visitor& visitor)
    {
        auto& state = sm.template get_state<State>();

        if constexpr (mp11::mp_contains<typename visit_set::states_to_visit,
                                        State>::value)
        {
            visitor(state);
        }

        if constexpr (mp11::mp_contains<
                          typename visit_set::submachines_to_traverse,
                          State>::value)
        {
            visit_if<Mode, Predicates...>(state, visitor);
        }
    }
};
template <typename StateMachine,
          visit_mode Mode,
          template <typename> typename... Predicates>
using state_visitor_base =
    state_visitor_base_impl<StateMachine,
                            has_flag(Mode, visit_mode::recursive),
                            Predicates...>;

template <
    typename StateMachine,
    typename Visitor,
    visit_mode Mode,
    bool AllStates,
    template <typename> typename... Predicates>
class state_visitor_impl;

template <
    typename StateMachine,
    typename Visitor,
    visit_mode Mode,
    template <typename> typename... Predicates>
class state_visitor_impl<
    StateMachine,
    Visitor,
    Mode,
    /*AllStates=*/false,
    Predicates...>
    : public state_visitor_base<StateMachine, Mode, Predicates...>
{
    using base = state_visitor_base<StateMachine, Mode, Predicates...>;

  public:
    static void visit(StateMachine& sm, Visitor& visitor)
    {
        if constexpr (base::needs_traversal::value)
        {
            if (sm.m_machine_state != machine_state::stopped)
            {
                using state_identities = mp11::mp_transform<
                            mp11::mp_identity,
                            typename base::states_to_traverse>;
                for (const auto active_state_id : sm.get_active_state_ids())
                {
                    mp11::mp_for_each<state_identities>(
                        [&sm, &visitor, active_state_id](auto state_identity)
                        {
                            using State =
                                typename decltype(state_identity)::type;
                            constexpr auto state_id =
                                StateMachine::template get_state_id<State>();
                            if (active_state_id == state_id)
                            {
                                base::template accept<Mode, State>(sm, visitor);
                            }
                        });
                }
            }
        }
    }
};

template <
    typename StateMachine,
    typename Visitor,
    visit_mode Mode,
    template <typename> typename... Predicates>
class state_visitor_impl<
    StateMachine,
    Visitor,
    Mode,
    /*AllStates=*/true,
    Predicates...>
    : public state_visitor_base<StateMachine, Mode, Predicates...>
{
    using base = state_visitor_base<StateMachine, Mode, Predicates...>;

  public:
    static void visit(StateMachine& sm, Visitor& visitor)
    {
        if constexpr (base::needs_traversal::value)
        {
            using state_identities = mp11::mp_transform<
                mp11::mp_identity,
                typename base::states_to_traverse>;
            mp11::mp_for_each<state_identities>(
                [&sm, &visitor](auto state_identity)
                {
                    using State = typename decltype(state_identity)::type;
                    base::template accept<Mode, State>(sm, visitor);
                }
            );
        }
    }
};

// State visitor.
// States to visit can be selected with Mode
// and additionally compile-time filtered with Predicates.
template <
    typename StateMachine,
    typename Visitor,
    visit_mode Mode,
    template <typename> typename... Predicates>
using state_visitor = state_visitor_impl<
    StateMachine,
    Visitor,
    Mode,
    has_flag(Mode, visit_mode::all_states),
    Predicates...>;

// Custom visitor tailored to event deferral.
// We need an active_recursive implementation with the ability 
// to additionally pass the Fsm parameter.
template <
    typename StateMachine,
    typename Visitor,
    template <typename> typename... Predicates>
class event_deferral_visitor
{
    using visit_set = recursive_visit_set<StateMachine, Predicates...>;

  public:
    using needs_traversal = typename visit_set::needs_traversal;

    static void visit(StateMachine& sm, Visitor& visitor)
    {
        // Ensure the necessity to instantiate this function
        // is evaluated before it's called.
        static_assert(visit_set::needs_traversal::value,
                      "The visitor must have at least one state to visit");
        if (sm.m_machine_state != machine_state::stopped)
        {
            using state_identities = mp11::mp_transform<
                        mp11::mp_identity,
                        typename visit_set::states_to_traverse>;
            for (const auto active_state_id : sm.get_active_state_ids())
            {
                mp11::mp_for_each<state_identities>(
                    [&sm, &visitor, active_state_id](auto state_identity)
                    {
                        using State = typename decltype(state_identity)::type;
                        constexpr auto state_id =
                            StateMachine::template get_state_id<State>();
                        if (active_state_id == state_id)
                        {
                            accept<State>(sm, visitor);
                        }
                    });
            }
        }
    }

  private:
    template <typename State>
    static void accept(StateMachine& sm, Visitor& visitor)
    {
        auto& state = sm.template get_state<State>();

        if constexpr (mp11::mp_contains<typename visit_set::states_to_visit,
                                        State>::value)
        {
            visitor(state, sm.get_fsm_argument());
        }

        if constexpr (mp11::mp_contains<typename visit_set::submachines_to_traverse,
                                        State>::value)
        {
            using submachine_visitor =
                event_deferral_visitor<const State, Visitor, Predicates...>;
            submachine_visitor::visit(state, visitor);
        }
    }
};

// Visit states with a compile-time filter (reduces template instantiations).
template <visit_mode Mode, template <typename> typename... Predicates,
          typename StateMachine, typename Visitor>
void visit_if(StateMachine& sm, Visitor&& visitor)
{
    using state_visitor =
        state_visitor<StateMachine, Visitor, Mode, Predicates...>;
    state_visitor::visit(sm, visitor);
}

// Predefined visitor functors used in backmp11.

class is_event_deferred_visitor_base
{
  public:
    // Apply (at least) a pre-filter with the 'has_deferred_events' predicate
    // to consider only deferring states, this subset is cheap to instantiate.
    // Compile policies can decide to apply additional filters.
    template <typename State>
    using predicate = has_deferred_events<State>;

    bool result() const
    {
        return m_result;
    }

  protected:
    bool m_result{false};
};

template <typename State>
class is_state_active_visitor
{
  public:
    template <typename StateToCheck>
    using predicate = std::is_same<State, StateToCheck>;

    void operator()(const State&)
    {
        m_result = true;
    }

    bool result() const
    {
        return m_result;
    }

  private:
    bool m_result{false};
};

template <typename Flag, typename BinaryOp>
class is_flag_active_visitor;
template <typename Flag>
class is_flag_active_visitor<Flag, flag_or>
{
  public:
    template <typename State>
    using predicate = mp11::mp_contains<get_flag_list<State>, Flag>;

    template <typename State>
    void operator()(const State&)
    {
        m_result = true;
    }

    bool result() const
    {
        return m_result;
    }

  private:
    bool m_result{false};
};
template <typename Flag>
class is_flag_active_visitor<Flag, flag_and>
{
  public:
    template <typename State>
    using predicate =
        mp11::mp_not<mp11::mp_contains<get_flag_list<State>, Flag>>;

    template <typename State>
    void operator()(const State&)
    {
        m_result = false;
    }

    bool result() const
    {
        return m_result;
    }

  private:
    bool m_result{true};
};

template <typename RootSm>
class init_state_visitor
{
  public:
    template <typename State>
    using predicate = mp11::mp_or<has_exit_pseudostate_be_tag<State>,
                                  has_state_machine_tag<State>>;

    init_state_visitor(RootSm& root_sm) : m_root_sm(root_sm)
    {
    }

    template <typename State>
    void operator()(State& state)
    {
        if constexpr (has_exit_pseudostate_be_tag<State>::value)
        {
            state.template init<RootSm>();
        }

        if constexpr (has_state_machine_tag<State>::value)
        {
            static_assert(
                std::is_same_v<typename State::root_sm_t, no_root_sm> ||
                std::is_same_v<RootSm, typename State::root_sm_t>,
                "The configured root_sm must match the used one");
            static_assert(
                std::is_same_v<typename State::context_t, no_context> ||
                std::is_same_v<typename State::context_t, typename RootSm::context_t>,
                "The configured context must match the root sm's one");
            static_assert(std::is_same_v<typename RootSm::config_t::compile_policy,
                                         typename State::config_t::compile_policy>,
                          "All compile policies must be identical");

            *state.m_root_sm = &m_root_sm;
        }
    }

  private:
    RootSm& m_root_sm;
};

} // boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_STATE_VISITOR_HPP
