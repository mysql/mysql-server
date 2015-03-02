/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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


/* A lexical scanner for optimizer hints pseudo-commentary syntax */

#include "sql_lex_hints.h"
#include "lex_hash.h"
#include "parse_tree_helpers.h"
#include "sql_class.h"


Hint_scanner::Hint_scanner(THD *thd_arg,
                           size_t lineno_arg,
                           const char *buf,
                           size_t len)
  : thd(thd_arg),
    cs(thd->charset()),
    is_ansi_quotes(thd->variables.sql_mode & MODE_ANSI_QUOTES),
    lineno(lineno_arg),
    char_classes(cs->state_maps->hint_map),
    input_buf(buf),
    input_buf_end(buf + len),
    ptr(buf),
    prev_token(0),
    raw_yytext(buf),
    yytext(buf),
    yyleng(0)
{}


void hint_lex_init_maps(CHARSET_INFO *cs, hint_lex_char_classes *hint_map)
{
  for (size_t i= 0; i < 256 ; i++)
  {
    if (my_ismb1st(cs, i))
      hint_map[i]= HINT_CHR_MB;
    else if (my_isalpha(cs, i))
      hint_map[i]= HINT_CHR_IDENT;
    else if (my_isdigit(cs, i))
      hint_map[i]= HINT_CHR_DIGIT;
    else if (my_isspace(cs, i))
    {
      DBUG_ASSERT(!my_ismb1st(cs, i));
      hint_map[i]= HINT_CHR_SPACE;
    }
    else
      hint_map[i]= HINT_CHR_CHAR;
  }
  hint_map[(uchar) '*']= HINT_CHR_ASTERISK;
  hint_map[(uchar) '@']= HINT_CHR_AT;
  hint_map[(uchar) '`']= HINT_CHR_BACKQUOTE;
  hint_map[(uchar) '"']= HINT_CHR_DOUBLEQUOTE;
  hint_map[(uchar) '_']= HINT_CHR_IDENT;
  hint_map[(uchar) '$']= HINT_CHR_IDENT;
  hint_map[(uchar) '/']= HINT_CHR_SLASH;
  hint_map[(uchar) '\n']= HINT_CHR_NL;
}


void HINT_PARSER_error(THD *thd, Hint_scanner *scanner, PT_hint_list **,
                       const char *msg)
{
  if (strcmp(msg, "syntax error") == 0)
    msg= ER_THD(thd, ER_WARN_OPTIMIZER_HINT_SYNTAX_ERROR);
  scanner->syntax_warning(msg);
}


/**
  @brief Push a warning message into MySQL error stack with line
  and position information.

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the error stack, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.
*/

void Hint_scanner::syntax_warning(const char *msg) const
{
  /* Push an error into the error stack */
  ErrConvString err(raw_yytext, input_buf_end - raw_yytext,
                    thd->variables.character_set_client);

  push_warning_printf(thd, Sql_condition::SL_WARNING,
                      ER_PARSE_ERROR,  ER_THD(thd, ER_PARSE_ERROR),
                      msg, err.ptr(), lineno);
}

