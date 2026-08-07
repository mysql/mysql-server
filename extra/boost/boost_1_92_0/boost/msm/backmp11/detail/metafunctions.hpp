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

#ifndef BOOST_MSM_BACKMP11_DETAIL_METAFUNCTIONS_HPP
#define BOOST_MSM_BACKMP11_DETAIL_METAFUNCTIONS_HPP

#include <cstdint>

#include <boost/mp11.hpp>
#include <boost/mp11/mpl_list.hpp>

#include <boost/msm/backmp11/detail/state_tags.hpp>
#include <boost/msm/back/traits.hpp>
#include <boost/msm/front/completion_event.hpp>

// Forward declarations to support MPL->Mp11 conversions
// without MPL header dependencies.
namespace boost::mpl
{
    template <typename P1, typename P2>
    struct copy;

    template <typename Sequence>
    struct back_inserter;

    template <typename T>
    struct is_sequence;
}

namespace boost::msm::backmp11::detail
{

// Call a functor on all elements of List, until the functor returns true.
// Performs short-circuit evaluation at runtime.
template <typename... Ts>
struct mp_for_each_until_impl
{
    template <typename F>
    static constexpr bool invoke(F &&func)
    {
        return (... || func(Ts{}));
    }
};
template <typename L, typename F>
constexpr bool mp_for_each_until(F &&func)
{
    return mp11::mp_apply<mp_for_each_until_impl, L>::invoke(std::forward<F>(func));
}

// Helper to convert a single type or MPL sequence to Mp11
template<typename T, typename Enable = void>
struct to_mp_list
{
    typedef typename mpl::copy<T, mpl::back_inserter<mp11::mp_list<>>>::type type;
};
template<typename T>
struct to_mp_list<T, std::enable_if_t<!mpl::is_sequence<T>::value>>
{
    using type = mp11::mp_list<T>;
};
template<typename ...T>
struct to_mp_list<mp11::mp_list<T...>>
{
    typedef mp11::mp_list<T...> type;
};
template<typename ...T>
using to_mp_list_t = typename to_mp_list<T...>::type;

// Convert a list with integral constants of the same value_type to a value array.
template <typename List>
struct value_array_impl;
template <typename... Ts>
struct value_array_impl<mp11::mp_list<Ts...>>
{
    using value_type =
        typename mp11::mp_front<mp11::mp_list<Ts...>>::value_type;
    static constexpr std::array<value_type, sizeof...(Ts)> value{Ts::value...};
};
template <typename List>
static constexpr const auto& value_array = value_array_impl<List>::value;

// Helper to convert a front-end state to a back-end state.
template <typename StateMachine, typename State, typename Enable = void>
struct convert_state_impl
{
    using type = State;
};
// Specialization for a 'direct' state wrapper struct used as target state (defined in the back-end).
template <typename StateMachine, typename State>
struct convert_state_impl<StateMachine, State, std::enable_if_t<has_explicit_entry_be_tag<State>::value>>
{
    using type = typename State::owner;
};
// Specialization for a "direct fork", a sequence of 'direct' state wrappers used directly as the target state.
template <typename StateMachine, typename State>
struct convert_state_impl<StateMachine, State, std::enable_if_t<mpl::is_sequence<State>::value>>
{
    using target_states = to_mp_list_t<State>;
    using type = typename mp11::mp_front<target_states>::owner;
};
// Specialization for a 'entry_pt' state wrapper struct (defined in the back-end).
template <typename StateMachine, typename State>
struct convert_state_impl<StateMachine, State, std::enable_if_t<has_entry_pseudostate_be_tag<State>::value>>
{
    using type = typename State::owner;
};
// Specialization for an 'exit_pseudo_state' struct (defined in the front-end).
// This converts the FE definition to a BE definition to establish the
// connection to the target SM.
template <typename StateMachine, typename State>
struct convert_state_impl<StateMachine, State, std::enable_if_t<front::detail::has_exit_pseudostate_tag<State>::value>>
{
    using type = typename StateMachine::template exit_pt<State>;
};
// Specialization for a 'exit_pt' struct (defined in the back-end).
template <typename StateMachine, typename State>
struct convert_state_impl<StateMachine, State, std::enable_if_t<has_exit_pseudostate_be_tag<State>::value>>
{
    using type = typename State::owner;
};

template <typename StateMachine, typename State, typename Enable = void>
struct convert_source_state_impl : convert_state_impl<StateMachine, State> {};
template <typename StateMachine, typename State>
struct convert_source_state_impl<
    StateMachine,
    State,
    std::enable_if_t<front::detail::has_exit_pseudostate_tag<State>::value>>
    : convert_state_impl<StateMachine, State>
{
    // An 'exit_pseudo_state' denotes the first target of a compound transition,
    // it must not be used as source state.
    static_assert(!front::detail::has_exit_pseudostate_tag<State>::value,
                  "'exit_pseudo_state' is only allowed as target state");
};
template <typename StateMachine, typename State>
struct convert_source_state_impl<
    StateMachine,
    State,
    std::enable_if_t<has_explicit_entry_be_tag<State>::value>>
    : convert_state_impl<StateMachine, State>
{
    // Explicit entries can only denote targets.
    static_assert(!has_explicit_entry_be_tag<State>::value,
                  "'direct' is only allowed as target state");
};
template <typename StateMachine, typename State>
struct convert_source_state_impl<
    StateMachine,
    State,
    std::enable_if_t<mpl::is_sequence<State>::value>>
    : convert_state_impl<StateMachine, State>
{
    // Explicit entries can only denote targets.
    static_assert(!mpl::is_sequence<State>::value,
                  "'fork' is only allowed as target state");
};
template <typename StateMachine, typename State>
using convert_source_state = typename convert_source_state_impl<StateMachine, State>::type;

template <typename StateMachine, typename State, typename Enable = void>
struct convert_target_state_impl : convert_state_impl<StateMachine, State> {};
template <typename StateMachine, typename State>
struct convert_target_state_impl<
    StateMachine,
    State,
    std::enable_if_t<has_exit_pseudostate_be_tag<State>::value>>
    : convert_state_impl<StateMachine, State>
{
    // An exit_pt denotes the second source of a compound transition,
    // it must not be used as target state.
    // This also ensures that this transition can only be executed as a result of the
    // predecessor transition (with the 'exit_pseudo_state' as target state)
    // having been executed.
    static_assert(!has_exit_pseudostate_be_tag<State>::value,
                  "'exit_pt' is only allowed as source state");
};
template <typename StateMachine, typename State>
struct convert_target_state_impl<
    StateMachine,
    State,
    std::enable_if_t<front::detail::has_entry_pseudostate_tag<State>::value>>
    : convert_state_impl<StateMachine, State>
{
    static_assert(!front::detail::has_entry_pseudostate_tag<State>::value,
                  "'entry_pseudo_state' is only allowed as source state");
};
template <typename StateMachine, typename State>
using convert_target_state = typename convert_target_state_impl<StateMachine, State>::type;

// Parses a state machine to generate a state set.
// The implementation in this metafunction defines the state id order:
// - source states
// - target states
// - initial states
//   (if not already mentioned in the transition table)
// - states in the explicit_creation property
//   (if not already mentioned in the transition table and the property exists)
template <typename StateMachine>
struct generate_state_set_impl
{
    using front_end_t = typename StateMachine::front_end_t;
    using transition_table = to_mp_list_t<typename front_end_t::transition_table>;

    // First add the source states.
    template <typename V, typename T>
    using set_push_source_state = mp11::mp_if_c<
        !std::is_same_v<typename T::Source, front::none>,
        mp11::mp_set_push_back<V, convert_source_state<StateMachine, typename T::Source>>,
        V>;
    using partial_state_set_0 =
        mp11::mp_fold<transition_table, mp11::mp_list<>, set_push_source_state>;
    
    // Then add the target states.
    template <typename V, typename T>
    using set_push_target_state = mp11::mp_if_c<
        !std::is_same_v<typename T::Target, front::none>,
        mp11::mp_set_push_back<V, convert_target_state<StateMachine, typename T::Target>>,
        V>;
    using partial_state_set_1 =
        mp11::mp_fold<transition_table, partial_state_set_0, set_push_target_state>;
    
    // Then add the initial states.
    using initial_states = to_mp_list_t<typename front_end_t::initial_state>;
    static_assert(mp11::mp_is_set<initial_states>::value, "Each initial state must be unique");
    using partial_state_set_2 = mp11::mp_set_union<partial_state_set_1, initial_states>;

    // Then add the states marked for explicit creation.
    template <typename T>
    using add_explicit_creation_states = 
        mp11::mp_set_union<partial_state_set_2, to_mp_list_t<typename T::explicit_creation>>;
    using type = mp11::mp_eval_if_c<
        !has_explicit_creation<front_end_t>::value,
        partial_state_set_2,
        add_explicit_creation_states,
        front_end_t
        >;
};
template <typename StateMachine>
using generate_state_set = typename generate_state_set_impl<StateMachine>::type;

// extends a state set to a map with key=state and value=id
template <class StateSet>
struct generate_state_map_impl
{
    using indices = mp11::mp_iota<
        std::integral_constant<uint16_t, mp11::mp_size<StateSet>::value>>;
    using type = mp11::mp_transform_q<
        mp11::mp_bind<mp11::mp_list, mp11::_1, mp11::_2>,
        StateSet,
        indices>;
};
template <class StateSet>
using generate_state_map = typename generate_state_map_impl<StateSet>::type;

// returns the id of a given state
template <class StateMap, class State>
struct get_state_id_impl
{
    using type = mp11::mp_second<mp11::mp_map_find<
        StateMap,
        State>>;
};
template <class StateMap, class State>
using get_state_id = typename get_state_id_impl<StateMap, State>::type;

// iterates through the transition table and generate a set containing all the events
template <typename FeTransitionTable>
struct generate_event_set_impl
{
    template <typename V, typename Row>
    using set_push_event = mp11::mp_set_push_back<V, typename Row::Evt>;
    using type = mp11::mp_fold<
        to_mp_list_t<FeTransitionTable>,
        mp11::mp_list<>,
        set_push_event>;
};
template <typename FeTransitionTable>
using generate_event_set =
    typename generate_event_set_impl<FeTransitionTable>::type;

template <typename State>
using has_deferred_events =
    mp11::mp_not<mp11::mp_empty<to_mp_list_t<typename State::deferred_events>>>;

template <typename State, typename Event>
using has_deferred_event =
    mp11::mp_contains<to_mp_list_t<typename State::deferred_events>, Event>;

// Builds flags from flag_list + internal_flag_list,
// internal_flag_list is used for terminate/interrupt states.
template <typename State>
using get_flag_list = mp11::mp_append<
        to_mp_list_t<typename State::flag_list>,
        to_mp_list_t<typename State::internal_flag_list>>;

template <class State>
struct is_state_blocking_impl 
{
    template<typename T>
    using has_event_blocking_flag = typename has_event_blocking_flag<T>::type;
    typedef typename mp11::mp_any_of<
        get_flag_list<State>,
        has_event_blocking_flag
        > type;

};
template<typename T>
using is_state_blocking = typename is_state_blocking_impl<T>::type;

// Helper to print types within metafunctions for debugging.
template <typename... Ts>
struct [[deprecated]] print_types {};

} // boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_METAFUNCTIONS_HPP
