//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_ANY_AUTHENTICATOR
#define BOOST_MQTT5_ANY_AUTHENTICATOR

#include <boost/mqtt5/types.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_traits/is_detected.hpp>
#include <boost/type_traits/is_detected_convertible.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;
using error_code = boost::system::error_code;

using auth_handler_type = asio::any_completion_handler<
    void (error_code, std::string)
>;

template <typename T, typename ...Ts>
using async_auth_sig = decltype(
    std::declval<T>().async_auth(std::declval<Ts>()...)
);

template <typename T>
using method_sig = decltype(
    std::declval<T>().method()
);

template <typename T>
constexpr bool is_authenticator =
    boost::is_detected<
        async_auth_sig, T,
        auth_step_e, std::string, auth_handler_type
    >::value &&
    boost::is_detected_convertible_v<std::string_view, method_sig, T>;

class auth_fun_base {
    using auth_func = void(*)(
        auth_step_e, std::string, auth_handler_type, auth_fun_base*
    );
    auth_func _auth_func;

public:
    auth_fun_base(auth_func f) : _auth_func(f) {}
    ~auth_fun_base() = default;

    void async_auth(
        auth_step_e step, std::string data,
        auth_handler_type auth_handler
    ) {
        _auth_func(step, std::move(data), std::move(auth_handler), this);
    }
};

template <
    typename Authenticator,
    typename = std::enable_if_t<is_authenticator<Authenticator>>
>
class auth_fun : public auth_fun_base {
    Authenticator _authenticator;

public:
    auth_fun(Authenticator authenticator) :
        auth_fun_base(&async_auth),
        _authenticator(std::forward<Authenticator>(authenticator))
    {}

    static void async_auth(
        auth_step_e step, std::string data, auth_handler_type auth_handler,
        auth_fun_base* base_ptr
    ) {
        auto auth_fun_ptr = static_cast<auth_fun*>(base_ptr);
        auth_fun_ptr->_authenticator.async_auth(
            step, std::move(data), std::move(auth_handler)
        );
    }
};

class any_authenticator {
    std::string _method;
    std::shared_ptr<detail::auth_fun_base> _auth_fun;

public:
    any_authenticator() = default;

    template <
        typename Authenticator,
        std::enable_if_t<detail::is_authenticator<Authenticator>, bool> = true
    >
    any_authenticator(Authenticator&& a) :
        _method(a.method()),
        _auth_fun(
            new detail::auth_fun<Authenticator>(
                std::forward<Authenticator>(a)
            )
        )
    {}

    std::string_view method() const {
        return _method;
    }

    template <typename CompletionToken>
    decltype(auto) async_auth(
        auth_step_e step, std::string data,
        CompletionToken&& token
    ) {
        using Signature = void (error_code, std::string);

        auto initiation = [](
            auto handler, any_authenticator& self,
            auth_step_e step, std::string data
        ) {
            self._auth_fun->async_auth(
                step, std::move(data), std::move(handler)
            );
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this), step, std::move(data)
        );
    }
};

} // end namespace boost::mqtt5::detail


#endif // !BOOST_MQTT5_ANY_AUTHENTICATOR
