//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_WRAPPED_ITERATOR_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_WRAPPED_ITERATOR_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/iterator.hpp>

namespace boost {
namespace container {

namespace detail_algo {

template<class Category, class Iterator>
struct wrapped_iter_category
{  typedef Category type; };

template<class Iterator>
struct wrapped_iter_category<void, Iterator>
{  typedef typename iterator_traits<Iterator>::iterator_category type; };

} // namespace detail_algo

//! Iterator adaptor that wraps an existing iterator and re-declares its
//! \c iterator_category to \c Category.  All underlying iterator operations
//! are forwarded; only the category seen by tag-dispatching algorithms changes.
//!
//! When \c Category is \c void, the wrapped iterator preserves the original
//! \c iterator_category from \c iterator_traits<Iterator>.
//!
//! \tparam Iterator  The type of the wrapped iterator.
//! \tparam Category  The iterator category tag declared by the wrapper
//!   (\c std::forward_iterator_tag, \c std::bidirectional_iterator_tag,
//!    \c std::random_access_iterator_tag, or \c void to keep the original).
template<class Iterator, class Category = void>
class wrapped_iterator
{
   Iterator m_it;

public:
   typedef typename iterator_traits<Iterator>::value_type      value_type;
   typedef typename iterator_traits<Iterator>::difference_type  difference_type;
   typedef typename iterator_traits<Iterator>::pointer          pointer;
   typedef typename iterator_traits<Iterator>::reference        reference;
   typedef typename detail_algo::wrapped_iter_category<Category, Iterator>::type iterator_category;

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator() BOOST_NOEXCEPT : m_it() {}
   BOOST_CONTAINER_FORCEINLINE explicit wrapped_iterator(Iterator it) BOOST_NOEXCEPT : m_it(it) {}

   BOOST_CONTAINER_FORCEINLINE operator Iterator () const BOOST_NOEXCEPT {  return m_it; }

   // Forward operations
   BOOST_CONTAINER_FORCEINLINE reference operator*()  const BOOST_NOEXCEPT {  return *m_it;    }
   BOOST_CONTAINER_FORCEINLINE pointer   operator->() const BOOST_NOEXCEPT {  return &(*m_it); }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator& operator++() BOOST_NOEXCEPT
   {  ++m_it; return *this; }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator operator++(int) BOOST_NOEXCEPT
   {  wrapped_iterator tmp(*this); ++m_it; return tmp; }

   // Bidirectional operations
   BOOST_CONTAINER_FORCEINLINE wrapped_iterator& operator--() BOOST_NOEXCEPT
   {  --m_it; return *this; }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator operator--(int) BOOST_NOEXCEPT
   {  wrapped_iterator tmp(*this); --m_it; return tmp; }

   // Random-access operations
   BOOST_CONTAINER_FORCEINLINE wrapped_iterator& operator+=(difference_type n) BOOST_NOEXCEPT
   {  m_it += n; return *this; }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator& operator-=(difference_type n) BOOST_NOEXCEPT
   {  m_it -= n; return *this; }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator operator+(difference_type n) const BOOST_NOEXCEPT
   {  wrapped_iterator tmp(*this); tmp += n; return tmp; }

   BOOST_CONTAINER_FORCEINLINE wrapped_iterator operator-(difference_type n) const BOOST_NOEXCEPT
   {  wrapped_iterator tmp(*this); tmp -= n; return tmp; }

   BOOST_CONTAINER_FORCEINLINE reference operator[](difference_type n) const BOOST_NOEXCEPT
   {  return m_it[n]; }

   BOOST_CONTAINER_FORCEINLINE friend wrapped_iterator operator+(difference_type n, const wrapped_iterator& x) BOOST_NOEXCEPT
   {  wrapped_iterator tmp(x); tmp += n; return tmp; }

   BOOST_CONTAINER_FORCEINLINE friend difference_type operator-(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return x.m_it - y.m_it; }

   // Comparisons
   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return x.m_it == y.m_it; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return x.m_it != y.m_it; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return x.m_it < y.m_it; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return y < x; }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<=(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return !(y < x); }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>=(const wrapped_iterator& x, const wrapped_iterator& y) BOOST_NOEXCEPT
   {  return !(x < y); }
};

//! Factory function: wraps \c it in a \c wrapped_iterator with the given
//! \c Category.  \c Iterator is deduced; \c Category must be specified
//! explicitly, e.g.
//! \code make_wrapped_iterator<std::forward_iterator_tag>(ra_iter) \endcode
template<class Category, class Iterator>
BOOST_CONTAINER_FORCEINLINE
wrapped_iterator<Iterator, Category>
   make_wrapped_iterator(Iterator it) BOOST_NOEXCEPT
{  return wrapped_iterator<Iterator, Category>(it); }

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_WRAPPED_ITERATOR_HPP
