/******************************************************
SQL parser: input file for the GNU Bison parser generator

(c) 1997 Innobase Oy

Created 12/14/1997 Heikki Tuuri
*******************************************************/

%{
/* The value of the semantic attribute is a pointer to a query tree node
que_node_t */
#define YYSTYPE que_node_t*
#define alloca	mem_alloc

#include "univ.i"
#include <math.h>
#include "pars0pars.h"
#include "mem0mem.h"
#include "que0types.h"
#include "que0que.h"
#include "row0sel.h"

/* #define __STDC__ */

int
yylex(void);
%}
     
%token PARS_INT_LIT
%token PARS_FLOAT_LIT
%token PARS_STR_LIT
%token PARS_NULL_LIT
%token PARS_ID_TOKEN
%token PARS_AND_TOKEN
%token PARS_OR_TOKEN
%token PARS_NOT_TOKEN
%token PARS_GE_TOKEN
%token PARS_LE_TOKEN
%token PARS_NE_TOKEN
%token PARS_PROCEDURE_TOKEN
%token PARS_IN_TOKEN
%token PARS_OUT_TOKEN
%token PARS_INT_TOKEN
%token PARS_INTEGER_TOKEN
%token PARS_FLOAT_TOKEN
%token PARS_CHAR_TOKEN
%token PARS_IS_TOKEN
%token PARS_BEGIN_TOKEN
%token PARS_END_TOKEN
%token PARS_IF_TOKEN
%token PARS_THEN_TOKEN
%token PARS_ELSE_TOKEN
%token PARS_ELSIF_TOKEN
%token PARS_LOOP_TOKEN
%token PARS_WHILE_TOKEN
%token PARS_RETURN_TOKEN
%token PARS_SELECT_TOKEN
%token PARS_SUM_TOKEN
%token PARS_COUNT_TOKEN
%token PARS_DISTINCT_TOKEN
%token PARS_FROM_TOKEN
%token PARS_WHERE_TOKEN
%token PARS_FOR_TOKEN
%token PARS_DDOT_TOKEN
%token PARS_CONSISTENT_TOKEN
%token PARS_READ_TOKEN
%token PARS_ORDER_TOKEN
%token PARS_BY_TOKEN
%token PARS_ASC_TOKEN
%token PARS_DESC_TOKEN
%token PARS_INSERT_TOKEN
%token PARS_INTO_TOKEN
%token PARS_VALUES_TOKEN
%token PARS_UPDATE_TOKEN
%token PARS_SET_TOKEN
%token PARS_DELETE_TOKEN
%token PARS_CURRENT_TOKEN
%token PARS_OF_TOKEN
%token PARS_CREATE_TOKEN
%token PARS_TABLE_TOKEN
%token PARS_INDEX_TOKEN
%token PARS_UNIQUE_TOKEN
%token PARS_CLUSTERED_TOKEN
%token PARS_DOES_NOT_FIT_IN_MEM_TOKEN
%token PARS_ON_TOKEN
%token PARS_ASSIGN_TOKEN
%token PARS_DECLARE_TOKEN
%token PARS_CURSOR_TOKEN
%token PARS_SQL_TOKEN
%token PARS_OPEN_TOKEN
%token PARS_FETCH_TOKEN
%token PARS_CLOSE_TOKEN
%token PARS_NOTFOUND_TOKEN
%token PARS_TO_CHAR_TOKEN
%token PARS_TO_NUMBER_TOKEN
%token PARS_TO_BINARY_TOKEN
%token PARS_BINARY_TO_NUMBER_TOKEN
%token PARS_SUBSTR_TOKEN
%token PARS_REPLSTR_TOKEN
%token PARS_CONCAT_TOKEN
%token PARS_INSTR_TOKEN
%token PARS_LENGTH_TOKEN
%token PARS_SYSDATE_TOKEN
%token PARS_PRINTF_TOKEN
%token PARS_ASSERT_TOKEN
%token PARS_RND_TOKEN
%token PARS_RND_STR_TOKEN
%token PARS_ROW_PRINTF_TOKEN
%token PARS_COMMIT_TOKEN
%token PARS_ROLLBACK_TOKEN
%token PARS_WORK_TOKEN

%left PARS_AND_TOKEN PARS_OR_TOKEN
%left PARS_NOT_TOKEN
%left '=' '<' '>' PARS_GE_TOKEN PARS_LE_TOKEN
%left '-' '+'
%left '*' '/'
%left NEG     /* negation--unary minus */
%left '%'

/* Grammar follows */
%%

statement:
	procedure_definition ';'
	| stored_procedure_call
	| predefined_procedure_call ';'
	| while_statement ';'
	| for_statement ';'
	| if_statement ';'
	| return_statement ';'
	| assignment_statement ';'
	| select_statement ';'
	| insert_statement ';'
	| row_printf_statement ';'
	| delete_statement_searched ';'
	| delete_statement_positioned ';'
	| update_statement_searched ';'
	| update_statement_positioned ';'
	| open_cursor_statement ';'
	| fetch_statement ';'
	| close_cursor_statement ';'
	| commit_statement ';'
	| rollback_statement ';'
	| create_table ';'
	| create_index ';'
;

statement_list:
	statement		{ $$ = que_node_list_add_last(NULL, $1); }
	| statement_list statement
				{ $$ = que_node_list_add_last($1, $2); }
;

exp:
	PARS_ID_TOKEN		{ $$ = $1;}
	| function_name '(' exp_list ')'
				{ $$ = pars_func($1, $3); }
	| PARS_INT_LIT		{ $$ = $1;}
	| PARS_FLOAT_LIT	{ $$ = $1;}
	| PARS_STR_LIT		{ $$ = $1;}
	| PARS_NULL_LIT		{ $$ = $1;}
	| PARS_SQL_TOKEN	{ $$ = $1;}
	| exp '+' exp        	{ $$ = pars_op('+', $1, $3); }
	| exp '-' exp        	{ $$ = pars_op('-', $1, $3); }
	| exp '*' exp        	{ $$ = pars_op('*', $1, $3); }
	| exp '/' exp        	{ $$ = pars_op('/', $1, $3); }
	| '-' exp %prec NEG 	{ $$ = pars_op('-', $2, NULL); }
	| '(' exp ')'        	{ $$ = $2; }
	| exp '=' exp		{ $$ = pars_op('=', $1, $3); }
	| exp '<' exp		{ $$ = pars_op('<', $1, $3); }
	| exp '>' exp		{ $$ = pars_op('>', $1, $3); }
	| exp PARS_GE_TOKEN exp	{ $$ = pars_op(PARS_GE_TOKEN, $1, $3); }
	| exp PARS_LE_TOKEN exp	{ $$ = pars_op(PARS_LE_TOKEN, $1, $3); }
	| exp PARS_NE_TOKEN exp	{ $$ = pars_op(PARS_NE_TOKEN, $1, $3); }
	| exp PARS_AND_TOKEN exp{ $$ = pars_op(PARS_AND_TOKEN, $1, $3); }
	| exp PARS_OR_TOKEN exp	{ $$ = pars_op(PARS_OR_TOKEN, $1, $3); }
	| PARS_NOT_TOKEN exp	{ $$ = pars_op(PARS_NOT_TOKEN, $2, NULL); }
	| PARS_ID_TOKEN '%' PARS_NOTFOUND_TOKEN
				{ $$ = pars_op(PARS_NOTFOUND_TOKEN, $1, NULL); }
	| PARS_SQL_TOKEN '%' PARS_NOTFOUND_TOKEN
				{ $$ = pars_op(PARS_NOTFOUND_TOKEN, $1, NULL); }
;

function_name:
	PARS_TO_CHAR_TOKEN	{ $$ = &pars_to_char_token; }
	| PARS_TO_NUMBER_TOKEN	{ $$ = &pars_to_number_token; }
	| PARS_TO_BINARY_TOKEN	{ $$ = &pars_to_binary_token; }
	| PARS_BINARY_TO_NUMBER_TOKEN
				{ $$ = &pars_binary_to_number_token; }
	| PARS_SUBSTR_TOKEN	{ $$ = &pars_substr_token; }
	| PARS_CONCAT_TOKEN	{ $$ = &pars_concat_token; }
	| PARS_INSTR_TOKEN	{ $$ = &pars_instr_token; }
	| PARS_LENGTH_TOKEN	{ $$ = &pars_length_token; }
	| PARS_SYSDATE_TOKEN	{ $$ = &pars_sysdate_token; }
	| PARS_RND_TOKEN	{ $$ = &pars_rnd_token; }
	| PARS_RND_STR_TOKEN	{ $$ = &pars_rnd_str_token; }
;

question_mark_list:
	/* Nothing */
	| '?'
	| question_mark_list ',' '?'
;

stored_procedure_call:
	'{' PARS_ID_TOKEN '(' question_mark_list ')' '}'
				{ $$ = pars_stored_procedure_call($2); }
;

predefined_procedure_call:
	predefined_procedure_name '(' exp_list ')'
				{ $$ = pars_procedure_call($1, $3); }
;

predefined_procedure_name:
	PARS_REPLSTR_TOKEN	{ $$ = &pars_replstr_token; }
	| PARS_PRINTF_TOKEN	{ $$ = &pars_printf_token; }
	| PARS_ASSERT_TOKEN	{ $$ = &pars_assert_token; }
;

table_list:
	PARS_ID_TOKEN		{ $$ = que_node_list_add_last(NULL, $1); }
	| table_list ',' PARS_ID_TOKEN
				{ $$ = que_node_list_add_last($1, $3); }
;

variable_list:
	/* Nothing */		{ $$ = NULL; }
	| PARS_ID_TOKEN		{ $$ = que_node_list_add_last(NULL, $1); }
	| variable_list ',' PARS_ID_TOKEN
				{ $$ = que_node_list_add_last($1, $3); }
;

exp_list:
	/* Nothing */		{ $$ = NULL; }
	| exp			{ $$ = que_node_list_add_last(NULL, $1);}
	| exp_list ',' exp	{ $$ = que_node_list_add_last($1, $3); }
;

select_item:
	exp			{ $$ = $1; }
	| PARS_COUNT_TOKEN '(' '*' ')'
				{ $$ = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); }
	| PARS_COUNT_TOKEN '(' PARS_DISTINCT_TOKEN PARS_ID_TOKEN ')'
				{ $$ = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, $4)))); }
	| PARS_SUM_TOKEN '(' exp ')'
				{ $$ = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									$3)); }
;
							
select_item_list:
	/* Nothing */		{ $$ = NULL; }
	| select_item		{ $$ = que_node_list_add_last(NULL, $1); }
	| select_item_list ',' select_item
				{ $$ = que_node_list_add_last($1, $3); }
;
							
select_list:
	'*'			{ $$ = pars_select_list(&pars_star_denoter,
								NULL); }
	| select_item_list PARS_INTO_TOKEN variable_list
				{ $$ = pars_select_list($1, $3); }
	| select_item_list	{ $$ = pars_select_list($1, NULL); }
;

search_condition:
	/* Nothing */		{ $$ = NULL; }
	| PARS_WHERE_TOKEN exp	{ $$ = $2; }
;

for_update_clause:
	/* Nothing */		{ $$ = NULL; }
	| PARS_FOR_TOKEN PARS_UPDATE_TOKEN
				{ $$ = &pars_update_token; }
;

consistent_read_clause:
	/* Nothing */		{ $$ = NULL; }
	| PARS_CONSISTENT_TOKEN PARS_READ_TOKEN
				{ $$ = &pars_consistent_token; }
;

order_direction:
	/* Nothing */		{ $$ = &pars_asc_token; }
	| PARS_ASC_TOKEN	{ $$ = &pars_asc_token; }
	| PARS_DESC_TOKEN	{ $$ = &pars_desc_token; }
;

order_by_clause:
	/* Nothing */		{ $$ = NULL; }
	| PARS_ORDER_TOKEN PARS_BY_TOKEN PARS_ID_TOKEN order_direction
				{ $$ = pars_order_by($3, $4); }
;

select_statement:
	PARS_SELECT_TOKEN select_list
	PARS_FROM_TOKEN table_list
	search_condition
	for_update_clause
	consistent_read_clause
	order_by_clause		{ $$ = pars_select_statement($2, $4, $5,
								$6, $7, $8); }
;

insert_statement_start:
	PARS_INSERT_TOKEN PARS_INTO_TOKEN
	PARS_ID_TOKEN		{ $$ = $3; }
;

insert_statement:
	insert_statement_start PARS_VALUES_TOKEN '(' exp_list ')'
				{ $$ = pars_insert_statement($1, $4, NULL); }
	| insert_statement_start select_statement
				{ $$ = pars_insert_statement($1, NULL, $2); }
;

column_assignment:
	PARS_ID_TOKEN '=' exp	{ $$ = pars_column_assignment($1, $3); }
;

column_assignment_list:
	column_assignment	{ $$ = que_node_list_add_last(NULL, $1); }
	| column_assignment_list ',' column_assignment
				{ $$ = que_node_list_add_last($1, $3); }
;

cursor_positioned:
	PARS_WHERE_TOKEN
	PARS_CURRENT_TOKEN PARS_OF_TOKEN
	PARS_ID_TOKEN 		{ $$ = $4; }
;

update_statement_start:
	PARS_UPDATE_TOKEN PARS_ID_TOKEN
	PARS_SET_TOKEN
	column_assignment_list	{ $$ = pars_update_statement_start(FALSE,
								$2, $4); }
;

update_statement_searched:
	update_statement_start
	search_condition	{ $$ = pars_update_statement($1, NULL, $2); }
;

update_statement_positioned:
	update_statement_start
	cursor_positioned	{ $$ = pars_update_statement($1, $2, NULL); }
;

delete_statement_start:
	PARS_DELETE_TOKEN PARS_FROM_TOKEN
	PARS_ID_TOKEN		{ $$ = pars_update_statement_start(TRUE,
								$3, NULL); }
;

delete_statement_searched:
	delete_statement_start
	search_condition	{ $$ = pars_update_statement($1, NULL, $2); }
;

delete_statement_positioned:
	delete_statement_start
	cursor_positioned	{ $$ = pars_update_statement($1, $2, NULL); }
;
	
row_printf_statement:
	PARS_ROW_PRINTF_TOKEN select_statement
				{ $$ = pars_row_printf_statement($2); }
;

assignment_statement:
	PARS_ID_TOKEN PARS_ASSIGN_TOKEN exp
				{ $$ = pars_assignment_statement($1, $3); }
;

elsif_element:
	PARS_ELSIF_TOKEN
	exp PARS_THEN_TOKEN statement_list
				{ $$ = pars_elsif_element($2, $4); }
;

elsif_list:
	elsif_element		{ $$ = que_node_list_add_last(NULL, $1); }
	| elsif_list elsif_element
				{ $$ = que_node_list_add_last($1, $2); }
;

else_part:
	/* Nothing */		{ $$ = NULL; }
	| PARS_ELSE_TOKEN statement_list
				{ $$ = $2; }
	| elsif_list		{ $$ = $1; }
;

if_statement:
	PARS_IF_TOKEN exp PARS_THEN_TOKEN statement_list
	else_part
	PARS_END_TOKEN PARS_IF_TOKEN
				{ $$ = pars_if_statement($2, $4, $5); }
;

while_statement:
	PARS_WHILE_TOKEN exp PARS_LOOP_TOKEN statement_list
	PARS_END_TOKEN PARS_LOOP_TOKEN
				{ $$ = pars_while_statement($2, $4); }
;

for_statement:
	PARS_FOR_TOKEN PARS_ID_TOKEN PARS_IN_TOKEN
	exp PARS_DDOT_TOKEN exp
	PARS_LOOP_TOKEN statement_list
	PARS_END_TOKEN PARS_LOOP_TOKEN
				{ $$ = pars_for_statement($2, $4, $6, $8); }
;

return_statement:
	PARS_RETURN_TOKEN	{ $$ = pars_return_statement(); }
;

open_cursor_statement:
	PARS_OPEN_TOKEN PARS_ID_TOKEN
				{ $$ = pars_open_statement(
						ROW_SEL_OPEN_CURSOR, $2); }
;

close_cursor_statement:
	PARS_CLOSE_TOKEN PARS_ID_TOKEN
				{ $$ = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR, $2); }
;

fetch_statement:
	PARS_FETCH_TOKEN PARS_ID_TOKEN PARS_INTO_TOKEN variable_list
				{ $$ = pars_fetch_statement($2, $4); }
;

column_def:
	PARS_ID_TOKEN type_name	{ $$ = pars_column_def($1, $2); }
;

column_def_list:
	column_def		{ $$ = que_node_list_add_last(NULL, $1); }
	| column_def_list ',' column_def
				{ $$ = que_node_list_add_last($1, $3); }
;

not_fit_in_memory:
	/* Nothing */		{ $$ = NULL; }
	| PARS_DOES_NOT_FIT_IN_MEM_TOKEN
				{ $$ = &pars_int_token;
					/* pass any non-NULL pointer */ }
;
		
create_table:
	PARS_CREATE_TOKEN PARS_TABLE_TOKEN
	PARS_ID_TOKEN '(' column_def_list ')'
	not_fit_in_memory	{ $$ = pars_create_table($3, $5, $7); }
;

column_list:
	PARS_ID_TOKEN		{ $$ = que_node_list_add_last(NULL, $1); }
	| column_list ',' PARS_ID_TOKEN
				{ $$ = que_node_list_add_last($1, $3); }
;

unique_def:
	/* Nothing */		{ $$ = NULL; }
	| PARS_UNIQUE_TOKEN	{ $$ = &pars_unique_token; }
;

clustered_def:
	/* Nothing */		{ $$ = NULL; }
	| PARS_CLUSTERED_TOKEN	{ $$ = &pars_clustered_token; }
;

create_index:
	PARS_CREATE_TOKEN unique_def
	clustered_def
	PARS_INDEX_TOKEN
	PARS_ID_TOKEN PARS_ON_TOKEN PARS_ID_TOKEN
	'(' column_list ')'	{ $$ = pars_create_index($2, $3, $5, $7, $9); }
;

commit_statement:
	PARS_COMMIT_TOKEN PARS_WORK_TOKEN
				{ $$ = pars_commit_statement(); }
;

rollback_statement:
	PARS_ROLLBACK_TOKEN PARS_WORK_TOKEN
				{ $$ = pars_rollback_statement(); }
;

type_name:
	PARS_INT_TOKEN		{ $$ = &pars_int_token; }
	| PARS_CHAR_TOKEN	{ $$ = &pars_char_token; }
;

parameter_declaration:
	PARS_ID_TOKEN PARS_IN_TOKEN type_name
				{ $$ = pars_parameter_declaration($1,
							PARS_INPUT, $3); }
	| PARS_ID_TOKEN PARS_OUT_TOKEN type_name
				{ $$ = pars_parameter_declaration($1,
							PARS_OUTPUT, $3); }
;

parameter_declaration_list:
	/* Nothing */		{ $$ = NULL; }
	| parameter_declaration	{ $$ = que_node_list_add_last(NULL, $1); }
	| parameter_declaration_list ',' parameter_declaration
				{ $$ = que_node_list_add_last($1, $3); }
;

variable_declaration:
	PARS_ID_TOKEN type_name ';'
				{ $$ = pars_variable_declaration($1, $2); }
;

variable_declaration_list:
	/* Nothing */		
	| variable_declaration	
	| variable_declaration_list variable_declaration
;

cursor_declaration:
	PARS_DECLARE_TOKEN PARS_CURSOR_TOKEN PARS_ID_TOKEN
	PARS_IS_TOKEN select_statement ';'
				{ $$ = pars_cursor_declaration($3, $5); }
;

declaration_list:
	/* Nothing */
	| cursor_declaration
	| declaration_list cursor_declaration
;

procedure_definition:
	PARS_PROCEDURE_TOKEN PARS_ID_TOKEN '(' parameter_declaration_list ')'
	PARS_IS_TOKEN
	variable_declaration_list
	declaration_list
	PARS_BEGIN_TOKEN
	statement_list
	PARS_END_TOKEN		{ $$ = pars_procedure_definition($2, $4,
								$10); }
;
	
%%
