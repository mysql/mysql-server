/* Copyright (C) 2000 MySQL AB

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
