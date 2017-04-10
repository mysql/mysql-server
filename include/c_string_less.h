/*
   Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef C_STRING_LESS_INCLUDED
#define C_STRING_LESS_INCLUDED

#include <string.h>

/**
  @file include/c_string_less.h
*/

/**
  Compares two strings for order, assures ascending lexicographical order.
  Is used as a const char * std::less specialization.
*/
struct c_string_less
{
  /**
    Compares two strings for order.

    @param s1 String to compare.
    @param s2 String to compare.
    @return Result of comparison, assures ascending lexicographical order.
  */
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};


#endif
