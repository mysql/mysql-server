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
  Fast and simple mode.  This mode is used for indexing, and natural
  language queries.

  The parser is expected to return only those words that go into the
  index. Stopwords or too short/long words should not be returned. The
  'boolean_info' argument of mysql_add_word() does not have to be set.
*/
#define MYSQL_FTPARSER_SIMPLE_MODE             0

/*
  Parse with stopwords mode.  This mode is used in boolean searches for
  "phrase matching."

  The parser is not allowed to ignore words in this mode.  Every word
  should be returned, including stopwords and words that are too short
  or long.  The 'boolean_info' argument of mysql_add_word() does not
  have to be set.
*/
#define MYSQL_FTPARSER_WITH_STOPWORDS          1

/*
  Parse in boolean mode.  This mode is used to parse a boolean query string.

  The parser should provide a valid MYSQL_FTPARSER_BOOLEAN_INFO
  structure in the 'boolean_info' argument to mysql_add_word().
  Usually that means that the parser should recognize boolean operators
  in the parsing stream and set appropriate fields in
  MYSQL_FTPARSER_BOOLEAN_INFO structure accordingly.  As for
  MYSQL_FTPARSER_WITH_STOPWORDS mode, no word should be ignored.
  Instead, use FT_TOKEN_STOPWORD for the token type of such a word.
*/
#define MYSQL_FTPARSER_FULL_BOOLEAN_INFO       2

/*
  Token types for boolean mode searching (used for the type member of
  MYSQL_FTPARSER_BOOLEAN_INFO struct)

  FT_TOKEN_EOF: End of data.
  FT_TOKEN_WORD: Regular word.
  FT_TOKEN_LEFT_PAREN: Left parenthesis (start of group/sub-expression).
  FT_TOKEN_RIGHT_PAREN: Right parenthesis (end of group/sub-expression).
  FT_TOKEN_STOPWORD: Stopword.
*/

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
  mysql_add_word (the callback function in the MYSQL_FTPARSER_PARAM
  structure) when a query is parsed in boolean mode.

  type: The token type.  Should be one of the enum_ft_token_type values.

  yesno: Whether the word must be present for a match to occur:
    >0 Must be present
    <0 Must not be present
    0  Neither; the word is optional but its presence increases the relevance
  With the default settings of the ft_boolean_syntax system variable,
  >0 corresponds to the '+' operator, <0 corrresponds to the '-' operator,
  and 0 means neither operator was used.

  weight_adjust: A weighting factor that determines how much a match
  for the word counts.  Positive values increase, negative - decrease the
  relative word's importance in the query.

  wasign: The sign of the word's weight in the query. If it's non-negative
  the match for the word will increase document relevance, if it's
  negative - decrease (the word becomes a "noise word", the less of it the
  better).

  trunc: Corresponds to the '*' operator in the default setting of the
  ft_boolean_syntax system variable.
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
  filled in by MySQL server and passed to the parsing function of the
  plugin as an in/out parameter.

  mysql_parse: A pointer to the built-in parser implementation of the
  server. It's set by the server and can be used by the parser plugin
  to invoke the MySQL default parser.  If plugin's role is to extract
  textual data from .doc, .pdf or .xml content, it might extract
  plaintext from the content, and then pass the text to the default
  MySQL parser to be parsed.  When mysql_parser is called, its param
  argument should be given as the mysql_ftparam value.

  mysql_add_word: A server callback to add a new word.  When parsing
  a document, the server sets this to point at a function that adds
  the word to MySQL full-text index.  When parsing a search query,
  this function will add the new word to the list of words to search
  for.  When mysql_add_word is called, its param argument should be
  given as the mysql_ftparam value.  boolean_info can be NULL for all
  cases except when mode is MYSQL_FTPARSER_FULL_BOOLEAN_INFO.

  ftparser_state: A generic pointer. The plugin can set it to point
  to information to be used internally for its own purposes.

  mysql_ftparam: This is set by the server.  It is passed as the first
  argument to the mysql_parse or mysql_add_word callback.  The plugin
  should not modify it.

  cs: Information about the character set of the document or query string.

  doc: A pointer to the document or query string to be parsed.

  length: Length of the document or query string, in bytes.

  mode: The parsing mode.  With boolean operators, with stopwords, or
  nothing.  See MYSQL_FTPARSER_* constants above.
*/

typedef struct st_mysql_ftparser_param
{
  int (*mysql_parse)(void *param, byte *doc, uint doc_len);
  int (*mysql_add_word)(void *param, byte *word, uint word_len,
                        MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info);
  void *ftparser_state;
  void *mysql_ftparam;
  CHARSET_INFO *cs;
  byte *doc;
  uint length;
  int mode;
} MYSQL_FTPARSER_PARAM;

/*
  Full-text parser descriptor.

  interface_version is, e.g., MYSQL_FTPARSER_INTERFACE_VERSION.
  The parsing, initialization, and deinitialization functions are
  invoked per SQL statement for which the parser is used.
*/

struct st_mysql_ftparser
{
  int interface_version;
  int (*parse)(MYSQL_FTPARSER_PARAM *param);
  int (*init)(MYSQL_FTPARSER_PARAM *param);
  int (*deinit)(MYSQL_FTPARSER_PARAM *param);
};
#endif

