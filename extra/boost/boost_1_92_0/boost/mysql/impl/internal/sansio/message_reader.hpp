//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_READER_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_MESSAGE_READER_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/sansio/read_buffer.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// Flow:
//   Prepare a read operation with prepare_read()
//   In a loop, until done():
//      prepare_buffer() to resize the buffer to an appropriate size
//      Read bytes against buffer()
//      Call resume with the number of bytes read
// Or call prepare_read() and check done() to attempt to get a cached message
//    (further prepare_read calls should use keep_state=true)
class message_reader
{
public:
    message_reader(
        std::size_t initial_buffer_size,
        std::size_t max_buffer_size = static_cast<std::size_t>(-1),
        std::size_t max_frame_size = max_packet_size
    )
        : buffer_(initial_buffer_size, max_buffer_size), max_frame_size_(max_frame_size)
    {
    }

    void reset()
    {
        buffer_.reset();
        state_ = parse_state();
    }

    std::size_t max_buffer_size() const { return buffer_.max_size(); }

    // Prepares a read operation. sequence_number should be kept alive until
    // the next read is prepared or no more calls to resume() are expected.
    // If keep_state=true, and the op is not complete, parsing state is preserved
    void prepare_read(std::uint8_t& sequence_number, bool keep_state = false)
    {
        if (!keep_state || done())
            state_ = parse_state(sequence_number);
        else
            state_.sequence_number = &sequence_number;
        resume(0);
    }

    // Is parsing the current message done?
    bool done() const { return state_.resume_point == -1; }

    // Returns any errors generated during parsing. Requires this->done()
    error_code error() const
    {
        BOOST_ASSERT(done());
        return state_.ec;
    }

    // Returns the last parsed message. Valid until prepare_buffer()
    // is next called. Requires done() && !error()
    span<const std::uint8_t> message() const
    {
        BOOST_ASSERT(done());
        BOOST_ASSERT(!error());
        return buffer_.current_message();
    }

    // Returns buffer space suitable to read bytes to
    span<std::uint8_t> buffer() { return buffer_.free_area(); }

    // Removes old messages stored in the buffer, and resizes it, if required, to accommodate
    // the message currently being parsed.
    BOOST_ATTRIBUTE_NODISCARD
    error_code prepare_buffer()
    {
        constexpr std::size_t small_move_threshold = 1024;
        const std::size_t occupied_space = buffer_.pending_size() + buffer_.current_message_size();
        // Compact the buffer (remove reserved area) if one of the following holds:
        // 1. The cost of memmove is low: active data (current_message + pending)
        // is small enough to make the copy cheap.
        // 2. Compaction could prevent a reallocation.
        if (occupied_space <= small_move_threshold ||
            (state_.required_size > buffer_.free_size()))
        {
            buffer_.remove_reserved();
        }
        auto ec = buffer_.grow_to_fit(state_.required_size);
        if (ec)
            return ec;
        state_.required_size = 0;
        return error_code();
    }

    // The main operation. Call it after reading bytes against buffer(),
    // with the number of bytes read
    void resume(std::size_t bytes_read)
    {
        frame_header header{};
        buffer_.move_to_pending(bytes_read);

        switch (state_.resume_point)
        {
        case 0:

            // Move the previously parsed message to the reserved area, if any
            buffer_.move_to_reserved(buffer_.current_message_size());

            while (true)
            {
                // Read the header
                set_required_size(frame_header_size);
                while (buffer_.pending_size() < frame_header_size)
                    BOOST_MYSQL_YIELD_VOID(state_.resume_point, 1)

                // Mark the header as belonging to the current message
                buffer_.move_to_current_message(frame_header_size);

                // Deserialize the header
                header = deserialize_frame_header(span<const std::uint8_t, frame_header_size>(
                    buffer_.pending_first() - frame_header_size,
                    frame_header_size
                ));

                // Process the sequence number
                if (*state_.sequence_number != header.sequence_number)
                {
                    state_.ec = client_errc::sequence_number_mismatch;
                    state_.resume_point = -1;
                    return;
                }
                ++*state_.sequence_number;

                // Process the packet size
                state_.body_bytes = header.size;
                state_.more_frames_follow = (state_.body_bytes == max_frame_size_);

                // We are done with the header
                if (state_.is_first_frame)
                {
                    // If it's the 1st frame, we can just move the header bytes to the reserved
                    // area, avoiding a big memmove
                    buffer_.move_to_reserved(frame_header_size);
                }
                else
                {
                    buffer_.remove_current_message_last(frame_header_size);
                }
                state_.is_first_frame = false;

                // Read the body
                set_required_size(state_.body_bytes);
                while (buffer_.pending_size() < state_.body_bytes)
                    BOOST_MYSQL_YIELD_VOID(state_.resume_point, 2)

                buffer_.move_to_current_message(state_.body_bytes);

                // Check if we're done
                if (!state_.more_frames_follow)
                {
                    state_.resume_point = -1;
                    return;
                }
            }
        }
    }

    // Exposed for testing
    const read_buffer& internal_buffer() const { return buffer_; }

private:
    read_buffer buffer_;
    std::size_t max_frame_size_;

    struct parse_state
    {
        int resume_point{0};
        std::uint8_t* sequence_number{};
        bool is_first_frame{true};
        std::size_t body_bytes{0};
        bool more_frames_follow{false};
        std::size_t required_size{0};
        error_code ec;

        parse_state() = default;
        parse_state(std::uint8_t& seqnum) noexcept : sequence_number(&seqnum) {}
    } state_;

    void set_required_size(std::size_t required_bytes)
    {
        if (required_bytes > buffer_.pending_size())
            state_.required_size = required_bytes - buffer_.pending_size();
        else
            state_.required_size = 0;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
