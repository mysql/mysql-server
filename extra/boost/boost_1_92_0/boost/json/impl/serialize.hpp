//
// Copyright (c) 2023 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_SERIALIZE_HPP
#define BOOST_JSON_IMPL_SERIALIZE_HPP

#include <boost/json/serializer.hpp>

namespace boost {
namespace json {
namespace detail {

BOOST_JSON_DECL
void
serialize_impl(std::string& s, serializer& sr);

} // namespace detail

template<class T>
std::string
serialize(T const& t, serialize_options const& opts)
{
    unsigned char buf[256];
    serializer sr(
        storage_ptr(),
        buf,
        sizeof(buf),
        opts);
    std::string s;
    sr.reset(&t);
    detail::serialize_impl(s, sr);
    return s;
}

} // namespace json
} // namespace boost

#endif // BOOST_JSON_IMPL_SERIALIZE_HPP
