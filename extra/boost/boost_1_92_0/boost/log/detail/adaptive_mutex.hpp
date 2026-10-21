/*
 *          Copyright Andrey Semashev 2007 - 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   adaptive_mutex.hpp
 * \author Andrey Semashev
 * \date   01.08.2010
 *
 * \brief  This header is the Boost.Log library implementation, see the library documentation
 *         at http://www.boost.org/doc/libs/release/libs/log/doc/html/index.html.
 */

#ifndef BOOST_LOG_DETAIL_ADAPTIVE_MUTEX_HPP_INCLUDED_
#define BOOST_LOG_DETAIL_ADAPTIVE_MUTEX_HPP_INCLUDED_

#include <boost/log/detail/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#ifndef BOOST_LOG_NO_THREADS

#include <boost/atomic/capabilities.hpp>

#if defined(BOOST_WINDOWS) || (BOOST_ATOMIC_INT_LOCK_FREE == 2) && (BOOST_ATOMIC_HAS_NATIVE_INT_WAIT_NOTIFY == 2)
#define BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_ATOMIC
#else
#define BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_PTHREAD
#endif

#if defined(BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_ATOMIC)

#include <boost/memory_order.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/atomic/thread_pause.hpp>
#include <boost/log/detail/header.hpp>

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

//! A mutex that performs spinning or thread blocking in case of contention
class adaptive_mutex
{
private:
    enum state : unsigned int
    {
        locked_bit = 1u,
        waiters_one = 1u << 1u
    };

    enum pause_constants : unsigned int
    {
        initial_pause = 1u,
        max_pause = 16u
    };

    //! Mutex state. Lowest bit indicates whether the mutex is locked or not, all higher bits store the number of blocked waiters. See \c state enum.
    boost::atomics::atomic< unsigned int > m_state;

public:
    adaptive_mutex() noexcept : m_state(0u) {}

    //  Non-copyable
    adaptive_mutex(adaptive_mutex const&) = delete;
    adaptive_mutex& operator= (adaptive_mutex const&) = delete;

    bool try_lock() noexcept
    {
        unsigned int old_state = m_state.load(boost::memory_order::relaxed);
        return (old_state & locked_bit) == 0u &&
            m_state.compare_exchange_strong(old_state, old_state | locked_bit, boost::memory_order::acquire, boost::memory_order::relaxed);
    }

    void lock() noexcept
    {
        unsigned int pause_count = initial_pause;
        unsigned int waiter_added = 0u;
        unsigned int old_state = m_state.load(boost::memory_order::relaxed);
        while (true)
        {
            if ((old_state & locked_bit) == 0u)
            {
                unsigned int new_state = (old_state - waiter_added) | locked_bit;
                if (m_state.compare_exchange_weak(old_state, new_state, boost::memory_order::acquire, boost::memory_order::relaxed))
                    break;

                continue;
            }

            if (pause_count < max_pause)
            {
                if (waiter_added != 0u)
                {
                    old_state = m_state.sub(waiters_one, boost::memory_order::relaxed);
                    waiter_added = 0u;
                }
                else
                {
                    for (unsigned int i = 0u; i < pause_count; ++i)
                    {
                        boost::atomics::thread_pause();
                    }
                    pause_count += pause_count;

                    old_state = m_state.load(boost::memory_order::relaxed);
                }
            }
            else if (waiter_added == 0u)
            {
                old_state = m_state.add(waiters_one, boost::memory_order::relaxed);
                waiter_added = waiters_one;
            }
            else
            {
                // Restart spinning after waking up this thread
                pause_count = initial_pause;
                old_state = m_state.wait(old_state, boost::memory_order::relaxed);
            }
        }
    }

    void unlock() noexcept
    {
        if (m_state.and_and_test(~static_cast< unsigned int >(locked_bit), boost::memory_order::release))
        {
            // If the resulting state is non-zero then there are blocked waiters
            m_state.notify_one();
        }
    }
};

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#include <boost/log/detail/footer.hpp>

#elif defined(BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_PTHREAD)

#include <pthread.h>
#include <cerrno>
#include <system_error>
#include <boost/assert.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/log/detail/header.hpp>

#if defined(PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP)
#define BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_PTHREAD_MUTEX_ADAPTIVE_NP
#endif

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

//! A mutex that performs spinning or thread blocking in case of contention
class adaptive_mutex
{
private:
    pthread_mutex_t m_state;

public:
    adaptive_mutex()
    {
#if defined(BOOST_LOG_AUX_ADAPTIVE_MUTEX_USE_PTHREAD_MUTEX_ADAPTIVE_NP)
        pthread_mutexattr_t attrs;
        pthread_mutexattr_init(&attrs);
        pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_ADAPTIVE_NP);

        const int err = pthread_mutex_init(&m_state, &attrs);
        pthread_mutexattr_destroy(&attrs);
#else
        const int err = pthread_mutex_init(&m_state, nullptr);
#endif
        if (BOOST_UNLIKELY(err != 0))
            throw_system_error(err, "Failed to initialize an adaptive mutex", "adaptive_mutex::adaptive_mutex()", __FILE__, __LINE__);
    }

    //  Non-copyable
    adaptive_mutex(adaptive_mutex const&) = delete;
    adaptive_mutex& operator= (adaptive_mutex const&) = delete;

    ~adaptive_mutex()
    {
        BOOST_VERIFY(pthread_mutex_destroy(&m_state) == 0);
    }

    bool try_lock()
    {
        const int err = pthread_mutex_trylock(&m_state);
        if (err == 0)
            return true;
        if (BOOST_UNLIKELY(err != EBUSY))
            throw_system_error(err, "Failed to lock an adaptive mutex", "adaptive_mutex::try_lock()", __FILE__, __LINE__);
        return false;
    }

    void lock()
    {
        const int err = pthread_mutex_lock(&m_state);
        if (BOOST_UNLIKELY(err != 0))
            throw_system_error(err, "Failed to lock an adaptive mutex", "adaptive_mutex::lock()", __FILE__, __LINE__);
    }

    void unlock() noexcept
    {
        BOOST_VERIFY(pthread_mutex_unlock(&m_state) == 0);
    }

private:
    static BOOST_NOINLINE BOOST_LOG_NORETURN void throw_system_error(int err, const char* descr, const char* func, const char* file, int line)
    {
        boost::throw_exception(std::system_error(std::error_code(err, std::system_category()), descr), boost::source_location(file, line, func));
    }
};

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#include <boost/log/detail/footer.hpp>

#endif

#endif // BOOST_LOG_NO_THREADS

#endif // BOOST_LOG_DETAIL_ADAPTIVE_MUTEX_HPP_INCLUDED_
