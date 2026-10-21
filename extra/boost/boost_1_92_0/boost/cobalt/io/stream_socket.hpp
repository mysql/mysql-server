//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_STREAM_SOCKET_HPP
#define BOOST_COBALT_IO_STREAM_SOCKET_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/endpoint.hpp>
#include <boost/cobalt/io/socket.hpp>
#include <boost/cobalt/io/stream.hpp>

#include <boost/asio/generic/datagram_protocol.hpp>
#include <boost/asio/basic_stream_socket.hpp>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE stream_socket final : socket, stream
{

  BOOST_COBALT_IO_DECL stream_socket(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL stream_socket(stream_socket && lhs);
  BOOST_COBALT_IO_DECL stream_socket(native_handle_type h, protocol_type protocol = protocol_type(),
                  const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL stream_socket(endpoint ep,
                  const cobalt::executor & executor = this_thread::get_executor());

  [[nodiscard]] write_op write_some(const_buffer_sequence buffer) override
  {
    return {buffer, this, initiate_write_some_};
  }
  [[nodiscard]] read_op read_some(mutable_buffer_sequence buffer) override
  {
    return {buffer, this, initiate_read_some_};
  }

 public:
  BOOST_COBALT_IO_DECL void adopt_endpoint_(endpoint & ep) override;

  BOOST_COBALT_IO_DECL static void initiate_read_some_ (void *, mutable_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);
  BOOST_COBALT_IO_DECL static void initiate_write_some_(void *,   const_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);

  asio::basic_stream_socket<protocol_type, executor> stream_socket_;
  friend struct ssl_stream;
};


inline system::result<std::pair<stream_socket, stream_socket>> make_pair(decltype(local_stream) protocol)
{
  std::pair<stream_socket, stream_socket> res;
  auto c = connect_pair(protocol, res.first, res.second);
  if (c)
    return res;
  else
    return c.error();
}

}

#endif //BOOST_COBALT_IOSTREAM_SOCKET_HPP
