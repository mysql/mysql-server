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

#include "Interval.hpp"

#undef min
#undef max
Uint32 max(Uint32 a, Uint32 b)  { return a > b ? a : b; }
Uint32 min(Uint32 a, Uint32 b)  { return a < b ? a : b; }

Interval::Interval() 
{ 
  set(1, 0); // EmptyInterval
}

Interval::Interval(Uint32 f, Uint32 l) 
{ 
  set(f, l); 
}

bool
Interval::isEmpty() const 
{
  return (m_first > m_last) ? true : false; 
}

bool 
Interval::isEqual(Uint32 a, Uint32 b) const 
{
  return (a==m_first && b==m_last);
}

bool 
Interval::inInterval(Uint32 a) const 
{
  return (m_first <= a && a <= m_last);
}

void 
Interval::set(Uint32 first, Uint32 last) 
{
  m_first = first;
  m_last = last;
  normalize();
}

void 
Interval::set(const Interval i) 
{
  m_first = i.first();
  m_last = i.last();
  normalize();
}

void 
Interval::setFirst(Uint32 first) 
{ 
  m_first = first; 
}

void
Interval::setLast(Uint32 last) 
{ 
  m_last = last; 
}

void
Interval::onlyLeft(Uint32 n) 
{
  if (size() > n) m_last = m_first + n - 1;
}  

void
Interval::onlyUpToValue(Uint32 n) 
{
  m_last = min(n, m_last);
  normalize();
}

/*****************************************************************************/

void 
Interval::normalize() 
{
  if (isEmpty()) {
    m_first = 1;
    m_last = 0;
  }
}


/*****************************************************************************/

bool 
intervalAdd(const Interval a, const Interval b, Interval * r) 
{
  /**
   * Non-empty disjoint intervals
   */
  if (!a.isEmpty() &&
      !b.isEmpty() && 
      (a.last() + 1 < b.first() || 
       b.last() + 1 < a.first()) ) {
    return false; // Illegal add
  }
  
  /**
   * Interval A empty -> return B
   */
  if (a.isEmpty()) { 
    r->set(b); 
    return true; 
  }

  /**
   * Interval B empty -> return A
   */
  if (b.isEmpty()) { 
    r->set(a); 
    return true; 
  }
  
  r->set(min(a.first(), b.first()), 
	 max(a.last(), b.last()));
  return true;
}

/**
 * Subtract the left part of interval 'a' up to last of 'b'.
 *
 * @note  This is NOT ordinary arithmetic interval minus.
 *        In ordinary arithmetic, [11-25] - [12-15] would be undefined,
 *        but here it is [11-25] - [12-15] = [16-25].
 */
void
intervalLeftMinus(const Interval a, const Interval b, Interval * r) 
{
  if(b.last() != intervalMax)
    r->set(max(a.first(), b.last()+1), a.last());
  else 
    r->set(max(a.first(), intervalMax), a.last());
}

void
intervalCut(const Interval a, const Interval b, Interval * r) 
{
  r->set(max(a.first(), b.first()), min(a.last(), b.last()));
  r->normalize();
}

bool 
intervalDisjoint(const Interval a, const Interval b) 
{
  return (a.isEmpty() || 
	  b.isEmpty() || 
	  a.last() < b.first() || 
	  b.last() < a.first());
}
