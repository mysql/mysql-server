/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdio.h>
#include <my_global.h>
#include <my_sys.h>
#include <string.h>

/* buf should be of appropriate size. Otherwise the word will be truncated */
static int get_word(FILE *file, char *buf, size_t size)
{
  int currchar;

  currchar= getc(file);

  /* skip space */
  while (my_isspace(default_charset_info, (char) currchar) &&
         currchar != EOF && size > 1)
  {
    currchar= getc(file);
  }

  while (!my_isspace(default_charset_info, (char) currchar) &&
         currchar != EOF && size > 1)
  {
    *buf++= (char) currchar;
    currchar= getc(file);
    size--;
  }

  *buf= '\0';
  return 0;
}


int parse_output_and_get_value(const char *command, const char *word,
                               char *result, size_t result_len)
{
  FILE *output;
  int wordlen;

  wordlen= strlen(word);

  output= popen(command, "r");

  /*
    We want fully buffered stream. We also want system to
    allocate appropriate buffer.
  */
  setvbuf(output, NULL, _IOFBF, 0);

  get_word(output, result, result_len);
  while (strncmp(word, result, wordlen) && *result != '\0')
  {
    get_word(output, result, result_len);
  }

  /*
    If we have found the word, return the next one. This is usually
    an option value.
  */
  if (*result != '\0')
    get_word(output, result, result_len);

  if (pclose(output))
    return 1;

  return 0;
}
