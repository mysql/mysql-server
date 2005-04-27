/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "../Interval.hpp"

#define TEST_REQUIRE(X);  if (!(X)) { \
  ndbout_c("Test failed in line %d", __LINE__); testPassed = false; }


int
main () {
  bool testPassed = true;

  Interval a, b, c;

  /**
   *  isEmpty
   */
  TEST_REQUIRE(a.isEmpty());

  a.set(3,1);
  TEST_REQUIRE(a.isEmpty());

  /**
   *  isEqual
   */
  a.set(1,2);
  TEST_REQUIRE(a.isEqual(1,2));

  a.set(3,1);
  TEST_REQUIRE(a.isEqual(1,0));  // The result should be normalized

  /**
   *  intervalAdd -- non-disjoint 
   */
  a.set(1,3);
  b.set(3,10);
  TEST_REQUIRE(intervalAdd(a, b, &c));
  TEST_REQUIRE(c.isEqual(1,10));

  a.set(3,10);
  b.set(1,3);
  TEST_REQUIRE(intervalAdd(a, b, &c));
  TEST_REQUIRE(c.isEqual(1,10));

  /**
   *  intervalAdd -- consequtive
   */
  a.set(1,3);
  b.set(4,10);
  TEST_REQUIRE(intervalAdd(a, b, &c));
  TEST_REQUIRE(c.isEqual(1,10));

  a.set(4,10);
  b.set(1,3);
  TEST_REQUIRE(intervalAdd(a, b, &c));
  TEST_REQUIRE(c.isEqual(1,10));

  /**
   *  intervalAdd -- disjoint
   */
  a.set(1,3);
  b.set(5,10);
  c.set(4711,4711);
  TEST_REQUIRE(!intervalAdd(a, b, &c));  // This should not work
  TEST_REQUIRE(c.isEqual(4711,4711));

  a.set(5,10);
  b.set(1,3);
  c.set(4711,4711);
  TEST_REQUIRE(!intervalAdd(a, b, &c));  // This should not work
  TEST_REQUIRE(c.isEqual(4711,4711));

  /**
   *  intervalLeftMinus -- non-disjoint
   */
  a.set(1,3);
  b.set(5,10);
  intervalLeftMinus(a, b, &c);          
  TEST_REQUIRE(c.isEmpty());

  a.set(5,10);
  b.set(1,3);
  intervalLeftMinus(a, b, &c);          
  TEST_REQUIRE(c.isEqual(5,10));

  /**
   *  intervalLeftMinus -- consequtive
   */
  a.set(1,3);
  b.set(4,10);
  intervalLeftMinus(a, b, &c);
  TEST_REQUIRE(c.isEmpty());

  a.set(4,10);
  b.set(1,3);
  intervalLeftMinus(a, b, &c);
  TEST_REQUIRE(c.isEqual(4,10));

  /**
   *  intervalLeftMinus -- disjoint
   */
  a.set(1,3);
  b.set(5,10);
  intervalLeftMinus(a, b, &c);
  TEST_REQUIRE(c.isEmpty());

  a.set(5,10);
  b.set(1,3);
  intervalLeftMinus(a, b, &c);
  TEST_REQUIRE(c.isEqual(5,10));
  
  ndbout << "Test " << (testPassed ? "passed" : "failed") << "." << endl;
}
