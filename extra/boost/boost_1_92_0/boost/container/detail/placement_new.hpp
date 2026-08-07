#ifndef BOOST_CONTAINER_DETAIL_PLACEMENT_NEW_HPP
#define BOOST_CONTAINER_DETAIL_PLACEMENT_NEW_HPP
///////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
///////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <boost/container/detail/workaround.hpp>   //BOOST_CONTAINER_RETURNS_NONNULL

struct boost_container_new_t{};

//returns_nonnull lets the compiler drop the null check the new-expression
//otherwise performs on the result of this custom placement operator new;
//nonnull(2) tells it the passed pointer 'p' is never null either.
BOOST_CONTAINER_RETURNS_NONNULL BOOST_CONTAINER_NONNULL(2)
BOOST_CONTAINER_FORCEINLINE void *operator new(std::size_t, void *p, boost_container_new_t)
{  return p;  }

BOOST_CONTAINER_FORCEINLINE  void operator delete(void *, void *, boost_container_new_t)
{}

struct boost_container_init_life_t{};

//p can be null
BOOST_CONTAINER_FORCEINLINE  void *operator new(std::size_t, void *p, boost_container_init_life_t)
{  return p;  }

BOOST_CONTAINER_FORCEINLINE  void operator delete(void *, void *, boost_container_init_life_t)
{}

#endif   //BOOST_CONTAINER_DETAIL_PLACEMENT_NEW_HPP
