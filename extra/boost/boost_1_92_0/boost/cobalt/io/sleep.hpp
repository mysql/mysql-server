//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_SLEEP_HPP
#define BOOST_COBALT_IO_SLEEP_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/io/steady_timer.hpp>
#include <boost/cobalt/io/system_timer.hpp>

namespace boost::cobalt::detail::io
{

struct BOOST_COBALT_IO_DECL steady_sleep final : op<system::error_code>
{
  steady_sleep(const std::chrono::steady_clock::time_point & tp);
  steady_sleep(const std::chrono::steady_clock::duration & du);

  std::chrono::steady_clock::time_point tp;

  void ready(handler<system::error_code> h) final override
  {
    if (tp < std::chrono::steady_clock::now())
      h({});
  }
  void initiate(completion_handler<system::error_code> h) final override;
  ~steady_sleep() = default;

  std::optional< asio::basic_waitable_timer<std::chrono::steady_clock, asio::wait_traits<std::chrono::steady_clock>, executor> > timer_;
};

struct BOOST_COBALT_IO_DECL system_sleep final : op<system::error_code>
{
  system_sleep(const std::chrono::system_clock::time_point & tp);
  system_sleep(const std::chrono::system_clock::duration & du);

  std::chrono::system_clock::time_point tp;

  void ready(handler<system::error_code> h) final override
  {
    if (tp < std::chrono::system_clock::now())
      h({});
  }

  void initiate(completion_handler<system::error_code> h) final override;
  ~system_sleep() = default;

  std::optional<asio::basic_waitable_timer<std::chrono::system_clock, asio::wait_traits<std::chrono::system_clock>, executor> > timer_;
};

}

namespace boost::cobalt::io
{

[[nodiscard]] inline auto sleep(const std::chrono::steady_clock::duration & d)    { return ::boost::cobalt::detail::io::steady_sleep{d};}
[[nodiscard]] inline auto sleep(const std::chrono::steady_clock::time_point & tp) { return ::boost::cobalt::detail::io::steady_sleep{tp};}
[[nodiscard]] inline auto sleep(const std::chrono::system_clock::time_point & tp) { return ::boost::cobalt::detail::io::system_sleep{tp};}

template<typename Duration>
[[nodiscard]] inline auto sleep(const std::chrono::time_point<std::chrono::steady_clock, Duration> & tp)
{
  return sleep(std::chrono::time_point_cast<std::chrono::steady_clock::duration >(tp));
}

template<typename Duration>
[[nodiscard]] inline auto sleep(const std::chrono::time_point<std::chrono::system_clock, Duration> & tp)
{
  return sleep(std::chrono::time_point_cast<std::chrono::system_clock::duration >(tp));
}

template<typename Rep, typename Period>
[[nodiscard]] inline auto sleep(const std::chrono::duration<Rep, Period> & dur)
{
  return sleep(std::chrono::duration_cast<std::chrono::steady_clock::duration >(dur));
}

}

#endif //BOOST_COBALT_IO_IO_SLEEP_HPP
