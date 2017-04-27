/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/parse_error.h"

#include <sys/types.h>

#include "check_stack.h"
#include "derror.h" // ER_THD
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql_class.h"
#include "sql_const.h"
#include "sql_error.h"
#include "sql_lex.h"
#include "system_variables.h"


/**
  Push an error message into MySQL diagnostic area with line number and position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a syntax error is discovered according to the Bison grammar.
  Unlike the syntax_error_at() function, the error position points to the last
  parsed token.

  @param thd    Thread handler.
  @param s      Error message text.
*/
void my_syntax_error(THD *thd, const char *s)
{
  Lex_input_stream *lip= & thd->m_parser_state->m_lip;

  const char *yytext= lip->get_tok_start();
  if (!yytext)
    yytext= "";

  /* Push an error into the diagnostic area */
  ErrConvString err(yytext, thd->variables.character_set_client);
  my_printf_error(ER_PARSE_ERROR,  ER_THD(thd, ER_PARSE_ERROR), MYF(0), s,
                  err.ptr(), lip->yylineno);
}


/**
  Push an error message into MySQL diagnostic area with line number and position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the error stack, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.
  Unlike the my_syntax_error() function, the error position points to the 
  @c location value.

  @param thd            Thread handler.
  @param location       YYSTYPE object: error position
  @param s              error message: NULL default means ER(ER_SYNTAX_ERROR)
*/
void syntax_error_at(THD *thd, const YYLTYPE &location, const char *s)
{
  uint lineno= location.raw.start ?
    thd->m_parser_state->m_lip.get_lineno(location.raw.start) : 1;
  const char *pos= location.raw.start ? location.raw.start : "";
  ErrConvString err(pos, thd->variables.character_set_client);
  my_printf_error(ER_PARSE_ERROR, ER_THD(thd, ER_PARSE_ERROR), MYF(0),
                  s ? s : ER_THD(thd, ER_SYNTAX_ERROR), err.ptr(), lineno);
}


/**
  Push an error message into MySQL diagnostic area with line number and position

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the error stack, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.

  @param thd            YYTHD
  @param location       YYSTYPE object: error position
  @param format         An error message format string.
  @param args           Arguments to the format string.
*/

void vsyntax_error_at(THD *thd, const YYLTYPE &location,
                     const char *format, va_list args)
{
  char buff[MYSQL_ERRMSG_SIZE];
  if (check_stack_overrun(thd, STACK_MIN_SIZE, (uchar *)buff))
    return;

  uint lineno= location.raw.start ?
    thd->m_parser_state->m_lip.get_lineno(location.raw.start) : 1;
  const char *pos= location.raw.start ? location.raw.start : "";
  ErrConvString err(pos, thd->variables.character_set_client);
  (void) my_vsnprintf(buff, sizeof(buff), format, args);
  my_printf_error(ER_PARSE_ERROR, ER_THD(thd, ER_PARSE_ERROR), MYF(0), buff,
                  err.ptr(), lineno);
}
