/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <stat_utils.hpp>

/**
 * Test stability of the the moving average implemented in class NdbStatistics.
 * Sample a generated sequence of [100,101,99,101,99,101, ...].
 * Expectation is that the calculated mean value should be in the
 * range [99..101], and eventually stabilize around ~100.
 * stdDev should be ~1.0 for this number series.
 */
int main(int argc, char** argv)
{
  NdbStatistics stats;
  bool pass = true;
  const float delta = 0.1;

  // prime it with an initial value
  stats.update(100);

  for (int i = 0; i < 100; i++)
  {
    double sample = 99 + 2*(i % 2);
    stats.update (sample);

    printf("i: %d, sample:%f, mean:%f, stdDev:%f\n",
           i, sample, stats.getMean(), stats.getStdDev());
    // Expect 'mean' to be in range [99..101}
    if (stats.getMean() <= 99 || stats.getMean() >= 101)
      pass = false;

    // Expect stdDev ~1, allow a small 'delta'
    if (stats.getStdDev()  > 1.0 + delta)
      pass = false;
  }

  // Expect 'mean' to stabilize around ~100.
  if (stats.getMean() <= 100-delta || stats.getMean() >= 100+delta)
    pass = false;

  if (pass)
    printf("Test of 'class NdbStatistics' passed\n");
  else
    printf("Test of 'class NdbStatistics' FAILED\n");
}
