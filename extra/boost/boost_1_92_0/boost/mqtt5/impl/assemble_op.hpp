//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_ASSEMBLE_OP_HPP
#define BOOST_MQTT5_ASSEMBLE_OP_HPP

#include <boost/mqtt5/error.hpp>

#include <boost/mqtt5/detail/control_packet.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>

#include <boost/mqtt5/impl/codecs/message_decoders.hpp>

#include <boost/asio/append.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

class data_span : private std::pair<byte_citer, byte_citer> {
    using base = std::pair<byte_citer, byte_citer>;
public:
    using base::base;

    auto first() const {
        return base::first;
    }
    auto last() const {
        return base::second;
    }
    void expand_suffix(size_t num_chars) {
        base::second += num_chars;
    }
    void remove_prefix(size_t num_chars) {
        base::first += num_chars;
    }
    size_t size() const {
        return std::distance(base::first, base::second);
    }
};


template <typename ClientService, typename Handler>
class assemble_op {
    using client_service = ClientService;
    using handler_type = Handler;

    struct on_read {};

    client_service& _svc;
    handler_type _handler;

    std::string& _read_buff;
    data_span& _data_span;

public:
    assemble_op(
        client_service& svc, handler_type&& handler,
        std::string& read_buff, data_span& active_span
    ) :
        _svc(svc),
        _handler(std::move(handler)),
        _read_buff(read_buff), _data_span(active_span)
    {}

    assemble_op(assemble_op&&) noexcept = default;
    assemble_op(const assemble_op&) = delete;

    assemble_op& operator=(assemble_op&&) noexcept = default;
    assemble_op& operator=(const assemble_op&) = delete;

    using allocator_type = asio::associated_allocator_t<handler_type>;
    allocator_type get_allocator() const noexcept {
        return asio::get_associated_allocator(_handler);
    }

    using executor_type = asio::associated_executor_t<handler_type>;
    executor_type get_executor() const noexcept {
        return asio::get_associated_executor(_handler);
    }

    template <typename CompletionCondition>
    void perform(CompletionCondition cc) {
        if (cc(error_code {}, 0) == 0 && _data_span.size()) {
            return asio::post(
                _svc.get_executor(),
                asio::prepend(
                    std::move(*this), on_read {}, error_code {},
                    0, std::move(cc)
                )
            );
        }

        prepare_buffer(1);

        // Must be evaluated before this is moved
        auto store_begin = _read_buff.data()
            + std::distance(_read_buff.cbegin(), _data_span.last());
        auto store_size = std::distance(_data_span.last(), _read_buff.cend());

        _svc._stream.async_read_some(
            asio::buffer(store_begin, store_size), compute_read_timeout(),
            asio::prepend(
                asio::append(std::move(*this), std::move(cc)),
                on_read {}
            )
        );
    }

    template <typename CompletionCondition>
    void operator()(
        on_read, error_code ec, size_t bytes_read,
        CompletionCondition cc
    ) {
        if (ec == asio::error::try_again) {
            _svc.update_session_state();
            _svc._async_sender.resend();
            _data_span = { _read_buff.cbegin(), _read_buff.cbegin() };
            return perform(std::move(cc));
        }

        if (ec)
            return complete(ec, 0, {}, {});

        _data_span.expand_suffix(bytes_read);
        BOOST_ASSERT(_data_span.size());

        auto control_byte = uint8_t(*_data_span.first());

        if ((control_byte & 0b11110000) == 0)
            // close the connection, cancel
            return complete(client::error::malformed_packet, 0, {}, {});

        auto first = _data_span.first() + 1;
        auto varlen = decoders::type_parse(
            first, _data_span.last(), decoders::basic::varint_
        );

        if (!varlen) {
            if (_data_span.size() < 5)
                return perform(asio::transfer_at_least(1));
            return complete(client::error::malformed_packet, 0, {}, {});
        }

        if (
            static_cast<uint32_t>(*varlen)
                > max_recv_size() - std::distance(_data_span.first(), first)
        )
            return complete(client::error::packet_too_large, 0, {}, {});

        if (std::distance(first, _data_span.last()) < *varlen) {
            prepare_buffer(*varlen - std::distance(first, _data_span.last()));
            return perform(asio::transfer_at_least(1));
        }

        _data_span.remove_prefix(
            std::distance(_data_span.first(), first) + *varlen
        );

        dispatch(control_byte, first, first + *varlen);
    }

private:
    void prepare_buffer(std::ptrdiff_t extra_len) {
        if (std::distance(_data_span.last(), _read_buff.cend()) >= extra_len)
            return;

        // make room for the packet by erasing bytes we already parsed from the
        // beginning of the read buffer

        const auto data_span_size = _data_span.size();
        _read_buff.erase(_read_buff.cbegin(), _data_span.first());
        _read_buff.resize(max_recv_size());
        _data_span = {
            _read_buff.cbegin(),
            _read_buff.cbegin() + data_span_size
        };
    }

    uint32_t max_recv_size() const {
        return (std::min)(
            _svc.connect_property(prop::maximum_packet_size)
                .value_or(default_max_recv_size),
            static_cast<uint32_t>(default_max_send_size)
        );
    }

    duration compute_read_timeout() const {
        auto negotiated_ka = _svc.negotiated_keep_alive();
        return negotiated_ka ?
            duration(std::chrono::milliseconds(3 * negotiated_ka * 1000 / 2)) :
            (duration::max)();
    }

    static bool valid_header(uint8_t control_byte) {
        auto code = control_code_e(control_byte & 0b11110000);

        if (code == control_code_e::publish)
            return true;

        auto res = control_byte & 0b00001111;
        if (code == control_code_e::pubrel)
            return res == 0b00000010;
        return res == 0b00000000;
    }

    void dispatch(
        uint8_t control_byte, byte_citer first, byte_citer last
    ) {
        using namespace decoders;

        if (!valid_header(control_byte))
            return complete(client::error::malformed_packet, 0, {}, {});

        auto code = control_code_e(control_byte & 0b11110000);

        if (code == control_code_e::pingresp)
            return perform(asio::transfer_at_least(0));

        bool is_reply = code != control_code_e::publish &&
            code != control_code_e::auth &&
            code != control_code_e::disconnect;

        if (is_reply) {
            auto packet_id = decoders::decode_packet_id(first, last);
            if (!packet_id)
                return complete(client::error::malformed_packet, 0, {}, {});
            _svc._replies.dispatch(error_code {}, code, *packet_id, first, last);
            return perform(asio::transfer_at_least(0));
        }

        complete(error_code {}, control_byte, first, last);
    }

    void complete(
        error_code ec, uint8_t control_code,
        byte_citer first, byte_citer last
    ) {
        if (ec)
            _data_span = { _read_buff.cbegin(), _read_buff.cbegin() };
        std::move(_handler)(ec, control_code, first, last);
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_ASSEMBLE_OP_HPP
