#ifndef BOOST_SMART_PTR_DETAIL_SP_COUNTED_BASE_HPP_INCLUDED
#define BOOST_SMART_PTR_DETAIL_SP_COUNTED_BASE_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//
//  detail/sp_counted_base.hpp
//
//  Copyright 2005-2013 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/smart_ptr/detail/deprecated_macros.hpp>

#if defined( BOOST_SP_DISABLE_THREADS )
# include <boost/smart_ptr/detail/sp_counted_base_nt.hpp>

#else
# include <boost/smart_ptr/detail/sp_counted_base_std_atomic.hpp>

#endif

#endif  // #ifndef BOOST_SMART_PTR_DETAIL_SP_COUNTED_BASE_HPP_INCLUDED
