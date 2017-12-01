/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_BASENAME_INCLUDED
#define MY_BASENAME_INCLUDED 1

/**
  @file include/my_basename.h
  A macro that gives __FILE__ without the directory name (e.g. foo.cc instead of
  ../sql/foo.cc), calculated compile-time. Note that the entire __FILE__
  string is still present in the binary; only the pointer is adjusted.
*/

static constexpr int basename_index(const char * const path, const int index)
{
  return (path[index] == '/' || path[index] == '\\' || index == -1) ?
    index + 1 : basename_index(path, index - 1);
}

#define MY_BASENAME (__FILE__ + basename_index(__FILE__, sizeof(__FILE__) - 1))

#endif  // MY_BASENAME_INCLUDED
