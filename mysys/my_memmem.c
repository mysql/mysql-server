/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.
   Use is subject to license terms

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <my_global.h>
#include <m_string.h>

/*
  my_memmem, port of a GNU extension.

  Returns a pointer to the beginning of the substring, needle, or NULL if the
  substring is not found in haystack.
*/

void *my_memmem(const void *haystack, size_t haystacklen,
                const void *needle, size_t needlelen)
{
  const unsigned char *cursor;
  const unsigned char *last_possible_needle_location =
    (unsigned char *)haystack + haystacklen - needlelen;

  /* Easy answers */
  if (needlelen > haystacklen) return(NULL);
  if (needle == NULL) return(NULL);
  if (haystack == NULL) return(NULL);
  if (needlelen == 0) return(NULL);
  if (haystacklen == 0) return(NULL);

  for (cursor = haystack; cursor <= last_possible_needle_location; cursor++) {
    if (memcmp(needle, cursor, needlelen) == 0) {
      return((void *) cursor);
    }
  }
  return(NULL);
}

  

#ifdef MAIN
#include <assert.h>

int main(int argc, char *argv[]) {
  char haystack[10], needle[3];

  memmove(haystack, "0123456789", 10);

  memmove(needle, "no", 2);
  assert(my_memmem(haystack, 10, needle, 2) == NULL);

  memmove(needle, "345", 3);
  assert(my_memmem(haystack, 10, needle, 3) != NULL);

  memmove(needle, "789", 3);
  assert(my_memmem(haystack, 10, needle, 3) != NULL);
  assert(my_memmem(haystack, 9, needle, 3) == NULL);

  memmove(needle, "012", 3);
  assert(my_memmem(haystack, 10, needle, 3) != NULL);
  assert(my_memmem(NULL, 10, needle, 3) == NULL);

  assert(my_memmem(NULL, 10, needle, 3) == NULL);
  assert(my_memmem(haystack, 0, needle, 3) == NULL);
  assert(my_memmem(haystack, 10, NULL, 3) == NULL);
  assert(my_memmem(haystack, 10, needle, 0) == NULL);

  assert(my_memmem(haystack, 1, needle, 3) == NULL);

  printf("success\n");
  return(0);
}

#endif
