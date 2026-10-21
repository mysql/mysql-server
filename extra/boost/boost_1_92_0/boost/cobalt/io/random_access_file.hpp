//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_RANDOM_ACCESS_FILE_HPP
#define BOOST_COBALT_IO_RANDOM_ACCESS_FILE_HPP

#include <boost/asio/basic_random_access_file.hpp>

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/file.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/io/random_access_device.hpp>
#include <boost/cobalt/noop.hpp>
#include <boost/cobalt/op.hpp>

namespace boost::cobalt::io
{


struct BOOST_SYMBOL_VISIBLE random_access_file : file, random_access_device
{
  using native_handle_type = file::native_handle_type;

  BOOST_COBALT_IO_DECL random_access_file(const executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL random_access_file(const char * path, file::flags open_flags,
                                       const executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL random_access_file(const std::string & path, file::flags open_flags,
                                       const executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL random_access_file(const native_handle_type & native_file,
                                       const executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL random_access_file(random_access_file && sf) noexcept;

  write_at_op write_some_at(std::uint64_t offset, const_buffer_sequence buffer)
  {
    return { offset, buffer, this, initiate_write_some_at_};
  }

  read_at_op read_some_at(std::uint64_t offset, mutable_buffer_sequence buffer)
  {
    return { offset, buffer, this, initiate_read_some_at_};
  }
 private:
  BOOST_COBALT_IO_DECL static void initiate_read_some_at_(void *, std::uint64_t,  mutable_buffer_sequence, completion_handler<system::error_code, std::size_t>);
  BOOST_COBALT_IO_DECL static void initiate_write_some_at_(void *, std::uint64_t, const_buffer_sequence,   completion_handler<system::error_code, std::size_t>);
#if defined(BOOST_ASIO_HAS_FILE)
  asio::basic_random_access_file<executor> implementation_;
#endif
};


}

#endif //BOOST_COBALT_IO_RANDOM_ACCESS_FILE_HPP
