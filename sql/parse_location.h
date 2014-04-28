/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PARSE_LOCATION_INCLUDED
#define PARSE_LOCATION_INCLUDED


/**
  Helper class for the YYLTYPE
*/
struct Symbol_location 
{
  const char *start; // token start
  const char *end;   // the 1st byte after the token

  bool is_empty() const { return length() == 0; }
  size_t length() const { return static_cast<size_t>(end - start); }
};


/**
  Bison "location" class
*/
typedef struct YYLTYPE
{
  Symbol_location  cpp; // token location in the preprocessed buffer
  Symbol_location  raw; // token location in the raw buffer

  bool is_empty() const { return cpp.is_empty(); }

} YYLTYPE;

#define YYLTYPE_IS_DECLARED 1 // signal Bison that we have our own YYLTYPE

/**
  Bison calls this macro:
  1. each time a rule is matched and
  2. to compute a syntax error location.

  @param Current [out] location of the whole matched rule
  @param Rhs           locations of all right hand side elements in the rule
  @param N             number of right hand side elements in the rule
*/
#define YYLLOC_DEFAULT(Current, Rhs, N)                             \
    do                                                              \
      if (N)                                                        \
      {                                                             \
        (Current).cpp.start= YYRHSLOC(Rhs, 1).cpp.start;            \
        (Current).cpp.end=   YYRHSLOC(Rhs, N).cpp.end;              \
        (Current).raw.start= YYRHSLOC(Rhs, 1).raw.start;            \
        (Current).raw.end=   YYRHSLOC(Rhs, N).raw.end;              \
      }                                                             \
      else                                                          \
      {                                                             \
        (Current).cpp.start=                                        \
        (Current).cpp.end=   YYRHSLOC(Rhs, 0).cpp.end;              \
        (Current).raw.start=                                        \
        (Current).raw.end=   YYRHSLOC(Rhs, 0).raw.end;              \
      }                                                             \
    while (0)

#endif /* PARSE_LOCATION_INCLUDED */
