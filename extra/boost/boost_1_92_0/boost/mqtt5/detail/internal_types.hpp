//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_INTERNAL_TYPES_HPP
#define BOOST_MQTT5_INTERNAL_TYPES_HPP

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/any_authenticator.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace boost::mqtt5::detail {

using byte_citer = std::string::const_iterator;

struct credentials {
    std::string client_id;
    std::optional<std::string> username;
    std::optional<std::string> password;

    credentials() = default;
    credentials(
        std::string client_id,
        std::string username, std::string password
    ) :
        client_id(std::move(client_id))
    {
        if (!username.empty())
            this->username = std::move(username);
        if (!password.empty())
            this->password = std::move(password);
    }
};

class session_state {
    uint8_t _flags = 0b00;

    static constexpr uint8_t session_present_flag = 0b01;
    static constexpr uint8_t subscriptions_present_flag = 0b10;
public:
    void session_present(bool present) {
        return update_flag(present, session_present_flag);
    }

    bool session_present() const {
        return _flags & session_present_flag;
    }

    void subscriptions_present(bool present) {
        return update_flag(present, subscriptions_present_flag);
    }

    bool subscriptions_present() const {
        return _flags & subscriptions_present_flag;
    }

private:
    void update_flag(bool set, uint8_t flag) {
        if (set)
            _flags |= flag;
        else
            _flags &= ~flag;
    }
};

struct mqtt_ctx {
    credentials creds;
    std::optional<will> will_msg;
    uint16_t keep_alive = 60;
    connect_props co_props;
    connack_props ca_props;
    session_state state;
    any_authenticator authenticator;

    mqtt_ctx() = default;

    mqtt_ctx(const mqtt_ctx& other) :
        creds(other.creds), will_msg(other.will_msg),
        keep_alive(other.keep_alive), co_props(other.co_props),
        ca_props {}, state {},
        authenticator(other.authenticator)
    {}
};

struct disconnect_ctx {
    disconnect_rc_e reason_code = disconnect_rc_e::normal_disconnection;
    disconnect_props props = {};
    bool terminal = false;
};

using serial_num_t = uint32_t;
constexpr serial_num_t no_serial = 0;

namespace send_flag {

constexpr unsigned none = 0b000;
constexpr unsigned throttled = 0b001;
constexpr unsigned prioritized = 0b010;
constexpr unsigned terminal = 0b100;

};

#ifdef BOOST_MQTT5_UNIT_TESTS
using timer_type = asio::basic_waitable_timer<BOOST_MQTT5_DETAIL_CLOCK_TYPE>;
using resolver_type = BOOST_MQTT5_DETAIL_RESOLVER_TYPE;
#else
using timer_type = asio::steady_timer;
using resolver_type = asio::ip::tcp::resolver;
#endif

using duration = timer_type::duration;

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_INTERNAL_TYPES_HPP
