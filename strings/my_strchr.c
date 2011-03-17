/* Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"

#define NEQ(A, B) ((A) != (B))
#define EQU(A, B) ((A) == (B))

/**
  Macro for the body of the string scanning.

  @param CS  The character set of the string
  @param STR Pointer to beginning of string
  @param END Pointer to one-after-end of string
  @param ACC Pointer to beginning of accept (or reject) string
  @param LEN Length of accept (or reject) string
  @param CMP is a function-like for doing the comparison of two characters.
 */

#define SCAN_STRING(CS, STR, END, ACC, LEN, CMP)                        \
  do {                                                                  \
    uint mbl;                                                           \
    const char *ptr_str, *ptr_acc;                                      \
    const char *acc_end= (ACC) + (LEN);                                 \
    for (ptr_str= (STR) ; ptr_str < (END) ; ptr_str+= mbl)              \
    {                                                                   \
      mbl= my_mbcharlen((CS), *(uchar*)ptr_str);                        \
      if (mbl < 2)                                                      \
      {                                                                 \
        DBUG_ASSERT(mbl == 1);                                          \
        for (ptr_acc= (ACC) ; ptr_acc < acc_end ; ++ptr_acc)            \
          if (CMP(*ptr_acc, *ptr_str))                                  \
            goto end;                                                   \
      }                                                                 \
    }                                                                   \
end:                                                                    \
    return (size_t) (ptr_str - (STR));                                  \
  } while (0)


/*
  my_strchr(cs, str, end, c) returns a pointer to the first place in
  str where c (1-byte character) occurs, or NULL if c does not occur
  in str. This function is multi-byte safe.
  TODO: should be moved to CHARSET_INFO if it's going to be called
  frequently.
*/

char *my_strchr(const CHARSET_INFO *cs, const char *str, const char *end,
                pchar c)
{
  uint mbl;
  while (str < end)
  {
    mbl= my_mbcharlen(cs, *(uchar *)str);
    if (mbl < 2)
    {
      if (*str == c)
        return((char *)str);
      str++;
    }
    else
      str+= mbl;
  }
  return(0);
}

/**
  Calculate the length of the initial segment of 'str' which consists
  entirely of characters not in 'reject'.

  @note The reject string points to single-byte characters so it is
  only possible to find the first occurrence of a single-byte
  character.  Multi-byte characters in 'str' are treated as not
  matching any character in the reject string.

  @todo should be moved to CHARSET_INFO if it's going to be called
  frequently.

  @internal The implementation builds on the assumption that 'str' is long,
  while 'reject' is short. So it compares each character in string
  with the characters in 'reject' in a tight loop over the characters
  in 'reject'.
*/

size_t my_strcspn(const CHARSET_INFO *cs, const char *str,
                  const char *str_end, const char *reject)
{
  SCAN_STRING(cs, str, str_end, reject, strlen(reject), EQU);
}
