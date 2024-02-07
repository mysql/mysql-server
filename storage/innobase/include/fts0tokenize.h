/*****************************************************************************

Copyright (c) 2014, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/fts0tokenize.h
 Full Text Search plugin tokenizer refer to MyISAM

 Created 2014/11/17 Shaohua Wang
 ***********************************************************************/

#include <cstdint>

#include "ft_global.h"
#include "mysql/plugin_ftparser.h"
#include "mysql/strings/m_ctype.h"

/* Macros and structs below are from ftdefs.h in MyISAM */
/** Check a char is true word */
inline bool true_word_char(int c, uint8_t ch) {
  return ((c & (MY_CHAR_U | MY_CHAR_L | MY_CHAR_NMR)) != 0) || ch == '_';
}

/** Boolean search syntax */
static const char *fts_boolean_syntax = DEFAULT_FTB_SYNTAX;

#define FTB_YES (fts_boolean_syntax[0])
#define FTB_EGAL (fts_boolean_syntax[1])
#define FTB_NO (fts_boolean_syntax[2])
#define FTB_INC (fts_boolean_syntax[3])
#define FTB_DEC (fts_boolean_syntax[4])
#define FTB_LBR (fts_boolean_syntax[5])
#define FTB_RBR (fts_boolean_syntax[6])
#define FTB_NEG (fts_boolean_syntax[7])
#define FTB_TRUNC (fts_boolean_syntax[8])
#define FTB_LQUOT (fts_boolean_syntax[10])
#define FTB_RQUOT (fts_boolean_syntax[11])

/** FTS query token */
struct FT_WORD {
  uchar *pos;    /*!< word start pointer */
  uint len;      /*!< word len */
  double weight; /*!< word weight, unused in innodb */
};

/** Tokenizer for ngram referring to ft_get_word(ft_parser.c) in MyISAM.
Differences: a. code format changed; b. stopword processing removed.
@param[in]      cs      charset
@param[in,out]  start   doc start pointer
@param[in,out]  end     doc end pointer
@param[in,out]  word    token
@param[in,out]  info    token info
@retval 0       eof
@retval 1       word found
@retval 2       left bracket
@retval 3       right bracket
@retval 4       stopword found */
inline uchar fts_get_word(const CHARSET_INFO *cs, uchar **start, uchar *end,
                          FT_WORD *word, MYSQL_FTPARSER_BOOLEAN_INFO *info) {
  uchar *doc = *start;
  int ctype;
  int mbl;

  info->yesno = (FTB_YES == ' ') ? 1 : (info->quot != nullptr);
  info->weight_adjust = info->wasign = 0;
  info->type = FT_TOKEN_EOF;

  while (doc < end) {
    for (; doc < end; doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      mbl = cs->cset->ctype(cs, &ctype, doc, end);

      if (true_word_char(ctype, *doc)) {
        break;
      }

      if (*doc == FTB_RQUOT && info->quot) {
        *start = doc + 1;
        info->type = FT_TOKEN_RIGHT_PAREN;

        return (info->type);
      }

      if (!info->quot) {
        if (*doc == FTB_LBR || *doc == FTB_RBR || *doc == FTB_LQUOT) {
          /* param->prev=' '; */
          *start = doc + 1;
          if (*doc == FTB_LQUOT) {
            info->quot = (char *)1;
          }

          info->type =
              (*doc == FTB_RBR ? FT_TOKEN_RIGHT_PAREN : FT_TOKEN_LEFT_PAREN);

          return (info->type);
        }

        if (info->prev == ' ') {
          if (*doc == FTB_YES) {
            info->yesno = +1;
            continue;
          } else if (*doc == FTB_EGAL) {
            info->yesno = 0;
            continue;
          } else if (*doc == FTB_NO) {
            info->yesno = -1;
            continue;
          } else if (*doc == FTB_INC) {
            info->weight_adjust++;
            continue;
          } else if (*doc == FTB_DEC) {
            info->weight_adjust--;
            continue;
          } else if (*doc == FTB_NEG) {
            info->wasign = !info->wasign;
            continue;
          }
        }
      }

      info->prev = *doc;
      info->yesno = (FTB_YES == ' ') ? 1 : (info->quot != nullptr);
      info->weight_adjust = info->wasign = 0;
    }

    for (word->pos = doc; doc < end;
         doc += (mbl > 0 ? mbl : (mbl < 0 ? -mbl : 1))) {
      mbl = cs->cset->ctype(cs, &ctype, doc, end);

      if (!true_word_char(ctype, *doc)) {
        break;
      }
    }

    /* Be sure *prev is true_word_char. */
    info->prev = 'A';
    word->len = (uint)(doc - word->pos);

    if ((info->trunc = (doc < end && *doc == FTB_TRUNC))) {
      doc++;
    }

    /* We don't check stopword here. */
    *start = doc;
    info->type = FT_TOKEN_WORD;

    return (info->type);
  }

  if (info->quot) {
    *start = doc;
    info->type = FT_TOKEN_RIGHT_PAREN;
  }

  return (info->type);
}
