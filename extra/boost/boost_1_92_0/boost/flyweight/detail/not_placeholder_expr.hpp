/* Copyright 2006-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#ifndef BOOST_FLYWEIGHT_DETAIL_NOT_PLACEHOLDER_EXPR_HPP
#define BOOST_FLYWEIGHT_DETAIL_NOT_PLACEHOLDER_EXPR_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mpl/aux_/lambda_arity_param.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/lambda_fwd.hpp>

/* BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION can be inserted at the end
 * of a class template parameter declaration:
 *   template<
 *     typename X0,...,typename Xn
 *     BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION  
 *   >
 *   struct foo...
 * to prevent instantiations from being treated as MPL placeholder
 * expressions in the presence of placeholder arguments; this is useful
 * to avoid masking of a metafunction class nested ::apply during
 * MPL invocation.
 */

namespace boost{

namespace flyweights{

namespace detail{

struct not_a_ph_expr;

} /* namespace flyweights::detail */

} /* namespace flyweights */

} /* namespace boost */

#define BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG \
(boost::flyweights::detail::not_a_ph_expr*)0
#define BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION     \
,boost::flyweights::detail::not_a_ph_expr* =             \
  BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
#define BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF \
,boost::flyweights::detail::not_a_ph_expr*

/* Even though, under the definition given by Boost.MPL, the inclusion of a
 * non-type template parameter makes a class template instantiation not a
 * placeholder expression, https://wg21.link/p0522r0, which allows
 * template-parameters to bind ignoring default arguments, causes
 * boost::mpl::lambda to fail to properly honor
 * BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION (in P0552R0-compliant
 * compilers). We fix this by specializing boost::mpl::lambda accordingly.
 */

namespace boost{

namespace mpl{

template<
  template<
    typename
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
  > class F,
  typename T1,
  typename Tag
>
struct lambda<
  F<T1,BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG>,
  Tag
  BOOST_MPL_AUX_LAMBDA_ARITY_PARAM(int_<1>)
>
{
  typedef false_                                     is_le;
  typedef F<
    T1,
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
  >                                                  result_;
  typedef result_                                    type;
};

template<
  template<
    typename,typename
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
  > class F,
  typename T1,typename T2,
  typename Tag
>
struct lambda<
  F<T1,T2,BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG>,
  Tag
  BOOST_MPL_AUX_LAMBDA_ARITY_PARAM(int_<2>)
>
{
  typedef false_                                     is_le;
  typedef F<
    T1,T2,
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
  >                                                  result_;
  typedef result_                                    type;
};

template<
  template<
    typename,typename,typename
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
  > class F,
  typename T1,typename T2,typename T3,
  typename Tag
>
struct lambda<
  F<T1,T2,T3,BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG>,
  Tag
  BOOST_MPL_AUX_LAMBDA_ARITY_PARAM(int_<3>)
>
{
  typedef false_                                     is_le;
  typedef F<
    T1,T2,T3,
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
  >                                                  result_;
  typedef result_                                    type;
};

template<
  template<
    typename,typename,typename,typename
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
  > class F,
  typename T1,typename T2,typename T3,typename T4,
  typename Tag
>
struct lambda<
  F<T1,T2,T3,T4,BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG>,
  Tag
  BOOST_MPL_AUX_LAMBDA_ARITY_PARAM(int_<4>)
>
{
  typedef false_                                     is_le;
  typedef F<
    T1,T2,T3,T4,
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
  >                                                  result_;
  typedef result_                                    type;
};

template<
  template<
    typename,typename,typename,typename,typename
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
  > class F,
  typename T1,typename T2,typename T3,typename T4,typename T5,
  typename Tag
>
struct lambda<
  F<T1,T2,T3,T4,T5,BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG>,
  Tag
  BOOST_MPL_AUX_LAMBDA_ARITY_PARAM(int_<5>)
>
{
  typedef false_                                     is_le;
  typedef F<
    T1,T2,T3,T4,T5,
    BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_ARG
  >                                                  result_;
  typedef result_                                    type;
};

} /* namespace mpl */

} /* namespace boost */

#endif
