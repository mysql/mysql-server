//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_INITIATION_BASE_HPP
#define BOOST_MYSQL_DETAIL_INITIATION_BASE_HPP

#include <boost/mysql/with_diagnostics.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associator.hpp>
#include <boost/asio/deferred.hpp>

#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

struct executor_with_default : asio::any_io_executor
{
    using default_completion_token_type = with_diagnostics_t<asio::deferred_t>;

    template <
        typename InnerExecutor1,
        class = typename std::enable_if<!std::is_same<InnerExecutor1, executor_with_default>::value>::type>
    executor_with_default(const InnerExecutor1& ex) noexcept : asio::any_io_executor(ex)
    {
    }
};

// Base class for initiation objects. Includes a bound executor, so they're compatible
// with asio::cancel_after and similar. The bound executor has our default completion token.
// Use only in the ops that should use this token.
struct initiation_base
{
    executor_with_default ex;

    initiation_base(asio::any_io_executor ex) noexcept : ex(std::move(ex)) {}

    using executor_type = executor_with_default;
    const executor_type& get_executor() const noexcept { return ex; }
};

}  // namespace detail
}  // namespace mysql

}  // namespace boost

#endif
