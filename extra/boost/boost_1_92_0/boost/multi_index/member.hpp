/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_MEMBER_HPP
#define BOOST_MULTI_INDEX_MEMBER_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/core/enable_if.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <cstddef>

namespace boost{

template<class T> class reference_wrapper; /* fwd decl. */

namespace multi_index{

namespace detail{

/* member is a read/write key extractor for accessing a given
 * member of a class.
 * Additionally, member is overloaded to support referece_wrappers
 * of T and "chained pointers" to T's. By chained pointer to T we mean
 * a type P  such that, given a p of Type P
 *   *...n...*x is convertible to T&, for some n>=1.
 * Examples of chained pointers are raw and smart pointers, iterators and
 * arbitrary combinations of these (vg. T** or unique_ptr<T*>.)
 */

template<class Class,typename Type,Type Class::*PtrToMember>
struct const_member_base
{
  typedef Type result_type;

  template<typename ChainedPtr>

  typename disable_if<
    is_convertible<const ChainedPtr&,const Class&>,Type&>::type
  operator()(const ChainedPtr& x)const
  {
    return operator()(*x);
  }

  Type& operator()(const Class& x)const
  {
    return x.*PtrToMember;
  }

  Type& operator()(const reference_wrapper<const Class>& x)const
  {
    return operator()(x.get());
  }

  Type& operator()(const reference_wrapper<Class>& x)const
  { 
    return operator()(x.get());
  }
};

template<class Class,typename Type,Type Class::*PtrToMember>
struct non_const_member_base
{
  typedef Type result_type;

  template<typename ChainedPtr>

  typename disable_if<
    is_convertible<const ChainedPtr&,const Class&>,Type&>::type
  operator()(const ChainedPtr& x)const
  {
    return operator()(*x);
  }

  const Type& operator()(const Class& x)const
  {
    return x.*PtrToMember;
  }

  Type& operator()(Class& x)const
  { 
    return x.*PtrToMember;
  }

  const Type& operator()(const reference_wrapper<const Class>& x)const
  {
    return operator()(x.get());
  }

  Type& operator()(const reference_wrapper<Class>& x)const
  { 
    return operator()(x.get());
  }
};

} /* namespace multi_index::detail */

template<class Class,typename Type,Type Class::*PtrToMember>
struct member:
  mp11::mp_if<
    is_const<Type>,
    detail::const_member_base<Class,Type,PtrToMember>,
    detail::non_const_member_base<Class,Type,PtrToMember>
  >
{
};

namespace detail{

/* MSVC++ 6.0 does not support properly pointers to members as
 * non-type template arguments, as reported in
 *   http://support.microsoft.com/default.aspx?scid=kb;EN-US;249045
 * A similar problem (though not identical) is shown by MSVC++ 7.0.
 * We provide an alternative to member<> accepting offsets instead
 * of pointers to members. This happens to work even for non-POD
 * types (although the standard forbids use of offsetof on these),
 * so it serves as a workaround in this compiler for all practical
 * purposes.
 * Surprisingly enough, other compilers, like Intel C++ 7.0/7.1 and
 * Visual Age 6.0, have similar bugs. This replacement of member<>
 * can be used for them too.
 *
 * Support for such old compilers is dropped and
 * [non_]const_member_offset_base is deprecated.
 */

template<class Class,typename Type,std::size_t OffsetOfMember>
struct const_member_offset_base
{
  typedef Type result_type;

  template<typename ChainedPtr>

  typename disable_if<
    is_convertible<const ChainedPtr&,const Class&>,Type&>::type
  operator()(const ChainedPtr& x)const
  {
    return operator()(*x);
  }

  Type& operator()(const Class& x)const
  {
    return *static_cast<const Type*>(
      static_cast<const void*>(
        static_cast<const char*>(
          static_cast<const void *>(&x))+OffsetOfMember));
  }

  Type& operator()(const reference_wrapper<const Class>& x)const
  {
    return operator()(x.get());
  }

  Type& operator()(const reference_wrapper<Class>& x)const
  {
    return operator()(x.get());
  }
};

template<class Class,typename Type,std::size_t OffsetOfMember>
struct non_const_member_offset_base
{
  typedef Type result_type;

  template<typename ChainedPtr>

  typename disable_if<
    is_convertible<const ChainedPtr&,const Class&>,Type&>::type
  operator()(const ChainedPtr& x)const
  {
    return operator()(*x);
  }

  const Type& operator()(const Class& x)const
  {
    return *static_cast<const Type*>(
      static_cast<const void*>(
        static_cast<const char*>(
          static_cast<const void *>(&x))+OffsetOfMember));
  }

  Type& operator()(Class& x)const
  { 
    return *static_cast<Type*>(
      static_cast<void*>(
        static_cast<char*>(static_cast<void *>(&x))+OffsetOfMember));
  }

  const Type& operator()(const reference_wrapper<const Class>& x)const
  {
    return operator()(x.get());
  }

  Type& operator()(const reference_wrapper<Class>& x)const
  {
    return operator()(x.get());
  }
};

} /* namespace multi_index::detail */

template<class Class,typename Type,std::size_t OffsetOfMember>
struct member_offset:
  mp11::mp_if<
    is_const<Type>,
    detail::const_member_offset_base<Class,Type,OffsetOfMember>,
    detail::non_const_member_offset_base<Class,Type,OffsetOfMember>
  >
{
};

/* BOOST_MULTI_INDEX_MEMBER used to resolve to member_offset in those defective
 * compilers for which BOOST_NO_POINTER_TO_MEMBER_TEMPLATE_PARAMETERS is
 * defined, none of which is supported any longer.
 */

#define BOOST_MULTI_INDEX_MEMBER(Class,Type,MemberName) \
::boost::multi_index::member< Class,Type,&Class::MemberName >

} /* namespace multi_index */

} /* namespace boost */

#endif
