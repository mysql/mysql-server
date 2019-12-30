/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include <sys/types.h>

#include "ctype.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/ftdefs.h"
#include "storage/myisam/myisamdef.h"
#include "template_utils.h"

struct FT_DOCSTAT {
  FT_WORD *list;
  uint uniq;
  double sum;
};

struct MY_FT_PARSER_PARAM {
  TREE *wtree;
  MEM_ROOT *mem_root;
};

static int FT_WORD_cmp(const void *a, const void *b, const void *c) {
  const CHARSET_INFO *cs = static_cast<const CHARSET_INFO *>(a);
  const FT_WORD *w1 = static_cast<const FT_WORD *>(b);
  const FT_WORD *w2 = static_cast<const FT_WORD *>(c);
  return ha_compare_text(cs, (uchar *)w1->pos, w1->len, (uchar *)w2->pos,
                         w2->len, 0);
}

static int walk_and_copy(void *v_word, uint32 count, void *v_docstat) {
  FT_WORD *word = static_cast<FT_WORD *>(v_word);
  FT_DOCSTAT *docstat = static_cast<FT_DOCSTAT *>(v_docstat);
  word->weight = LWS_IN_USE;
  docstat->sum += word->weight;
  memcpy((docstat->list)++, word, sizeof(FT_WORD));
  return 0;
}

/* transforms tree of words into the array, applying normalization */

FT_WORD *ft_linearize(TREE *wtree, MEM_ROOT *mem_root) {
  FT_WORD *wlist, *p;
  FT_DOCSTAT docstat;
  DBUG_TRACE;

  if ((wlist = (FT_WORD *)mem_root->Alloc(sizeof(FT_WORD) *
                                          (1 + wtree->elements_in_tree)))) {
    docstat.list = wlist;
    docstat.uniq = wtree->elements_in_tree;
    docstat.sum = 0;
    tree_walk(wtree, &walk_and_copy, &docstat, left_root_right);
  }
  delete_tree(wtree);
  if (!wlist) return NULL;

  docstat.list->pos = NULL;

  for (p = wlist; p->pos; p++) {
    p->weight = PRENORM_IN_USE;
  }

  for (p = wlist; p->pos; p++) {
    p->weight /= NORM_IN_USE;
  }

  return wlist;
}

bool ft_boolean_check_syntax_string(const uchar *str) {
  uint i, j;

  if (!str ||
      (strlen(pointer_cast<const char *>(str)) + 1 !=
       sizeof(DEFAULT_FTB_SYNTAX)) ||
      (str[0] != ' ' && str[1] != ' '))
    return 1;
  for (i = 0; i < sizeof(DEFAULT_FTB_SYNTAX); i++) {
    /* limiting to 7-bit ascii only */
    if ((unsigned char)(str[i]) > 127 || isalnum(str[i])) return 1;
    for (j = 0; j < i; j++)
      if (str[i] == str[j] && (i != 11 || j != 10)) return 1;
  }
  return 0;
}

/*
  RETURN VALUE
  0 - eof
  1 - word found
  2 - left bracket
  3 - right bracket
  4 - stopword found
*/
uchar ft_get_word(const CHARSET_INFO *cs, uchar **start, uchar *end,
                  FT_WORD *word, MYSQL_FTPARSER_BOOLEAN_INFO *param) {
  uchar *doc = *start;
  int ctype;
  uint mwc, length;
  int mbl;

  param->yesno = (FTB_YES == ' ') ? 1 : (param->quot != 0);
  param->weight_adjust = param->wasign = 0;
  param->type = FT_TOKEN_EOF;

  while (doc < end) {
    for (; doc < end; doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      mbl = cs->cset->ctype(cs, &ctype, (uchar *)doc, (uchar *)end);
      if (true_word_char(ctype, *doc)) break;
      if (*doc == FTB_RQUOT && param->quot) {
        *start = doc + 1;
        param->type = FT_TOKEN_RIGHT_PAREN;
        goto ret;
      }
      if (!param->quot) {
        if (*doc == FTB_LBR || *doc == FTB_RBR || *doc == FTB_LQUOT) {
          /* param->prev=' '; */
          *start = doc + 1;
          if (*doc == FTB_LQUOT) param->quot = (char *)1;
          param->type =
              (*doc == FTB_RBR ? FT_TOKEN_RIGHT_PAREN : FT_TOKEN_LEFT_PAREN);
          goto ret;
        }
        if (param->prev == ' ') {
          if (*doc == FTB_YES) {
            param->yesno = +1;
            continue;
          } else if (*doc == FTB_EGAL) {
            param->yesno = 0;
            continue;
          } else if (*doc == FTB_NO) {
            param->yesno = -1;
            continue;
          } else if (*doc == FTB_INC) {
            param->weight_adjust++;
            continue;
          } else if (*doc == FTB_DEC) {
            param->weight_adjust--;
            continue;
          } else if (*doc == FTB_NEG) {
            param->wasign = !param->wasign;
            continue;
          }
        }
      }
      param->prev = *doc;
      param->yesno = (FTB_YES == ' ') ? 1 : (param->quot != 0);
      param->weight_adjust = param->wasign = 0;
    }

    mwc = length = 0;
    for (word->pos = doc; doc < end;
         length++, doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      mbl = cs->cset->ctype(cs, &ctype, (uchar *)doc, (uchar *)end);
      if (true_word_char(ctype, *doc))
        mwc = 0;
      else if (!misc_word_char(*doc) || mwc)
        break;
      else
        mwc++;
    }
    param->prev = 'A'; /* be sure *prev is true_word_char */
    word->len = (uint)(doc - word->pos) - mwc;
    if ((param->trunc = (doc < end && *doc == FTB_TRUNC))) doc++;

    if (((length >= ft_min_word_len &&
          !is_stopword((char *)word->pos, word->len)) ||
         param->trunc) &&
        length < ft_max_word_len) {
      *start = doc;
      param->type = FT_TOKEN_WORD;
      goto ret;
    } else if (length) /* make sure length > 0 (if start contains spaces only)
                        */
    {
      *start = doc;
      param->type = FT_TOKEN_STOPWORD;
      goto ret;
    }
  }
  if (param->quot) {
    *start = doc;
    param->type = static_cast<enum_ft_token_type>(3); /* FT_RBR */
    goto ret;
  }
ret:
  return param->type;
}

uchar ft_simple_get_word(const CHARSET_INFO *cs, uchar **start,
                         const uchar *end, FT_WORD *word, bool skip_stopwords) {
  uchar *doc = *start;
  uint mwc, length;
  int mbl;
  int ctype;
  DBUG_TRACE;

  do {
    for (;; doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      if (doc >= end) return 0;
      mbl = cs->cset->ctype(cs, &ctype, doc, end);
      if (true_word_char(ctype, *doc)) break;
    }

    mwc = length = 0;
    for (word->pos = doc; doc < end;
         length++, doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      mbl = cs->cset->ctype(cs, &ctype, doc, end);
      if (true_word_char(ctype, *doc))
        mwc = 0;
      else if (!misc_word_char(*doc) || mwc)
        break;
      else
        mwc++;
    }

    word->len = (uint)(doc - word->pos) - mwc;

    if (skip_stopwords == false ||
        (length >= ft_min_word_len && length < ft_max_word_len &&
         !is_stopword((char *)word->pos, word->len))) {
      *start = doc;
      return 1;
    }
  } while (doc < end);
  return 0;
}

void ft_parse_init(TREE *wtree, const CHARSET_INFO *cs) {
  DBUG_TRACE;
  if (!is_tree_inited(wtree))
    init_tree(wtree, 0, 0, sizeof(FT_WORD), &FT_WORD_cmp, 0, NULL, cs);
}

static int ft_add_word(MYSQL_FTPARSER_PARAM *param, char *word, int word_len,
                       MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info
                           MY_ATTRIBUTE((unused))) {
  TREE *wtree;
  FT_WORD w;
  MY_FT_PARSER_PARAM *ft_param = (MY_FT_PARSER_PARAM *)param->mysql_ftparam;
  DBUG_TRACE;
  wtree = ft_param->wtree;
  if (param->flags & MYSQL_FTFLAGS_NEED_COPY) {
    uchar *ptr;
    DBUG_ASSERT(wtree->with_delete == 0);
    ptr = (uchar *)ft_param->mem_root->Alloc(word_len);
    memcpy(ptr, word, word_len);
    w.pos = ptr;
  } else
    w.pos = (uchar *)word;
  w.len = word_len;
  if (!tree_insert(wtree, &w, 0, wtree->custom_arg)) {
    delete_tree(wtree);
    return 1;
  }
  return 0;
}

static int ft_parse_internal(MYSQL_FTPARSER_PARAM *param, char *doc_arg,
                             int doc_len) {
  uchar *doc = (uchar *)doc_arg;
  uchar *end = doc + doc_len;
  MY_FT_PARSER_PARAM *ft_param = (MY_FT_PARSER_PARAM *)param->mysql_ftparam;
  TREE *wtree = ft_param->wtree;
  FT_WORD w;
  DBUG_TRACE;

  while (
      ft_simple_get_word(static_cast<const CHARSET_INFO *>(wtree->custom_arg),
                         &doc, end, &w, true))
    if (param->mysql_add_word(param, (char *)w.pos, w.len, 0)) return 1;
  return 0;
}

int ft_parse(TREE *wtree, uchar *doc, int doclen,
             struct st_mysql_ftparser *parser, MYSQL_FTPARSER_PARAM *param,
             MEM_ROOT *mem_root) {
  MY_FT_PARSER_PARAM my_param;
  DBUG_TRACE;
  DBUG_ASSERT(parser);

  my_param.wtree = wtree;
  my_param.mem_root = mem_root;

  param->mysql_parse = ft_parse_internal;
  param->mysql_add_word = ft_add_word;
  param->mysql_ftparam = &my_param;
  param->cs = static_cast<const CHARSET_INFO *>(wtree->custom_arg);
  param->doc = (char *)doc;
  param->length = doclen;
  param->mode = MYSQL_FTPARSER_SIMPLE_MODE;
  return parser->parse(param);
}

#define MAX_PARAM_NR 2

MYSQL_FTPARSER_PARAM *ftparser_alloc_param(MI_INFO *info) {
  if (!info->ftparser_param) {
    /*
.     info->ftparser_param can not be zero after the initialization,
      because it always includes built-in fulltext parser. And built-in
      parser can be called even if the table has no fulltext indexes and
      no varchar/text fields.

      ftb_find_relevance... parser (ftb_find_relevance_parse,
      ftb_find_relevance_add_word) calls ftb_check_phrase... parser
      (ftb_check_phrase_internal, ftb_phrase_add_word). Thus MAX_PARAM_NR=2.
    */
    info->ftparser_param = (MYSQL_FTPARSER_PARAM *)my_malloc(
        mi_key_memory_FTPARSER_PARAM,
        MAX_PARAM_NR * sizeof(MYSQL_FTPARSER_PARAM) * info->s->ftkeys,
        MYF(MY_WME | MY_ZEROFILL));
    init_alloc_root(mi_key_memory_ft_memroot, &info->ft_memroot,
                    FTPARSER_MEMROOT_ALLOC_SIZE, 0);
  }
  return info->ftparser_param;
}

MYSQL_FTPARSER_PARAM *ftparser_call_initializer(MI_INFO *info, uint keynr,
                                                uint paramnr) {
  uint32 ftparser_nr;
  struct st_mysql_ftparser *parser;

  if (!ftparser_alloc_param(info)) return 0;

  if (keynr == NO_SUCH_KEY) {
    ftparser_nr = 0;
    parser = &ft_default_parser;
  } else {
    ftparser_nr = info->s->keyinfo[keynr].ftkey_nr;
    parser = info->s->keyinfo[keynr].parser;
  }
  DBUG_ASSERT(paramnr < MAX_PARAM_NR);
  ftparser_nr = ftparser_nr * MAX_PARAM_NR + paramnr;
  if (!info->ftparser_param[ftparser_nr].mysql_add_word) {
    /* Note, that mysql_add_word is used here as a flag:
       mysql_add_word == 0 - parser is not initialized
       mysql_add_word != 0 - parser is initialized, or no
                             initialization needed. */
    info->ftparser_param[ftparser_nr].mysql_add_word = (int (*)(
        MYSQL_FTPARSER_PARAM *, char *, int, MYSQL_FTPARSER_BOOLEAN_INFO *))1;
    if (parser->init && parser->init(&info->ftparser_param[ftparser_nr]))
      return 0;
  }
  return &info->ftparser_param[ftparser_nr];
}

void ftparser_call_deinitializer(MI_INFO *info) {
  uint i, j, keys = info->s->state.header.keys;
  free_root(&info->ft_memroot, MYF(0));
  if (!info->ftparser_param) return;
  for (i = 0; i < keys; i++) {
    MI_KEYDEF *keyinfo = &info->s->keyinfo[i];
    for (j = 0; j < MAX_PARAM_NR; j++) {
      MYSQL_FTPARSER_PARAM *ftparser_param =
          &info->ftparser_param[keyinfo->ftkey_nr * MAX_PARAM_NR + j];
      if (keyinfo->flag & HA_FULLTEXT && ftparser_param->mysql_add_word) {
        if (keyinfo->parser->deinit) keyinfo->parser->deinit(ftparser_param);
        ftparser_param->mysql_add_word = 0;
      } else
        break;
    }
  }
}
