//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_CONTROL_PACKET_HPP
#define BOOST_MQTT5_CONTROL_PACKET_HPP

#include <boost/mqtt5/types.hpp>

#include <boost/assert.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace boost::mqtt5::detail {

/* max varint number (268'435'455) + fixed header size (1 + 4) */
static constexpr int32_t default_max_send_size = 268'435'460;

static constexpr int32_t default_max_recv_size = 65'536;

enum class control_code_e : std::uint8_t {
    no_packet = 0b00000000, // 0

    connect = 0b00010000, // 1
    connack = 0b00100000, // 2
    publish = 0b00110000, // 3
    puback = 0b01000000, // 4
    pubrec = 0b01010000, // 5
    pubrel = 0b01100000, // 6
    pubcomp = 0b01110000, // 7
    subscribe = 0b10000000, // 8
    suback = 0b10010000, // 9
    unsubscribe = 0b10100000, // 10
    unsuback = 0b10110000, // 11
    pingreq = 0b11000000, // 12
    pingresp = 0b11010000, // 13
    disconnect = 0b11100000, // 14
    auth = 0b11110000, // 15
};

constexpr struct with_pid_ {} with_pid {};
constexpr struct no_pid_ {} no_pid {};

template <typename Allocator>
class control_packet {
    uint16_t _packet_id;

    using alloc_type = Allocator;
    using deleter = boost::alloc_deleter<std::string, alloc_type>;
    std::unique_ptr<std::string, deleter> _packet;

    control_packet(
        const Allocator& a,
        uint16_t packet_id, std::string packet
    ) :
        _packet_id(packet_id),
        _packet(boost::allocate_unique<std::string>(a, std::move(packet)))
    {}

public:
    control_packet(control_packet&&) noexcept = default;
    control_packet(const control_packet&) = delete;

    control_packet& operator=(control_packet&&) noexcept = default;
    control_packet& operator=(const control_packet&) = delete;

    template <
        typename EncodeFun,
        typename ...Args
    >
    static control_packet of(
        with_pid_, const Allocator& alloc,
        EncodeFun&& encode, uint16_t packet_id, Args&&... args
    ) {
        return control_packet {
            alloc, packet_id, encode(packet_id, std::forward<Args>(args)...)
        };
    }

    template <
        typename EncodeFun,
        typename ...Args
    >
    static control_packet of(
        no_pid_, const Allocator& alloc,
        EncodeFun&& encode, Args&&... args
    ) {
        return control_packet {
            alloc, uint16_t(0), encode(std::forward<Args>(args)...)
        };
    }

    size_t size() const {
        return _packet->size();
    }

    control_code_e control_code() const {
        return control_code_e(uint8_t(*(_packet->data())) & 0b11110000);
    }

    uint16_t packet_id() const {
        return _packet_id;
    }

    qos_e qos() const {
        BOOST_ASSERT(control_code() == control_code_e::publish);
        auto byte = (uint8_t(*(_packet->data())) & 0b00000110) >> 1;
        return qos_e(byte);
    }

    control_packet& set_dup() {
        BOOST_ASSERT(control_code() == control_code_e::publish);
        auto& byte = *(_packet->data());
        byte |= 0b00001000;
        return *this;
    }

    std::string_view wire_data() const {
        return *_packet;
    }
};

class packet_id_allocator {
    struct interval {
        uint16_t start, end;

        interval(uint16_t start, uint16_t end) :
            start(start), end(end)
        {}
    };

    std::vector<interval> _free_ids;
    static constexpr uint16_t MAX_PACKET_ID = 65535;

public:
    packet_id_allocator() {
        _free_ids.emplace_back(MAX_PACKET_ID, uint16_t(0));
    }

    packet_id_allocator(packet_id_allocator&&) noexcept = default;
    packet_id_allocator(const packet_id_allocator&) = delete;

    packet_id_allocator& operator=(packet_id_allocator&&) noexcept = default;
    packet_id_allocator& operator=(const packet_id_allocator&) = delete;

    uint16_t allocate() {
        if (_free_ids.empty()) return 0;
        auto& last = _free_ids.back();
        if (last.start == ++last.end) {
            auto ret = last.end;
            _free_ids.pop_back();
            return ret;
        }
        return last.end;
    }

    void free(uint16_t pid) {
        auto it = std::upper_bound(
            _free_ids.begin(), _free_ids.end(), pid,
            [](const uint16_t x, const interval& i) { return x > i.start; }
        );
        uint16_t* end_p = nullptr;
        if (it != _free_ids.begin()) {
            auto pit = std::prev(it);
            if (pit->end == pid)
                end_p = &pit->end;
        }
        if (it != _free_ids.end() && pid - 1 == it->start) {
            if (!end_p)
                it->start = pid;
            else {
                *end_p = it->end;
                _free_ids.erase(it);
            }
        }
        else {
            if (!end_p)
                _free_ids.insert(it, interval(pid, pid - 1));
            else
                *end_p = pid - 1;
        }
    }
};

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_CONTROL_PACKET_HPP
