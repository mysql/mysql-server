// Copyright 2025 Boost.Python Contributors
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PYTHON_DETAIL_PYMUTEX_HPP
#define BOOST_PYTHON_DETAIL_PYMUTEX_HPP

#include <boost/python/detail/prefix.hpp>
#ifdef Py_GIL_DISABLED
// needed for pymutex wrapper
#include <atomic>
#include <cstddef>
#endif

namespace boost { namespace python { namespace detail {

#ifdef Py_GIL_DISABLED

// Re-entrant wrapper around PyMutex for free-threaded Python
// Similar to _PyRecursiveMutex or threading.RLock
class pymutex {
    PyMutex m_mutex;
    std::atomic<unsigned long> m_owner;
    std::size_t m_level;

public:
    pymutex() : m_mutex({}), m_owner(0), m_level(0) {}

    // Non-copyable, non-movable
    pymutex(const pymutex&) = delete;
    pymutex& operator=(const pymutex&) = delete;

    void lock() {
        unsigned long thread = PyThread_get_thread_ident();
        if (m_owner.load(std::memory_order_relaxed) == thread) {
            m_level++;
            return;
        }
        PyMutex_Lock(&m_mutex);
        m_owner.store(thread, std::memory_order_relaxed);
        // m_level should be 0 when we acquire the lock
    }

    void unlock() {
        unsigned long thread = PyThread_get_thread_ident();
        // Verify current thread owns the lock
        if (m_owner.load(std::memory_order_relaxed) != thread) {
            // This should never happen - programming error
            return;
        }
        if (m_level > 0) {
            m_level--;
            return;
        }
        m_owner.store(0, std::memory_order_relaxed);
        PyMutex_Unlock(&m_mutex);
    }

    bool is_locked_by_current_thread() const {
        unsigned long thread = PyThread_get_thread_ident();
        return m_owner.load(std::memory_order_relaxed) == thread;
    }
};


// RAII lock guard for pymutex
class pymutex_guard {
    pymutex& m_mutex;

public:
    explicit pymutex_guard(pymutex& mutex) : m_mutex(mutex) {
        m_mutex.lock();
    }

    ~pymutex_guard() {
        m_mutex.unlock();
    }

    // Non-copyable, non-movable
    pymutex_guard(const pymutex_guard&) = delete;
    pymutex_guard& operator=(const pymutex_guard&) = delete;
};

// Global mutex for protecting all Boost.Python internal state
// Similar to pybind11's internals.mutex
BOOST_PYTHON_DECL pymutex& get_global_mutex();

// Macro for acquiring the global lock
// Similar to pybind11's PYBIND11_LOCK_INTERNALS
#define BOOST_PYTHON_LOCK_STATE() \
    ::boost::python::detail::pymutex_guard lock(::boost::python::detail::get_global_mutex())

#else

// No-op macro when not in free-threaded mode
#define BOOST_PYTHON_LOCK_STATE()

#endif // Py_GIL_DISABLED

}}} // namespace boost::python::detail

#endif // BOOST_PYTHON_DETAIL_PYMUTEX_HPP
