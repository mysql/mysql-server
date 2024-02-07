/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBT_STATS_HPP
#define NDBT_STATS_HPP

#include <ndb_global.h>

class NDBT_Stats {
 public:
  NDBT_Stats() { reset(); }

  void reset() {
    sum = sum2 = 0.0;
    max = DBL_MIN;
    ;
    min = DBL_MAX;
    n = 0;
  }

  void addObservation(double t) {
    sum += t;
    sum2 += (t * t);
    n++;
    if (min > t) min = t;
    if (max < t) max = t;
  }

  void addObservation(Uint64 t) { addObservation(double(t)); }

  double getMean() const { return sum / n; }
  double getStddev() const { return sqrt(getVariance()); }
  double getVariance() const { return (n * sum2 - (sum * sum)) / (n * n); }
  double getMin() const { return min; }
  double getMax() const { return max; }
  int getCount() const { return n; }

  NDBT_Stats &operator+=(const NDBT_Stats &c) {
    sum += c.sum;
    sum2 += c.sum2;
    n += c.n;
    if (min > c.min) min = c.min;
    if (max < c.max) max = c.max;
    return *this;
  }

 private:
  double sum;
  double sum2;
  int n;
  double min, max;
};

#endif
