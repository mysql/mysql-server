//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IOSEQ_PACKET_SOCKET_HPP
#define BOOST_COBALT_IOSEQ_PACKET_SOCKET_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/endpoint.hpp>
#include <boost/cobalt/io/socket.hpp>

#include <boost/asio/generic/datagram_protocol.hpp>
#include <boost/asio/basic_seq_packet_socket.hpp>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE seq_packet_socket final : socket
{
  BOOST_COBALT_IO_DECL seq_packet_socket(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL seq_packet_socket(seq_packet_socket && lhs);
  BOOST_COBALT_IO_DECL seq_packet_socket(native_handle_type h, protocol_type protocol = protocol_type(),
                                         const executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL seq_packet_socket(endpoint ep,
                                         const executor & executor = this_thread::get_executor());


  struct BOOST_COBALT_IO_DECL send_op final : op<system::error_code, std::size_t>
  {
    message_flags in_flags;
    const_buffer_sequence buffer;

    send_op(message_flags in_flags, const_buffer_sequence buffer,
            asio::basic_seq_packet_socket<protocol_type, executor> & seq_packet_socket)
            : in_flags(in_flags), buffer(buffer), socket_(seq_packet_socket) {}

    void initiate(completion_handler<system::error_code, std::size_t> handler) final;
    ~send_op() = default;
   private:
    asio::basic_seq_packet_socket<protocol_type, executor> & socket_;
  };

  struct BOOST_COBALT_IO_DECL receive_op final : op<system::error_code, std::size_t>
  {
    message_flags in_flags, *out_flags;
    mutable_buffer_sequence buffer;

    receive_op(message_flags in_flags, message_flags * out_flags,
               mutable_buffer_sequence buffer, asio::basic_seq_packet_socket<protocol_type, executor> & seq_packet_socket)
    : in_flags(in_flags), out_flags(out_flags), buffer(buffer), socket_(seq_packet_socket) {}

    void initiate(completion_handler<system::error_code, std::size_t> handler) final;
    ~receive_op() = default;
   private:
    asio::basic_seq_packet_socket<protocol_type, executor> & socket_;
  };

  [[nodiscard]] receive_op receive(message_flags in_flags, message_flags& out_flags, mutable_buffer_sequence buffer)
  {
    return receive_op{in_flags, &out_flags, buffer, seq_packet_socket_};
  }

  [[nodiscard]] send_op send(message_flags in_flags, const_buffer_sequence buffer)
  {
    return send_op{in_flags, buffer, seq_packet_socket_};
  }
  [[nodiscard]] receive_op receive(message_flags in_flags, mutable_buffer_sequence buffer)
  {
    return receive_op{in_flags, nullptr, buffer, seq_packet_socket_};
  }

 public:
  BOOST_COBALT_IO_DECL void adopt_endpoint_(endpoint & ep) override;

  asio::basic_seq_packet_socket<protocol_type, executor> seq_packet_socket_;
};


inline system::result<std::pair<seq_packet_socket, seq_packet_socket>> make_pair(decltype(local_seqpacket) protocol)
{
  std::pair<seq_packet_socket, seq_packet_socket> res;
  auto c = connect_pair(protocol, res.first, res.second);
  if (c)
    return res;
  else
    return c.error();
}

}

#endif //BOOST_COBALT_IOSEQ_PACKET_SOCKET_HPP
