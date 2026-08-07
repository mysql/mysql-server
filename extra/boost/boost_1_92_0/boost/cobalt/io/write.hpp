//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_WRITE_HPP
#define BOOST_COBALT_IO_WRITE_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/ops.hpp>

#include <concepts>

namespace boost::cobalt::io
{

struct BOOST_COBALT_IO_DECL write_all  final : op<system::error_code, std::size_t>
{
  write_op step;
  write_all(write_op op) : step(op) {}

  ~write_all() = default;
  void initiate(completion_handler<system::error_code, std::size_t>) final;
};

template<typename Stream>
  requires requires (Stream & str, const_buffer_sequence buffer)
  {
    {str.write_some(buffer)} -> std::same_as<write_op>;
  }
[[nodiscard]] BOOST_COBALT_MSVC_NOINLINE
write_all write(Stream & str, const_buffer_sequence buffer)
{
  return write_all{str.write_some(buffer)};
}


struct BOOST_COBALT_IO_DECL write_all_at  final : op<system::error_code, std::size_t>
{
  write_at_op step;
  write_all_at(write_at_op op) : step(op) {}
  ~write_all_at() = default;
  void initiate(completion_handler<system::error_code, std::size_t>) final;
};

template<typename Stream>
requires requires (Stream & str, std::uint64_t offset, const_buffer_sequence buffer)
{
  {str.write_some_at(offset, buffer)} -> std::same_as<write_op>;
}
[[nodiscard]] BOOST_COBALT_MSVC_NOINLINE
write_all_at write_at(Stream & str, std::uint64_t offset, const_buffer_sequence buffer)
{
  return write_all_at{str.write_some_at(offset, buffer)};
}


}

#endif //BOOST_COBALT_IO_WRITE_HPP
