#ifndef BOOST_LEAF_CONFIG_VISIBILITY_HPP_INCLUDED
#define BOOST_LEAF_CONFIG_VISIBILITY_HPP_INCLUDED

// Copyright 2018-2024 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

namespace boost { namespace leaf {

class BOOST_LEAF_SYMBOL_VISIBLE error_id;
class BOOST_LEAF_SYMBOL_VISIBLE error_info;
class BOOST_LEAF_SYMBOL_VISIBLE diagnostic_info;
class BOOST_LEAF_SYMBOL_VISIBLE diagnostic_details;

struct BOOST_LEAF_SYMBOL_VISIBLE e_api_function;
struct BOOST_LEAF_SYMBOL_VISIBLE e_file_name;
struct BOOST_LEAF_SYMBOL_VISIBLE e_errno;
struct BOOST_LEAF_SYMBOL_VISIBLE e_type_info_name;
struct BOOST_LEAF_SYMBOL_VISIBLE e_at_line;
struct BOOST_LEAF_SYMBOL_VISIBLE e_source_location;

class BOOST_LEAF_SYMBOL_VISIBLE bad_result;
template <class> class BOOST_LEAF_SYMBOL_VISIBLE result;

namespace detail
{
    template <class> class BOOST_LEAF_SYMBOL_VISIBLE slot;

    class BOOST_LEAF_SYMBOL_VISIBLE exception_base;

    template <class> class BOOST_LEAF_SYMBOL_VISIBLE exception;

#if BOOST_LEAF_CFG_CAPTURE
    class BOOST_LEAF_SYMBOL_VISIBLE dynamic_allocator;
#endif

#if BOOST_LEAF_CFG_STD_SYSTEM_ERROR
    class BOOST_LEAF_SYMBOL_VISIBLE leaf_error_category;
    template <class> struct BOOST_LEAF_SYMBOL_VISIBLE get_leaf_error_category;
#endif
}

} } // namespace boost::leaf

#endif // BOOST_LEAF_CONFIG_VISIBILITY_HPP_INCLUDED
