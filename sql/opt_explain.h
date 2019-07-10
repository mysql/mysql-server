/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#ifndef OPT_EXPLAIN_INCLUDED
#define OPT_EXPLAIN_INCLUDED

/** @file "EXPLAIN <command>" 

Single table UPDATE/DELETE commands are explained by the 
explain_single_table_modification() function.

A query expression (complete SELECT query possibly including
subqueries and unions), INSERT...SELECT and multitable UPDATE/DELETE
commands are explained like this:

(1) explain_query_expression()

Is the entry point. Forwards the job to explain_unit().

(2) explain_unit()

Is for a SELECT_LEX_UNIT, prepares, optimizes, explains one JOIN for
each "top-level" SELECT_LEXs of the unit (like: all SELECTs of a
UNION; but not subqueries), and one JOIN for the fake SELECT_LEX of
UNION); each JOIN explain (JOIN::exec()) calls explain_query_specification()

(3) explain_query_specification()

Is for a single SELECT_LEX (fake or not). It needs a prepared and
optimized JOIN, for which it builds the EXPLAIN rows. But it also
launches the EXPLAIN process for "inner units" (==subqueries of this
SELECT_LEX), by calling explain_unit() for each of them. 
*/

#include <my_base.h>

class JOIN;
class select_result;
class select_result_interceptor;
class SQL_SELECT;
struct TABLE;
class THD;


extern const char *join_type_str[];

bool explain_no_table(THD *thd, JOIN *join, const char *message);
bool explain_no_table(THD *thd, const char *message,
                      ha_rows rows= HA_POS_ERROR);
bool explain_single_table_modification(THD *thd,
                                       TABLE *table,
                                       const SQL_SELECT *select,
                                       uint key,
                                       ha_rows limit,
                                       bool need_tmp_table,
                                       bool need_sort,
                                       bool is_update,
                                       bool used_key_is_modified= false);
bool explain_query_specification(THD *thd, JOIN *join);
bool explain_multi_table_modification(THD *thd,
                                      select_result_interceptor *result);
bool explain_query_expression(THD *thd, select_result *result);

#endif /* OPT_EXPLAIN_INCLUDED */
