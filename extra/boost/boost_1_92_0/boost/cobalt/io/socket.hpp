//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IOSOCKET_HPP
#define BOOST_COBALT_IOSOCKET_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/endpoint.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/basic_socket.hpp>


namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE socket
{
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> open(protocol_type prot = protocol_type {});
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> close();
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> cancel();
  [[nodiscard]] BOOST_COBALT_IO_DECL bool is_open() const;

  // asio acceptor compatibility
  template<typename T>
  struct rebind_executor {using other = socket;};

  using shutdown_type      = asio::socket_base::shutdown_type;
  using wait_type          = asio::socket_base::wait_type;
  using message_flags      = asio::socket_base::message_flags;
  constexpr static int message_peek          = asio::socket_base::message_peek;
  constexpr static int message_out_of_band   = asio::socket_base::message_out_of_band;
  constexpr static int message_do_not_route  = asio::socket_base::message_do_not_route;
  constexpr static int message_end_of_record = asio::socket_base::message_end_of_record;

  using native_handle_type = asio::basic_socket<protocol_type, executor>::native_handle_type;
  BOOST_COBALT_IO_DECL native_handle_type native_handle();

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> shutdown(shutdown_type = shutdown_type::shutdown_both);

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<endpoint> local_endpoint() const;
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<endpoint> remote_endpoint() const;


  BOOST_COBALT_IO_DECL system::result<void> assign(protocol_type protocol, native_handle_type native_handle);
  BOOST_COBALT_IO_DECL system::result<native_handle_type> release();

  /// copied from what asio does
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<std::size_t> bytes_readable();

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_debug(bool debug);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_debug() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_do_not_route(bool do_not_route);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_do_not_route() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_enable_connection_aborted(bool enable_connection_aborted);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_enable_connection_aborted() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_keep_alive(bool keep_alive);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_keep_alive() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_linger(bool linger, int timeout);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<std::pair<bool, int>> get_linger() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>        set_receive_buffer_size(int receive_buffer_size);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<int> get_receive_buffer_size() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>        set_send_buffer_size(int send_buffer_size);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<int> get_send_buffer_size() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>        set_receive_low_watermark(int receive_low_watermark);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<int> get_receive_low_watermark() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>        set_send_low_watermark(int send_low_watermark);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<int> get_send_low_watermark() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_reuse_address(bool reuse_address);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_reuse_address() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> set_no_delay(bool reuse_address);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<bool> get_no_delay() const;

  struct BOOST_COBALT_IO_DECL wait_op final : op<system::error_code>
  {
    wait_type wt;
    void ready(boost::cobalt::handler<system::error_code>) final;
    void initiate(boost::cobalt::completion_handler<system::error_code>) final;

    wait_op(wait_type wt, socket & sock) :
        wt(wt), sock_(sock) {}
    ~wait_op() = default;
   private:
    socket & sock_;

  };
  [[nodiscard]]  wait_op wait(wait_type wt = wait_type::wait_read)
  {
    return {wt, *this};
  }

  struct BOOST_COBALT_IO_DECL connect_op final : op<system::error_code>
  {
    struct endpoint endpoint;

    void initiate(boost::cobalt::completion_handler<system::error_code>) final;

    connect_op(struct endpoint endpoint, socket & socket) :
        endpoint(endpoint), sock_(socket) {}
    ~connect_op() = default;
   private:
    socket & sock_;
  };

  [[nodiscard]] connect_op connect(endpoint ep)
  {
    return {ep, *this};
  }


  struct BOOST_COBALT_IO_DECL ranged_connect_op final : op<system::error_code, endpoint>
  {
    endpoint_sequence endpoints;

    void initiate(boost::cobalt::completion_handler<system::error_code, endpoint>) final;

    ranged_connect_op(endpoint_sequence eps, socket & socket) :
        endpoints(eps), sock_(socket) {}
    ~ranged_connect_op() = default;
   private:
    socket & sock_;
  };
  [[nodiscard]] ranged_connect_op connect(endpoint_sequence ep)
  {
    return {std::move(ep), *this};
  }


 protected:
  virtual void adopt_endpoint_(endpoint & ) {}
  socket(asio::basic_socket<protocol_type, executor> & socket) : socket_(socket) {}
 private:

  friend struct acceptor;
  asio::basic_socket<protocol_type, executor> & socket_;
};

BOOST_COBALT_IO_DECL system::result<void> connect_pair(protocol_type protocol, socket & socket1, socket & socket2);


}

#endif //BOOST_COBALT_IOSOCKET_HPP
