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

#ifndef BOOST_MSM_BACKMP11_DETAIL_EVENT_POOL_PROCESSOR_HPP
#define BOOST_MSM_BACKMP11_DETAIL_EVENT_POOL_PROCESSOR_HPP

#include <cstdint>
#include <optional>

#include <boost/config.hpp>

#include <boost/msm/backmp11/detail/basic_polymorphic.hpp>
#include <boost/msm/backmp11/detail/common.hpp>
#include <boost/msm/backmp11/state_machine_config.hpp>

namespace boost::msm::backmp11::detail
{

// Occurrence of an event.
// Event occurrences are placed in an event pool for later processing.
class event_occurrence
{
  public:
    using process_fn_t = std::optional<process_result> (*)(
        event_occurrence&, void* /*processor*/, uint16_t /*seq_cnt*/);

    event_occurrence(process_fn_t process_fn) : m_process_fn(process_fn)
    {
    }

    // Try to process the event.
    // A return value std::nullopt means that the conditions for processing
    // were not given and the event has not been dispatched.
    std::optional<process_result> try_process(void* processor, uint16_t seq_cnt)
    {
        return m_process_fn(*this, processor, seq_cnt);
    }

    bool is_processed() const
    {
        return m_process_fn == nullptr;
    }

  protected:
    template <typename Derived, typename StateMachine>
    static std::optional<process_result> try_process_thunk(
        event_occurrence& self, void* processor, uint16_t seq_cnt)
    {
        return static_cast<Derived&>(self).try_process(
            static_cast<StateMachine&>(
                *static_cast<typename StateMachine::event_pool_processor*>(
                    processor)),
            seq_cnt);
    }

    void mark_processed()
    {
        m_process_fn = nullptr;
    }

  private:
    process_fn_t m_process_fn{};
};

template <typename Event>
class deferred_event : public event_occurrence
{
  public:
    template <typename StateMachine>
    deferred_event(StateMachine&, const Event& event, uint16_t seq_cnt) noexcept
        : event_occurrence(&try_process_thunk<deferred_event, StateMachine>),
          m_seq_cnt(seq_cnt), m_event(event)
    {
    }

    template <typename StateMachine>
    std::optional<process_result> try_process(StateMachine& sm,
                                              uint16_t seq_cnt)
    {
        if ((m_seq_cnt == seq_cnt) || sm.is_event_deferred(m_event))
        {
            return std::nullopt;
        }
        mark_processed();
        return sm.process_event_observed(m_event, process_info::event_pool);
    }

  private:
    uint16_t m_seq_cnt;
    Event m_event;
};

template <typename Config,
          typename = void>
class event_pool_processor
{
    struct alignas(Config::inline_align) dummy_event
    {
        char data[Config::inline_capacity];
    };
    using deferred_dummy = deferred_event<dummy_event>;

  protected:
    static constexpr bool has_event_pool = true;
    using processable_event =
        basic_polymorphic<event_occurrence, sizeof(deferred_dummy),
                          alignof(deferred_dummy)>;
    using event_pool_container_t =
        typename Config::template container_t<processable_event>;

    struct event_pool_t
    {
        event_pool_container_t events;
        uint16_t cur_seq_cnt{};
    };

    event_pool_t& get_event_pool()
    {
        return this->m_event_pool;
    }

    const event_pool_t& get_event_pool() const
    {
        return this->m_event_pool;
    }

    // Core logic for event pool processing.
    // Explicitly not inline, because code size can significantly increase if
    // this method's content is inlined in all entries and process_event calls.
    BOOST_NOINLINE size_t process_event_pool(size_t max_events)
    {
        size_t processed_events = 0;
        auto it = m_event_pool.events.begin();
        while (it != m_event_pool.events.end())
        {
            event_occurrence& event = **it;
            if (event.is_processed())
            {
                it = m_event_pool.events.erase(it);
                continue;
            }

            std::optional<process_result> result =
                event.try_process(this, m_event_pool.cur_seq_cnt);
            // The event has not been dispatched.
            if (!result.has_value())
            {
                it++;
                continue;
            }

            // Consider anything except "only deferred" to be a processed event.
            if (*result != process_result::deferred)
            {
                processed_events++;
                if (processed_events == max_events)
                {
                    break;
                }
            }

            // Start from the beginning, we might be able to process
            // events that were deferred before.
            it = m_event_pool.events.begin();
            // Consider newly deferred events only if
            // the event was not deferred at the same time
            // (required to prevent infinitely processing the same event,
            // if it was handled and at the same time action-deferred
            // in orthogonal regions).
            if (!(any(*result & process_result::deferred)))
            {
                m_event_pool.cur_seq_cnt += 1;
            }
        }
        return processed_events;
    }

  private:
    event_pool_t m_event_pool;
};

template <typename Config>
class event_pool_processor<
    Config,
    std::enable_if_t<std::is_same_v<Config, no_event_pool>>>
{
  protected:
    static constexpr bool has_event_pool = false;
};

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_EVENT_POOL_PROCESSOR_HPP
