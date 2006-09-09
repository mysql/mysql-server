/*
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

#include <stdlib.h>
#include <ctype.h>
#include <mysql/plugin.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static long number_of_calls= 0; /* for SHOW STATUS, see below */

/*
  Simple full-text parser plugin that acts as a replacement for the
  built-in full-text parser:
  - All non-whitespace characters are significant and are interpreted as
   "word characters."
  - Whitespace characters are space, tab, CR, LF.
  - There is no minimum word length.  Non-whitespace sequences of one
    character or longer are words.
  - Stopwords are used in non-boolean mode, not used in boolean mode.
*/

/*
  simple_parser interface functions:

  Plugin declaration functions:
  - simple_parser_plugin_init()
  - simple_parser_plugin_deinit()

  Parser descriptor functions:
  - simple_parser_parse()
  - simple_parser_init()
  - simple_parser_deinit()
*/


/*
  Initialize the parser plugin at server start or plugin installation.

  SYNOPSIS
    simple_parser_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_plugin_init(void)
{
  return(0);
}


/*
  Terminate the parser plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    simple_parser_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int simple_parser_plugin_deinit(void)
{
  return(0);
}


/*
  Initialize the parser on the first use in the query

  SYNOPSIS
    simple_parser_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_init(MYSQL_FTPARSER_PARAM *param
                              __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser at the end of the query

  SYNOPSIS
    simple_parser_deinit()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_deinit(MYSQL_FTPARSER_PARAM *param
                                __attribute__((unused)))
{
  return(0);
}


/*
  Pass a word back to the server.

  SYNOPSIS
    add_word()
      param              parsing context of the plugin
      word               a word
      len                word length

  DESCRIPTION
    Fill in boolean metadata for the word (if parsing in boolean mode)
    and pass the word to the server.  The server adds the word to
    a full-text index when parsing for indexing, or adds the word to
    the list of search terms when parsing a search string.
*/

static void add_word(MYSQL_FTPARSER_PARAM *param, char *word, size_t len)
{
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info=
    { FT_TOKEN_WORD, 0, 0, 0, 0, ' ', 0 };

  param->mysql_add_word(param, word, len, &bool_info);
}

/*
  Parse a document or a search query.

  SYNOPSIS
    simple_parser_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in
    param->mode.  This function simply splits the text into words
    and passes every word to the MySQL full-text indexing engine.
*/

static int simple_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *end, *start, *docend= param->doc + param->length;

  number_of_calls++;

  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end > start)
        add_word(param, start, end - start);
      break;
    }
    else if (isspace(*end))
    {
      if (end > start)
        add_word(param, start, end - start);
      start= end + 1;
    }
  }
  return(0);
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser simple_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  simple_parser_parse,              /* parsing function       */
  simple_parser_init,               /* parser init function   */
  simple_parser_deinit              /* parser deinit function */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var simple_status[]=
{
  {"static",     (char *)"just a static text",     SHOW_CHAR},
  {"called",     (char *)&number_of_calls, SHOW_LONG},
  {0,0,0}
};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(ftexample)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &simple_parser_descriptor,  /* descriptor                      */
  "simple_parser",            /* name                            */
  "MySQL AB",                 /* author                          */
  "Simple Full-Text Parser",  /* description                     */
  simple_parser_plugin_init,  /* init function (when loaded)     */
  simple_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0001,                     /* version                         */
  simple_status,              /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;

