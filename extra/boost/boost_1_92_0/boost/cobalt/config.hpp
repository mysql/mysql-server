//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>

#ifndef BOOST_COBALT_CONFIG_HPP
#define BOOST_COBALT_CONFIG_HPP

#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_COBALT_DYN_LINK)
#if defined(BOOST_COBALT_SOURCE)
#define BOOST_COBALT_DECL BOOST_SYMBOL_EXPORT
#else
#define BOOST_COBALT_DECL BOOST_SYMBOL_IMPORT
#endif
#if defined(BOOST_COBALT_IO_SOURCE)
#define BOOST_COBALT_IO_DECL BOOST_SYMBOL_EXPORT
#else
#define BOOST_COBALT_IO_DECL BOOST_SYMBOL_IMPORT
#endif
#if defined(BOOST_COBALT_SSL_SOURCE)
#define BOOST_COBALT_SSL_DECL  BOOST_SYMBOL_EXPORT
#else
#define BOOST_COBALT_SSL_DECL BOOST_SYMBOL_IMPORT
#endif
#else
#define BOOST_COBALT_SSL_DECL
#define BOOST_COBALT_DECL
#define BOOST_COBALT_IO_DECL
#endif

#if defined(BOOST_MSVC)
#define BOOST_COBALT_MSVC_NOINLINE BOOST_NOINLINE
#else
#define BOOST_COBALT_MSVC_NOINLINE
#endif

#if defined(BOOST_COBALT_USE_IO_CONTEXT)
# include <boost/asio/io_context.hpp>
#elif !defined(BOOST_COBALT_CUSTOM_EXECUTOR)
# include <boost/asio/any_io_executor.hpp>
#endif

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1950)
// msvc doesn't correctly suspend for self-deletion, hence we must workaround here
#define BOOST_COBALT_NO_SELF_DELETE 1
#endif

#if !defined(BOOST_COBALT_USE_STD_PMR) && \
    !defined(BOOST_COBALT_USE_BOOST_CONTAINER_PMR) && \
    !defined(BOOST_COBALT_USE_CUSTOM_PMR) && \
    !defined(BOOST_COBALT_NO_PMR)
#define BOOST_COBALT_USE_STD_PMR 1
#endif

#if defined(BOOST_COBALT_USE_BOOST_CONTAINER_PMR)
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/vector.hpp>
#endif

#if defined(BOOST_COBALT_USE_STD_PMR)
#include <memory_resource>
#endif

#if !defined(BOOST_COBALT_OP_SBO_SIZE)
#define BOOST_COBALT_SBO_BUFFER_SIZE 4096
#endif

namespace boost::cobalt
{

#if defined(BOOST_COBALT_USE_IO_CONTEXT)
using executor = boost::asio::io_context::executor_type;
#elif !defined(BOOST_COBALT_CUSTOM_EXECUTOR)
using executor = boost::asio::any_io_executor;
#endif

#if defined(BOOST_COBALT_USE_BOOST_CONTAINER_PMR)
namespace pmr = boost::container::pmr;
#endif

#if defined(BOOST_COBALT_USE_STD_PMR)
namespace pmr = std::pmr;
#endif

}

#if !defined(BOOST_COBALT_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_COBALT_NO_LIB)
#define BOOST_LIB_NAME boost_cobalt
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_COBALT_DYN_LINK)
#define BOOST_DYN_LINK
#endif
#include <boost/config/auto_link.hpp>
#endif

#endif //BOOST_COBALT_CONFIG_HPP
