/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Do udiv and urem if machine dosn't have it */

#include <global.h>
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
