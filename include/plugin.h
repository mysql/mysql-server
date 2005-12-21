/* Copyright (C) 2005 MySQL AB

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

#ifndef _my_plugin_h
#define _my_plugin_h

/*************************************************************************
  Plugin API. Common for all plugin types.
*/

#define MYSQL_PLUGIN_INTERFACE_VERSION 0x0001

/*
  The allowable types of plugins
*/
#define MYSQL_UDF_PLUGIN             0  /* User-defined function        */
#define MYSQL_STORAGE_ENGINE_PLUGIN  1  /* Storage Engine               */
#define MYSQL_FTPARSER_PLUGIN        2  /* Full-text [pre]parser        */
#define MYSQL_MAX_PLUGIN_TYPE_NUM    3  /* The number of plugin types   */

/*
  Macros for beginning and ending plugin declarations.  Between
  mysql_declare_plugin and mysql_declare_plugin_end there should
  be a st_mysql_plugin struct for each plugin to be declared.
*/

#define mysql_declare_plugin                                          \
int _mysql_plugin_interface_version_= MYSQL_PLUGIN_INTERFACE_VERSION; \
struct st_mysql_plugin _mysql_plugin_declarations_[]= {
#define mysql_declare_plugin_end ,{0,0,0,0,0,0,0}}

/*
  Plugin description structure.
*/

struct st_mysql_plugin
{
  int type;             /* the plugin type (a MYSQL_XXX_PLUGIN value)   */
  void *info;           /* pointer to type-specific plugin descriptor   */
  const char *name;     /* plugin name                                  */
  const char *author;   /* plugin author (for SHOW PLUGINS)             */
  const char *descr;    /* general descriptive text (for SHOW PLUGINS ) */
  int (*init)(void);    /* the function to invoke when plugin is loaded */
  int (*deinit)(void);  /* the function to invoke when plugin is unloaded */
  uint version;         /* plugin version (for SHOW PLUGINS)            */
};

/*************************************************************************
  API for Full-text [pre]parser plugin. (MYSQL_FTPARSER_PLUGIN)
*/

#define MYSQL_FTPARSER_INTERFACE_VERSION 0x0000

/* Parsing modes. Set in  MYSQL_FTPARSER_PARAM::mode */
/*
  The fast and simple mode. Parser is expected to return only those words that
  go into the index. Stopwords or too short/long words should not be returned.
  'boolean_info' argument of mysql_add_word() does not have to be set.

  This mode is used for indexing, and natural language queries.
*/
#define MYSQL_FTPARSER_SIMPLE_MODE             0

/*
  The parser is not allowed to ignore words in this mode.  Every word should
  be returned, including stopwords and words that are too short or long.
  'boolean_info' argument of mysql_add_word() does not have to be set.

  This mode is used in boolean searches for "phrase matching."
*/
#define MYSQL_FTPARSER_WITH_STOPWORDS          1

/*
  Parse in boolean mode.  The parser should provide a valid
  MYSQL_FTPARSER_BOOLEAN_INFO structure in the 'boolean_info' argument
  to mysql_add_word().  Usually that means that the parser should
  recognize boolean operators in the parsing stream and set appropriate
  fields in MYSQL_FTPARSER_BOOLEAN_INFO structure accordingly.  As
  for MYSQL_FTPARSER_WITH_STOPWORDS mode, no word should be ignored.
  Instead, use FT_TOKEN_STOPWORD for the token type of such a word.

  This mode is used to parse a boolean query string.
*/
#define MYSQL_FTPARSER_FULL_BOOLEAN_INFO       2

enum enum_ft_token_type
{
  FT_TOKEN_EOF= 0,
  FT_TOKEN_WORD= 1,
  FT_TOKEN_LEFT_PAREN= 2,
  FT_TOKEN_RIGHT_PAREN= 3,
  FT_TOKEN_STOPWORD= 4
};

/*
  This structure is used in boolean search mode only. It conveys
  boolean-mode metadata to the MySQL search engine for every word in
  the search query. A valid instance of this structure must be filled
  in by the plugin parser and passed as an argument in the call to
  mysql_add_word (the function from structure MYSQL_FTPARSER_PARAM)
  when a query is parsed in boolean mode.
*/

typedef struct st_mysql_ftparser_boolean_info
{
  enum enum_ft_token_type type;
  int yesno;
  int weight_adjust;
  bool wasign;
  bool trunc;
  /* These are parser state and must be removed. */
  byte prev;
  byte *quot;
} MYSQL_FTPARSER_BOOLEAN_INFO;


/*
  An argument of the full-text parser plugin. This structure is
  filled by MySQL server and passed to the parsing function of the
  plugin as an in/out parameter.
*/

typedef struct st_mysql_ftparser_param
{
  /*
    A fallback pointer to the built-in parser implementation
    of the server. It's set by the server and can be used
    by the parser plugin to invoke the MySQL default parser.
    If plugin's role is to extract textual data from .doc,
    .pdf or .xml content, it might use the default MySQL parser
    to parse the extracted plaintext string.
  */
  int (*mysql_parse)(void *param, byte *doc, uint doc_len);
  /*
    A server callback to add a new word.
    When parsing a document, the server sets this to point at
    a function that adds the word to MySQL full-text index.
    When parsing a search query, this function will
    add the new word to the list of words to search for.
    boolean_info can be NULL for all cases except
    MYSQL_FTPARSER_FULL_BOOLEAN_INFO mode.
  */
  int (*mysql_add_word)(void *param, byte *word, uint word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  /* A pointer to the parser local state. This is an inout parameter. */
  void *ftparser_state;
  void *mysql_ftparam;
  /* Character set of the document or the query */
  CHARSET_INFO *cs;
  /* A pointer to the document or the query to be parsed */
  byte *doc;
  /* Document/query length */
  uint length;
  /*
    Parsing mode: with boolean operators, with stopwords, or nothing.
    See MYSQL_FTPARSER_* constants above.
  */
  int mode;
} MYSQL_FTPARSER_PARAM;

struct st_mysql_ftparser
{
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
};
#endif

