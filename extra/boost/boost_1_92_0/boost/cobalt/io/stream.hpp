//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_STREAM_HPP
#define BOOST_COBALT_IO_STREAM_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/ops.hpp>

namespace boost::cobalt::io
{

// tag::outline[]
struct BOOST_SYMBOL_VISIBLE write_stream
{
  virtual ~write_stream() = default;
  [[nodiscard]] virtual write_op write_some(const_buffer_sequence buffer) = 0;
};

struct BOOST_SYMBOL_VISIBLE read_stream
{
  virtual ~read_stream() = default;
  [[nodiscard]] virtual read_op read_some(mutable_buffer_sequence buffer) = 0;
};

struct stream : read_stream, write_stream
{
};
// end::outline[]

}

#endif //BOOST_COBALT_IO_STREAM_HPP
