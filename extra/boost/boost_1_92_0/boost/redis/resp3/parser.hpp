/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_PARSER_HPP
#define BOOST_REDIS_RESP3_PARSER_HPP

#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <array>
#include <optional>
#include <string_view>

namespace boost::redis::resp3 {

class parser {
public:
   using node_type = basic_node<std::string_view>;
   using result = std::optional<node_type>;

   static constexpr std::size_t max_embedded_depth = 5;
   static constexpr std::string_view sep = "\r\n";

private:
   using sizes_type = std::array<std::size_t, max_embedded_depth + 1>;

   // sizes_[0] = 2 because the sentinel must be more than 1.
   static constexpr sizes_type default_sizes = {
      {2, 1, 1, 1, 1, 1}
   };
   static constexpr auto default_bulk_length = static_cast<std::size_t>(-1);

   // The current depth. Simple data types will have depth 0, whereas
   // the elements of aggregates will have depth 1. Embedded types
   // will have increasing depth.
   std::size_t depth_;

   // The parser supports up to 5 levels of nested structures. The
   // first element in the sizes stack is a sentinel and must be
   // different from 1.
   sizes_type sizes_;

   // Contains the length expected in the next bulk read.
   std::size_t bulk_length_;

   // The type of the next bulk. Contains type::invalid if no bulk is
   // expected.
   type bulk_;

   // The number of bytes consumed from the buffer.
   std::size_t consumed_;

   // Returns the number of bytes that have been consumed.
   auto consume_impl(type t, std::string_view elem, system::error_code& ec) -> node_type;

   void commit_elem() noexcept;

   // The bulk type expected in the next read. If none is expected
   // returns type::invalid.
   [[nodiscard]]
   auto bulk_expected() const noexcept -> bool
   {
      return bulk_ != type::invalid;
   }

public:
   parser();

   // Returns true when the parser is done with the current message.
   [[nodiscard]]
   auto done() const noexcept -> bool;

   auto get_consumed() const noexcept -> std::size_t;

   auto consume(std::string_view view, system::error_code& ec) noexcept -> result;

   void reset();

   bool is_parsing() const noexcept;
};

// Returns false if more data is needed. If true is returned the
// parser is either done or an error occured, that can be checked on
// ec.
template <class Adapter>
bool parse(parser& p, std::string_view const& msg, Adapter& adapter, system::error_code& ec)
{
   // This if could be avoid with a state machine that jumps into the
   // correct position.
   if (!p.is_parsing())
      adapter.on_init();

   while (!p.done()) {
      auto const res = p.consume(msg, ec);
      if (ec)
         return true;

      if (!res)
         return false;

      adapter.on_node(res.value(), ec);
      if (ec)
         return true;
   }

   adapter.on_done();
   return true;
}

}  // namespace boost::redis::resp3

#endif  // BOOST_REDIS_RESP3_PARSER_HPP
