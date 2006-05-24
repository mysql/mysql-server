/* Copyright (C) 2000-2003 MySQL AB

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* some definitions for full-text indices */

/* #include "myisam.h" */

#ifndef _ft_global_h
#define _ft_global_h
#ifdef  __cplusplus
extern "C" {
#endif

#define HA_FT_MAXBYTELEN 254
#define HA_FT_MAXCHARLEN (HA_FT_MAXBYTELEN/3)

typedef struct st_ft_info FT_INFO;
struct _ft_vft
{
  int       (*read_next)(FT_INFO *, char *);
  float     (*find_relevance)(FT_INFO *, byte *, uint);
  void      (*close_search)(FT_INFO *);
  float     (*get_relevance)(FT_INFO *);
  void      (*reinit_search)(FT_INFO *);
};

#ifndef FT_CORE
struct st_ft_info
{
  struct _ft_vft *please; /* INTERCAL style :-) */
};
#endif

extern const char *ft_stopword_file;
extern const char *ft_precompiled_stopwords[];

extern ulong ft_min_word_len;
extern ulong ft_max_word_len;
extern ulong ft_query_expansion_limit;
extern char  ft_boolean_syntax[15];
extern struct st_mysql_ftparser ft_default_parser;

int ft_init_stopwords(void);
void ft_free_stopwords(void);

#define FT_NL     0
#define FT_BOOL   1
#define FT_SORTED 2
#define FT_EXPAND 4   /* query expansion */

FT_INFO *ft_init_search(uint,void *, uint, byte *, uint,CHARSET_INFO *, byte *);
my_bool ft_boolean_check_syntax_string(const byte *);

/* Internal symbols for fulltext between maria and MyISAM */

#define HA_FT_WTYPE  HA_KEYTYPE_FLOAT
#define HA_FT_WLEN   4
#define FT_SEGS      2

#define ft_sintXkorr(A)    mi_sint4korr(A)
#define ft_intXstore(T,A)  mi_int4store(T,A)

extern const HA_KEYSEG ft_keysegs[FT_SEGS];

#ifdef  __cplusplus
}
#endif
#endif
