//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_REPLIES_HPP
#define BOOST_MQTT5_REPLIES_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

class replies {
public:
    using executor_type = asio::any_io_executor;
private:
    using Signature = void (error_code, byte_citer, byte_citer);

    static constexpr auto max_reply_time = std::chrono::seconds(20);

    class reply_handler {
        asio::any_completion_handler<Signature> _handler;
        control_code_e _code;
        uint16_t _packet_id;
        std::chrono::time_point<std::chrono::system_clock> _ts;
    public:
        template <typename H>
        reply_handler(control_code_e code, uint16_t pid, H&& handler) :
            _handler(std::forward<H>(handler)), _code(code), _packet_id(pid),
            _ts(std::chrono::system_clock::now())
        {}

        reply_handler(reply_handler&&) = default;
        reply_handler(const reply_handler&) = delete;

        reply_handler& operator=(reply_handler&&) = default;
        reply_handler& operator=(const reply_handler&) = delete;

        void complete(
            error_code ec,
            byte_citer first = byte_citer {}, byte_citer last = byte_citer {}
        ) {
            std::move(_handler)(ec, first, last);
        }

        void complete_post(const executor_type& ex, error_code ec) {
            asio::post(
                ex,
                asio::prepend(
                    std::move(_handler), ec, byte_citer {}, byte_citer {}
                )
            );
        }

        uint16_t packet_id() const noexcept {
            return _packet_id;
        }

        control_code_e code() const noexcept {
            return _code;
        }

        auto time() const noexcept {
            return _ts;
        }
    };

    executor_type _ex;

    using handlers = std::vector<reply_handler>;
    handlers _handlers;

    struct fast_reply {
        control_code_e code;
        uint16_t packet_id;
        std::unique_ptr<std::string> packet;
    };
    using fast_replies = std::vector<fast_reply>;
    fast_replies _fast_replies;

public:
    template <typename Executor>
    explicit replies(Executor ex) : _ex(std::move(ex)) {}

    replies(replies&&) = default;
    replies(const replies&) = delete;

    replies& operator=(replies&&) = default;
    replies& operator=(const replies&) = delete;

    template <typename CompletionToken>
    decltype(auto) async_wait_reply(
        control_code_e code, uint16_t packet_id, CompletionToken&& token
    ) {
        auto dup_handler_ptr = find_handler(code, packet_id);
        if (dup_handler_ptr != _handlers.end()) {
            dup_handler_ptr->complete_post(_ex, asio::error::operation_aborted);
            _handlers.erase(dup_handler_ptr);
        }

        auto freply = find_fast_reply(code, packet_id);

        if (freply == _fast_replies.end()) {
            auto initiation = [](
                auto handler, replies& self,
                control_code_e code, uint16_t packet_id
            ) {
                self._handlers.emplace_back(
                    code, packet_id, std::move(handler)
                );
            };
            return asio::async_initiate<CompletionToken, Signature>(
                initiation, token, std::ref(*this), code, packet_id
            );
        }

        auto fdata = std::move(*freply);
        _fast_replies.erase(freply);

        auto initiation = [](
            auto handler, std::unique_ptr<std::string> packet,
            const executor_type& ex
        ) {
            byte_citer first = packet->cbegin();
            byte_citer last = packet->cend();

            asio::post(
                ex,
                asio::consign(
                    asio::prepend(
                        std::move(handler), error_code {}, first, last
                    ),
                    std::move(packet)
                )
            );
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::move(fdata.packet), _ex
        );
    }

    void dispatch(
        error_code ec, control_code_e code, uint16_t packet_id,
        byte_citer first, byte_citer last
    ) {
        auto handler_ptr = find_handler(code, packet_id);

        if (handler_ptr == _handlers.end()) {
            _fast_replies.push_back({
                code, packet_id,
                std::make_unique<std::string>(first, last)
            });
            return;
        }

        auto handler = std::move(*handler_ptr);
        _handlers.erase(handler_ptr);
        handler.complete(ec, first, last);
    }

    void resend_unanswered() {
        auto ua = std::move(_handlers);
        for (auto& h : ua)
            h.complete(asio::error::try_again);
    }

    void cancel_unanswered() {
        auto ua = std::move(_handlers);
        for (auto& h : ua)
            h.complete_post(_ex, asio::error::operation_aborted);
    }

    bool any_expired() {
        auto now = std::chrono::system_clock::now();
        return std::any_of(
            _handlers.begin(), _handlers.end(),
            [now](const auto& h) {
                return now - h.time() > max_reply_time;
            }
        );
    }

    void clear_fast_replies() {
        _fast_replies.clear();
    }

    void clear_pending_pubrels() {
        for (auto it = _handlers.begin(); it != _handlers.end();) {
            if (it->code() == control_code_e::pubrel) {
                it->complete(asio::error::operation_aborted);
                it = _handlers.erase(it);
            }
            else
                ++it;
        }
    }

private:
    handlers::iterator find_handler(control_code_e code, uint16_t packet_id) {
        return std::find_if(
            _handlers.begin(), _handlers.end(),
            [code, packet_id](const auto& h) {
                return h.code() == code && h.packet_id() == packet_id;
            }
        );
    }

    fast_replies::iterator find_fast_reply(
        control_code_e code, uint16_t packet_id
    ) {
        return std::find_if(
            _fast_replies.begin(), _fast_replies.end(),
            [code, packet_id](const auto& f) {
                return f.code == code && f.packet_id == packet_id;
            }
        );
    }

};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_REPLIES_HPP
