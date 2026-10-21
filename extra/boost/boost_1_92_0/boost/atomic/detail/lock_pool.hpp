/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2011 Helge Bahmann
 * Copyright (c) 2013-2014, 2020 Andrey Semashev
 */
/*!
 * \file   atomic/detail/lock_pool.hpp
 *
 * This header contains declaration of the lock pool used to emulate atomic ops.
 */

#ifndef BOOST_ATOMIC_DETAIL_LOCK_POOL_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_LOCK_POOL_HPP_INCLUDED_

#include <cstddef>
#include <chrono>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/link.hpp>
#include <boost/atomic/detail/intptr.hpp>
#if defined(BOOST_WINDOWS)
#include <boost/winapi/thread.hpp>
#else // defined(BOOST_WINDOWS)
#include <time.h>
#if !defined(BOOST_HAS_NANOSLEEP)
#include <unistd.h>
#endif // !defined(BOOST_HAS_NANOSLEEP)
#endif // defined(BOOST_WINDOWS)
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

BOOST_FORCEINLINE void wait_some() noexcept
{
#if defined(BOOST_WINDOWS)
    boost::winapi::SwitchToThread();
#elif defined(BOOST_HAS_NANOSLEEP)
    // Do not use sched_yield or pthread_yield as at least on Linux it doesn't block the thread if there are no other
    // pending threads on the current CPU. Proper sleeping is guaranteed to block the thread, which allows other threads
    // to potentially migrate to this CPU and complete the tasks we're waiting for.
    timespec ts{};
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;
    nanosleep(&ts, nullptr);
#else
    usleep(1);
#endif
}

namespace lock_pool {

BOOST_ATOMIC_DECL void* short_lock(atomics::detail::uintptr_t h) noexcept;
BOOST_ATOMIC_DECL void* long_lock(atomics::detail::uintptr_t h) noexcept;
BOOST_ATOMIC_DECL void unlock(void* vls) noexcept;

BOOST_ATOMIC_DECL void* allocate_wait_state(void* vls, const volatile void* addr) noexcept;
BOOST_ATOMIC_DECL void free_wait_state(void* vls, void* vws) noexcept;
BOOST_ATOMIC_DECL void wait(void* vls, void* vws) noexcept;
#if !defined(BOOST_WINDOWS)
BOOST_ATOMIC_DECL bool wait_until(void* vls, void* vws, clockid_t clock_id, timespec const& abs_timeout) noexcept;
#endif // !defined(BOOST_WINDOWS)
BOOST_ATOMIC_DECL bool wait_for(void* vls, void* vws, std::chrono::nanoseconds rel_timeout) noexcept;
BOOST_ATOMIC_DECL void notify_one(void* vls, const volatile void* addr) noexcept;
BOOST_ATOMIC_DECL void notify_all(void* vls, const volatile void* addr) noexcept;

BOOST_ATOMIC_DECL void thread_fence() noexcept;
BOOST_ATOMIC_DECL void signal_fence() noexcept;

template< std::size_t Alignment >
BOOST_FORCEINLINE atomics::detail::uintptr_t hash_ptr(const volatile void* addr) noexcept
{
    atomics::detail::uintptr_t ptr = reinterpret_cast< atomics::detail::uintptr_t >(addr);
    atomics::detail::uintptr_t h = ptr / Alignment;

    // Since many malloc/new implementations return pointers with higher alignment
    // than indicated by Alignment, it makes sense to mix higher bits
    // into the lower ones. On 64-bit platforms, malloc typically aligns to 16 bytes,
    // on 32-bit - to 8 bytes.
    constexpr std::size_t malloc_alignment = sizeof(void*) >= 8u ? 16u : 8u;
    BOOST_IF_CONSTEXPR (Alignment != malloc_alignment)
        h ^= ptr / malloc_alignment;

    return h;
}

template< std::size_t Alignment, bool LongLock = false >
class scoped_lock
{
private:
    void* m_lock;

public:
    explicit scoped_lock(const volatile void* addr) noexcept
    {
        atomics::detail::uintptr_t h = lock_pool::hash_ptr< Alignment >(addr);
        BOOST_IF_CONSTEXPR (!LongLock)
            m_lock = lock_pool::short_lock(h);
        else
            m_lock = lock_pool::long_lock(h);
    }

    scoped_lock(scoped_lock const&) = delete;
    scoped_lock& operator=(scoped_lock const&) = delete;

    ~scoped_lock() noexcept
    {
        lock_pool::unlock(m_lock);
    }

    void* get_lock_state() const noexcept
    {
        return m_lock;
    }
};

template< std::size_t Alignment >
class scoped_wait_state :
    public scoped_lock< Alignment, true >
{
private:
    void* m_wait_state;

public:
    explicit scoped_wait_state(const volatile void* addr) noexcept :
        scoped_lock< Alignment, true >(addr)
    {
        m_wait_state = lock_pool::allocate_wait_state(this->get_lock_state(), addr);
    }

    scoped_wait_state(scoped_wait_state const&) = delete;
    scoped_wait_state& operator=(scoped_wait_state const&) = delete;

    ~scoped_wait_state() noexcept
    {
        lock_pool::free_wait_state(this->get_lock_state(), m_wait_state);
    }

    void wait() noexcept
    {
        lock_pool::wait(this->get_lock_state(), m_wait_state);
    }

#if !defined(BOOST_WINDOWS)
    bool wait_until(clockid_t clock_id, timespec const& abs_timeout) noexcept
    {
        return lock_pool::wait_until(this->get_lock_state(), m_wait_state, clock_id, abs_timeout);
    }
#endif // !defined(BOOST_WINDOWS)

    bool wait_for(std::chrono::nanoseconds rel_timeout) noexcept
    {
        return lock_pool::wait_for(this->get_lock_state(), m_wait_state, rel_timeout);
    }
};

} // namespace lock_pool
} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_LOCK_POOL_HPP_INCLUDED_
