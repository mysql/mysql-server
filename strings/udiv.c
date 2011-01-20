/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Do udiv and urem if machine dosn't have it */

#include <my_global.h>
#include <math.h>

unsigned long udiv(long unsigned int a, long unsigned int b)
{
  if (a < INT_MAX32 && b < INT_MAX32)
    return (unsigned long) ((long) a / (long) b);
  if (!(b & 1))
    return (unsigned long) ((long) (a >> 1) / (long) (b >> 1));

  return (unsigned long) floor(((double) a / (double) b));
}

unsigned long urem(long unsigned int a, long unsigned int b)
{
  if (a < INT_MAX32 && b < INT_MAX32)
    return (unsigned long) ((long) a % (long) b);
  return a-udiv(a,b)*b;
}
