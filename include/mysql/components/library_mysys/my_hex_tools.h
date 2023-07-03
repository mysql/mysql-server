/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMPONENT_HEX_TOOLS_H_
#define COMPONENT_HEX_TOOLS_H_

/**
  Convert byte array to hex string

  @param [out] to      Output buffer
  @param [in]  from    Input byte array
  @param [in]  length  Length of input

  @returns Length of output string
*/
extern "C" unsigned long hex_string(char *to, const char *from,
                                    unsigned long length);

/**
  Convert hex string to byte array.

  @param [in]  first  Pointer to first element of range to convert
  @param [in]  last   Pointer to one-after-last element of range to convert
  @param [out] output Beginning of destination range.

  @returns Length of output string
*/
extern "C" unsigned long unhex_string(const char *first, const char *last,
                                      char *output);

#endif  // COMPONENT_HEX_TOOLS_H_
