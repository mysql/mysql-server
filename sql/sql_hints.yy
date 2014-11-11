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

%token DEBUG_HINT1
%token DEBUG_HINT2
%token DEBUG_HINT3

/* Other tokens */

%token HINT_ARG_NUMBER
%token HINT_ARG_IDENT
%token HINT_ARG_QB_NAME

%token HINT_CLOSE
%token HINT_ERROR

/* Types */

%type <hint> hint
%type <hint_list> hint_list

%type <hint_string> hint_param_index
%type <hint_param_index_list> hint_param_index_list opt_hint_param_index_list
%type <hint_param_table> hint_param_table
%type <hint_param_table_list> hint_param_table_list

%type <hint>
  max_execution_time_hint
  debug_hint

%type <hint_string>
  HINT_ARG_IDENT
  HINT_ARG_NUMBER
  HINT_ARG_QB_NAME
  opt_qb_name

%%


start:
          hint_list HINT_CLOSE
          { *ret= $1; }
        | hint_list error HINT_CLOSE
          { *ret= $1; }
        | error HINT_CLOSE
          { *ret= NULL; }
        | error
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
          max_execution_time_hint
        | debug_hint
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

hint_param_table_list:
          hint_param_table
          {
            $$= (Hint_param_table_list *)
                thd->alloc(sizeof(Hint_param_table_list));
            if ($$ == NULL)
              YYABORT;
            new ($$) Hint_param_table_list(thd->mem_root);
            if ($$->push_back($1))
              YYABORT; // OOM
          }
        | hint_param_table_list hint_param_table
          {
            if ($1->push_back($2))
              YYABORT; // OOM
            $$= $1;
          }
        ;

opt_hint_param_index_list:
          /* empty */ { $$= NULL; }
        | hint_param_index_list
        ;

hint_param_index_list:
          hint_param_index
          {
            $$= (Hint_param_index_list *)
                thd->alloc(sizeof(Hint_param_index_list));
            if ($$ == NULL)
              YYABORT; // OOM
            new ($$) Hint_param_index_list(thd->mem_root);
            if ($$->push_back($1))
              YYABORT; // OOM
          }
        | hint_param_index_list hint_param_index
          {
            if ($1->push_back($2))
              YYABORT; // OOM
            $$= $1;
          }
        ;

hint_param_index:
          HINT_ARG_IDENT
        ;

hint_param_table:
          HINT_ARG_IDENT opt_qb_name
          {
            $$.table= $1;
            $$.opt_query_block= $2;
          }
        ;

opt_qb_name:
          /* empty */ { $$= NULL_CSTR; }
        | HINT_ARG_QB_NAME
        ;

debug_hint:
          DEBUG_HINT1 '(' opt_qb_name hint_param_table_list ')'
          {
            $$= NEW_PTN PT_hint_debug1($3, $4);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | DEBUG_HINT2 '(' opt_hint_param_index_list ')'
          {
            $$= NEW_PTN PT_hint_debug2($3);
            if ($$ == NULL)
              YYABORT; // OOM
          }
        | DEBUG_HINT3
          {
            scanner->syntax_warning("This warning is expected");
            $$= NULL;
          }
        ;

