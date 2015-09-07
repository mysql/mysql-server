/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

/*
  Optimizer hint parser grammar
*/

%{
#include "sql_class.h"
#include "parse_tree_hints.h"
#include "sql_lex_hints.h"
#include "sql_const.h"

#define NEW_PTN new (thd->mem_root)
%}

%pure-parser
%yacc

%parse-param { class THD *thd }
%parse-param { class Hint_scanner *scanner }
%parse-param { class PT_hint_list **ret }

%lex-param { class Hint_scanner *scanner }

%expect 0


/* Hint keyword tokens */

%token MAX_EXECUTION_TIME_HINT

%token BKA_HINT
%token BNL_HINT
%token DUPSWEEDOUT_HINT
%token FIRSTMATCH_HINT
%token INTOEXISTS_HINT
%token LOOSESCAN_HINT
%token MATERIALIZATION_HINT
%token NO_BKA_HINT
%token NO_BNL_HINT
%token NO_ICP_HINT
%token NO_MRR_HINT
%token NO_RANGE_OPTIMIZATION_HINT
%token NO_SEMIJOIN_HINT
%token MRR_HINT
%token QB_NAME_HINT
%token SEMIJOIN_HINT
%token SUBQUERY_HINT

/* Other tokens */

%token HINT_ARG_NUMBER
%token HINT_ARG_IDENT
%token HINT_ARG_QB_NAME

%token HINT_CLOSE
%token HINT_ERROR

/* Types */
%type <hint_type>
  key_level_hint_type_on
  key_level_hint_type_off
  table_level_hint_type_on
  table_level_hint_type_off

%type <hint>
  hint
  max_execution_time_hint
  index_level_hint
  table_level_hint
  qb_level_hint
  qb_name_hint

%type <hint_list> hint_list

%type <hint_string> hint_param_index

%type <hint_param_index_list> hint_param_index_list opt_hint_param_index_list

%type <hint_param_table>
  hint_param_table
  hint_param_table_ext
  hint_param_table_empty_qb

%type <hint_param_table_list>
  hint_param_table_list
  opt_hint_param_table_list
  hint_param_table_list_empty_qb
  opt_hint_param_table_list_empty_qb

%type <hint_string>
  HINT_ARG_IDENT
  HINT_ARG_NUMBER
  HINT_ARG_QB_NAME
  opt_qb_name

%type <ulong_num>
  semijoin_strategy semijoin_strategies
  subquery_strategy
%%


start:
          hint_list HINT_CLOSE
          { *ret= $1; }
        | hint_list error HINT_CLOSE
          { *ret= $1; }
        | error HINT_CLOSE
          { *ret= NULL; }
        ;

hint_list:
          hint
          {
            $$= NEW_PTN PT_hint_list(thd->mem_root);
            if ($$ == NULL || $$->push_back($1))
              YYABORT; // OOM
          }
        | hint_list hint
          {
            $1->push_back($2);
            $$= $1;
          }
        ;

hint:
          index_level_hint
        | table_level_hint
        | qb_level_hint
        | qb_name_hint
        | max_execution_time_hint
        ;


max_execution_time_hint:
          MAX_EXECUTION_TIME_HINT '(' HINT_ARG_NUMBER ')'
          {
            int error;
            char *end= const_cast<char *>($3.str + $3.length);
            longlong n= my_strtoll10($3.str, &end, &error);
            if (error != 0 || end != $3.str + $3.length || n > UINT_MAX32)
            {
              scanner->syntax_warning(ER_THD(thd,
                                             ER_WARN_BAD_MAX_EXECUTION_TIME));
              $$= NULL;
            }
            else
            {
              $$= NEW_PTN PT_hint_max_execution_time(n);
              if ($$ == NULL)
                YYABORT; // OOM
            }
          }
        ;


opt_hint_param_table_list:
          /* empty */ { $$.init(thd->mem_root); }
        | hint_param_table_list
        ;

hint_param_table_list:
          hint_param_table
          {
            $$.init(thd->mem_root);
            if ($$.push_back($1))
              YYABORT; // OOM
          }
        | hint_param_table_list ',' hint_param_table
          {
            if ($1.push_back($3))
              YYABORT; // OOM
            $$= $1;
          }
        ;

opt_hint_param_table_list_empty_qb:
          /* empty */ { $$.init(thd->mem_root); }
        | hint_param_table_list_empty_qb
        ;

hint_param_table_list_empty_qb:
          hint_param_table_empty_qb
          {
            $$.init(thd->mem_root);
            if ($$.push_back($1))
              YYABORT; // OOM
          }
        | hint_param_table_list_empty_qb ',' hint_param_table_empty_qb
          {
            if ($1.push_back($3))
              YYABORT; // OOM
            $$= $1;
          }
        ;

opt_hint_param_index_list:
          /* empty */ { $$.init(thd->mem_root); }
        | hint_param_index_list
        ;

hint_param_index_list:
          hint_param_index
          {
            $$.init(thd->mem_root);
            if ($$.push_back($1))
              YYABORT; // OOM
          }
        | hint_param_index_list ',' hint_param_index
          {
            if ($1.push_back($3))
              YYABORT; // OOM
            $$= $1;
          }
        ;

hint_param_index:
          HINT_ARG_IDENT
        ;

hint_param_table_empty_qb:
          HINT_ARG_IDENT
          {
            $$.table= $1;
            $$.opt_query_block= NULL_CSTR;
          }
        ;

hint_param_table:
          HINT_ARG_IDENT opt_qb_name
          {
            $$.table= $1;
            $$.opt_query_block= $2;
          }
        ;

hint_param_table_ext:
          hint_param_table
        | HINT_ARG_QB_NAME HINT_ARG_IDENT
          {
            $$.table= $2;
            $$.opt_query_block= $1;
          }
        ;

opt_qb_name:
          /* empty */ { $$= NULL_CSTR; }
        | HINT_ARG_QB_NAME
        ;

qb_level_hint:
          SEMIJOIN_HINT '(' opt_qb_name semijoin_strategies ')'
          {
            $$= NEW_PTN PT_qb_level_hint($3, TRUE, SEMIJOIN_HINT_ENUM, $4);
            if ($$ == NULL)
              YYABORT; // OOM
          }
          |
          NO_SEMIJOIN_HINT '(' opt_qb_name semijoin_strategies ')'
          {
            $$= NEW_PTN PT_qb_level_hint($3, FALSE, SEMIJOIN_HINT_ENUM, $4);
            if ($$ == NULL)
              YYABORT; // OOM
          }
          |
          SUBQUERY_HINT '(' opt_qb_name subquery_strategy ')'
          {
            $$= NEW_PTN PT_qb_level_hint($3, TRUE, SUBQUERY_HINT_ENUM, $4);
            if ($$ == NULL)
              YYABORT; // OOM
          }
          ;

semijoin_strategies:
          /* empty */ { $$= 0; }
	| semijoin_strategy
          {
            $$= $1;
          }
        | semijoin_strategies ',' semijoin_strategy
          {
            $$= $1 | $3;
          }
        ;

semijoin_strategy:
          FIRSTMATCH_HINT      { $$= OPTIMIZER_SWITCH_FIRSTMATCH; }
        | LOOSESCAN_HINT       { $$= OPTIMIZER_SWITCH_LOOSE_SCAN; }
        | MATERIALIZATION_HINT { $$= OPTIMIZER_SWITCH_MATERIALIZATION; }
        | DUPSWEEDOUT_HINT     { $$= OPTIMIZER_SWITCH_DUPSWEEDOUT; }
        ;

subquery_strategy:
          MATERIALIZATION_HINT { $$=
                                   Item_exists_subselect::EXEC_MATERIALIZATION; }
        | INTOEXISTS_HINT      { $$= Item_exists_subselect::EXEC_EXISTS; }
        ;


table_level_hint:
          table_level_hint_type_on '(' opt_hint_param_table_list ')'
          {
            $$= NEW_PTN PT_table_level_hint(NULL_CSTR, $3, TRUE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | table_level_hint_type_on
          '(' HINT_ARG_QB_NAME opt_hint_param_table_list_empty_qb ')'
          {
            $$= NEW_PTN PT_table_level_hint($3, $4, TRUE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | table_level_hint_type_off '(' opt_hint_param_table_list ')'
          {
            $$= NEW_PTN PT_table_level_hint(NULL_CSTR, $3, FALSE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | table_level_hint_type_off
          '(' HINT_ARG_QB_NAME opt_hint_param_table_list_empty_qb ')'
          {
            $$= NEW_PTN PT_table_level_hint($3, $4, FALSE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        ;

index_level_hint:
          key_level_hint_type_on
          '(' hint_param_table_ext opt_hint_param_index_list ')'
          {
            $$= NEW_PTN PT_key_level_hint($3, $4, TRUE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | key_level_hint_type_off
          '(' hint_param_table_ext opt_hint_param_index_list ')'
          {
            $$= NEW_PTN PT_key_level_hint($3, $4, FALSE, $1);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        ;

table_level_hint_type_on:
          BKA_HINT
          {
            $$= BKA_HINT_ENUM;
          }
        | BNL_HINT
          {
            $$= BNL_HINT_ENUM;
          }
        ;

table_level_hint_type_off:
          NO_BKA_HINT
          {
            $$= BKA_HINT_ENUM;
          }
        | NO_BNL_HINT
          {
            $$= BNL_HINT_ENUM;
          }
        ;

key_level_hint_type_on:
          MRR_HINT
          {
            $$= MRR_HINT_ENUM;
          }
        | NO_RANGE_OPTIMIZATION_HINT
          {
            $$= NO_RANGE_HINT_ENUM;
          }
        ;

key_level_hint_type_off:
          NO_ICP_HINT
          {
            $$= ICP_HINT_ENUM;
          }
        | NO_MRR_HINT
          {
            $$= MRR_HINT_ENUM;
          }
        ;

qb_name_hint:
          QB_NAME_HINT '(' HINT_ARG_IDENT ')'
          {
            $$= NEW_PTN PT_hint_qb_name($3);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        ;
