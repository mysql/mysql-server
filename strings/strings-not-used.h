/* Copyright (C) 2000 MySQL AB

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

/*  File   : strings.h
    Author : Richard A. O'Keefe.
    Updated: 1 June 1984
    Purpose: Header file for the "string(3C)" package.

    All  the  routines	in  this  package  are	the  original  work   of
    R.A.O'Keefe.   Any	resemblance  between  them  and  any routines in
    licensed software is due entirely  to  these  routines  having  been
    written  using the "man 3 string" UNIX manual page, or in some cases
    the "man 1 sort" manual page as a specification.  See the READ-ME to
    find the conditions under which these routines may be used & copied.
*/

#ifndef NullS

#include <my_global.h>				/* Define standar vars */
#include "m_string.h"

#define NUL	'\0'
#define _AlphabetSize	256

#endif	/* NullS */
