/******************************************************
SQL parser lexical analyzer: input file for the GNU Flex lexer generator

(c) 1997 Innobase Oy

Created 12/14/1997 Heikki Tuuri
*******************************************************/

%{
#define YYSTYPE que_node_t*

#include "univ.i"
#include "pars0pars.h"
#include "pars0grm.h"
#include "pars0sym.h"
#include "mem0mem.h"
#include "os0proc.h"

#define isatty(A)	0
#define malloc(A)	mem_alloc(A)
#define free(A)		mem_free(A)
#define realloc(P, A)	mem_realloc(P, A)
#define exit(A) 	ut_a(0)

#define YY_INPUT(buf, result, max_size) pars_get_lex_chars(buf, &result,\
								max_size)
%}
								
DIGIT	[0-9]
ID	[a-z_A-Z][a-z_A-Z0-9]*
%x comment
%%

{DIGIT}+	{
			yylval = sym_tab_add_int_lit(pars_sym_tab_global,
								atoi(yytext));
			return(PARS_INT_LIT);
}

{DIGIT}+"."{DIGIT}* {
			ut_error;	/* not implemented */

			return(PARS_FLOAT_LIT);
}

"\'"[^\']*"\'"	{
			/* Remove the single quotes around the string */

			yylval = sym_tab_add_str_lit(pars_sym_tab_global,
						(byte*)yytext,
						ut_strlen(yytext));
			return(PARS_STR_LIT);
}

"NULL"		{
			yylval = sym_tab_add_null_lit(pars_sym_tab_global);

			return(PARS_NULL_LIT);
}

"SQL"		{
			/* Implicit cursor name */
			yylval = sym_tab_add_str_lit(pars_sym_tab_global,
							(byte*)"\'SQL\'", 5);
			return(PARS_SQL_TOKEN);
}

"AND"		{
			return(PARS_AND_TOKEN);
}

"OR"		{
			return(PARS_OR_TOKEN);
}

"NOT"		{
			return(PARS_NOT_TOKEN);
}

"PROCEDURE"	{
			return(PARS_PROCEDURE_TOKEN);
}

"IN"		{
			return(PARS_IN_TOKEN);
}

"OUT"		{
			return(PARS_OUT_TOKEN);
}

"INT"		{
	 		return(PARS_INT_TOKEN);
}

"INTEGER"	{
	 		return(PARS_INT_TOKEN);
}

"FLOAT"		{
	 		return(PARS_FLOAT_TOKEN);
}

"CHAR"		{
	 		return(PARS_CHAR_TOKEN);
}

"IS"		{
			return(PARS_IS_TOKEN);
}

"BEGIN"		{
			return(PARS_BEGIN_TOKEN);
}

"END"		{
			return(PARS_END_TOKEN);
}

"IF"		{
			return(PARS_IF_TOKEN);
}

"THEN"		{
			return(PARS_THEN_TOKEN);
}

"ELSE"		{
			return(PARS_ELSE_TOKEN);
}

"ELSIF"		{
			return(PARS_ELSIF_TOKEN);
}

"LOOP"		{
			return(PARS_LOOP_TOKEN);
}

"WHILE"		{
			return(PARS_WHILE_TOKEN);
}

"RETURN"	{
			return(PARS_RETURN_TOKEN);
}

"SELECT"	{
			return(PARS_SELECT_TOKEN);
}

"SUM"		{
			return(PARS_SUM_TOKEN);
}

"COUNT"		{
			return(PARS_COUNT_TOKEN);
}

"DISTINCT"	{
			return(PARS_DISTINCT_TOKEN);
}

"FROM"		{
			return(PARS_FROM_TOKEN);
}

"WHERE"		{
			return(PARS_WHERE_TOKEN);
}

"FOR"		{
			return(PARS_FOR_TOKEN);
}

"CONSISTENT"	{
			return(PARS_CONSISTENT_TOKEN);
}

"READ"		{
			return(PARS_READ_TOKEN);
}

"ORDER"		{
			return(PARS_ORDER_TOKEN);
}

"BY"		{
			return(PARS_BY_TOKEN);
}

"ASC"		{
			return(PARS_ASC_TOKEN);
}

"DESC"		{
			return(PARS_DESC_TOKEN);
}

"INSERT"	{
			return(PARS_INSERT_TOKEN);
}

"INTO"		{
			return(PARS_INTO_TOKEN);
}

"VALUES"	{
			return(PARS_VALUES_TOKEN);
}

"UPDATE"	{
			return(PARS_UPDATE_TOKEN);
}

"SET"		{
			return(PARS_SET_TOKEN);
}

"DELETE"	{
			return(PARS_DELETE_TOKEN);
}

"CURRENT"	{
			return(PARS_CURRENT_TOKEN);
}

"OF"		{
			return(PARS_OF_TOKEN);
}

"CREATE"	{
			return(PARS_CREATE_TOKEN);
}

"TABLE"		{
			return(PARS_TABLE_TOKEN);
}

"INDEX"		{
	 		return(PARS_INDEX_TOKEN);
}

"UNIQUE"	{
	 		return(PARS_UNIQUE_TOKEN);
}

"CLUSTERED"	{
	 		return(PARS_CLUSTERED_TOKEN);
}

"DOES_NOT_FIT_IN_MEMORY"	{
			return(PARS_DOES_NOT_FIT_IN_MEM_TOKEN);
}

"ON"		{
	 		return(PARS_ON_TOKEN);
}

"DECLARE"	{
			return(PARS_DECLARE_TOKEN);
}

"CURSOR"	{
			return(PARS_CURSOR_TOKEN);
}

"OPEN"	{
			return(PARS_OPEN_TOKEN);
}

"FETCH"	{
			return(PARS_FETCH_TOKEN);
}

"CLOSE"	{
			return(PARS_CLOSE_TOKEN);
}

"NOTFOUND"	{
			return(PARS_NOTFOUND_TOKEN);
}

"TO_CHAR"	{
			return(PARS_TO_CHAR_TOKEN);
}

"TO_NUMBER"	{
			return(PARS_TO_NUMBER_TOKEN);
}

"TO_BINARY"	{
			return(PARS_TO_BINARY_TOKEN);
}

"BINARY_TO_NUMBER" {
			return(PARS_BINARY_TO_NUMBER_TOKEN);
}

"SUBSTR"	{
			return(PARS_SUBSTR_TOKEN);
}

"REPLSTR"	{
			return(PARS_REPLSTR_TOKEN);
}

"CONCAT"	{
			return(PARS_CONCAT_TOKEN);
}

"INSTR"		{
			return(PARS_INSTR_TOKEN);
}

"LENGTH"	{
			return(PARS_LENGTH_TOKEN);
}

"SYSDATE"	{
			return(PARS_SYSDATE_TOKEN);
}

"PRINTF"	{
			return(PARS_PRINTF_TOKEN);
}

"ASSERT"	{
			return(PARS_ASSERT_TOKEN);
}

"RND"		{
			return(PARS_RND_TOKEN);
}

"RND_STR"	{
			return(PARS_RND_STR_TOKEN);
}

"ROW_PRINTF"	{
			return(PARS_ROW_PRINTF_TOKEN);
}

"COMMIT"	{
			return(PARS_COMMIT_TOKEN);
}

"ROLLBACK"	{
			return(PARS_ROLLBACK_TOKEN);
}

"WORK"		{
			return(PARS_WORK_TOKEN);
}

{ID}		{
			yylval = sym_tab_add_id(pars_sym_tab_global,
							(byte*)yytext,
							ut_strlen(yytext));
			return(PARS_ID_TOKEN);
}

".."		{
			return(PARS_DDOT_TOKEN);
}

":="		{
			return(PARS_ASSIGN_TOKEN);
}

"<="		{
			return(PARS_LE_TOKEN);
}

">="		{
			return(PARS_GE_TOKEN);
}

"<>"		{
			return(PARS_NE_TOKEN);
}

"("		{

			return((int)(*yytext));
}

"="		{

			return((int)(*yytext));
}

">"		{

			return((int)(*yytext));
}

"<"		{

			return((int)(*yytext));
}

","		{

			return((int)(*yytext));
}

";"		{

			return((int)(*yytext));
}

")"		{

			return((int)(*yytext));
}

"+" 		{

			return((int)(*yytext));
}

"-"		{

			return((int)(*yytext));
}

"*"		{

			return((int)(*yytext));
}

"/"		{

			return((int)(*yytext));
}

"%"		{

			return((int)(*yytext));
}

"{"		{

			return((int)(*yytext));
}

"}"		{

			return((int)(*yytext));
}

"?"		{

			return((int)(*yytext));
}

"/*"			BEGIN(comment); /* eat up comment */

<comment>[^*\n]*
<comment>[^*\n]*\n
<comment>"*"+[^*/\n]*
<comment>"*"+[^*/\n]*\n
<comment>"*"+"/"        BEGIN(INITIAL);

[ \t\n]+		/* eat up whitespace */


.		{
			printf("Unrecognized character: %s\n", yytext);

			ut_error;

			return(0);
}

%%
