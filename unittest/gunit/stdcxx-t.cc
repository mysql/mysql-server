/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#if defined(_LIBCPP_VERSION)
#include <unordered_map>
#elif defined(__GNUC__) && __GNUC__ > 3
#include <tr1/unordered_map>
#elif defined(__WIN__)
#include <hash_map>
#elif  defined(__SUNPRO_CC)
#include <hash_map>
#else 
#error "Don't know how to implement hash_map"
#endif


template<typename K, typename T>
struct MyHashMap
{
#if defined(_LIBCPP_VERSION)
  typedef std::unordered_map<K, T> Type;
#elif defined(__GNUC__) && __GNUC__ > 3
  typedef std::tr1::unordered_map<K, T> Type;
#elif defined(__WIN__)
  typedef stdext::hash_map<K, T> Type;
#elif defined(__SUNPRO_CC)
  typedef std::hash_map<K, T> Type;
#endif
};


TEST(STDfeatures, HashMap)
{
  MyHashMap<int, int>::Type intmap;
  for (int ix= 0; ix < 10; ++ix)
  {
    intmap[ix]= ix * ix;
  }
  int t= intmap[0];
  EXPECT_EQ(0, t);
  EXPECT_TRUE(0 == intmap.count(42));
  EXPECT_TRUE(intmap.end() == intmap.find(42));
}
