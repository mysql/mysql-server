// Copyright David Abrahams 2001.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef MODULE_DWA2001128_HPP
# define MODULE_DWA2001128_HPP

# include <boost/python/detail/prefix.hpp>

# include <boost/python/module_init.hpp>
# define BOOST_PYTHON_MODULE BOOST_PYTHON_MODULE_INIT
# if PY_VERSION_HEX >= 0x03050000
#  define BOOST_PYTHON_MODULE_MULTI_PHASE BOOST_PYTHON_MODULE_MULTI_PHASE_INIT
#  define BOOST_PYTHON_MODULE_WITH_STATE BOOST_PYTHON_MODULE_WITH_STATE_INIT
# endif

#endif // MODULE_DWA20011221_HPP
