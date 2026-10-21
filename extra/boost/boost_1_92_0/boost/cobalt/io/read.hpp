//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_READ_HPP
#define BOOST_COBALT_IO_READ_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/ops.hpp>

#include <concepts>

namespace boost::cobalt::io
{

struct BOOST_COBALT_IO_DECL read_all final : op<system::error_code, std::size_t>
{
  read_op step;
  read_all(read_op op) : step(op) {}
  ~read_all() = default;

   void initiate(completion_handler<system::error_code, std::size_t>) final;
};

template<typename Stream>
  requires requires (Stream & str, mutable_buffer_sequence buffer)
  {
    {str.read_some(buffer)} -> std::same_as<read_op>;
  }
[[nodiscard]] BOOST_COBALT_MSVC_NOINLINE
read_all read(Stream & str, mutable_buffer_sequence buffer)
{
  return read_all{str.read_some(buffer)};
}


struct BOOST_COBALT_IO_DECL read_all_at  final : op<system::error_code, std::size_t>
{
  read_at_op step;
  read_all_at(read_at_op op) : step(op) {}
  ~read_all_at() = default;

  void initiate(completion_handler<system::error_code, std::size_t>) final;
};

template<typename Stream>
requires requires (Stream & str, std::uint64_t offset,  mutable_buffer_sequence buffer)
{
  {str.read_some_at(offset, buffer)} -> std::same_as<read_at_op>;
}
[[nodiscard]] BOOST_COBALT_MSVC_NOINLINE
read_all_at read_at(Stream & str, std::uint64_t offset, mutable_buffer_sequence buffer)
{
  return read_all_at{str.read_some_at(offset, buffer)};
}


}

#endif //BOOST_COBALT_IO_READ_HPP
