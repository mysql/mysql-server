/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Helper macros used for setting different __attributes__
  on functions in a portable fashion
*/

#ifndef _my_attribute_h
#define _my_attribute_h

#if defined(__GNUC__)
# ifndef GCC_VERSION
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
# endif
#endif

/*
  Disable MY_ATTRIBUTE() on g++ < 3.4, and non-gcc compilers.
  Some forms of __attribute__ are actually supported in earlier versions of
  g++, but we just disable them all because we only use them to generate
  compilation warnings.
*/
#ifndef MY_ATTRIBUTE
#if defined(__GNUC__) && GCC_VERSION > 3003
#  define MY_ATTRIBUTE(A) __attribute__(A)
#else
#  define MY_ATTRIBUTE(A)
#endif
#endif

/*
  __attribute__((format(...))) is only supported in g++ >= 3.4
  But that's already covered by the MY_ATTRIBUTE tests above, so this is
  just a convenience macro.
*/
#ifndef ATTRIBUTE_FORMAT
#  define ATTRIBUTE_FORMAT(style, m, n) MY_ATTRIBUTE((format(style, m, n)))
#endif

#ifndef ATTRIBUTE_FORMAT_FPTR
#  define ATTRIBUTE_FORMAT_FPTR(style, m, n) ATTRIBUTE_FORMAT(style, m, n)
#endif


#endif
