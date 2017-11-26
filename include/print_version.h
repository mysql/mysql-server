/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _print_version_h_
#define _print_version_h_

/**
  @file include/print_version.h
*/

/**
  This function prints a standard version string. Should be used by
  all utilities.
*/

void print_version();

/**
  This function prints a standard version string, with '-debug' added
  to the name of the executable. Used by utilties that have an
  explicit need to state that they have been compiled in debug mode.
*/

void print_version_debug();

/**
  This function prints a version string with the released version
  supplied by the caller. Used by the server process which needs to
  print if it is compiled with debug, ASAN, UBSAN or running with
  Valgrind.

  @param[in] version  Null-terminated release version string
*/

void print_explicit_version(const char* version);

#endif /* _print_version_h_  */
