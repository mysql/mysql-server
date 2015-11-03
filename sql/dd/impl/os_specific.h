/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__OS_SPECIFIC_INCLUDED
#define DD__OS_SPECIFIC_INCLUDED

#ifdef _WIN32

#define DD_HEADER_BEGIN \
  __pragma(warning(push)); \
  __pragma(warning(disable : 4250))

#define DD_HEADER_END \
  __pragma(warning(pop)) // C4250

#else // !_WIN32

#define DD_HEADER_BEGIN
#define DD_HEADER_END

#endif // _WIN32


#endif // DD__OS_SPECIFIC_INCLUDED
