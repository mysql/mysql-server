//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_HANDSHAKE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_HANDSHAKE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/ok_view.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/protocol/static_buffer.hpp>
#include <boost/mysql/impl/internal/sansio/auth_plugin_common.hpp>
#include <boost/mysql/impl/internal/sansio/caching_sha2_password.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/mysql_native_password.hpp>

#include <boost/core/span.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <cstring>

namespace boost {
namespace mysql {
namespace detail {

// Stores which authentication plugin we're using, plus any required state. Variant-like
class any_authentication_plugin
{
    enum class type_t
    {
        mnp,
        csha2p
    };

    // Which authentication plugin are we using?
    type_t type_{type_t::mnp};

    // State for algorithms that require stateful exchanges.
    // mysql_native_password is stateless, so only caching_sha2_password has an entry here
    csha2p_algo csha2p_;

public:
    any_authentication_plugin() = default;

    // Emplaces a plugin of the type given by plugin_name. Errors on unknown plugin
    error_code emplace_by_name(string_view plugin_name)
    {
        if (plugin_name == mnp_plugin_name)
        {
            type_ = type_t::mnp;
            return error_code();
        }
        else if (plugin_name == csha2p_plugin_name)
        {
            type_ = type_t::csha2p;
            csha2p_ = csha2p_algo();  // Reset any leftover state, just in case
            return error_code();
        }
        else
        {
            return client_errc::unknown_auth_plugin;
        }
    }

    // Hashes the password with the selected plugin
    static_buffer<max_hash_size> hash_password(
        string_view password,
        span<const std::uint8_t, scramble_size> scramble
    ) const
    {
        switch (type_)
        {
        case type_t::mnp: return mnp_hash_password(password, scramble);
        case type_t::csha2p: return csha2p_hash_password(password, scramble);
        default: BOOST_ASSERT(false); return {};  // LCOV_EXCL_LINE
        }
    }

    // Invokes the plugin action. Use when a more_data packet is received.
    next_action resume(
        connection_state_data& st,
        span<const std::uint8_t> server_data,
        string_view password,
        span<const std::uint8_t, scramble_size> scramble,
        bool secure_channel,
        std::uint8_t& seqnum
    )
    {
        switch (type_)
        {
        case type_t::mnp:
            // This algorithm doesn't allow more data frames
            return error_code(client_errc::bad_handshake_packet_type);
        case type_t::csha2p:
            return csha2p_.resume(st, server_data, password, scramble, secure_channel, seqnum);
        default:
            BOOST_ASSERT(false);
            return next_action(client_errc::bad_handshake_packet_type);  // LCOV_EXCL_LINE
        }
    }

    string_view name() const
    {
        switch (type_)
        {
        case type_t::mnp: return mnp_plugin_name;
        case type_t::csha2p: return csha2p_plugin_name;
        default: BOOST_ASSERT(false); return {};  // LCOV_EXCL_LINE
        }
    }
};

class handshake_algo
{
    int resume_point_{0};
    handshake_params hparams_;
    any_authentication_plugin plugin_;
    std::array<std::uint8_t, scramble_size> scramble_;
    std::uint8_t sequence_number_{0};
    bool secure_channel_{false};
    character_set server_default_charset_{};

    static capabilities conditional_capability(bool condition, capabilities cap)
    {
        return condition ? cap : capabilities{};
    }

    // Given our params and the capabilities that the server sent us,
    // performs capability negotiation, returning either the capabilities to
    // send to the server or an error
    static system::result<capabilities> negotiate_capabilities(
        const handshake_params& params,
        capabilities server_caps,
        bool transport_supports_ssl
    )
    {
        // The capabilities that we absolutely require. These are always set except in extremely old servers
        constexpr capabilities mandatory_capabilities =
            // We don't speak the older protocol
            capabilities::protocol_41 |

            // We only know how to deserialize the hello frame if this is set
            capabilities::plugin_auth |

            // Same as above
            capabilities::plugin_auth_lenenc_data |

            // This makes processing execute responses easier
            capabilities::deprecate_eof |

            // Used in MariaDB to signal 4.1 protocol. Always set in MySQL, too
            capabilities::secure_connection;

        // The capabilities that we support but don't require
        constexpr capabilities optional_capabilities = capabilities::multi_results |
                                                       capabilities::ps_multi_results;

        auto ssl = transport_supports_ssl ? params.ssl() : ssl_mode::disable;
        capabilities required_caps = mandatory_capabilities |
                                     conditional_capability(
                                         !params.database().empty(),
                                         capabilities::connect_with_db
                                     ) |
                                     conditional_capability(
                                         params.multi_queries(),
                                         capabilities::multi_statements
                                     ) |
                                     conditional_capability(ssl == ssl_mode::require, capabilities::ssl);
        if (has_capabilities(required_caps, capabilities::ssl) &&
            !has_capabilities(server_caps, capabilities::ssl))
        {
            // This happens if the server doesn't have SSL configured. This special
            // error code helps users diagnosing their problem a lot (server_unsupported doesn't).
            return make_error_code(client_errc::server_doesnt_support_ssl);
        }
        else if (!has_capabilities(server_caps, required_caps))
        {
            return make_error_code(client_errc::server_unsupported);
        }
        return server_caps & (required_caps | optional_capabilities |
                              conditional_capability(ssl == ssl_mode::enable, capabilities::ssl));
    }

    // Attempts to map the collection_id to a character set. We try to be conservative
    // here, since servers will happily accept unknown collation IDs, silently defaulting
    // to the server's default character set (often latin1, which is not Unicode).
    static character_set collation_id_to_charset(
        character_set server_default_charset,
        std::uint16_t collation_id
    )
    {
        switch (collation_id)
        {
        case invalid_collation_id: return server_default_charset;
        case mysql_collations::utf8mb4_bin:
        case mysql_collations::utf8mb4_general_ci: return utf8mb4_charset;
        case mysql_collations::ascii_general_ci:
        case mysql_collations::ascii_bin: return ascii_charset;
        default: return character_set{};
        }
    }

    // Saves the scramble, checking that it has the right size
    error_code save_scramble(span<const std::uint8_t> value)
    {
        // All scrambles must have exactly this size. Otherwise, it's a protocol violation error
        if (value.size() != scramble_size)
            return client_errc::protocol_value_error;

        // Store the scramble
        std::memcpy(scramble_.data(), value.data(), scramble_size);

        // Done
        return error_code();
    }

    error_code process_hello(connection_state_data& st, diagnostics& diag, span<const std::uint8_t> buffer)
    {
        // Deserialize server hello
        server_hello hello{};
        auto err = deserialize_server_hello(buffer, hello, diag);
        if (err)
            return err;

        // Check capabilities
        auto negotiated_caps = negotiate_capabilities(hparams_, hello.server_capabilities, st.tls_supported);
        if (negotiated_caps.has_error())
            return negotiated_caps.error();

        // Set capabilities, db flavor and connection ID
        st.current_capabilities = *negotiated_caps;
        st.flavor = hello.server;
        st.connection_id = hello.connection_id;

        // If we're using SSL, mark the channel as secure
        secure_channel_ = secure_channel_ || has_capabilities(*negotiated_caps, capabilities::ssl);

        // Save which authentication plugin we're using. Do this before saving the scramble,
        // as an unknown plugin might have a scramble size different to what we know
        err = plugin_.emplace_by_name(hello.auth_plugin_name);
        if (err)
            return err;

        // Save the scramble for later
        return save_scramble(hello.auth_plugin_data);
    }

    // Response to that initial greeting
    ssl_request compose_ssl_request(const connection_state_data& st)
    {
        return ssl_request{
            st.current_capabilities,
            static_cast<std::uint32_t>(max_packet_size),
            hparams_.connection_collation(),
        };
    }

    next_action serialize_login_request(connection_state_data& st)
    {
        auto hashed_password = plugin_.hash_password(hparams_.password(), scramble_);
        return st.write(
            login_request{
                st.current_capabilities,
                static_cast<std::uint32_t>(max_packet_size),
                hparams_.connection_collation(),
                hparams_.username(),
                hashed_password,
                hparams_.database(),
                plugin_.name(),
            },
            sequence_number_
        );
    }

    // Processes auth_switch and auth_more_data messages, and leaves the result in auth_resp_
    next_action process_auth_switch(connection_state_data& st, auth_switch msg)
    {
        // Emplace the new authentication plugin
        auto ec = plugin_.emplace_by_name(msg.plugin_name);
        if (ec)
            return ec;

        // Store the scramble for later (required by caching_sha2_password, for instance)
        ec = save_scramble(msg.auth_data);
        if (ec)
            return ec;

        // Hash the password
        auto hashed_password = plugin_.hash_password(hparams_.password(), scramble_);

        // Serialize the response
        return st.write(auth_switch_response{hashed_password}, sequence_number_);
    }

    void on_success(connection_state_data& st, const ok_view& ok)
    {
        st.status = connection_status::ready;
        st.backslash_escapes = ok.backslash_escapes();
        st.server_default_charset = server_default_charset_;
        st.current_charset = collation_id_to_charset(
            server_default_charset_,
            hparams_.connection_collation()
        );
    }

    next_action resume_impl(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        if (ec)
            return ec;

        handshake_server_response resp(error_code{});
        next_action act;

        switch (resume_point_)
        {
        case 0:
            // Handshake wipes out state, so no state checks are performed.
            // Setup
            st.reset();

            // Read server greeting
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(sequence_number_))

            // Process server greeting
            ec = process_hello(st, diag, st.reader.message());
            if (ec)
                return ec;

            // SSL
            if (has_capabilities(st.current_capabilities, capabilities::ssl))
            {
                // Send SSL request
                BOOST_MYSQL_YIELD(resume_point_, 2, st.write(compose_ssl_request(st), sequence_number_))

                // SSL handshake
                BOOST_MYSQL_YIELD(resume_point_, 3, next_action::ssl_handshake())

                // Mark the connection as using ssl
                st.tls_active = true;
            }

            // Compose and send handshake response
            BOOST_MYSQL_YIELD(resume_point_, 4, serialize_login_request(st))

            // Receive the response
            BOOST_MYSQL_YIELD(resume_point_, 5, st.read(sequence_number_))

            // Process it
            resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);

            // Auth switches are only legal at this point. Handle the case here
            if (resp.type == handshake_server_response::type_t::auth_switch)
            {
                // Write our packet
                BOOST_MYSQL_YIELD(resume_point_, 6, process_auth_switch(st, resp.data.auth_sw))

                // Read another packet
                BOOST_MYSQL_YIELD(resume_point_, 7, st.read(sequence_number_))

                // Deserialize it
                resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);
            }

            // Now we will send/receive raw data packets from the server until an OK or error happens.
            // Packets requiring responses are auth_more_data packets
            while (resp.type == handshake_server_response::type_t::auth_more_data)
            {
                // Invoke the authentication plugin algorithm
                act = plugin_.resume(
                    st,
                    resp.data.more_data,
                    hparams_.password(),
                    scramble_,
                    secure_channel_,
                    sequence_number_
                );

                // Do what the plugin says
                if (act.type() == next_action_type::none)
                {
                    // The plugin signalled an error. Exit
                    BOOST_ASSERT(act.error());
                    return act;
                }
                else if (act.type() == next_action_type::write)
                {
                    // The plugin wants us to first write the message in the write buffer, then read
                    BOOST_MYSQL_YIELD(resume_point_, 8, act)
                    BOOST_MYSQL_YIELD(resume_point_, 9, st.read(sequence_number_))
                }
                else
                {
                    // The plugin wants us to read another packet
                    BOOST_ASSERT(act.type() == next_action_type::read);
                    BOOST_MYSQL_YIELD(resume_point_, 10, act)
                }

                // If we got here, we've successfully read a packet. Deserialize it
                resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);
            }

            // If we got here, we've received a packet that terminates the algorithm
            if (resp.type == handshake_server_response::type_t::ok)
            {
                // Auth success, quit
                on_success(st, resp.data.ok);
                return next_action();
            }
            else if (resp.type == handshake_server_response::type_t::error)
            {
                // Error, quit
                return resp.data.err;
            }
            else
            {
                // Auth switches are no longer allowed at this point
                BOOST_ASSERT(resp.type == handshake_server_response::type_t::auth_switch);
                return error_code(client_errc::bad_handshake_packet_type);
            }
        }

        // We should never get here
        BOOST_ASSERT(false);
        return next_action();  // LCOV_EXCL_LINE
    }

public:
    handshake_algo(handshake_algo_params params) noexcept
        : hparams_(params.hparams),
          secure_channel_(params.secure_channel),
          server_default_charset_(params.server_default_charset)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        // On error, reset the connection's state to well-known values
        auto act = resume_impl(st, diag, ec);
        if (act.is_done() && act.error())
            st.reset();
        return act;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
