//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_IS_CONSTRUCTIBLE_HPP
#define BOOST_CONTAINER_DETAIL_IS_CONSTRUCTIBLE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <cstddef> // for size_t
#include <boost/move/utility_core.hpp> //declval

namespace boost_container_is_constructible {

   typedef char yes_type;
   struct no_type { char padding[8]; };

   template<bool Cond, typename T = void> struct enable_if {};
   template<typename T> struct enable_if<true, T> { typedef T type; };
   template<typename E, typename T> struct t_enable { typedef T type; };

}  //namespace boost_container_is_constructible {

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_DECLTYPE)

   namespace boost {
   namespace container {

   template <typename T, typename ...Args>
   struct is_constructible
   {
      private:
      typedef char yes_type;
      struct no_type { char padding[8]; };

      template <typename U>
      static decltype( new U(boost::move_detail::declval<Args>()...), yes_type() ) test(int);
         
      template <typename U>
      static no_type test(...);
         
      public:
      static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);
   };

   }  //namespace container {
   }  //namespace boost {

#else //Use macro expansion

   #include <boost/move/detail/fwd_macros.hpp>

   #if defined(BOOST_NO_CXX11_DECLTYPE)

      namespace boost_container_is_constructible {

      //       Example for N == 2
      //
      //template <typename T, typename P0, typename P1>
      //struct is_constructible2
      //{
      //    private:
      //    template<typename U>
      //    static char test( typename enable_if< (sizeof( new U(boost::move_detail::declval<P0>(), boost::move_detail::declval<P1>()))!= 0), int>::type );
      //
      //    template <typename U>
      //    static no_type test(...);
      //
      //    public:
      //    static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);
      //};
      #define BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL(N) \
      template <typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
      struct BOOST_MOVE_CAT(is_constructible_impl, N)\
      {\
         private:\
         template <typename U>\
         static yes_type test( typename enable_if< (sizeof( new U(BOOST_MOVE_DECLVAL##N))!= 0), int>::type );\
         \
         template <typename U>\
         static no_type test(...);\
         \
         public:\
         static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);\
      };\
      //
      BOOST_MOVE_ITERATE_0TO9(BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL)
      #undef BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL

      }//   namespace boost_container_is_constructible {

   #else

      namespace boost_container_is_constructible {

      //       Example for N == 2
      //
      //template <typename T, typename P0, typename P1>
      //struct is_constructible2
      //{
      //    private:
      //    template<typename U, typename E = decltype( new U(boost::move_detail::declval<P0>(), boost::move_detail::declval<P1>()) ) >
      //    static char test( int );
      //
      //    template <typename U>
      //    static no_type test(...);
      //
      //    public:
      //    static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);
      //};

      #define BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL(N) \
      template <typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
      struct BOOST_MOVE_CAT(is_constructible_impl, N)\
      {\
         private:\
         template <typename U>\
         static decltype( new U(BOOST_MOVE_DECLVAL##N), yes_type() ) test(int);\
         \
         template <typename U>\
         static no_type test(...);\
         \
         public:\
         static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);\
      };\
      //
      BOOST_MOVE_ITERATE_0TO9(BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL)
      #undef BOOST_INTRUSIVE_IS_CONSTRUCTIBLE_IMPL

      }//   namespace boost_container_is_constructible {

   #endif

      namespace boost {
      namespace container {

      template<class T, BOOST_MOVE_CLASSDFLT9, class = void>
      struct is_constructible;

      //       Example for N == 2
      //
      //template <typename T, typename P0, typename P1>
      //struct is_constructible
      //  <T, P0, P1>
      //    : boost_container_is_constructible::is_constructible_impl2<T, P0, P1>
      //{};

      #define BOOST_INTRUSIVE_IS_CONSTRUCTIBLE(N) \
         template <typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
         struct is_constructible\
            <T BOOST_MOVE_I##N BOOST_MOVE_TARG##N>\
            : boost_container_is_constructible::is_constructible_impl##N<T BOOST_MOVE_I##N BOOST_MOVE_TARG##N>\
         {};\
         //
      BOOST_MOVE_ITERATE_0TO9(BOOST_INTRUSIVE_IS_CONSTRUCTIBLE)
      #undef BOOST_INTRUSIVE_IS_CONSTRUCTIBLE

      }  //namespace container {
      }  //namespace boost {

#endif

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_DETAIL_IS_CONSTRUCTIBLE_HPP
