/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Joaquin M Lopez Munoz  2006-2013
// (C) Copyright Ion Gaztanaga          2014-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP
#define BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/workaround.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {
namespace intrusive {
namespace detail {

template<typename T, typename Tag = void>
class ebo_functor_holder
   :  public T
{
   BOOST_COPYABLE_AND_MOVABLE(ebo_functor_holder)

   public:
   typedef T functor_type;

   inline ebo_functor_holder()
      : T()
   {}

   inline explicit ebo_functor_holder(const T &t)
      : T(t)
   {}

   inline explicit ebo_functor_holder(BOOST_RV_REF(T) t)
      : T(::boost::move(t))
   {}

   template<class Arg1, class Arg2>
   inline ebo_functor_holder(BOOST_FWD_REF(Arg1) arg1, BOOST_FWD_REF(Arg2) arg2)
      : T(::boost::forward<Arg1>(arg1), ::boost::forward<Arg2>(arg2))
   {}

   inline ebo_functor_holder(const ebo_functor_holder &x)
      : T(static_cast<const T&>(x))
   {}

   inline ebo_functor_holder(BOOST_RV_REF(ebo_functor_holder) x)
      : T(BOOST_MOVE_BASE(T, x))
   {}

   inline ebo_functor_holder& operator=(BOOST_COPY_ASSIGN_REF(ebo_functor_holder) x)
   {
      const ebo_functor_holder&r = x;
      this->get() = r;
      return *this;
   }

   inline ebo_functor_holder& operator=(BOOST_RV_REF(ebo_functor_holder) x)
   {
      this->get() = ::boost::move(x.get());
      return *this;
   }

   inline ebo_functor_holder& operator=(const T &t)
   {
      this->get() = t;
      return *this;
   }

   inline ebo_functor_holder& operator=(BOOST_RV_REF(T) t)
   {
      this->get() = ::boost::move(t);
      return *this;
   }

   inline T&       get(){return *this;}
   inline const T& get()const{return *this;}
};

template<typename T, typename Tag>
class ebo_functor_holder<T *, Tag>
{
   BOOST_COPYABLE_AND_MOVABLE(ebo_functor_holder)

   public:
   typedef T functor_type;

   inline ebo_functor_holder()
      : t_()
   {}

   inline explicit ebo_functor_holder(T * t)
      : t_(t)
   {}

   inline ebo_functor_holder(const ebo_functor_holder &x)
      : t_(x.t_)
   {}

   inline ebo_functor_holder(BOOST_RV_REF(ebo_functor_holder) x)
      : t_(x.t_)
   {}

   inline ebo_functor_holder& operator=(BOOST_COPY_ASSIGN_REF(ebo_functor_holder) x)
   {
      this->t_ = x.t_;
      return *this;
   }

   inline ebo_functor_holder& operator=(BOOST_RV_REF(ebo_functor_holder) x)
   {
      this->t_ = ::boost::move(x.t_);
      return *this;
   }

   inline ebo_functor_holder& operator=(T * t)
   {
      this->t_ = t;
      return *this;
   }

   inline T&       get(){return *t_;}

   private:
   T * t_;
};

}  //namespace detail {
}  //namespace intrusive {
}  //namespace boost {

#endif   //#ifndef BOOST_INTRUSIVE_DETAIL_EBO_HOLDER_HPP
