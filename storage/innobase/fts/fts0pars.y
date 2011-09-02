%{

#include "mem0mem.h"
#include "fts0ast.h"
#include "fts0blex.h"
#include "fts0tlex.h"

extern	int fts_lexer();
extern	int fts_blexer();
extern	int fts_tlexer();

extern int ftserror(const char* p);

/* Required for reentrant parser */ 
#define ftslex	fts_lexer

#define YYERROR_VERBOSE

/* For passing an argument to yyparse() */
#define YYPARSE_PARAM state
#define YYLEX_PARAM ((fts_ast_state_t*)state)->lexer

/*typedef	int	(*fts_scanner)(YYSTYPE* val, yyscan_t yyscanner); */
typedef	int	(*fts_scanner)();

struct fts_lexer_struct {
	fts_scanner	scanner;
	void*		yyscanner;
};

%}

%union {
	int		oper;
	char*		token;
	fts_ast_node_t*	node;
};

/* Enable re-entrant parser */
%pure_parser

%token<oper>	FTS_OPER
%token<token>	FTS_TEXT FTS_TERM FTS_NUMB

%type<node>	prefix term text expr sub_expr expr_lst query

%nonassoc	'+' '-' '~' '<' '>'

%%

query	: expr_lst	{
		$$ = $1;
		((fts_ast_state_t*)state)->root = $$;
	}
	;

expr_lst: /* Empty */	{
		$$ = NULL;
	}

	| expr_lst expr	{
		$$ = $1;

		if (!$$) {
			$$ = fts_ast_create_node_list(state, $2);
		} else {
			fts_ast_add_node($$, $2);
		}
	}

	| expr_lst sub_expr		{
		$$ = $1;

		if (!$$) {
			$$ = fts_ast_create_node_subexp_list(state, $2);
		} else {
			fts_ast_add_node($$, $2);
		}
	}
	;

sub_expr: '(' expr_lst ')'		{
		$$ = $2;
	}

	| prefix '(' expr_lst ')'	{
		$$ = fts_ast_create_node_subexp_list(state, $1);

		if ($3) {
			fts_ast_add_node($$, $3);
		}
	}
	;

expr	: term		{
		$$ = $1;
	}

	| text		{
		$$ = $1;
	}

	| term '*' {
		fts_ast_term_set_wildcard($1);
	}

	| text '@' FTS_NUMB {
		fts_ast_term_set_distance($1, strtoul($3, NULL, 10));
		free($3);
	}

	| prefix term '*' {
		$$ = fts_ast_create_node_list(state, $1);
		fts_ast_add_node($$, $2);
		fts_ast_term_set_wildcard($2);
	}

	| prefix term	{
		$$ = fts_ast_create_node_list(state, $1);
		fts_ast_add_node($$, $2);
	}

	| prefix text '@' FTS_NUMB {
		$$ = fts_ast_create_node_list(state, $1);
		fts_ast_add_node($$, $2);
		fts_ast_term_set_distance($2, strtoul($4, NULL, 10));
		free($4);
	}

	| prefix text {
		$$ = fts_ast_create_node_list(state, $1);
		fts_ast_add_node($$, $2);
	}
	;

prefix	: '-'		{
		$$ = fts_ast_create_node_oper(state, FTS_IGNORE);
	}

	| '+'		{
		$$ = fts_ast_create_node_oper(state, FTS_EXIST);
	}

	| '~'		{
		$$ = fts_ast_create_node_oper(state, FTS_NEGATE);
	}

	| '<'		{
		$$ = fts_ast_create_node_oper(state, FTS_DECR_RATING);
	}

	| '>'		{
		$$ = fts_ast_create_node_oper(state, FTS_INCR_RATING);
	}
	;

term	: FTS_TERM	{
		$$  = fts_ast_create_node_term(state, $1);
		free($1);
	}

	| FTS_NUMB	{
		$$  = fts_ast_create_node_term(state, $1);
		free($1);
	}

	;

text	: FTS_TEXT	{
		$$  = fts_ast_create_node_text(state, $1);
		free($1);
	}
	;
%%

/********************************************************************
*/
int
ftserror(
/*=====*/
	const char*	p)
{
	fprintf(stderr, "%s\n", p);
	return(0);
}

/********************************************************************
Create a fts_lexer_t instance.*/

fts_lexer_t*
fts_lexer_create(
/*=============*/
	ibool		boolean_mode,
	const byte*	query,
	ulint		query_len)
{
	fts_lexer_t*	fts_lexer = ut_malloc(sizeof(fts_lexer_t));

	if (boolean_mode) {
		fts0blex_init(&fts_lexer->yyscanner);
		fts0b_scan_bytes((char*)query, query_len, fts_lexer->yyscanner);
		fts_lexer->scanner = fts_blexer;
		/* FIXME: Debugging */
		/* fts0bset_debug(1 , fts_lexer->yyscanner); */
	} else {
		fts0tlex_init(&fts_lexer->yyscanner);
		fts0t_scan_bytes((char*)query, query_len, fts_lexer->yyscanner);
		fts_lexer->scanner = fts_tlexer;
	}

	return(fts_lexer);
}

/********************************************************************
Free an fts_lexer_t instance.*/
void

fts_lexer_free(
/*===========*/
	fts_lexer_t*	fts_lexer)
{
	if (fts_lexer->scanner == fts_blexer) {
		fts0blex_destroy(fts_lexer->yyscanner);
	} else {
		fts0tlex_destroy(fts_lexer->yyscanner);
	}

	ut_free(fts_lexer);
}

/********************************************************************
Call the appropaiate scanner.*/

int
fts_lexer(
/*======*/
	YYSTYPE*	val,
	fts_lexer_t*	fts_lexer)
{
	return(fts_lexer->scanner(val, fts_lexer->yyscanner));
}

/********************************************************************
Parse the query.*/
int
fts_parse(
/*======*/
	fts_ast_state_t*	state)
{
	extern int ftsparse();	/* To shutup the compiler */

	return(ftsparse(state));
}
