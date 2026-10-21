//
// Copyright (c) 2022 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_VALUE_HPP
#define BOOST_JSON_IMPL_VALUE_HPP

namespace boost {
namespace json {

value&
value::at_pointer(string_view ptr, source_location const& loc) &
{
    auto const& self = *this;
    return const_cast<value&>( self.at_pointer(ptr, loc) );
}

value&&
value::at_pointer(string_view ptr, source_location const& loc) &&
{
    return std::move( at_pointer(ptr, loc) );
}

} // namespace json
} // namespace boost

#endif // BOOST_JSON_IMPL_VALUE_HPP
