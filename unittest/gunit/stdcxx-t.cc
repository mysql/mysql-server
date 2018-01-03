/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include "sql/cpp11_lib_check.h"

TEST(STDfeatures, HashMap)
{
  std::unordered_map<int, int> intmap;
  for (int ix= 0; ix < 10; ++ix)
  {
    intmap[ix]= ix * ix;
  }
  int t= intmap[0];
  EXPECT_EQ(0, t);
  EXPECT_TRUE(0 == intmap.count(42));
  EXPECT_TRUE(intmap.end() == intmap.find(42));
}


TEST(STDfeatures, TwoHashMaps)
{
  std::unordered_map<int, int> intmap1;
  std::unordered_map<int, int> intmap2;
  intmap1[0]= 42;
  intmap2[0]= 666;
#if defined(_WIN32)
  // On windows we get a runtime error: list iterators incompatible
#else
  EXPECT_TRUE(intmap1.end() == intmap2.end());
#endif
}


TEST(STDfeatures, Regex)
{
  EXPECT_FALSE(cpp11_re_match("foo", "bar"));
  EXPECT_FALSE(cpp11_re_match("foo", "foobar"));
  EXPECT_TRUE(cpp11_re_match("foo.*", "foobar"));
  EXPECT_TRUE(cpp11_re_match("foo|bar", "bar"));
}

static std::atomic<int> intvar { 0 };

void add_1000()
{
  for (int i= 0; i < 1000; i++)
    intvar++;
}

TEST(STDfeatures, Thread)
{
  std::thread t0 { add_1000 };
  std::thread t1 { add_1000 };
  std::thread t2 { add_1000 };
  std::thread t3 { add_1000 };
  std::thread t4 { add_1000 };
  std::thread t5 { add_1000 };
  std::thread t6 { add_1000 };
  std::thread t7 { add_1000 };
  std::thread t8 { add_1000 };
  std::thread t9 { add_1000 };
  t0.join();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
  t7.join();
  t8.join();
  t9.join();
  EXPECT_EQ(10000, intvar);
}
