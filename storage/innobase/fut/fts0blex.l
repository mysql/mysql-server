%{

#include "univ.i"
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
	val->token = strdup(fts0bget_text(yyscanner));

	return(FTS_NUMB);
}

[a-zA-Z_][a-zA-Z0-9_]*	{
	val->token = strdup(fts0bget_text(yyscanner));

	return(FTS_TERM);
}

\"[^\"\n]*\"		{
	val->token = strdup(fts0bget_text(yyscanner));

	return(FTS_TEXT);
}

\n

%%
