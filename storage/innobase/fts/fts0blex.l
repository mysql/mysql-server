%top{
/*****************************************************************************

Copyright (c) 2007, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**
 * @file fts/fts0blex.l
 * FTS parser lexical analyzer
 *
 * Created 2007/5/9 Sunny Bains
 */

#include "univ.i"
}

%{
#include "fts0ast.h"
#include "fts0pars.h"

/* Required for reentrant parser */
#define YY_DECL int fts_blexer(YYSTYPE* val, yyscan_t yyscanner)
#define exit(A)   ut_error

%}

%option noinput
%option nounput
%option noyywrap
%option nostdinit
%option reentrant
%option never-interactive

%%

[\t ]+  /* Ignore whitespace */ ;

[*()+\-<>~@]            {
        val->oper = fts0bget_text(yyscanner)[0];

        return(val->oper);
}

[0-9]+                  {
        val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

        return(FTS_NUMB);
}

[^" \n*()+\-<>~@%]*             {
        val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

        return(FTS_TERM);
}

\"[^\"\n]*\"            {
        val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

        return(FTS_TEXT);
}

\n

%%
