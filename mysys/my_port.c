/* Copyright (C) 2002 MySQL AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* 
   Small functions to make code portable
*/

#include "mysys_priv.h"

#ifdef _AIX

/*
  On AIX, at least with gcc 3.1, the expression
  '(double) (ulonglong) var' doesn't always work for big unsigned
  integers like '18446744073709551615'.  The end result is that the
  high bit is simply dropped. (probably bug in gcc optimizations)
  Handling the conversion in a sub function seems to work.
*/



double my_ulonglong2double(unsigned long long nr)
{
  return (double) nr;
}
#endif /* _AIX */
