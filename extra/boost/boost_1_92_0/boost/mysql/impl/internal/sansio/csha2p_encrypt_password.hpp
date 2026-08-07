//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CSHA2P_ENCRYPT_PASSWORD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CSHA2P_ENCRYPT_PASSWORD_HPP

// Having this in a separate file allows us to mock the OpenSSL API in the tests

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/sansio/auth_plugin_common.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_category.hpp>
#include <boost/system/system_category.hpp>

#include <cstdint>
#include <memory>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace boost {
namespace mysql {
namespace detail {

// The OpenSSL category is passed as parameter to avoid including asio/ssl headers here.
// Doing so would make mocking OpenSSL more difficult (more functions used)
inline error_code translate_openssl_error(
    unsigned long code,
    const source_location* loc,
    const system::error_category& openssl_category
)
{
    // If ERR_SYSTEM_ERROR is true, the error code is a system error.
    // This function only exists since OpenSSL 3
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    if (ERR_SYSTEM_ERROR(code))
    {
        return error_code(ERR_GET_REASON(code), system::system_category(), loc);
    }
#endif

    // In OpenSSL < 3, error codes > 0x80000000 are reserved for the user,
    // so it's unlikely that we will encounter these here. Overflow here
    // is implementation-defined behavior (and not UB), so we're fine.
    // This is what Asio does, anyway.
    int int_code = static_cast<int>(code);

    // An error code of zero would mean success, while this function is always
    // called because an OpenSSL primitive failed. It might indicate that OpenSSL
    // did not provide any extra error information. But it should still be an error
    if (int_code == 0)
        return error_code(client_errc::unknown_openssl_error, loc);
    else
        return error_code(int_code, openssl_category, loc);
}

inline container::small_vector<std::uint8_t, 512> csha2p_salt_password(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble
)
{
    // Salt the password, as a NULL-terminated string
    container::small_vector<std::uint8_t, 512> res(password.size() + 1u, 0);
    for (std::size_t i = 0; i < password.size(); ++i)
        res[i] = password[i] ^ scramble[i % scramble.size()];

    // Add the NULL terminator. It should be salted, too. Since 0 ^ U = U,
    // the byte should be the scramble at the position we're in
    res[password.size()] = scramble[password.size() % scramble.size()];

    return res;
}

inline error_code csha2p_encrypt_password(
    string_view password,
    span<const std::uint8_t, scramble_size> scramble,
    span<const std::uint8_t> server_key,
    container::small_vector<std::uint8_t, 512>& output,
    const system::error_category& openssl_category
)
{
    // RAII helpers
    struct bio_deleter
    {
        void operator()(BIO* bio) const noexcept { BIO_free(bio); }
    };
    using unique_bio = std::unique_ptr<BIO, bio_deleter>;

    struct evp_pkey_deleter
    {
        void operator()(EVP_PKEY* pkey) const noexcept { EVP_PKEY_free(pkey); }
    };
    using unique_evp_pkey = std::unique_ptr<EVP_PKEY, evp_pkey_deleter>;

    struct evp_pkey_ctx_deleter
    {
        void operator()(EVP_PKEY_CTX* ctx) const noexcept { EVP_PKEY_CTX_free(ctx); }
    };
    using unique_evp_pkey_ctx = std::unique_ptr<EVP_PKEY_CTX, evp_pkey_ctx_deleter>;

    // Apply a sanity check to the key buffer size
    constexpr std::size_t max_key_buffer_size = 1024u * 1024u;  // 1MB
    if (server_key.size() > max_key_buffer_size)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return error_code(client_errc::protocol_value_error, &loc);
    }

    // Try to parse the private key
    unique_bio bio{BIO_new_mem_buf(server_key.data(), static_cast<int>(server_key.size()))};
    if (!bio)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }
    unique_evp_pkey key(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    if (!key)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }

    // Salt the password
    auto salted_password = csha2p_salt_password(password, scramble);

    // Set up the encryption context
    unique_evp_pkey_ctx ctx(EVP_PKEY_CTX_new(key.get(), nullptr));
    if (!ctx)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }
    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }
    int rsa_pad_res = EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING);
    if (rsa_pad_res <= 0)
    {
        // If the server passed us a key type that does not support encryption,
        // OpenSSL returns -2 and does not add an error to the stack (ERR_get_error returns 0).
        // This shouldn't happen with real servers, so we re-use an existing error code and set
        // the source location to allow diagnosis
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        if (rsa_pad_res == -2)
            return error_code(client_errc::protocol_value_error, &loc);
        else
            return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }

    // Allocate a buffer for encryption
    int max_size = EVP_PKEY_size(key.get());
    if (max_size <= 0)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }
    output.resize(max_size);

    // Encrypt
    std::size_t actual_size = static_cast<std::size_t>(max_size);
    if (EVP_PKEY_encrypt(
            ctx.get(),
            output.data(),
            &actual_size,
            salted_password.data(),
            salted_password.size()
        ) <= 0)
    {
        static constexpr auto loc = BOOST_CURRENT_LOCATION;
        return translate_openssl_error(ERR_get_error(), &loc, openssl_category);
    }

    // Adjust size
    BOOST_ASSERT(actual_size <= output.size());
    output.resize(actual_size);

    // Done
    return error_code();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
