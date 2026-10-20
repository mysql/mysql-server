//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ENGINE_HPP
#define BOOST_MYSQL_DETAIL_ENGINE_HPP

#include <boost/mysql/detail/any_resumable_ref.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>

namespace boost {
namespace mysql {
namespace detail {

class engine
{
public:
    using executor_type = asio::any_io_executor;

    virtual ~engine() {}
    virtual executor_type get_executor() = 0;
    virtual bool supports_ssl() const = 0;
    virtual void run(any_resumable_ref resumable, error_code& err) = 0;
    virtual void async_run(any_resumable_ref resumable, asio::any_completion_handler<void(error_code)>) = 0;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
