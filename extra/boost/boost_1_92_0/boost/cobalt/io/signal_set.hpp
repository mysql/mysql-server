//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_SIGNAL_SET_HPP
#define BOOST_COBALT_IO_SIGNAL_SET_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/op.hpp>

#include <boost/asio/basic_signal_set.hpp>
#include <boost/system/result.hpp>

#include <initializer_list>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE signal_set
{
  BOOST_COBALT_IO_DECL signal_set(const cobalt::executor & executor = this_thread::get_executor());
  BOOST_COBALT_IO_DECL signal_set(std::initializer_list<int> sigs, const cobalt::executor & executor = this_thread::get_executor());

  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> cancel();
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> clear();
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> add(int signal_number);
  [[nodiscard]] BOOST_COBALT_IO_DECL system::result<void> remove(int signal_number);


 private:
  struct BOOST_COBALT_IO_DECL wait_op_ final : cobalt::op<system::error_code, int>
  {
    void initiate(completion_handler<system::error_code, int> h) final;
    wait_op_(asio::basic_signal_set<cobalt::executor> & signal_set) : signal_set_(signal_set) {}
    ~wait_op_() = default;
   private:
    asio::basic_signal_set<cobalt::executor> & signal_set_;
  };
 public:
  [[nodiscard]] auto wait() { return wait_op_{signal_set_}; }
 private:
  asio::basic_signal_set<cobalt::executor> signal_set_;
};


}

#endif //BOOST_COBALT_IO_SIGNAL_SET_HPP
