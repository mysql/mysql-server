%{

#include "univ.i"
#include "fts0ast.h"
#include "fts0pars.h"

/* Required for reentrant parser */
#define YY_DECL int fts_tlexer(YYSTYPE* val, yyscan_t yyscanner)

%}

%option noinput
%option nounput
%option noyywrap
%option nostdinit
%option reentrant
%option never-interactive


%%

[\t ]+	/* Ignore whitespace */ ;

[*]	{
	val->oper = fts0tget_text(yyscanner)[0];

	return(val->oper);
}

\"[^\"\n]*\"	{
	val->token = strdup(fts0tget_text(yyscanner));

	return(FTS_TEXT);
}

[a-zA-Z0-9\-_]+	{
	val->token = strdup(fts0tget_text(yyscanner));

	return(FTS_TERM);
}

\n

%%
