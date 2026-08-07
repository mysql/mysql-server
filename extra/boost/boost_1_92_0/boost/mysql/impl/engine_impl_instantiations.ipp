//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ENGINE_IMPL_INSTANTIATIONS_IPP
#define BOOST_MYSQL_IMPL_ENGINE_IMPL_INSTANTIATIONS_IPP

#pragma once

#include <boost/mysql/detail/engine_stream_adaptor.hpp>

// To be included only in src.hpp

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION
template class boost::mysql::detail::engine_impl<
    boost::mysql::detail::engine_stream_adaptor<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>>;
template class boost::mysql::detail::engine_impl<
    boost::mysql::detail::engine_stream_adaptor<boost::asio::ip::tcp::socket>>;
#endif

#endif
