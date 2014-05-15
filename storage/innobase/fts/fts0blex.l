/*****************************************************************************

Copyright (c) 2007, 2014, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**
 * @file fts/fts0blex.l
 * FTS parser lexical analyzer
 *
 * Created 2007/5/9 Sunny Bains
 */

%{

#include "fts0ast.h"
#include "fts0pars.h"

/* Required for reentrant parser */
#define YY_DECL int fts_blexer(YYSTYPE* val, yyscan_t yyscanner)

%}

%option noinput
%option nounput
%option noyywrap
%option nostdinit
%option reentrant
%option never-interactive

%%

[\t ]+	/* Ignore whitespace */ ;

[*()+\-<>~@]		{
	val->oper = fts0bget_text(yyscanner)[0];

	return(val->oper);
}

[0-9]+			{
	val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

	return(FTS_NUMB);
}

[^" \n*()+\-<>~@%]*		{
	val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

	return(FTS_TERM);
}

\"[^\"\n]*\"		{
	val->token = fts_ast_string_create(reinterpret_cast<const byte*>(fts0bget_text(yyscanner)), fts0bget_leng(yyscanner));

	return(FTS_TEXT);
}

\n

%%
