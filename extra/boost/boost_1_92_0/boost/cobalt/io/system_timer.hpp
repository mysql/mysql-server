//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_SYSTEM_TIMER_HPP
#define BOOST_COBALT_IO_SYSTEM_TIMER_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <boost/system/result.hpp>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE system_timer final
{
  /// The clock type.
  typedef std::chrono::system_clock clock_type;

  /// The duration type of the clock.
  typedef typename clock_type::duration duration;

  /// The time point type of the clock.
  typedef typename clock_type::time_point time_point;

  BOOST_COBALT_IO_DECL system_timer(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL system_timer(const time_point& expiry_time,
               const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL system_timer(const duration& expiry_time,
               const cobalt::executor & executor = this_thread::get_executor());

  void cancel();

  BOOST_COBALT_IO_DECL time_point expiry() const;
  BOOST_COBALT_IO_DECL void reset(const time_point& expiry_time);
  BOOST_COBALT_IO_DECL void reset(const duration& expiry_time);
  BOOST_COBALT_IO_DECL bool expired() const;

  [[nodiscard]] wait_op wait() { return {this, initiate_wait_, try_wait_}; }
 private:
  BOOST_COBALT_IO_DECL static void initiate_wait_(void *, boost::cobalt::completion_handler<system::error_code>);
  BOOST_COBALT_IO_DECL static void try_wait_(void *, boost::cobalt::handler<system::error_code>);
  asio::basic_waitable_timer<std::chrono::system_clock,
                             asio::wait_traits<std::chrono::system_clock>,
                             executor> timer_;
};

}

#endif //BOOST_COBALT_IO_SYSTEM_TIMER_HPP
