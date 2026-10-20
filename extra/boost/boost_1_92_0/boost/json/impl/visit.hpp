//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_IMPL_VISIT_HPP
#define BOOST_JSON_IMPL_VISIT_HPP

namespace boost {
namespace json {

namespace detail {

extern
BOOST_JSON_DECL
std::nullptr_t stable_np;

} // namespace detail

template<class Visitor>
auto
visit(
    Visitor&& v,
    value& jv) -> decltype(
        static_cast<Visitor&&>(v)( std::declval<std::nullptr_t&>() ) )
{
    switch(jv.kind())
    {
    default: // unreachable()?
    case kind::string:  return static_cast<Visitor&&>(v)( jv.get_string() );
    case kind::array:   return static_cast<Visitor&&>(v)( jv.get_array() );
    case kind::object:  return static_cast<Visitor&&>(v)( jv.get_object() );
    case kind::bool_:   return static_cast<Visitor&&>(v)( jv.get_bool() );
    case kind::int64:   return static_cast<Visitor&&>(v)( jv.get_int64() );
    case kind::uint64:  return static_cast<Visitor&&>(v)( jv.get_uint64() );
    case kind::double_: return static_cast<Visitor&&>(v)( jv.get_double() );
    case kind::null:    return static_cast<Visitor&&>(v)( detail::stable_np ) ;
    }
}

template<class Visitor>
auto
visit(
    Visitor&& v,
    value const& jv) -> decltype(
        static_cast<Visitor&&>(v)( std::declval<std::nullptr_t const&>() ) )
{
    detail::scalar const& sc = detail::access::get_scalar(jv);
    switch(jv.kind())
    {
    default: // unreachable()?
    case kind::string:  return static_cast<Visitor&&>(v)( jv.get_string() );
    case kind::array:   return static_cast<Visitor&&>(v)( jv.get_array() );
    case kind::object:  return static_cast<Visitor&&>(v)( jv.get_object() );
    // local variables work around a bug in older clangs
    case kind::bool_: {
        bool const& b = sc.b;
        return static_cast<Visitor&&>(v)(b);
    }
    case kind::int64: {
        std::int64_t const& i = sc.i;
        return static_cast<Visitor&&>(v)(i);
    }
    case kind::uint64: {
        std::uint64_t const& u =  sc.u;
        return static_cast<Visitor&&>(v)(u);
    }
    case kind::double_: {
        double const& d = sc.d;
        return static_cast<Visitor&&>(v)(d);
    }
    case kind::null: {
        auto const& np = detail::stable_np;
        return static_cast<Visitor&&>(v)(np) ;
    }
    }
}


template<class Visitor>
auto
visit(
    Visitor&& v,
    value&& jv) -> decltype(
        static_cast<Visitor&&>(v)( std::declval<std::nullptr_t&&>() ) )
{
    switch(jv.kind())
    {
    default: // unreachable()?
    case kind::string:  return static_cast<Visitor&&>(v)(std::move( jv.get_string() ));
    case kind::array:   return static_cast<Visitor&&>(v)(std::move( jv.get_array() ));
    case kind::object:  return static_cast<Visitor&&>(v)(std::move( jv.get_object() ));
    case kind::bool_:   return static_cast<Visitor&&>(v)(std::move( detail::access::get_scalar(jv).b ));
    case kind::int64:   return static_cast<Visitor&&>(v)(std::move( detail::access::get_scalar(jv).i ));
    case kind::uint64:  return static_cast<Visitor&&>(v)(std::move( detail::access::get_scalar(jv).u ));
    case kind::double_: return static_cast<Visitor&&>(v)(std::move( detail::access::get_scalar(jv).d ));
    case kind::null:    return static_cast<Visitor&&>(v)(std::move( detail::stable_np )) ;
    }
}

} // namespace json
} // namespace boost

#endif
