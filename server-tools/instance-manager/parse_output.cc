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

#include "parse.h"

#include <stdio.h>
#include <my_global.h>
#include <my_sys.h>
#include <string.h>


/*
  Parse output of the given command

  SYNOPSYS
    parse_output_and_get_value()

    command      the command to execue with popen.
    word         the word to look for (usually an option name)
    result       the buffer to store the next word (option value)
    result_len   self-explanatory

  DESCRIPTION

    Parse output of the "command". Find the "word" and return the next one

  RETURN
    0 - ok
    1 - error occured
*/

int parse_output_and_get_value(const char *command, const char *word,
                               char *result, size_t result_len)
{
  FILE *output;
  uint wordlen;
  /* should be enought to store the string from the output */
  enum { MAX_LINE_LEN= 512 };
  char linebuf[MAX_LINE_LEN];

  wordlen= strlen(word);

  if (!(output= popen(command, "r")))
    goto err;

  /*
    We want fully buffered stream. We also want system to
    allocate appropriate buffer.
  */
  setvbuf(output, NULL, _IOFBF, 0);

  while (fgets(linebuf, sizeof(linebuf) - 1, output))
  {
    uint lineword_len= 0;
    char *linep= linebuf;

    linebuf[sizeof(linebuf) - 1]= '\0';        /* safety */

    /*
      Get the word, which might contain non-alphanumeric characters. (Usually
      these are '/', '-' and '.' in the path expressions and filenames)
    */
    get_word((const char **) &linep, &lineword_len, NONSPACE);
    if (!strncmp(word, linep, wordlen))
    {
      /*
        If we have found the word, return the next one. This is usually
        an option value.
      */
      linep+= lineword_len;                     /* swallow the previous one */
      get_word((const char **) &linep, &lineword_len, NONSPACE);
      if (result_len <= lineword_len)
        goto err;
      strncpy(result, linep, lineword_len);
      result[lineword_len]= '\0';
      goto pclose;
    }
  }

pclose:
  /* we are not interested in the termination status */
  pclose(output);

  return 0;

err:
  return 1;
}
