#ifndef BOOST_LEAF_DETAIL_EXCEPTION_BASE_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_EXCEPTION_BASE_HPP_INCLUDED

// Copyright 2018-2026 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>

namespace boost { namespace leaf {

class error_id;

namespace detail
{
    struct type_name;

    class exception_base
    {
    public:
        virtual error_id get_error_id() const noexcept = 0;
        virtual type_name get_type_name() const = 0;
    protected:
        exception_base() noexcept { }
        ~exception_base() noexcept { }
    };
}

} }

#endif // #ifndef BOOST_LEAF_DETAIL_EXCEPTION_BASE_HPP_INCLUDED
