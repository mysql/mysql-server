#ifndef STRINGS_DEF_INCLUDED
#define STRINGS_DEF_INCLUDED
/* Copyright (C) 2011 Monty Program Ab

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

/* This file is to be include first in all files in the string directory */

#include <my_global.h>		/* Define standar vars */
#include "m_string.h"		/* Exernal defintions of string functions */

/*
  We can't use the original DBUG_ASSERT() (which includes _db_flush())
  in the strings library as libdbug is compiled after the the strings
  library and we don't want to have strings depending on libdbug which
  depends on mysys and strings.
*/

#if !defined(DBUG_OFF)
#undef DBUG_ASSERT
#define DBUG_ASSERT(A) assert(A)
#endif

/* SPACE_INT is a word that contains only spaces */
#if SIZEOF_INT == 4
#define SPACE_INT 0x20202020
#elif SIZEOF_INT == 8
#define SPACE_INT 0x2020202020202020
#else
#error define the appropriate constant for a word full of spaces
#endif

/**
  Skip trailing space.

  On most systems reading memory in larger chunks (ideally equal to the size of
  the chinks that the machine physically reads from memory) causes fewer memory
  access loops and hence increased performance.
  This is why the 'int' type is used : it's closest to that (according to how
  it's defined in C).
  So when we determine the amount of whitespace at the end of a string we do
  the following :
    1. We divide the string into 3 zones :
      a) from the start of the string (__start) to the first multiple
        of sizeof(int)  (__start_words)
      b) from the end of the string (__end) to the last multiple of sizeof(int)
        (__end_words)
      c) a zone that is aligned to sizeof(int) and can be safely accessed
        through an int *
    2. We start comparing backwards from (c) char-by-char. If all we find is
       space then we continue
    3. If there are elements in zone (b) we compare them as unsigned ints to a
       int mask (SPACE_INT) consisting of all spaces
    4. Finally we compare the remaining part (a) of the string char by char.
       This covers for the last non-space unsigned int from 3. (if any)

   This algorithm works well for relatively larger strings, but it will slow
   the things down for smaller strings (because of the additional calculations
   and checks compared to the naive method). Thus the barrier of length 20
   is added.

   @param     ptr   pointer to the input string
   @param     len   the length of the string
   @return          the last non-space character
*/

static inline const uchar *skip_trailing_space(const uchar *ptr,size_t len)
{
  const uchar *end= ptr + len;

  if (len > 20)
  {
    const uchar *end_words= (const uchar *)(intptr)
      (((ulonglong)(intptr)end) / SIZEOF_INT * SIZEOF_INT);
    const uchar *start_words= (const uchar *)(intptr)
       ((((ulonglong)(intptr)ptr) + SIZEOF_INT - 1) / SIZEOF_INT * SIZEOF_INT);

    DBUG_ASSERT(((ulonglong)(intptr)ptr) >= SIZEOF_INT);
    if (end_words > ptr)
    {
      while (end > end_words && end[-1] == 0x20)
        end--;
      if (end[-1] == 0x20 && start_words < end_words)
        while (end > start_words && ((unsigned *)end)[-1] == SPACE_INT)
          end -= SIZEOF_INT;
    }
  }
  while (end > ptr && end[-1] == 0x20)
    end--;
  return (end);
}
#endif
