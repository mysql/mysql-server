//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_SERIAL_PORT_HPP
#define BOOST_COBALT_IO_SERIAL_PORT_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/io/buffer.hpp>
#include <boost/cobalt/io/stream.hpp>

#include <boost/asio/basic_serial_port.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/system/result.hpp>

namespace boost::cobalt::io
{


struct BOOST_SYMBOL_VISIBLE serial_port final : stream
{
  BOOST_COBALT_IO_DECL system::result<void> close();
  BOOST_COBALT_IO_DECL system::result<void> cancel();
  BOOST_COBALT_IO_DECL bool is_open() const;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> send_break();

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>     set_baud_rate(unsigned rate);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<unsigned> get_baud_rate();

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>     set_character_size(unsigned rate);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<unsigned> get_character_size();

  using flow_control = asio::serial_port_base::flow_control::type;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>         set_flow_control(flow_control rate);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<flow_control> get_flow_control();

  using parity = asio::serial_port_base::parity::type;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>   set_parity(parity rate);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<parity> get_parity();

  using stop_bits = asio::serial_port_base::stop_bits::type;

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void>      set_stop_bits(stop_bits stop_bits);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<stop_bits> get_stop_bits();

  using native_handle_type = typename asio::basic_serial_port<executor>::native_handle_type;
  native_handle_type native_handle() {return serial_port_.native_handle();}

  BOOST_COBALT_IO_DECL serial_port(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL serial_port(serial_port && lhs) = default;
  BOOST_COBALT_IO_DECL serial_port(std::string_view device,         const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL serial_port(native_handle_type native_handle, const cobalt::executor & executor = this_thread::get_executor());

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> assign(native_handle_type native_handle);

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> open(std::string_view device);

  [[nodiscard]] write_op write_some(const_buffer_sequence buffer)
  {
    return {buffer, this, initiate_write_some_};
  }
  [[nodiscard]] read_op read_some(mutable_buffer_sequence buffer)
  {
    return {buffer, this, initiate_read_some_};
  }
 private:
  BOOST_COBALT_IO_DECL static void initiate_read_some_(void *, mutable_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);
  BOOST_COBALT_IO_DECL static void initiate_write_some_(void *, const_buffer_sequence, boost::cobalt::completion_handler<system::error_code, std::size_t>);

  asio::basic_serial_port<executor> serial_port_;
};


}

#endif //BOOST_COBALT_IO_SERIAL_PORT_HPP
