/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* make-conf.c
 * make a charset .conf file out of a ctype-charset.c file.
 */

#ifndef CHARSET
#error You must define the charset, e.g.:  -DCHARSET=latin1
#endif

/* some pre-processor tricks to get us going */
#define _STRINGIZE_HELPER(x)    #x
#define STRINGIZE(x)            _STRINGIZE_HELPER(x)

#define _JOIN_WORDS_HELPER(a, b)    a ## b
#define JOIN_WORDS(a, b)            _JOIN_WORDS_HELPER(a, b)

#define CH_SRC ctype- ## CHARSET ## .c
#define CH_INCLUDE STRINGIZE(CH_SRC)

/* aaaah, that's better */
#include <my_my_global.h>
#include CH_INCLUDE

#include <stdio.h>
#include <stdlib.h>

#define ROW_LEN   16

void print_array(const char *name, const uchar *array, uint size);

int main(void)
{
  printf("# Configuration file for the "
         STRINGIZE(CHARSET)
         " character set.\n");

  print_array("ctype",      JOIN_WORDS(ctype_,      CHARSET), 257);
  print_array("to_lower",   JOIN_WORDS(to_lower_,   CHARSET), 256);
  print_array("to_upper",   JOIN_WORDS(to_upper_,   CHARSET), 256);
  print_array("sort_order", JOIN_WORDS(sort_order_, CHARSET), 256);

  exit(EXIT_SUCCESS);
}

void print_array(const char *name, const uchar *array, uint size)
{
  uint i;

  printf("\n# The %s array must have %d elements.\n", name, size);

  for (i = 0; i < size; ++i) {
    printf("  %02X", array[i]);

    if ((i+1) % ROW_LEN == size % ROW_LEN)
      printf("\n");
  }
}
