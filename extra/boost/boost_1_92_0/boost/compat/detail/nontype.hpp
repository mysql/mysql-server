#ifndef BOOST_COMPAT_NONTYPE_HPP_INCLUDED
#define BOOST_COMPAT_NONTYPE_HPP_INCLUDED

// Copyright 2024 Christian Mazakas
// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

namespace boost {
namespace compat {

namespace detail {

template<class V, V v> struct nttp_holder
{
};

} // namespace detail

#if defined(__cpp_nontype_template_parameter_auto) && __cpp_nontype_template_parameter_auto >= 201606L

template<auto V> struct nontype_t: public detail::nttp_holder<decltype(V), V>
{
    explicit nontype_t() = default;
};

template<auto V>
BOOST_INLINE_CONSTEXPR nontype_t<V> nontype {};

#endif

} // namespace compat
} // namespace boost

#endif // BOOST_COMPAT_NONTYPE_HPP_INCLUDED
