//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_RANDOM_ACCESS_DEVICE_HPP
#define BOOST_COBALT_IO_RANDOM_ACCESS_DEVICE_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/ops.hpp>

namespace boost::cobalt::io
{

// tag::outline[]
struct BOOST_SYMBOL_VISIBLE random_access_write_device
{
  virtual ~random_access_write_device() = default;
  [[nodiscard]] virtual write_at_op write_some_at(std::uint64_t offset, const_buffer_sequence buffer) = 0;
};

struct BOOST_SYMBOL_VISIBLE random_access_read_device
{
  virtual ~random_access_read_device() = default;
  [[nodiscard]] virtual read_at_op read_some_at(std::uint64_t offset, mutable_buffer_sequence buffer) = 0;
};

struct random_access_device : random_access_read_device, random_access_write_device
{
};
// end::outline[]

}

#endif //BOOST_COBALT_IO_RANDOM_ACCESS_DEVICE_HPP
