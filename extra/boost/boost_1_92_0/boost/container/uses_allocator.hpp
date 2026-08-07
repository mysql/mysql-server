//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_USES_ALLOCATOR_HPP
#define BOOST_CONTAINER_USES_ALLOCATOR_HPP

#include <boost/container/uses_allocator_fwd.hpp>
#include <boost/container/detail/type_traits.hpp>

namespace boost {
namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

namespace dtl {

template<typename T, typename AllocArg>
struct uses_allocator_imp
{
   // Use SFINAE (Substitution Failure Is Not An Error) to detect the
   // presence of an 'allocator_type' nested type convertilble from AllocArg.
   private:
   typedef char yes_type;
   struct no_type{ char dummy[2]; };

   // Match this function if T::allocator_type exists and is
   // implicitly convertible from `AllocArg`
   template <class U>
   static yes_type test(typename U::allocator_type);

   // Match this function if T::allocator_type exists and its type is `erased_type`.
   template <class U, class V>
   static typename dtl::enable_if
      < dtl::is_same<typename U::allocator_type, erased_type>
      , yes_type
      >::type  test(const V&);

   // Match this function if TypeT::allocator_type does not exist or is
   // not convertible from `AllocArg`.
   template <typename U>
   static no_type test(...);
   static AllocArg alloc;  // Declared but not defined

   public:
   BOOST_STATIC_CONSTEXPR bool value = sizeof(test<T>(alloc)) == sizeof(yes_type);
};

}  //namespace dtl {

template <class T>
struct constructible_with_allocator_prefix
{  BOOST_STATIC_CONSTEXPR bool value = false; };

template <class T>
struct constructible_with_allocator_suffix
{  BOOST_STATIC_CONSTEXPR bool value = false; };

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//! <b>Remark</b>: Automatically detects whether `T` has a nested `allocator_type` that is convertible from
//! `AllocArg`. Meets the BinaryTypeTrait requirements ([meta.rqmts] 20.4.1). This trait is used to signal if
//! type `T` supports <b>uses-allocator construction</b>.
//! 
//! <b>uses-allocator construction</b> protocol specifies three conventions of passing an allocator argument
//! `alloc_arg` of type `AllocArg` to a constructor of some type T in addition to an arbitrary number of arguments
//! (specified as `args...` in this explanation):
//! 
//! * If `T` does not use a compatible allocator (`uses_allocator<T, AllocArg>::value` is false), then `alloc_arg` is ignored
//!   and `T` is constructed as `T(args...)`
//! 
//! * Otherwise, if `uses_allocator<T, AllocArg>::value` is true and `T` is constructible as `T(std::allocator_arg, alloc_arg, args...)`
//!   then uses-allocator construction uses this form.
//! 
//! * Otherwise, if `T` is constructible as `T(args..., alloc_arg)`, then uses-allocator construction uses this form.
//! 
//! * Otherwise, as an non-standard extension provided by Boost.Container, `alloc_arg` is ignored and `T(args...)` is called. 
//!   (Note that this extension is provided to enhance backwards compatibility for types created without uses-allocator 
//!   construction in mind that declare an `allocator_type` type but are not prepared to be built with an additional allocator argument)
//!
//! <b>Result</b>: `uses_allocator<T, AllocArg>::value == true` if a type `T::allocator_type`
//! exists and either `is_convertible<AllocArg, T::allocator_type>::value == true` or `T::allocator_type`
//! is an alias of `erased_type`. False otherwise.
//!
//! <b>Note</b>: A program may specialize this type to define `uses_allocator<X>::value` as true for a `T` of user-defined type
//! if `T` does not have a nested `allocator_type` but is nonetheless constructible using the specified `AllocArg`
//! where either: the first argument of a constructor has type `allocator_arg_t` and the second argument has type `AllocArg`
//! or the last argument of a constructor has type `AllocArg`.

template <typename T, typename AllocArg>
struct uses_allocator
#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   : dtl::uses_allocator_imp<T, AllocArg>
#endif   //BOOST_CONTAINER_DOXYGEN_INVOKED
{};

#if !defined(BOOST_NO_CXX14_VARIABLE_TEMPLATES)

template< class T, class AllocArg >
BOOST_CONSTEXPR bool uses_allocator_v = uses_allocator<T, AllocArg>::value;

#endif   //BOOST_NO_CXX14_VARIABLE_TEMPLATES

}} //namespace boost::container

#endif   //BOOST_CONTAINER_USES_ALLOCATOR_HPP
