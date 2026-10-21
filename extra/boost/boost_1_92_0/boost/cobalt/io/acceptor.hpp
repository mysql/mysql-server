//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_EXPERIMENTAL_IO_ACCEPTOR_HPP
#define BOOST_COBALT_EXPERIMENTAL_IO_ACCEPTOR_HPP


#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/io/stream_socket.hpp>
#include <boost/cobalt/io/seq_packet_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE acceptor
{
  using wait_type          = asio::socket_base::wait_type;
  constexpr static std::size_t max_listen_connections = asio::socket_base::max_listen_connections;

  BOOST_COBALT_IO_DECL acceptor(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL acceptor(endpoint ep, const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL system::result<void> bind(endpoint ep);
  BOOST_COBALT_IO_DECL system::result<void> listen(int backlog = max_listen_connections); // int backlog = net::max_backlog()
  BOOST_COBALT_IO_DECL endpoint local_endpoint();
 private:
  struct BOOST_COBALT_IO_DECL accept_op final : op<system::error_code>
  {
    void initiate(completion_handler<system::error_code> h) override;

    accept_op(accept_op &&) noexcept = default;
    accept_op(asio::basic_socket_acceptor<protocol_type, executor> & acceptor, socket& sock)
            : acceptor_(acceptor), sock_(sock) {}
    ~accept_op() = default;
   protected:
    asio::basic_socket_acceptor<protocol_type, executor> &acceptor_;
    socket & sock_;
  };

  struct BOOST_COBALT_IO_DECL accept_stream_op final : op<system::error_code, stream_socket>
  {
    void initiate(completion_handler<system::error_code, stream_socket> h) override;
    
    accept_stream_op(accept_stream_op &&) noexcept = default;
    accept_stream_op(asio::basic_socket_acceptor<protocol_type, executor> & acceptor) : acceptor_(acceptor)   {}
    ~accept_stream_op() = default;
   private:
    asio::basic_socket_acceptor<protocol_type, executor> &acceptor_;
    stream_socket sock_{acceptor_.get_executor()};
  };

  struct BOOST_COBALT_IO_DECL accept_seq_packet_op final : op<system::error_code, seq_packet_socket>
  {
    void initiate(completion_handler<system::error_code, seq_packet_socket> h) override;

     accept_seq_packet_op(accept_seq_packet_op &&) noexcept = default;
     accept_seq_packet_op(asio::basic_socket_acceptor<protocol_type, executor> & acceptor) : acceptor_(acceptor)   {}
    ~accept_seq_packet_op() = default;
   private:
    asio::basic_socket_acceptor<protocol_type, executor> &acceptor_;
    seq_packet_socket sock_{acceptor_.get_executor()};
  };


  struct BOOST_COBALT_IO_DECL wait_op final : op<system::error_code>
  {
    void initiate(completion_handler<system::error_code> h) override;

    wait_op(asio::basic_socket_acceptor<protocol_type, executor> & acceptor, wait_type wt)
    : acceptor_(acceptor), wt_(wt) {}
    ~wait_op() = default;
   private:
    asio::basic_socket_acceptor<protocol_type, executor> &acceptor_;
    wait_type wt_;

  };



 public:
  [[nodiscard]] accept_op accept(socket & sock)
  {
    return accept_op{acceptor_, sock};
  }

  template<protocol_type::family_t F = tcp.family(), protocol_type::protocol_t P = tcp.protocol()>
  [[nodiscard]] accept_stream_op accept(static_protocol<F, tcp.type(), P> stream_proto = tcp)
  {
    return accept_stream_op{acceptor_};
  }

  template<protocol_type::family_t F, protocol_type::protocol_t P>
  [[nodiscard]] accept_seq_packet_op accept(static_protocol<F, local_seqpacket.type(), P> stream_proto)
  {
    return accept_seq_packet_op{acceptor_};
  }


  [[nodiscard]] wait_op wait(wait_type wt = wait_type::wait_read)
  {
    return {acceptor_, wt};
  }

 private:

  asio::basic_socket_acceptor<protocol_type, executor> acceptor_;
};

}

#endif //BOOST_COBALT_EXPERIMENTAL_IO_ACCEPTOR_HPP
