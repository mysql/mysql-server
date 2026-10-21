//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_SANSIO_CONNECTION_NODE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_SANSIO_CONNECTION_NODE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

#include <algorithm>
#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

// The status the connection is in
enum class node_status
{
    // Connection task hasn't initiated yet.
    // This status doesn't count as pending. This facilitates tracking pending connections.
    initial,

    // Connection is trying to connect
    connect_in_progress,

    // Connect failed and we're sleeping
    sleep_connect_failed_in_progress,

    // Connection is trying to reset
    reset_in_progress,

    // Connection is trying to ping
    ping_in_progress,

    // Connection can be handed to the user
    idle,

    // Connection has been handed to the user
    in_use,

    // After cancel
    terminated,
};

// The next I/O action the connection should take. There's
// no 1-1 mapping to node_status
enum class next_connection_action
{
    // Do nothing, exit the loop
    none,

    // Issue a connect
    connect,

    // Connect failed, issue a sleep
    sleep_connect_failed,

    // Wait until a collection request is issued or the ping interval elapses
    idle_wait,

    // Issue a reset
    reset,

    // Issue a ping
    ping,
};

// A collection_state represents the possibility that a connection
// that was in_use was returned by the user
enum class collection_state
{
    // Connection wasn't returned
    none,

    // Connection was returned and needs reset
    needs_collect,

    // Connection was returned and doesn't need reset
    needs_collect_with_reset
};

// CRTP. Derived should implement the entering_xxx and exiting_xxx hook functions.
// Derived must derive from this class
template <class Derived>
class sansio_connection_node
{
    node_status status_;

    inline bool is_pending(node_status status) noexcept
    {
        return status != node_status::initial && status != node_status::idle &&
               status != node_status::in_use && status != node_status::terminated;
    }

    inline static next_connection_action status_to_action(node_status status) noexcept
    {
        switch (status)
        {
        case node_status::connect_in_progress: return next_connection_action::connect;
        case node_status::sleep_connect_failed_in_progress:
            return next_connection_action::sleep_connect_failed;
        case node_status::ping_in_progress: return next_connection_action::ping;
        case node_status::reset_in_progress: return next_connection_action::reset;
        case node_status::idle:
        case node_status::in_use: return next_connection_action::idle_wait;
        default: return next_connection_action::none;
        }
    }

    next_connection_action set_status(node_status new_status)
    {
        auto& derived = static_cast<Derived&>(*this);

        // Notify we're entering/leaving the idle status
        if (new_status == node_status::idle && status_ != node_status::idle)
            derived.entering_idle();
        else if (new_status != node_status::idle && status_ == node_status::idle)
            derived.exiting_idle();

        // Notify we're entering/leaving a pending status
        if (!is_pending(status_) && is_pending(new_status))
            derived.entering_pending();
        else if (is_pending(status_) && !is_pending(new_status))
            derived.exiting_pending();

        // Actually update status
        status_ = new_status;

        return status_to_action(new_status);
    }

public:
    sansio_connection_node(node_status initial_status = node_status::initial) noexcept
        : status_(initial_status)
    {
    }

    void mark_as_in_use() noexcept
    {
        BOOST_ASSERT(status_ == node_status::idle);
        set_status(node_status::in_use);
    }

    void cancel() { set_status(node_status::terminated); }

    next_connection_action resume(error_code ec, collection_state col_st)
    {
        switch (status_)
        {
        case node_status::initial: return set_status(node_status::connect_in_progress);
        case node_status::connect_in_progress:
            return ec ? set_status(node_status::sleep_connect_failed_in_progress)
                      : set_status(node_status::idle);
        case node_status::sleep_connect_failed_in_progress:
            return set_status(node_status::connect_in_progress);
        case node_status::idle:
            // The wait finished with no interruptions, and the connection
            // is still idle. Time to ping.
            return set_status(node_status::ping_in_progress);
        case node_status::in_use:
            // If col_st != none, the user has notified us to collect the connection.
            // This happens after they return the connection to the pool.
            // Update status and continue
            if (col_st == collection_state::needs_collect)
            {
                // No reset needed, we're idle
                return set_status(node_status::idle);
            }
            else if (col_st == collection_state::needs_collect_with_reset)
            {
                return set_status(node_status::reset_in_progress);
            }
            else
            {
                // The user is still using the connection (it's taking long, but can happen).
                // Idle wait again until they return the connection.
                return next_connection_action::idle_wait;
            }
        case node_status::ping_in_progress:
        case node_status::reset_in_progress:
            // Reconnect if there was an error. Otherwise, we're idle
            return ec ? set_status(node_status::connect_in_progress) : set_status(node_status::idle);
        case node_status::terminated:
        default: return next_connection_action::none;
        }
    }

    // Exposed for testing
    node_status status() const noexcept { return status_; }
};

// Composes a diagnostics object containing info about the last connect error.
// Suitable for the diagnostics output of async_get_connection
inline diagnostics create_connect_diagnostics(error_code connect_ec, const diagnostics& connect_diag)
{
    diagnostics res;
    if (connect_ec)
    {
        // Manipulating the internal representations is more efficient here,
        // and better than using stringstream
        auto& res_impl = access::get_impl(res);
        const auto& connect_diag_impl = access::get_impl(connect_diag);

        if (connect_ec == asio::error::operation_aborted)
        {
            // operation_aborted in this context means timeout
            res_impl.msg = "Last connection attempt timed out";
            res_impl.is_server = false;
        }
        else
        {
            // Add the error code information
            res_impl.msg = "Last connection attempt failed with: ";
            res_impl.msg += connect_ec.message();
            res_impl.msg += " [";
            res_impl.msg += connect_ec.to_string();
            res_impl.msg += "]";

            // Add any diagnostics
            if (connect_diag_impl.msg.empty())
            {
                // The resulting object doesn't contain server-supplied info
                res_impl.is_server = false;
            }
            else
            {
                // The resulting object may contain server-supplied info
                res_impl.msg += ": ";
                res_impl.msg += connect_diag_impl.msg;
                res_impl.is_server = connect_diag_impl.is_server;
            }
        }
    }
    return res;
}

// Given config params and the current state, computes the number
// of connections that the pool should create at any given point in time
inline std::size_t num_connections_to_create(
    std::size_t initial_size,         // config
    std::size_t max_size,             // config
    std::size_t current_connections,  // the number of connections in the pool, in any state
    std::size_t pending_connections,  // the number of connections in the pool in pending state
    std::size_t pending_requests      // the current number of async_get_connection requests that are waiting
)
{
    BOOST_ASSERT(initial_size <= max_size);
    BOOST_ASSERT(current_connections <= max_size);
    BOOST_ASSERT(pending_connections <= current_connections);

    // We aim to have one pending connection per pending request.
    // When these connections successfully connect, they will fulfill the pending requests.
    std::size_t required_by_requests = pending_requests > pending_connections
                                           ? static_cast<std::size_t>(pending_requests - pending_connections)
                                           : 0u;

    // We should always have at least min_connections.
    // This might not be the case if the pool is just starting.
    std::size_t required_by_min = current_connections < initial_size
                                      ? static_cast<std::size_t>(initial_size - current_connections)
                                      : 0u;

    // We can't excess max_connections. This is the room for new connections that we have
    std::size_t room = static_cast<std::size_t>(max_size - current_connections);

    return (std::min)((std::max)(required_by_requests, required_by_min), room);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
