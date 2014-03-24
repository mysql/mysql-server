/*****************************************************************************

Copyright (c) 2007, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file fts/fts0sql.cc
Full Text Search functionality.

Created 2007-03-27 Sunny Bains
*******************************************************/

#include "que0que.h"
#include "trx0roll.h"
#include "pars0pars.h"
#include "dict0dict.h"
#include "fts0types.h"
#include "fts0priv.h"

#ifndef UNIV_NONINL
#include "fts0types.ic"
#include "fts0vlc.ic"
#endif

/** SQL statements for creating the ancillary FTS tables. %s must be replaced
with the indexed table's id. */

/** Preamble to all SQL statements. */
static const char* fts_sql_begin=
	"PROCEDURE P() IS\n";

/** Postamble to non-committing SQL statements. */
static const char* fts_sql_end=
	"\n"
	"END;\n";

/******************************************************************//**
Get the table id.
@return number of bytes written */
UNIV_INTERN
int
fts_get_table_id(
/*=============*/
	const fts_table_t*
			fts_table,	/*!< in: FTS Auxiliary table */
	char*		table_id)	/*!< out: table id, must be at least
					FTS_AUX_MIN_TABLE_ID_LENGTH bytes
					long */
{
	int		len;
	bool		hex_name = DICT_TF2_FLAG_IS_SET(fts_table->table,
						DICT_TF2_FTS_AUX_HEX_NAME);

	ut_a(fts_table->table != NULL);

	switch (fts_table->type) {
	case FTS_COMMON_TABLE:
		len = fts_write_object_id(fts_table->table_id, table_id,
					  hex_name);
		break;

	case FTS_INDEX_TABLE:

		len = fts_write_object_id(fts_table->table_id, table_id,
					  hex_name);

		table_id[len] = '_';
		++len;
		table_id += len;

		len += fts_write_object_id(fts_table->index_id, table_id,
					   hex_name);
		break;

	default:
		ut_error;
	}

	ut_a(len >= 16);
	ut_a(len < FTS_AUX_MIN_TABLE_ID_LENGTH);

	return(len);
}

/******************************************************************//**
Construct the prefix name of an FTS table.
@return own: table name, must be freed with mem_free() */
UNIV_INTERN
char*
fts_get_table_name_prefix(
/*======================*/
	const fts_table_t*
			fts_table)	/*!< in: Auxiliary table type */
{
	int		len;
	const char*	slash;
	char*		prefix_name;
	int		dbname_len = 0;
	int		prefix_name_len;
	char		table_id[FTS_AUX_MIN_TABLE_ID_LENGTH];

	slash = static_cast<const char*>(
		memchr(fts_table->parent, '/', strlen(fts_table->parent)));

	if (slash) {
		/* Print up to and including the separator. */
		dbname_len = static_cast<int>(slash - fts_table->parent) + 1;
	}

	len = fts_get_table_id(fts_table, table_id);

	prefix_name_len = dbname_len + 4 + len + 1;

	prefix_name = static_cast<char*>(mem_alloc(prefix_name_len));

	len = sprintf(prefix_name, "%.*sFTS_%s",
		      dbname_len, fts_table->parent, table_id);

	ut_a(len > 0);
	ut_a(len == prefix_name_len - 1);

	return(prefix_name);
}

/******************************************************************//**
Construct the name of an ancillary FTS table.
@return own: table name, must be freed with mem_free() */
UNIV_INTERN
char*
fts_get_table_name(
/*===============*/
	const fts_table_t*	fts_table)
					/*!< in: Auxiliary table type */
{
	int		len;
	char*		name;
	int		name_len;
	char*		prefix_name;

	prefix_name = fts_get_table_name_prefix(fts_table);

	name_len = static_cast<int>(
		strlen(prefix_name) + 1 + strlen(fts_table->suffix) + 1);

	name = static_cast<char*>(mem_alloc(name_len));

	len = sprintf(name, "%s_%s", prefix_name, fts_table->suffix);

	ut_a(len > 0);
	ut_a(len == name_len - 1);

	mem_free(prefix_name);

	return(name);
}

/******************************************************************//**
Parse an SQL string. %s is replaced with the table's id.
@return query graph */
UNIV_INTERN
que_t*
fts_parse_sql(
/*==========*/
	fts_table_t*	fts_table,	/*!< in: FTS auxiliarry table info */
	pars_info_t*	info,		/*!< in: info struct, or NULL */
	const char*	sql)		/*!< in: SQL string to evaluate */
{
	char*		str;
	que_t*		graph;
	char*		str_tmp;
	ibool		dict_locked;

	if (fts_table != NULL) {
		char*	table_name;

		table_name = fts_get_table_name(fts_table);
		str_tmp = ut_strreplace(sql, "%s", table_name);
		mem_free(table_name);
	} else {
		ulint	sql_len = strlen(sql) + 1;

		str_tmp = static_cast<char*>(mem_alloc(sql_len));
		strcpy(str_tmp, sql);
	}

	str = ut_str3cat(fts_sql_begin, str_tmp, fts_sql_end);
	mem_free(str_tmp);

	dict_locked = (fts_table && fts_table->table->fts
		       && (fts_table->table->fts->fts_status
			   & TABLE_DICT_LOCKED));

	if (!dict_locked) {
		ut_ad(!mutex_own(&(dict_sys->mutex)));

		/* The InnoDB SQL parser is not re-entrant. */
		mutex_enter(&dict_sys->mutex);
	}

	graph = pars_sql(info, str);
	ut_a(graph);

	if (!dict_locked) {
		mutex_exit(&dict_sys->mutex);
	}

	mem_free(str);

	return(graph);
}

/******************************************************************//**
Parse an SQL string. %s is replaced with the table's id.
@return query graph */
UNIV_INTERN
que_t*
fts_parse_sql_no_dict_lock(
/*=======================*/
	fts_table_t*	fts_table,	/*!< in: FTS aux table info */
	pars_info_t*	info,		/*!< in: info struct, or NULL */
	const char*	sql)		/*!< in: SQL string to evaluate */
{
	char*		str;
	que_t*		graph;
	char*		str_tmp = NULL;

#ifdef UNIV_DEBUG
	ut_ad(mutex_own(&dict_sys->mutex));
#endif

	if (fts_table != NULL) {
		char*		table_name;

		table_name = fts_get_table_name(fts_table);
		str_tmp = ut_strreplace(sql, "%s", table_name);
		mem_free(table_name);
	}

	if (str_tmp != NULL) {
		str = ut_str3cat(fts_sql_begin, str_tmp, fts_sql_end);
		mem_free(str_tmp);
	} else {
		str = ut_str3cat(fts_sql_begin, sql, fts_sql_end);
	}

	//fprintf(stderr, "%s\n", str);

	graph = pars_sql(info, str);
	ut_a(graph);

	mem_free(str);

	return(graph);
}

/******************************************************************//**
Evaluate an SQL query graph.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_eval_sql(
/*=========*/
	trx_t*		trx,		/*!< in: transaction */
	que_t*		graph)		/*!< in: Query graph to evaluate */
{
	que_thr_t*	thr;

	graph->trx = trx;
	graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

	ut_a(thr = que_fork_start_command(graph));

	que_run_threads(thr);

	return(trx->error_state);
}

/******************************************************************//**
Construct the column specification part of the SQL string for selecting the
indexed FTS columns for the given table. Adds the necessary bound
ids to the given 'info' and returns the SQL string. Examples:

One indexed column named "text":

 "$sel0",
 info/ids: sel0 -> "text"

Two indexed columns named "subject" and "content":

 "$sel0, $sel1",
 info/ids: sel0 -> "subject", sel1 -> "content",
@return heap-allocated WHERE string */
UNIV_INTERN
const char*
fts_get_select_columns_str(
/*=======================*/
	dict_index_t*   index,		/*!< in: index */
	pars_info_t*    info,		/*!< in/out: parser info */
	mem_heap_t*     heap)		/*!< in: memory heap */
{
	ulint		i;
	const char*	str = "";

	for (i = 0; i < index->n_user_defined_cols; i++) {
		char*           sel_str;

		dict_field_t*   field = dict_index_get_nth_field(index, i);

		sel_str = mem_heap_printf(heap, "sel%lu", (ulong) i);

		/* Set copy_name to TRUE since it's dynamic. */
		pars_info_bind_id(info, TRUE, sel_str, field->name);

		str = mem_heap_printf(
			heap, "%s%s$%s", str, (*str) ? ", " : "", sel_str);
	}

	return(str);
}

/******************************************************************//**
Commit a transaction.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_sql_commit(
/*===========*/
	trx_t*		trx)		/*!< in: transaction */
{
	dberr_t	error;

	error = trx_commit_for_mysql(trx);

	/* Commit should always succeed */
	ut_a(error == DB_SUCCESS);

	return(DB_SUCCESS);
}

/******************************************************************//**
Rollback a transaction.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fts_sql_rollback(
/*=============*/
	trx_t*		trx)		/*!< in: transaction */
{
	return(trx_rollback_to_savepoint(trx, NULL));
}
