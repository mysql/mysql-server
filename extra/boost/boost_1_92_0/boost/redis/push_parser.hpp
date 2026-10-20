//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_PUSH_PARSER_HPP
#define BOOST_REDIS_PUSH_PARSER_HPP

#include <boost/redis/resp3/node.hpp>

#include <boost/core/span.hpp>

#include <iterator>
#include <optional>
#include <string_view>

namespace boost::redis {

/**
 * @brief A Pub/Sub message received from the server.
 *
 * Can represent messages from both regular subscriptions (`message`) and
 * pattern subscriptions (`pmessage`) with string payloads.
 *
 * @par Object lifetimes
 * This object contains views pointing into external storage
 * (typically a @ref resp3::flat_tree).
 */
struct push_view {
   /// @brief The channel where the message was published.
   std::string_view channel;

   /**
    * @brief The pattern that matched the channel.
    *
    * Set only for messages received through pattern subscriptions (`pmessage`);
    * @ref std::nullopt otherwise.
    */
   std::optional<std::string_view> pattern;

   /// The message payload.
   std::string_view payload;
};

/**
 * @brief A range that parses Pub/Sub messages from a sequence of RESP3 nodes.
 *
 * Given an input range of RESP3 nodes, this class attempts to parse them
 * as Pub/Sub messages. Parsing occurs incrementally, while advancing the iterators.
 *
 * This type models `std::input_range`.
 *
 * This class handles pushes with type `message` and `pmessage` with string payloads.
 * These are the messages containing the actual information in the usual Pub/Sub workflow.
 * Any RESP3 message with a different shape will be skipped. In particular, the following
 * message types are skipped:
 *
 *   @li Messages with type other than @ref resp3::type::push. This includes
 *       the output of the `MONITOR` command and errors.
 *   @li Subscribe and unsubscribe confirmation messages.
 *       These happen every time a `SUBSCRIBE`, `UNSUBSCRIBE`, `PSUBSCRIBE` or `PUNSUBSCRIBE`
 *       command is successfully executed, and don't carry application-level information.
 *   @li Pushes with type `message` and `pmessage` with a payload type different to
 *       @ref resp3::type::blob_string. Client-side caching generates this kind of messages.
 *
 * If you need to handle any of these, use the raw RESP3 nodes rather than this class.
 *
 * @par Object lifetimes
 * Iteration state is held by the parser. Iterators reference the parser.
 * The parser must be kept alive for the duration of the iteration.
 *
 * No copies of the input range are made. The returned @ref push_view
 * values are views.
 *
 * @par Example
 * @code
 * for (const push_view& msg : push_parser(tree)) {
 *    std::cout << "Channel: " << msg.channel << ", Payload: " << msg.payload << "\n";
 * }
 * @endcode
 */
class push_parser {
   const resp3::node_view* first_{};
   const resp3::node_view* last_{};
   push_view current_{};
   bool done_{false};

   void advance() noexcept;

public:
   /**
    * @brief Iterator over parsed Pub/Sub messages.
    *
    * Models `std::input_iterator`. Iterators are invalidated when the
    * @ref push_parser they refer to is destroyed or when the underlying
    * node range is modified.
    *
    * The exact iterator type is unspecified.
    */
   class iterator {
      push_parser* gen_{};

      friend class push_parser;

      explicit iterator(push_parser* gen) noexcept
      : gen_{gen}
      { }

   public:
      using value_type = push_view;
      using difference_type = std::ptrdiff_t;
      using reference = push_view const&;
      using pointer = push_view const*;
      using iterator_category = std::input_iterator_tag;

      iterator() = default;

      reference operator*() const noexcept { return gen_->current_; }
      pointer operator->() const noexcept { return &gen_->current_; }

      iterator& operator++() noexcept
      {
         BOOST_ASSERT(gen_);
         gen_->advance();
         if (gen_->done_)
            gen_ = nullptr;
         return *this;
      }

      void operator++(int) noexcept { ++*this; }

      friend bool operator==(const iterator& a, const iterator& b) noexcept
      {
         return a.gen_ == b.gen_;
      }

      friend bool operator!=(const iterator& a, const iterator& b) noexcept { return !(a == b); }
   };

   /**
    * @brief Constructs a parser over a range of RESP3 nodes.
    *
    * @par Object lifetimes
    * No copy of `nodes` is made. The underlying `nodes` range
    * must remain valid for the lifetime of this parser.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @param nodes A contiguous range of @ref resp3::node_view to parse.
    */
   explicit push_parser(span<const resp3::node_view> nodes) noexcept
   : first_{nodes.data()}
   , last_{nodes.data() + nodes.size()}
   {
      advance();
   }

   /**
    * @brief Returns an iterator to the first parsed Pub/Sub message.
    *
    * If the node range contains no valid `message` or `pmessage` pushes,
    * returns an iterator equal to @ref end.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns An iterator to the first @ref push_view, or past-the-end if none.
    */
   iterator begin() noexcept { return done_ ? iterator() : iterator(this); }

   /**
    * @brief Returns a past-the-end iterator.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns A past-the-end iterator for this parser.
    */
   iterator end() noexcept { return iterator(); }
};

}  // namespace boost::redis

#endif
