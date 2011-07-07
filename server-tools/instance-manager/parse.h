#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H
/* Copyright (c) 2004-2006 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

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
#include <my_sys.h>

class Command;
class Instance_map;

enum Log_type
{
  IM_LOG_ERROR= 0,
  IM_LOG_GENERAL,
  IM_LOG_SLOW
};

Command *parse_command(Instance_map *instance_map, const char *text);

/* define kinds of the word seek method */
enum { ALPHANUM= 1, NONSPACE };

/*
  tries to find next word in the text
  if found, returns the beginning and puts word length to word_len argument.
  if not found returns pointer to first non-space or to '\0', word_len == 0
*/

inline void get_word(const char **text, uint *word_len,
                     int seek_method= ALPHANUM)
{
  const char *word_end;

  /* skip space */
  while (my_isspace(default_charset_info, **text))
    ++(*text);

  word_end= *text;

  if (seek_method == ALPHANUM)
    while (my_isalnum(default_charset_info, *word_end))
      ++word_end;
  else
    while (!my_isspace(default_charset_info, *word_end) &&
           (*word_end != '\0'))
      ++word_end;

  *word_len= (uint) (word_end - *text);
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H */
