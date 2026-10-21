//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_PIPE_HPP
#define BOOST_COBALT_IO_PIPE_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/io/stream.hpp>
#include <boost/cobalt/op.hpp>

#include <boost/asio/basic_readable_pipe.hpp>
#include <boost/asio/basic_writable_pipe.hpp>

#include <boost/system/result.hpp>

namespace boost::cobalt::io
{


BOOST_COBALT_IO_DECL
system::result<std::pair<struct readable_pipe, struct writable_pipe>> pipe(
    const executor & executor = this_thread::get_executor()
    );


struct BOOST_SYMBOL_VISIBLE readable_pipe final : read_stream
{
  using native_handle_type = asio::basic_readable_pipe<executor>::native_handle_type;

  BOOST_COBALT_IO_DECL readable_pipe(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL readable_pipe(native_handle_type native_file, const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL readable_pipe(readable_pipe && sf) noexcept;

  BOOST_COBALT_IO_DECL system::result<void> assign(native_handle_type native_file);
  BOOST_COBALT_IO_DECL system::result<void> cancel();

  BOOST_COBALT_IO_DECL executor get_executor();
  BOOST_COBALT_IO_DECL bool is_open() const;

  BOOST_COBALT_IO_DECL system::result<void> close();

  BOOST_COBALT_IO_DECL native_handle_type native_handle();
  BOOST_COBALT_IO_DECL system::result<native_handle_type> release();

  [[nodiscard]] BOOST_COBALT_MSVC_NOINLINE read_op read_some(mutable_buffer_sequence buffer)
  {
    return {buffer, this, initiate_read_some_};
  }

 private:

  BOOST_COBALT_IO_DECL static void initiate_read_some_(void *, mutable_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);

  BOOST_COBALT_IO_DECL
  friend system::result<std::pair<struct readable_pipe, struct writable_pipe>> pipe(const cobalt::executor & executor);
  asio::basic_readable_pipe<executor> implementation_;

};


struct BOOST_SYMBOL_VISIBLE writable_pipe final : write_stream
{
  using native_handle_type = asio::basic_writable_pipe<executor>::native_handle_type;

  BOOST_COBALT_IO_DECL writable_pipe(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL writable_pipe(native_handle_type native_file, const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL writable_pipe(writable_pipe && sf) noexcept;

  BOOST_COBALT_IO_DECL system::result<void> assign(native_handle_type native_file);
  BOOST_COBALT_IO_DECL system::result<void> cancel();

  BOOST_COBALT_IO_DECL executor get_executor();
  BOOST_COBALT_IO_DECL bool is_open() const;

  [[nodiscard]] BOOST_COBALT_MSVC_NOINLINE write_op write_some(const_buffer_sequence buffer)
  {
    return {buffer, this, initiate_write_some_};
  }

  BOOST_COBALT_IO_DECL system::result<void> close();

  BOOST_COBALT_IO_DECL native_handle_type native_handle();

  BOOST_COBALT_IO_DECL system::result<native_handle_type> release();

 private:
  BOOST_COBALT_IO_DECL static void initiate_write_some_(void *, const_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);

  BOOST_COBALT_IO_DECL
  friend system::result<std::pair<struct readable_pipe, struct  writable_pipe>> pipe(const cobalt::executor & executor);
  asio::basic_writable_pipe<executor> implementation_;
};

}

namespace boost::process
{
template<typename T>
struct is_readable_pipe;

template<>
struct is_readable_pipe<boost::cobalt::io::readable_pipe> : std::true_type
{
};


template<typename T>
struct is_writable_pipe;

template<>
struct is_writable_pipe<boost::cobalt::io::writable_pipe> : std::true_type
{
};
}

#endif //BOOST_COBALT_IO_PIPE_HPP
