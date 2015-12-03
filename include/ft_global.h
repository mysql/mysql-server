#ifndef FT_GLOBAL_INCLUDED
#define FT_GLOBAL_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

/* #include "myisam.h" */

#include "my_global.h"
#include "my_base.h"
#include "m_ctype.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define HA_FT_MAXBYTELEN 336
#define HA_FT_MAXCHARLEN (HA_FT_MAXBYTELEN/4)

#define DEFAULT_FTB_SYNTAX "+ -><()~*:\"\"&|"

typedef struct st_ft_info FT_INFO;
struct _ft_vft
{
  int       (*read_next)(FT_INFO *, char *);
  float     (*find_relevance)(FT_INFO *, uchar *, uint);
  void      (*close_search)(FT_INFO *);
  float     (*get_relevance)(FT_INFO *);
  void      (*reinit_search)(FT_INFO *);
};

typedef struct st_ft_info_ext FT_INFO_EXT;
struct _ft_vft_ext
{
  uint      (*get_version)();        // Extended API version
  ulonglong (*get_flags)();
  ulonglong (*get_docid)(FT_INFO_EXT *);
  ulonglong (*count_matches)(FT_INFO_EXT *);
};

/* Flags for extended FT API */
#define FTS_ORDERED_RESULT                (1LL << 1)
#define FTS_DOCID_IN_RESULT               (1LL << 2)

#define FTS_DOC_ID_COL_NAME "FTS_DOC_ID"

#define FTS_NGRAM_PARSER_NAME "ngram"

#ifndef FT_CORE
struct st_ft_info
{
  struct _ft_vft *please; /* INTERCAL style :-) */
};

struct st_ft_info_ext
{
  struct _ft_vft     *please; /* INTERCAL style :-) */
  struct _ft_vft_ext *could_you;
};
#endif

extern const char *ft_stopword_file;
extern const char *ft_precompiled_stopwords[];

extern ulong ft_min_word_len;
extern ulong ft_max_word_len;
extern ulong ft_query_expansion_limit;
extern const char *ft_boolean_syntax;
extern struct st_mysql_ftparser ft_default_parser;

int ft_init_stopwords(void);
void ft_free_stopwords(void);

/**
  Operation types, used in FT_HINTS.
*/

enum ft_operation
{
  FT_OP_UNDEFINED, /** Operation undefined, use of hints is impossible */
  FT_OP_NO,        /** No operation, single MATCH function */
  FT_OP_GT,        /** 'Greater than' operation */
  FT_OP_GE         /** 'Greater than or equal to' operation */
};

#define FT_NL              0   /** Normal mode  */
#define FT_BOOL            1   /** Boolean mode */
#define FT_SORTED          2   /** perform internal sorting by rank */
#define FT_EXPAND          4   /** query expansion */
#define FT_NO_RANKING      8   /** skip rank calculation */

/**
  Info about FULLTEXT index hints,
  passed to the storage engine.
*/

struct ft_hints
{
  /** FULLTEXT flags, see FT_NL, etc */
  uint flags;
  /** Operation type */
  enum ft_operation op_type;
  /** Operation value */
  double op_value;
  /** LIMIT value, HA_POS_ERROR if not set */
  ha_rows limit;
};

FT_INFO *ft_init_search(uint,void *, uint, uchar *, uint,
                        const CHARSET_INFO *, uchar *);
my_bool ft_boolean_check_syntax_string(const uchar *);

#ifdef  __cplusplus
}
#endif
#endif /* FT_GLOBAL_INCLUDED */
