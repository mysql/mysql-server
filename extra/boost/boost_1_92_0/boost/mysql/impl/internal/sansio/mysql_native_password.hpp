//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MYSQL_NATIVE_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MYSQL_NATIVE_PASSWORD_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>
#include <boost/mysql/impl/internal/sansio/auth_plugin_common.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <openssl/sha.h>

// Reference:
// https://dev.mysql.com/doc/dev/mysql-server/8.4.4/page_protocol_connection_phase_authentication_methods_native_password_authentication.html

namespace boost {
namespace mysql {
namespace detail {

// Constants
BOOST_INLINE_CONSTEXPR std::size_t mnp_hash_size = 20;
BOOST_INLINE_CONSTEXPR const char* mnp_plugin_name = "mysql_native_password";
static_assert(mnp_hash_size <= max_hash_size, "");
static_assert(mnp_hash_size == SHA_DIGEST_LENGTH, "Buffer size mismatch");

// SHA1( password ) XOR SHA1( "20-bytes random data from server" <concat> SHA1( SHA1( password ) ) )
inline void mnp_hash_password_impl(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble,
    span<std::uint8_t, mnp_hash_size> output
)
{
    // SHA1 (password)
    std::array<std::uint8_t, mnp_hash_size> password_sha1;
    SHA1(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha1.data());

    // Add server scramble (salt)
    std::array<std::uint8_t, scramble_size + mnp_hash_size> salted_buffer;
    std::memcpy(salted_buffer.data(), scramble.data(), scramble.size());
    SHA1(password_sha1.data(), password_sha1.size(), salted_buffer.data() + mnp_hash_size);
    std::array<std::uint8_t, mnp_hash_size> salted_sha1;
    SHA1(salted_buffer.data(), salted_buffer.size(), salted_sha1.data());

    // XOR
    for (std::size_t i = 0; i < mnp_hash_size; ++i)
    {
        output[i] = password_sha1[i] ^ salted_sha1[i];
    }
}

// The static buffer size is chosen so that every plugin uses the same size
inline static_buffer<max_hash_size> mnp_hash_password(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble
)
{
    // Empty passwords are not hashed
    if (password.empty())
        return {};

    // Run the algorithm
    static_buffer<max_hash_size> res(mnp_hash_size);
    mnp_hash_password_impl(password, scramble, span<std::uint8_t, mnp_hash_size>(res.data(), mnp_hash_size));
    return res;
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
