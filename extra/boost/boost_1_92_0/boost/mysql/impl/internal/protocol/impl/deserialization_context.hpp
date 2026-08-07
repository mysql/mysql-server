//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_DESERIALIZATION_CONTEXT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_DESERIALIZATION_CONTEXT_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// We operate with this enum directly in the deserialization routines for efficiency, then transform it to an
// actual error code
enum class deserialize_errc
{
    ok = 0,
    incomplete_message = 1,
    protocol_value_error,
    server_unsupported
};

inline error_code to_error_code(deserialize_errc v)
{
    switch (v)
    {
    case deserialize_errc::ok: return error_code();
    case deserialize_errc::incomplete_message: return error_code(client_errc::incomplete_message);
    case deserialize_errc::protocol_value_error: return error_code(client_errc::protocol_value_error);
    case deserialize_errc::server_unsupported: return error_code(client_errc::server_unsupported);
    default: BOOST_ASSERT(false); return error_code();  // LCOV_EXCL_LINE
    }
}

class deserialization_context
{
    const std::uint8_t* first_;
    const std::uint8_t* last_;

public:
    explicit deserialization_context(span<const std::uint8_t> data) noexcept
        : first_(data.data()), last_(first_ + data.size())
    {
    }
    const std::uint8_t* first() const { return first_; }
    const std::uint8_t* last() const { return last_; }
    std::size_t size() const { return last_ - first_; }
    void advance(std::size_t sz)
    {
        first_ += sz;
        BOOST_ASSERT(last_ >= first_);
    }
    void rewind(std::size_t sz) { first_ -= sz; }
    bool enough_size(std::size_t required_size) const { return size() >= required_size; }
    string_view get_string(std::size_t sz) const
    {
        return string_view(reinterpret_cast<const char*>(first_), sz);
    }
    error_code check_extra_bytes() const
    {
        return last_ == first_ ? error_code() : error_code(client_errc::extra_bytes);
    }

    span<const std::uint8_t> to_span() const { return span<const std::uint8_t>(first_, last_); }

    deserialize_errc deserialize() { return deserialize_errc::ok; }

    template <class Deserializable1, class... Deserializable2>
    deserialize_errc deserialize(Deserializable1& value1, Deserializable2&... args)
    {
        deserialize_errc err = value1.deserialize(*this);
        if (err == deserialize_errc::ok)
        {
            err = deserialize(args...);
        }
        return err;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
