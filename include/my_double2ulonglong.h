/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_DOUBLE2ULONGLONG_INCLUDED
#define MY_DOUBLE2ULONGLONG_INCLUDED

/**
  @file include/my_double2ulonglong.h
  Utility functions for converting between ulonglong and double.
*/

#include "my_inttypes.h"

#ifdef _WIN32
inline unsigned long long my_double2ulonglong(double d)
{
  double t= d - (double) 0x8000000000000000ULL;

  if (t >= 0)
    return  ((unsigned long long) t) + 0x8000000000000000ULL;
  return (unsigned long long) d;
}
#define double2ulonglong my_double2ulonglong
#endif /* _WIN32 */

#ifndef ulonglong2double
#define ulonglong2double(A) ((double) (ulonglong) (A))
#define my_off_t2double(A)  ((double) (my_off_t) (A))
#endif
#ifndef double2ulonglong
#define double2ulonglong(A) ((ulonglong) (double) (A))
#endif

#endif  // MY_DOUBLE2ULONGLONG_INCLUDED
