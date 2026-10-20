//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SERIALIZATION_CONTEXT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_IMPL_SERIALIZATION_CONTEXT_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/protocol/frame_header.hpp>

#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// Disables framing in serialization_context
BOOST_INLINE_CONSTEXPR std::size_t disable_framing = static_cast<std::size_t>(-1);

// Helper to compose a packet with any required frame headers. Embedding knowledge
// of frame headers in serialization functions creates messages ready to send.
// We require the entire message to be created before it's sent, so we don't lose any functionality.
//
// This class knows the offset of the next frame header. Adding data will correctly
// insert space for headers as required while copying the data.
//
// Like format_context_base, contains an error that can be set if a serialization
// function helps (e.g. because it would overrun the buffer size limit).
// Once set, serializing is a no-op. This pattern allows us to check for errors just once.
class serialization_context
{
    std::vector<std::uint8_t>& buffer_;
    std::size_t max_buffer_size_;
    std::size_t max_frame_size_;
    std::size_t next_header_offset_;
    error_code err_;

    // max_frame_size_ == -1 can be used to disable framing. Used for testing
    bool framing_enabled() const { return max_frame_size_ != disable_framing; }

    void append_to_buffer(span<const std::uint8_t> contents)
    {
        // Check if the buffer has space for the given contents
        if (buffer_.size() + contents.size() > max_buffer_size_)
            add_error(client_errc::max_buffer_size_exceeded);

        // Copy if there was no error
        if (!err_)
            buffer_.insert(buffer_.end(), contents.begin(), contents.end());
    }

    void append_header() { append_to_buffer(std::array<std::uint8_t, frame_header_size>{}); }

    void add_impl(span<const std::uint8_t> content)
    {
        // Add the content in chunks, inserting space for headers where required
        std::size_t content_offset = 0;
        while (content_offset < content.size())
        {
            // Serialize what we've got space for
            BOOST_ASSERT(next_header_offset_ > buffer_.size());
            auto remaining_content = static_cast<std::size_t>(content.size() - content_offset);
            auto remaining_frame = static_cast<std::size_t>(next_header_offset_ - buffer_.size());
            auto size_to_write = (std::min)(remaining_content, remaining_frame);
            append_to_buffer(content.subspan(content_offset, size_to_write));
            content_offset += size_to_write;

            // Insert space for a frame header if required
            if (buffer_.size() == next_header_offset_)
            {
                append_header();
                next_header_offset_ += (max_frame_size_ + frame_header_size);
            }
        }
    }

    template <class Serializable, class... Rest>
    static constexpr std::size_t fixed_total_size(Serializable, Rest... rest)
    {
        return Serializable::size + fixed_total_size(rest...);
    }

    static constexpr std::size_t fixed_total_size() { return 0u; }

    template <class Serializable, class... Rest>
    static void serialize_fixed_impl(std::uint8_t* it, Serializable serializable, Rest... rest)
    {
        serializable.serialize_fixed(it);
        serialize_fixed_impl(it + Serializable::size, rest...);
    }

    static void serialize_fixed_impl(std::uint8_t*) {}

public:
    serialization_context(
        std::vector<std::uint8_t>& buff,
        std::size_t max_buffer_size = static_cast<std::size_t>(-1),
        std::size_t max_frame_size = max_packet_size
    )
        : buffer_(buff),
          max_buffer_size_(max_buffer_size),
          max_frame_size_(max_frame_size),
          next_header_offset_(
              framing_enabled() ? buffer_.size() + max_frame_size_ + frame_header_size
                                : static_cast<std::size_t>(-1)
          )
    {
        // Add space for the initial header
        if (framing_enabled())
            append_header();
    }

    // Exposed for testing
    std::size_t next_header_offset() const { return next_header_offset_; }

    void add(std::uint8_t value) { add_impl({&value, 1}); }

    // To be called by serialize() functions. Appends bytes to the buffer.
    void add(span<const std::uint8_t> content) { add_impl(content); }

    // Make serialization_context compatible with output_string
    void append(const char* content, std::size_t size)
    {
        add({reinterpret_cast<const std::uint8_t*>(content), size});
    }

    // Sets the error state
    void add_error(error_code ec)
    {
        if (!err_)
            err_ = ec;
    }

    error_code error() const { return err_; }

    // Write frame headers to an already serialized message with space for them
    std::uint8_t write_frame_headers(std::uint8_t seqnum, std::size_t initial_offset)
    {
        BOOST_ASSERT(framing_enabled());
        BOOST_ASSERT(!err_);
        BOOST_ASSERT(initial_offset < buffer_.size());

        // Actually write the headers
        std::size_t offset = initial_offset;
        while (offset < buffer_.size())
        {
            // Calculate the current frame size
            std::size_t frame_first = offset + frame_header_size;
            std::size_t frame_last = (std::min)(frame_first + max_frame_size_, buffer_.size());
            auto frame_size = static_cast<std::uint32_t>(frame_last - frame_first);

            // Write the frame header
            BOOST_ASSERT(frame_first <= buffer_.size());
            serialize_frame_header(
                span<std::uint8_t, frame_header_size>(buffer_.data() + offset, frame_header_size),
                frame_header{frame_size, seqnum++}
            );

            // Skip to the next frame
            offset = frame_last;
        }

        // We should have finished just at the buffer end
        BOOST_ASSERT(offset == buffer_.size());

        return seqnum;
    }

    // Optimization for fixed size types. We serialize them to an
    // intermediate, stack-based buffer, then copy them to the actual buffer.
    // This saves reallocations and space checks
    template <class... Serializable>
    void serialize_fixed(Serializable... s)
    {
        std::array<std::uint8_t, fixed_total_size(Serializable{}...)> buff;
        serialize_fixed_impl(buff.data(), s...);
        add(buff);
    }

    // Allow chaining
    template <class... Serializable>
    void serialize(Serializable... s)
    {
        int dummy[] = {(s.serialize(*this), 0)...};
        ignore_unused(dummy);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
