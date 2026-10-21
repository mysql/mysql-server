//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_ALGORITHM_HPP
#define BOOST_CONTAINER_DETAIL_ALGORITHM_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/algorithm.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {
namespace container {

using boost::intrusive::algo_equal;
using boost::intrusive::algo_lexicographical_compare;

template<class Func>
class binder1st
{
   public:
	typedef typename Func::second_argument_type  argument_type;
	typedef typename Func::result_type           result_type;

	binder1st(const Func& func, const typename Func::first_argument_type& arg)
   	: op(func), value(arg)
	{}

	result_type operator()(const argument_type& arg) const
   {	return op(value, arg);  }

	result_type operator()(argument_type& arg) const
   {  return op(value, arg);   }

   private:
	Func op;
	typename Func::first_argument_type value;
};

template<class Func, class T> 
inline binder1st<Func> bind1st(const Func& func, const T& arg)
{	return boost::container::binder1st<Func>(func, arg);  }

template<class Func>
class binder2nd
{
   public:
	typedef typename Func::first_argument_type   argument_type;
	typedef typename Func::result_type           result_type;

	binder2nd(const Func& func, const typename Func::second_argument_type& arg)
	   : op(func), value(arg)
   {}

	result_type operator()(const argument_type& arg) const
   {  return op(arg, value);  }

	result_type operator()(argument_type& arg) const
	{  return op(arg, value);  }

   private:
	Func op;
	typename Func::second_argument_type value;
};

template<class Func, class T>
inline binder2nd<Func> bind2nd(const Func& func, const T& arg)
{
   return (boost::container::binder2nd<Func>(func, arg));
}

template<class Func>
class unary_negate
{
   public:
   typedef typename Func::argument_type   argument_type;
   typedef typename Func::result_type     result_type;

	explicit unary_negate(const Func& func)
		: m_func(func)
   {}

	bool operator()(const typename Func::argument_type& arg) const
	{  return !m_func(arg);  }

   private:
	Func m_func;
};

template<class Func> inline
unary_negate<Func> not1(const Func& func)
{
   return boost::container::unary_negate<Func>(func);
}

template<class InputIt, class UnaryPredicate>
InputIt find_if(InputIt first, InputIt last, UnaryPredicate p)
{
   for (; first != last; ++first) {
      if (p(*first)) {
         return first;
      }
   }
   return last;
}

template<class ForwardIt1, class ForwardIt2, class BinaryPredicate>
  ForwardIt1 find_end (ForwardIt1 first1, ForwardIt1 last1
                      ,ForwardIt2 first2, ForwardIt2 last2
                      ,BinaryPredicate p)
{
   if (first2==last2)
      return last1;  // specified in C++11

   ForwardIt1 ret = last1;

   while (first1!=last1)
   {
      ForwardIt1 it1 = first1;
      ForwardIt2 it2 = first2;
      while ( p(*it1, *it2) ) {
         ++it1; ++it2;
         if (it2==last2) {
            ret=first1;
            break;
         }
         if (it1==last1)
         return ret;
      }
      ++first1;
   }
   return ret;
}

template<class InputIt, class ForwardIt, class BinaryPredicate>
InputIt find_first_of(InputIt first1, InputIt last1, ForwardIt first2, ForwardIt last2, BinaryPredicate p)
{
   for (; first1 != last1; ++first1) {
      for (ForwardIt it = first2; it != last2; ++it) {
         if (p(*first1, *it)) {
            return first1;
         }
      }
   }
   return last1;
}

template<class ForwardIt1, class ForwardIt2, class BinaryPredicate>
ForwardIt1 search(ForwardIt1 first1, ForwardIt1 last1,
                        ForwardIt2 first2, ForwardIt2 last2, BinaryPredicate p)
{
   for (; ; ++first1) {
      ForwardIt1 it = first1;
      for (ForwardIt2 it2 = first2; ; ++it, ++it2) {
         if (it2 == last2) {
            return first1;
         }
         if (it == last1) {
            return last1;
         }
         if (!p(*it, *it2)) {
            break;
         }
      }
   }
}

template<class InpIt, class U>
InpIt find(InpIt first, InpIt last, const U& value)
{
    for (; first != last; ++first)
        if (*first == value)
            return first;
 
    return last;
}


template<class FwdIt, class U>
FwdIt remove(FwdIt first, FwdIt last, const U& value)
{
    first = (find)(first, last, value);
    if (first != last)
        for (FwdIt i = first; ++i != last;)
            if (!(*i == value))
                *first++ = boost::move(*i);
    return first;
}

template<class FwdIt, class Pred>
FwdIt remove_if(FwdIt first, FwdIt last, Pred p)
{
    first = (find_if)(first, last, p);
    if (first != last)
        for (FwdIt i = first; ++i != last;)
            if (!p(*i))
                *first++ = boost::move(*i);
    return first;
}

template <class Cont, class Pred>
typename Cont::size_type container_erase_if(Cont& c, Pred p)
{
   typedef typename Cont::size_type size_type;
   typedef typename Cont::iterator  it_t;

   size_type prev_size = c.size();
   it_t it         = c.begin();

   //end() must be called each loop for non-node containers
   while ( it != c.end() ) {
      if (p(*it)) {
         it = c.erase(it);
      }
      else {
         ++it;
      }
   }

   return prev_size - c.size();
}

}  //namespace container {
}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_DETAIL_ALGORITHM_HPP
