//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_ASYNC_SENDER_HPP
#define BOOST_MQTT5_ASYNC_SENDER_HPP

#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

class write_req {
    static constexpr unsigned SERIAL_BITS = sizeof(serial_num_t) * 8;

    asio::const_buffer _buffer;
    serial_num_t _serial_num;
    unsigned _flags;

    using handler_type = asio::any_completion_handler<void (error_code)>;
    handler_type _handler;

public:
    write_req(
        asio::const_buffer buffer,
        serial_num_t serial_num, unsigned flags,
        handler_type handler
    ) :
        _buffer(buffer), _serial_num(serial_num), _flags(flags),
        _handler(std::move(handler))
    {}

    write_req(write_req&&) = default;
    write_req(const write_req&) = delete;

    write_req& operator=(write_req&&) = default;
    write_req& operator=(const write_req&) = delete;

    static serial_num_t next_serial_num(serial_num_t last) {
        return last + 1;
    }

    asio::const_buffer buffer() const {
        return _buffer;
    }

    void complete(error_code ec) {
        std::move(_handler)(ec);
    }

    void complete_post(const asio::any_io_executor& ex, error_code ec) {
        asio::post(
            ex,
            asio::prepend(std::move(_handler), ec)
        );
    }

    bool empty() const {
        return !_handler;
    }

    bool throttled() const {
        return _flags & send_flag::throttled;
    }

    bool terminal() const {
        return _flags & send_flag::terminal;
    }

    bool operator<(const write_req& other) const {
        if (prioritized() != other.prioritized())
            return prioritized();

        auto s1 = _serial_num;
        auto s2 = other._serial_num;

        if (s1 < s2)
            return (s2 - s1) < (1u << (SERIAL_BITS - 1));

        return (s1 - s2) >= (1u << (SERIAL_BITS - 1));
    }

private:
    bool prioritized() const {
        return _flags & send_flag::prioritized;
    }
};


template <typename ClientService>
class async_sender {
    using self_type = async_sender<ClientService>;

    using client_service = ClientService;

    using queue_allocator_type = asio::recycling_allocator<write_req>;
    using write_queue_t = std::vector<write_req, queue_allocator_type>;

    ClientService& _svc;
    write_queue_t _write_queue;
    bool _write_in_progress { false };

    static constexpr uint16_t MAX_LIMIT = 65535;
    uint16_t _limit { MAX_LIMIT };
    uint16_t _quota { MAX_LIMIT };

    serial_num_t _last_serial_num { 0 };

public:
    explicit async_sender(ClientService& svc) : _svc(svc) {}

    async_sender(async_sender&&) = default;
    async_sender(const async_sender&) = delete;

    async_sender& operator=(async_sender&&) = default;
    async_sender& operator=(const async_sender&) = delete;

    using allocator_type = queue_allocator_type;
    allocator_type get_allocator() const noexcept {
        return allocator_type {};
    }

    using executor_type = typename client_service::executor_type;
    executor_type get_executor() const noexcept {
        return _svc.get_executor();
    }

    serial_num_t next_serial_num() {
        return _last_serial_num = write_req::next_serial_num(_last_serial_num);
    }

    template <typename CompletionToken, typename BufferType>
    decltype(auto) async_send(
        const BufferType& buffer,
        serial_num_t serial_num, unsigned flags,
        CompletionToken&& token
    ) {
        using Signature = void (error_code);

        auto initiation = [](
            auto handler, self_type& self, const BufferType& buffer,
            serial_num_t serial_num, unsigned flags
        ) {
            self._write_queue.emplace_back(
                asio::buffer(buffer), serial_num, flags, std::move(handler)
            );
            self.do_write();
        };

        return asio::async_initiate<CompletionToken, Signature>(
            initiation, token, std::ref(*this),
            buffer, serial_num, flags
        );
    }

    void cancel() {
        auto ops = std::move(_write_queue);
        for (auto& op : ops)
            op.complete_post(_svc.get_executor(), asio::error::operation_aborted);
    }

    void resend() {
        if (_write_in_progress)
            return;

        // The _write_in_progress flag is set to true to prevent any write
        // operations executing before the _write_queue is filled with
        // all the packets that require resending.
        _write_in_progress = true;

        auto new_limit = _svc._stream_context.connack_property(prop::receive_maximum);
        _limit = new_limit.value_or(MAX_LIMIT);
        _quota = _limit;

        auto write_queue = std::move(_write_queue);
        _svc._replies.resend_unanswered();

        for (auto& op : write_queue)
            op.complete(asio::error::try_again);

        std::stable_sort(_write_queue.begin(), _write_queue.end());

        _write_in_progress = false;
        do_write();
    }

    void operator()(write_queue_t write_queue, error_code ec, size_t) {
        _write_in_progress = false;

        if (ec == asio::error::try_again) {
            _svc.update_session_state();
            _write_queue.insert(
                _write_queue.begin(),
                std::make_move_iterator(write_queue.begin()),
                std::make_move_iterator(write_queue.end())
            );
            return resend();
        }

        if (ec == asio::error::no_recovery)
            _svc.cancel();

        // errors, if any, are propagated to ops
        for (auto& op : write_queue)
            op.complete(ec);

        if (
            ec == asio::error::operation_aborted ||
            ec == asio::error::no_recovery
        )
            return;

        do_write();
    }

    void throttled_op_done() {
        if (_limit == MAX_LIMIT)
            return;

        ++_quota;
        do_write();
    }

private:
    void do_write() {
        if (_write_in_progress || _write_queue.empty())
            return;

        _write_in_progress = true;

        write_queue_t write_queue;

        auto terminal_req = std::find_if(
            _write_queue.begin(), _write_queue.end(),
            [](const auto& op) { return op.terminal(); }
        );

        if (terminal_req != _write_queue.end()) {
            write_queue.push_back(std::move(*terminal_req));
            _write_queue.erase(terminal_req);
        }
        else if (_limit == MAX_LIMIT) {
            write_queue = std::move(_write_queue);
        }
        else {
            for (write_req& req : _write_queue)
                if (!req.throttled())
                    write_queue.push_back(std::move(req));
                else if (_quota > 0) {
                    --_quota;
                    write_queue.push_back(std::move(req));
                }

            if (write_queue.empty()) {
                _write_in_progress = false;
                return;
            }

            auto it = std::remove_if(
                _write_queue.begin(), _write_queue.end(),
                [](const write_req& req) { return req.empty(); }
            );
            _write_queue.erase(it, _write_queue.end());
        }

        std::vector<asio::const_buffer> buffers;
        buffers.reserve(write_queue.size());
        for (const auto& op : write_queue)
            buffers.push_back(op.buffer());

        _svc._replies.clear_fast_replies();

        _svc._stream.async_write(
            buffers,
            asio::prepend(std::ref(*this), std::move(write_queue))
        );
    }

};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_ASYNC_SENDER_HPP
