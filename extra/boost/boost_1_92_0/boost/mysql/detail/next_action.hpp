//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NEXT_ACTION_HPP
#define BOOST_MYSQL_DETAIL_NEXT_ACTION_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

enum class next_action_type
{
    none,
    write,
    read,
    ssl_handshake,
    ssl_shutdown,
    connect,
    close,
};

class next_action
{
public:
    struct read_args_t
    {
        span<std::uint8_t> buffer;
        bool use_ssl;
    };

    struct write_args_t
    {
        span<const std::uint8_t> buffer;
        bool use_ssl;
    };

    next_action(error_code ec = {}) noexcept : type_(next_action_type::none), data_(ec) {}

    // Type
    next_action_type type() const noexcept { return type_; }
    bool is_done() const noexcept { return type_ == next_action_type::none; }
    bool success() const noexcept { return is_done() && !data_.ec; }

    // Arguments
    error_code error() const noexcept
    {
        BOOST_ASSERT(is_done());
        return data_.ec;
    }
    const void* connect_endpoint() const noexcept { return data_.connect_endpoint; }
    read_args_t read_args() const noexcept
    {
        BOOST_ASSERT(type_ == next_action_type::read);
        return data_.read_args;
    }
    write_args_t write_args() const noexcept
    {
        BOOST_ASSERT(type_ == next_action_type::write);
        return data_.write_args;
    }

    static next_action connect(const void* endpoint) noexcept
    {
        return next_action(next_action_type::connect, endpoint);
    }
    static next_action read(read_args_t args) noexcept { return next_action(next_action_type::read, args); }
    static next_action write(write_args_t args) noexcept
    {
        return next_action(next_action_type::write, args);
    }
    static next_action ssl_handshake() noexcept
    {
        return next_action(next_action_type::ssl_handshake, data_t());
    }
    static next_action ssl_shutdown() noexcept
    {
        return next_action(next_action_type::ssl_shutdown, data_t());
    }
    static next_action close() noexcept { return next_action(next_action_type::close, data_t()); }

private:
    next_action_type type_{next_action_type::none};
    union data_t
    {
        error_code ec;
        const void* connect_endpoint;
        read_args_t read_args;
        write_args_t write_args;

        data_t() noexcept : ec(error_code()) {}
        data_t(const void* endpoint) noexcept : connect_endpoint(endpoint) {}
        data_t(error_code ec) noexcept : ec(ec) {}
        data_t(read_args_t args) noexcept : read_args(args) {}
        data_t(write_args_t args) noexcept : write_args(args) {}
    } data_;

    next_action(next_action_type t, data_t data) noexcept : type_(t), data_(data) {}
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
