//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#ifndef BOOST_COBALT_RESOLVER_HPP
#define BOOST_COBALT_RESOLVER_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/io/endpoint.hpp>
#include <boost/cobalt/io/ops.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/result.hpp>

namespace boost::cobalt::io
{

struct BOOST_SYMBOL_VISIBLE resolver
{
  using flags = asio::ip::resolver_base::flags;

  resolver(const executor & exec = this_thread::get_executor());
  resolver(resolver && ) = delete;

  void cancel();

 private:
  struct BOOST_COBALT_IO_DECL resolve_op_ final : op<system::error_code, endpoint_sequence>
  {
    void initiate(completion_handler<system::error_code, endpoint_sequence> h) override;

    resolve_op_(asio::ip::basic_resolver<protocol_type, executor> & resolver,
                std::string_view host, std::string_view service, flags flags_ = {})
        : resolver_(resolver), host_(host), service_(service), flags_(flags_) {}
    ~resolve_op_() = default;
   private:
    asio::ip::basic_resolver<protocol_type, executor> & resolver_;
    std::string_view host_;
    std::string_view service_;
    flags flags_;
  };

 public:
  [[nodiscard]] auto resolve(std::string_view host, std::string_view service,
                             flags flags_ = {})
  {
    return resolve_op_{resolver_, host, service, flags_};
  }

 private:
  asio::ip::basic_resolver<protocol_type, executor> resolver_;
};

struct BOOST_COBALT_IO_DECL lookup final : op<system::error_code, endpoint_sequence>
{
  lookup(std::string_view host, std::string_view service,
         const executor & exec = this_thread::get_executor(),
         resolver::flags flags_ = {})
      : host_(host), service_(service), resolver_{exec}, flags_{flags_} {}

  void initiate(completion_handler<system::error_code, endpoint_sequence> h) final override;
  ~lookup() = default;
 private:
  std::string_view host_;
  std::string_view service_;
  asio::ip::basic_resolver<protocol_type, executor> resolver_;
  resolver::flags flags_;
};

}

#endif //BOOST_COBALT_RESOLVER_HPP
