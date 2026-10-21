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

#ifndef BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP
#define BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP

#include <cstdint>

#include <boost/msm/backmp11/detail/metafunctions.hpp>
#include <boost/msm/backmp11/detail/basic_polymorphic.hpp>
#include <boost/msm/backmp11/detail/event_pool_processor.hpp>

namespace boost::msm::backmp11
{

template <typename, typename, typename>
class state_machine;

namespace detail
{

// Wrapper for not modifying T during copy and move operations.
template <typename T>
class non_propagating
{
  public:
    non_propagating() = default;

    explicit non_propagating(const T& value) : m_value(value)
    {
    }

    non_propagating(const non_propagating&)
    {
    }

    non_propagating& operator=(const non_propagating&)
    {
        return *this;
    }

    non_propagating(non_propagating&&)
    {
    }

    non_propagating& operator=(non_propagating&&)
    {
        return *this;
    }

    T& operator*()
    {
        return m_value;
    }
    const T& operator*() const
    {
        return m_value;
    }

  private:
    T m_value;
};

enum class nesting_role
{
    root,
    nested,
    unknown
};

template <typename Config, typename Derived>
constexpr nesting_role get_nesting_role()
{
    if constexpr (std::is_same_v<typename Config::root_sm, no_root_sm>)
    {
        return nesting_role::unknown;
    }
    else if constexpr (std::is_same_v<typename Config::root_sm, Derived>)
    {
        return nesting_role::root;
    }
    else
    {
        return nesting_role::nested;
    }
}

constexpr nesting_role get_root_nesting_role(nesting_role value)
{
    switch (value)
    {
    case nesting_role::root:
    case nesting_role::nested:
        return nesting_role::root;
    case nesting_role::unknown:
        return nesting_role::unknown;
    }
}

template <typename Context, nesting_role NestingRole>
class context_member
{
  protected:
    static constexpr bool has_context_member = true;

    context_member(Context& context) noexcept : m_context(&context) {}

  private:
    template <typename, typename, typename>
    friend class backmp11::state_machine;
    template <typename, nesting_role>
    friend class state_machine_base;

    Context* m_context;
};

template <typename Context>
class context_member<Context, nesting_role::nested>
{
  protected:
    static constexpr bool has_context_member = false;
};

template <>
class context_member<no_context, nesting_role::root>
{
  protected:
    static constexpr bool has_context_member = false;
};

template <>
class context_member<no_context, nesting_role::unknown>
{
  protected:
    static constexpr bool has_context_member = false;
};

template <typename Observer, nesting_role NestingRole>
class observer_member
{
  protected:
    static constexpr bool has_observer_member = true;

    observer_member() = default;

    template <typename T,
              typename = std::enable_if_t<std::is_constructible_v<Observer, T>>>
    observer_member(T&& arg) : m_observer(std::forward<T>(arg)) {}

  private:
    template <typename, typename, typename>
    friend class backmp11::state_machine;
    template <typename, nesting_role>
    friend class state_machine_base;

    Observer m_observer;
};

template <typename Observer>
class observer_member<Observer, nesting_role::nested>
{
  protected:
    static constexpr bool has_observer_member = false;
};

template <>
class observer_member<no_observer, nesting_role::root>
{
  protected:
    static constexpr bool has_observer_member = false;
};

template <>
class observer_member<no_observer, nesting_role::unknown>
{
  protected:
    static constexpr bool has_observer_member = false;
};

template <typename Config, nesting_role NestingRole>
class state_machine_base
    : public event_pool_processor<typename Config::event_pool>,
      public context_member<typename Config::context, NestingRole>,
      public observer_member<typename Config::observer, NestingRole>
{
  public:
    using config_t = Config;
    using root_sm_t = typename Config::root_sm;
    using context_t = typename Config::context;
    using observer_t = typename Config::observer;

    /// Gets the current state of the state machine.
    /// See @ref machine_state.
    machine_state get_machine_state() const
    {
        return m_machine_state;
    }

    /// Gets the context of the state machine.
    /// See @ref state_machine_config::context.
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    context_t& get_context()
    {
        return const_cast<context_t&>(std::as_const(*this).get_context());
    }

    /// Gets the context of the state machine.
    /// See @ref state_machine_config::context.
    template <bool C = !std::is_same_v<context_t, no_context>,
              typename = std::enable_if_t<C>>
    const context_t& get_context() const
    {
        if constexpr (NestingRole == nesting_role::root)
        {
            return *this->m_context;
        }
        else
        {
            return *(*m_root_sm)->m_context;
        }
    }

    /// Get the observer of the state machine.
    /// See @ref state_machine_config::observer.
    template <bool C = !std::is_same_v<observer_t, no_observer>,
              typename = std::enable_if_t<C>>
    observer_t& get_observer()
    {
        return const_cast<observer_t&>(std::as_const(*this).get_observer());
    }

    /// Gets the observer of the state machine.
    /// See @ref state_machine_config::observer.
    template <bool C = !std::is_same_v<observer_t, no_observer>,
              typename = std::enable_if_t<C>>
    const observer_t& get_observer() const
    {
        if constexpr (NestingRole == nesting_role::root)
        {
            return this->m_observer;
        }
        else
        {
            return (*m_root_sm)->m_observer;
        }
    }

    /**
     * @brief Processes up to `max_events` from the event pool.
     * 
     * @param max_events 
     * @return size_t The no. of processed events.
     */
    template <bool C = state_machine_base::has_event_pool,
              typename = std::enable_if_t<C>>
    inline size_t process_event_pool(size_t max_events = SIZE_MAX)
    {
        if (this->get_event_pool().events.empty() ||
            get_machine_state() != machine_state::idle)
        {
            return 0;
        }
        return event_pool_processor::process_event_pool(max_events);
    }

    /// Gets the root machine.
    /// See @ref state_machine_config::root_sm.
    template <bool C = !std::is_same_v<root_sm_t, no_root_sm>,
              typename = std::enable_if_t<C>>
    root_sm_t& get_root_sm()
    {
        return *static_cast<root_sm_t*>(*m_root_sm);
    }
    /// Gets the root machine.
    /// See @ref state_machine_config::root_sm.
    template <bool C = !std::is_same_v<root_sm_t, no_root_sm>,
              typename = std::enable_if_t<C>>
    const root_sm_t& get_root_sm() const
    {
        return *static_cast<const root_sm_t*>(*m_root_sm);
    }

    /// Checks if the sm is contained in another sm.
    bool is_contained() const
    {
        return (static_cast<const void*>(this) != *m_root_sm);
    }

  protected:
    using context_member =
        detail::context_member<typename Config::context, NestingRole>;
    using observer_member =
        detail::observer_member<typename Config::observer, NestingRole>;

    using context_member::context_member;

    using observer_member::observer_member;

    state_machine_base() = default;

    template <typename T, typename = std::enable_if_t<
                              context_member::has_context_member &&
                              observer_member::has_observer_member &&
                              std::is_constructible_v<observer_t, T>>>
    state_machine_base(context_t& context, T&& arg)
        : context_member(context), observer_member(std::forward<T>(arg))
    {
    }

  private:
    template <typename, nesting_role>
    friend class state_machine_base;
    template <typename, typename, typename>
    friend class backmp11::state_machine;
    template <typename, typename, visit_mode, bool,
              template <typename> typename...>
    friend class state_visitor_impl;
    template <typename>
    friend class init_state_visitor;
    template <typename, typename, template <typename> typename...>
    friend class event_deferral_visitor;
    template <typename StateMachine>
    friend struct transition_table_impl;

    using event_pool_processor =
        detail::event_pool_processor<typename Config::event_pool>;
    using root_sm_base =
        state_machine_base<Config, get_root_nesting_role(NestingRole)>;

    static_assert(
        is_config<Config>::value,
        "Config must be an instance of state machine config");

    non_propagating<root_sm_base*> m_root_sm{nullptr};
    machine_state                  m_machine_state{machine_state::stopped};
};

} // namespace detail
} // namespace boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_DETAIL_STATE_MACHINE_BASE_HPP
