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

#ifndef INTERVAL_HPP
#define INTERVAL_HPP

#include <NdbOut.hpp>
#include <ndb_types.h>

/**
 * @class Interval
 * @brief Represents an interval
 */
class Interval {
public:
  Interval();
  Interval(Uint32, Uint32);

  /**
   *  Getters of first and last
   */
  inline Uint32 first() const { return m_first; }
  inline Uint32 last() const { return m_last; }

  /** 
   *  Check if interval is empty
   */
  bool isEmpty() const;
  bool isEqual(Uint32 a, Uint32 b) const;
  bool inInterval(Uint32 a) const;

  /**
   *  Size of interval
   */
  Uint32 size() const { 
    return (!isEmpty()) ? m_last - m_first + 1 : 0; 
  }
  
  /**
   *  Set interval
   */
  void set(Uint32 first, Uint32 last);
  void set(const Interval i);

  void setFirst(Uint32 first);
  void setLast(Uint32 last);

  /**
   *  Reduce the interval to only the n left elements of the 
   *  interval.  If the interval is shorter than n, then 
   *  interval is not changed.
   */
  void onlyLeft(Uint32 n);

  /**
   *  Reduce the interval to have at most the value n 
   *  as the last value.
   *  This method can make the interval empty.
   */
  void onlyUpToValue(Uint32 n);

  /**
   *  Print
   */
  void print() {
    ndbout << "[" << m_first << "," << m_last << "]";
  }

  void normalize();
private:
  Uint32 m_first;
  Uint32 m_last;
};

const Uint32 intervalMin = 0;
const Uint32 intervalMax = 0xffffffff;
const Interval emptyInterval(1, 0);
const Interval universeInterval(intervalMin, intervalMax);

/**
 *  @return true if intervals could be added
 */
bool intervalAdd(const Interval a, const Interval b, Interval * c);

void intervalLeftMinus(const Interval a, const Interval b, Interval * c);

void intervalCut(const Interval a, const Interval b, Interval * c);

/**
 *  @return true if intervals are disjoint
 */
bool intervalDisjoint(const Interval a, const Interval b);

#endif
