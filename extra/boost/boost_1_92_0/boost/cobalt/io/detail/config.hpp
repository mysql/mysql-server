//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>

#ifndef BOOST_COBALT_IO_DETAIL_CONFIG_HPP
#define BOOST_COBALT_IO_DETAIL_CONFIG_HPP

#if !defined(BOOST_COBALT_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_COBALT_NO_LIB) && !defined(BOOST_COBALT_IO_NO_LIB)
#define BOOST_LIB_NAME boost_cobalt_io
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_COBALT_DYN_LINK)
#define BOOST_DYN_LINK
#endif
#include <boost/config/auto_link.hpp>
#endif

#endif // BOOST_COBALT_IO_DETAIL_CONFIG_HPP

