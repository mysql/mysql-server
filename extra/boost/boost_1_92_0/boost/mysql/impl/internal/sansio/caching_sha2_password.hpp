//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CACHING_SHA2_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CACHING_SHA2_PASSWORD_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>
#include <boost/mysql/impl/internal/sansio/auth_plugin_common.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/csha2p_encrypt_password.hpp>

#include <boost/asio/ssl/error.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/core/span.hpp>
#include <boost/system/result.hpp>
#include <boost/system/system_category.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <openssl/sha.h>

// Reference:
// https://dev.mysql.com/doc/dev/mysql-server/latest/page_caching_sha2_authentication_exchanges.html

namespace boost {
namespace mysql {
namespace detail {

// Constants
BOOST_INLINE_CONSTEXPR std::size_t csha2p_hash_size = 32;
BOOST_INLINE_CONSTEXPR const char* csha2p_plugin_name = "caching_sha2_password";
static_assert(csha2p_hash_size <= max_hash_size, "");
static_assert(csha2p_hash_size == SHA256_DIGEST_LENGTH, "Buffer size mismatch");

inline void csha2p_hash_password_impl(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble,
    span<std::uint8_t, csha2p_hash_size> output
)
{
    // SHA(SHA(password_sha) concat scramble) XOR password_sha
    // hash1 = SHA(pass)
    std::array<std::uint8_t, csha2p_hash_size> password_sha;
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), password_sha.data());

    // SHA(password_sha) concat scramble = buffer
    std::array<std::uint8_t, csha2p_hash_size + scramble_size> buffer;
    SHA256(password_sha.data(), password_sha.size(), buffer.data());
    std::memcpy(buffer.data() + csha2p_hash_size, scramble.data(), scramble.size());

    // SHA(SHA(password_sha) concat scramble) = SHA(buffer) = salted_password
    std::array<std::uint8_t, csha2p_hash_size> salted_password;
    SHA256(buffer.data(), buffer.size(), salted_password.data());

    // salted_password XOR password_sha
    for (unsigned i = 0; i < csha2p_hash_size; ++i)
    {
        output[i] = salted_password[i] ^ password_sha[i];
    }
}

inline static_buffer<max_hash_size> csha2p_hash_password(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble
)
{
    // Empty passwords are not hashed
    if (password.empty())
        return {};

    // Run the algorithm
    static_buffer<max_hash_size> res(csha2p_hash_size);
    csha2p_hash_password_impl(
        password,
        scramble,
        span<std::uint8_t, csha2p_hash_size>(res.data(), csha2p_hash_size)
    );
    return res;
}

class csha2p_algo
{
    int resume_point_{0};

    static bool is_perform_full_auth(span<const std::uint8_t> server_data)
    {
        return server_data.size() == 1u && server_data[0] == 4;
    }

    static bool is_fast_auth_ok(span<const std::uint8_t> server_data)
    {
        return server_data.size() == 1u && server_data[0] == 3;
    }

    static next_action encrypt_password(
        connection_state_data& st,
        std::uint8_t& seqnum,
        string_view password,
        span<const std::uint8_t, scramble_size> scramble,
        span<const std::uint8_t> server_key
    )
    {
        container::small_vector<std::uint8_t, 512> buff;
        auto ec = csha2p_encrypt_password(password, scramble, server_key, buff, asio::error::ssl_category);
        if (ec)
            return ec;
        return st.write(
            string_eof{string_view(reinterpret_cast<const char*>(buff.data()), buff.size())},
            seqnum
        );
    }

public:
    csha2p_algo() = default;

    next_action resume(
        connection_state_data& st,
        span<const std::uint8_t> server_data,
        string_view password,
        span<const std::uint8_t, scramble_size> scramble,
        bool secure_channel,
        std::uint8_t& seqnum
    )
    {
        switch (resume_point_)
        {
        case 0:
            // If we got a more data packet, the server either required us to perform full auth,
            // or told us to read again because an OK packet or error packet is coming.
            if (is_perform_full_auth(server_data))
            {
                if (secure_channel)
                {
                    // We should send a packet with just the password, as a NULL-terminated string
                    BOOST_MYSQL_YIELD(resume_point_, 1, st.write(string_null{password}, seqnum))

                    // The server shouldn't send us any more packets
                    return error_code(client_errc::bad_handshake_packet_type);
                }
                else
                {
                    // Request the server's public key
                    BOOST_MYSQL_YIELD(resume_point_, 2, st.write(int1{2}, seqnum))

                    // Encrypt the password with the key we were given
                    BOOST_MYSQL_YIELD(
                        resume_point_,
                        3,
                        encrypt_password(st, seqnum, password, scramble, server_data)
                    )

                    // The server shouldn't send us any more packets
                    return error_code(client_errc::bad_handshake_packet_type);
                }
            }
            else if (is_fast_auth_ok(server_data))
            {
                // We should wait for the server to send an OK or an error
                BOOST_MYSQL_YIELD(resume_point_, 4, st.read(seqnum))
            }
            else
            {
                // The server sent a data packet and we don't know what it means.
                // Treat it as a protocol violation error and exit
                return error_code(client_errc::bad_handshake_packet_type);
            }
        }

        // If we got here, the server sent us more data, which is a protocol violation
        return error_code(client_errc::bad_handshake_packet_type);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
