// Copyright (C) 2003, 2008 Fernando Luis Cacciola Carballal.
// Copyright (C) 2014 - 2026 Andrzej Krzemieński.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the authors at:
//  fernando_cacciola@hotmail.com
//  akrzemi1@gmail.com
//
// You can file a GitHub issue  at:
//  https://github.com/boostorg/optional/issues


// This header provides definitions rof nonmember functions that still constitute
// the interface for class optional<>.

#ifndef BOOST_OPTIONAL_DETAIL_OPTIONAL_NONMEMBER_INTERFACE_01FEB2026_HPP
#define BOOST_OPTIONAL_DETAIL_OPTIONAL_NONMEMBER_INTERFACE_01FEB2026_HPP

namespace boost {


template <class T>
inline BOOST_CXX14_CONSTEXPR
optional<BOOST_OPTIONAL_DECAY(T)> make_optional ( T && v  )
{
  return optional<BOOST_OPTIONAL_DECAY(T)>(optional_detail::forward_<T>(v));
}

// Returns optional<T>(cond,v)
template <class T>
inline BOOST_CXX14_CONSTEXPR
optional<BOOST_OPTIONAL_DECAY(T)> make_optional ( bool cond, T && v )
{
  return optional<BOOST_OPTIONAL_DECAY(T)>(cond,optional_detail::forward_<T>(v));
}


// Returns a reference to the value if this is initialized, otherwise, the behaviour is UNDEFINED.
// No-throw
template <class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::reference_const_type
get ( optional<T> const& opt )
{
  return opt.get() ;
}

template <class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::reference_type
get ( optional<T>& opt )
{
  return opt.get() ;
}

// Returns a pointer to the value if this is initialized, otherwise, returns NULL.
// No-throw
template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::pointer_const_type
get ( optional<T> const* opt )
{
  return opt->get_ptr() ;
}

template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::pointer_type
get ( optional<T>* opt )
{
  return opt->get_ptr() ;
}

// Returns a reference to the value if this is initialized, otherwise, the behaviour is UNDEFINED.
// No-throw
template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::reference_const_type
get_optional_value_or ( optional<T> const& opt, BOOST_DEDUCED_TYPENAME optional<T>::reference_const_type v )
{
  return opt.get_value_or(v) ;
}

template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::reference_type
get_optional_value_or ( optional<T>& opt, BOOST_DEDUCED_TYPENAME optional<T>::reference_type v )
{
  return opt.get_value_or(v) ;
}

// Returns a pointer to the value if this is initialized, otherwise, returns NULL.
// No-throw
template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::pointer_const_type
get_pointer ( optional<T> const& opt )
{
  return opt.get_ptr() ;
}

template<class T>
inline BOOST_CXX14_CONSTEXPR
BOOST_DEDUCED_TYPENAME optional<T>::pointer_type
get_pointer ( optional<T>& opt )
{
  return opt.get_ptr() ;
}

} // namespace boost

#endif // BOOST_OPTIONAL_DETAIL_OPTIONAL_NONMEMBER_INTERFACE_01FEB2026_HPP
