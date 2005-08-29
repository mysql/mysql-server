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

#include <my_global.h>
#include "parse.h"
#include "parse_output.h"

#include <stdio.h>
#include <my_sys.h>
#include <m_string.h>
#include "portability.h"


/*
  Parse output of the given command

  SYNOPSYS
    parse_output_and_get_value()

    command          the command to execue with popen.
    word             the word to look for (usually an option name)
    result           the buffer to store the next word (option value)
    input_buffer_len self-explanatory
    flag             this equals to GET_LINE if we want to get all the line after
                     the matched word and GET_VALUE otherwise.

  DESCRIPTION

    Parse output of the "command". Find the "word" and return the next one
    if flag is GET_VALUE. Return the rest of the parsed string otherwise.

  RETURN
    0 - ok, the word has been found
    1 - error occured or the word is not found
*/

int parse_output_and_get_value(const char *command, const char *word,
                               char *result, size_t input_buffer_len,
                               uint flag)
{
  FILE *output;
  uint wordlen;
  /* should be enough to store the string from the output */
  enum { MAX_LINE_LEN= 512 };
  char linebuf[MAX_LINE_LEN];
  int rc= 1;

  wordlen= strlen(word);

  /*
    Successful return of popen does not tell us whether the command has been
    executed successfully: if the command was not found, we'll get EOF
    when reading the output buffer below.
  */
  if (!(output= popen(command, "r")))
    goto err;

  /*
    We want fully buffered stream. We also want system to
    allocate appropriate buffer.
  */
  setvbuf(output, NULL, _IOFBF, 0);

  while (fgets(linebuf, sizeof(linebuf) - 1, output))
  {
    uint found_word_len= 0;
    char *linep= linebuf;

    linebuf[sizeof(linebuf) - 1]= '\0';        /* safety */

    /*
      Get the word, which might contain non-alphanumeric characters. (Usually
      these are '/', '-' and '.' in the path expressions and filenames)
    */
    get_word((const char **) &linep, &found_word_len, NONSPACE);
    if (!strncmp(word, linep, wordlen))
    {
      /*
        If we have found the word, return the next one (this is usually
        an option value) or the whole line (if flag)
      */
      linep+= found_word_len;                     /* swallow the previous one */
      if (flag & GET_VALUE)
      {
        get_word((const char **) &linep, &found_word_len, NONSPACE);
        if (input_buffer_len <= found_word_len)
          goto err;
        strmake(result, linep, found_word_len);
      }
      else         /* currently there are only two options */
        strmake(result, linep, input_buffer_len - 1);
      rc= 0;
      break;
    }
  }

pclose:
  /* we are not interested in the termination status */
  pclose(output);

err:
  return rc;
}

