/***********************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/

/**************************************************//**
@file innodb_config.c
InnoDB Memcached configurations

Created 04/12/2011 Jimmy Yang
*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "innodb_api.h"
#include "innodb_config.h"
#include "innodb_cb_api.h"
#include "innodb_utility.h"

/** Configure options enum IDs, their "names" and their default value */
option_t	config_option_names[] =
{
        {OPTION_ID_COL_SEP, COLUMN_SEPARATOR, {"|", 1}},
        {OPTION_ID_TBL_MAP_SEP, TABLE_MAP_SEPARATOR, {".", 1}}
};

/**********************************************************************//**
Makes a NUL-terminated copy of a nonterminated string.
@return own: a copy of the string, must be deallocated by caller */
static
char*
my_strdupl(
/*=======*/
        const char*     str,    /*!< in: string to be copied */
        int             len)    /*!< in: length of str, in bytes */
{
	char*   s = (char*) malloc(len + 1);

	if (!s) {
		return(NULL);
	}

	s[len] = 0;

	return((char*) memcpy(s, str, len));
}

/**********************************************************************//**
This function frees meta info structures */
void
innodb_config_free(
/*===============*/
	meta_cfg_info_t*	item)	/*!< in: meta info structure */
{
	int	i;

	for (i = 0; i < CONTAINER_NUM_COLS; i++) {
		if (item->col_info[i].col_name) {
			free(item->col_info[i].col_name);
			item->col_info[i].col_name = NULL;
		}
	}

	if (item->index_info.idx_name) {
		free(item->index_info.idx_name);
		item->index_info.idx_name = NULL;
	}

	if (item->extra_col_info) {
		for (i = 0; i < item->n_extra_col; i++) {
			free(item->extra_col_info[i].col_name);
			item->extra_col_info[i].col_name = NULL;
		}

		free(item->extra_col_info);
		item->extra_col_info = NULL;
	}
}

/**********************************************************************//**
This function parses possible multiple column name separated by ",", ";"
or " " in the input "str" for the memcached "value" field.
@return true if everything works out fine */
static
bool
innodb_config_parse_value_col(
/*==========================*/
	meta_cfg_info_t*item,		/*!< in: meta info structure */
	char*		str,		/*!< in: column name(s) string */
	int		len)		/*!< in: length of above string */
{
        static const char*	sep = " ;,|\n";
        char*			last;
	char*			column_str;
	int			num_cols = 0;
	char*			my_str = my_strdupl(str, len);

	/* Find out how many column names in the string */
	for (column_str = strtok_r(my_str, sep, &last);
	     column_str;
	     column_str = strtok_r(NULL, sep, &last)) {
		num_cols++;
	}

	free(my_str);

	my_str = str;

	if (num_cols > 1) {
		int	i = 0;
		item->extra_col_info = malloc(
			num_cols * sizeof(*item->extra_col_info));

		if (!item->extra_col_info) {
			return(false);
		}

		for (column_str = strtok_r(my_str, sep, &last);
		     column_str;
		     column_str = strtok_r(NULL, sep, &last)) {
			item->extra_col_info[i].col_name_len = strlen(
				column_str);
			item->extra_col_info[i].col_name = my_strdupl(
				column_str,
				item->extra_col_info[i].col_name_len);
			item->extra_col_info[i].field_id = -1;
			i++;
		}

		item->n_extra_col = num_cols;
	} else {
		item->extra_col_info = NULL;
		item->n_extra_col = 0;
	}

	return(true);
}

/**********************************************************************//**
This function opens the cache_policy configuration table, and find the
table and column info that used for memcached data
@return true if everything works out fine */
static
bool
innodb_read_cache_policy(
/*=====================*/
	meta_cfg_info_t*	item)	/*!< in: meta info structure */
{
	ib_trx_t		ib_trx;
	ib_crsr_t		crsr = NULL;
	ib_crsr_t		idx_crsr = NULL;
	ib_tpl_t		tpl = NULL;
	ib_err_t		err = DB_SUCCESS;
	int			n_cols;
	int			i;
	ib_ulint_t		data_len;
	ib_col_meta_t		col_meta;

	ib_trx = innodb_cb_trx_begin(IB_TRX_READ_COMMITTED);

	err = innodb_api_begin(NULL, MCI_CFG_DB_NAME,
			       MCI_CFG_CACHE_POLICIES, NULL, ib_trx,
			       &crsr, &idx_crsr, IB_LOCK_S);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: Cannot open config table"
				"'%s' in database '%s'. Error %d\n",
			MCI_CFG_CACHE_POLICIES, MCI_CFG_DB_NAME,
			err);
		err = DB_ERROR;
		goto func_exit;
	}

	tpl = innodb_cb_read_tuple_create(crsr);

	/* Currently, we support one table per memcached setup.
	We could extend that limit later */
	err = innodb_cb_cursor_first(crsr);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate entry in"
				" config table '%s' in database '%s' \n",
			MCI_CFG_CACHE_POLICIES, MCI_CFG_DB_NAME);
		err = DB_ERROR;
		goto func_exit;
	}

	err = innodb_cb_read_row(crsr, tpl);

	n_cols = innodb_cb_tuple_get_n_cols(tpl);

	assert(n_cols >= CACHE_POLICY_NUM_COLS);

	for (i = 0; i < CACHE_POLICY_NUM_COLS; ++i) {
		char			opt_name;
		meta_cache_opt_t	opt_val;

		/* Skip cache policy name for now, We could have
		different cache policy stored, and switch dynamically */
		if (i == CACHE_POLICY_NAME) {
			continue;
		}

		data_len = innodb_cb_col_get_meta(tpl, i, &col_meta);

		if (data_len == IB_SQL_NULL) {
			opt_val = META_CACHE_OPT_INNODB;
		} else {
			opt_name = *(char*)innodb_cb_col_get_value(tpl, i);

			opt_val = (meta_cache_opt_t) opt_name;
		}

		if (opt_val >= META_CACHE_NUM_OPT
		    || opt_val < META_CACHE_OPT_INNODB) {
			fprintf(stderr, " InnoDB_Memcached: Invalid Cache"
					" Policy %d. Reset to innodb_only\n",
				(int) opt_val);
			opt_val = META_CACHE_OPT_INNODB;
		}

		switch (i) {
		case CACHE_POLICY_GET:
			item->get_option = opt_val;
			break;
		case CACHE_POLICY_SET:
			item->set_option = opt_val;
			break;
		case CACHE_POLICY_DEL:
			item->del_option = opt_val;
			break;
		case CACHE_POLICY_FLUSH:
			item->flush_option = opt_val;
			break;
		default:
			assert(0);
		}
	}

func_exit:

	if (crsr) {
		innodb_cb_cursor_close(crsr);
	}

	if (tpl) {
		innodb_cb_tuple_delete(tpl);
	}

	innodb_cb_trx_commit(ib_trx);

	return(err == DB_SUCCESS || err == DB_END_OF_INDEX);
}

/**********************************************************************//**
This function opens the config_options configuration table, and find the
table and column info that used for memcached data
@return true if everything works out fine */
static
bool
innodb_read_config_option(
/*======================*/
	meta_cfg_info_t*	item)	/*!< in: meta info structure */
{
	ib_trx_t		ib_trx;
	ib_crsr_t		crsr = NULL;
	ib_crsr_t		idx_crsr = NULL;
	ib_tpl_t		tpl = NULL;
	ib_err_t		err = DB_SUCCESS;
	int			n_cols;
	int			i;
	ib_ulint_t		data_len;
	ib_col_meta_t		col_meta;
	int			current_option = -1;

	ib_trx = innodb_cb_trx_begin(IB_TRX_READ_COMMITTED);
	err = innodb_api_begin(NULL, MCI_CFG_DB_NAME,
			       MCI_CFG_CONFIG_OPTIONS, NULL, ib_trx,
			       &crsr, &idx_crsr, IB_LOCK_S);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: Cannot open config table"
				"'%s' in database '%s'\n",
			MCI_CFG_CONFIG_OPTIONS, MCI_CFG_DB_NAME);
		err = DB_ERROR;
		goto func_exit;
	}

	tpl = innodb_cb_read_tuple_create(crsr);

	err = innodb_cb_cursor_first(crsr);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate entry in"
				" config table '%s' in database '%s' \n",
			MCI_CFG_CONFIG_OPTIONS, MCI_CFG_DB_NAME);
		err = DB_ERROR;
		goto func_exit;
	}


	do {
		err = innodb_cb_read_row(crsr, tpl);

		if (err != DB_SUCCESS) {
			fprintf(stderr, " InnoDB_Memcached: failed to read"
					" row from config table '%s' in"
					" database '%s' \n",
				MCI_CFG_CONFIG_OPTIONS, MCI_CFG_DB_NAME);
			err = DB_ERROR;
			goto func_exit;
		}

		n_cols = innodb_cb_tuple_get_n_cols(tpl);

		assert(n_cols >= CONFIG_OPT_NUM_COLS);

		for (i = 0; i < CONFIG_OPT_NUM_COLS; ++i) {
			char*	key;

			data_len = innodb_cb_col_get_meta(tpl, i, &col_meta);

			assert(data_len != IB_SQL_NULL);

			if (i == CONFIG_OPT_KEY) {
				int	j;
				key = (char*)innodb_cb_col_get_value(tpl, i);
				current_option = -1;

				for (j = 0; j < OPTION_ID_NUM_OPTIONS; j++) {
					/* Currently, we only support one
					configure option, that is the string
					"separator" */
					if (strcmp(
						key,
						config_option_names[j].name)
					    == 0) {
						current_option =
							config_option_names[j].id;
						break;
					}
				}
			}

			if (i == CONFIG_OPT_VALUE && current_option >= 0) {
				int	max_len;

				/* The maximum length for delimiter is
				MAX_DELIMITER_LEN */
				max_len = (data_len > MAX_DELIMITER_LEN)
					? MAX_DELIMITER_LEN
					: data_len;

				memcpy(item->options[current_option].value,
				       innodb_cb_col_get_value(tpl, i),
				       max_len);

				item->options[current_option].value[max_len]
					= 0;

				item->options[current_option].value_len
					= max_len;
			}
		}

		err = ib_cb_cursor_next(crsr);

	} while (err == DB_SUCCESS);

func_exit:

	if (crsr) {
		innodb_cb_cursor_close(crsr);
	}

	if (tpl) {
		innodb_cb_tuple_delete(tpl);
	}

	innodb_cb_trx_commit(ib_trx);

	return(err == DB_SUCCESS || err == DB_END_OF_INDEX);
}

/**********************************************************************//**
This function opens the "containers" configuration table, and find the
table and column info that used for memcached data, and instantiates
meta_cfg_info_t structure for such metadata.
@return instantiated configure info item if everything works out fine,
callers are responsible to free the memory returned by this function */
static
meta_cfg_info_t*
innodb_config_add_item(
/*===================*/
	ib_tpl_t	tpl,		/*!< in: container row we are fetching
					row from */
	hash_table_t*	eng_meta_hash)	/*!< in/out: hash table to insert
					the row */
{
	ib_err_t		err = DB_SUCCESS;
	int			n_cols;
	int			i;
	ib_ulint_t		data_len;
	meta_cfg_info_t*	item = NULL;
	ib_col_meta_t		col_meta;
	int			fold;

	n_cols = innodb_cb_tuple_get_n_cols(tpl);

	if (n_cols < CONTAINER_NUM_COLS) {
		fprintf(stderr, " InnoDB_Memcached: config table '%s' in"
				" database '%s' has only %d column(s),"
				" server is expecting %d columns\n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME,
			n_cols, CONTAINER_NUM_COLS);
		err = DB_ERROR;
		goto func_exit;
	}

	item = malloc(sizeof(*item));

	memset(item, 0, sizeof(*item));

	/* Get the column mappings (column for each memcached data */
	for (i = 0; i < CONTAINER_NUM_COLS; ++i) {

		data_len = innodb_cb_col_get_meta(tpl, i, &col_meta);

		if (data_len == IB_SQL_NULL) {
			fprintf(stderr, " InnoDB_Memcached: column %d in"
					" the entry for config table '%s' in"
					" database '%s' has an invalid"
					" NULL value\n",
				i, MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);

			err = DB_ERROR;
			goto func_exit;
		}

		item->col_info[i].col_name_len = data_len;

		item->col_info[i].col_name = my_strdupl(
			(char*)innodb_cb_col_get_value(tpl, i), data_len);

		item->col_info[i].field_id = -1;

		if (i == CONTAINER_VALUE) {
			innodb_config_parse_value_col(
				item, item->col_info[i].col_name, data_len);
		}
	}

	/* Last column is about the unique index name on key column */
	data_len = innodb_cb_col_get_meta(tpl, i, &col_meta);

	if (data_len == IB_SQL_NULL) {
		fprintf(stderr, " InnoDB_Memcached: There must be a unique"
				" index on memcached table's key column\n");
		err = DB_ERROR;
		goto func_exit;
	}

	item->index_info.idx_name = my_strdupl((char*)innodb_cb_col_get_value(
						tpl, i), data_len);

	if (!innodb_verify(item)) {
		err = DB_ERROR;
		goto func_exit;
	}

	fold = ut_fold_string(item->col_info[0].col_name);
	HASH_INSERT(meta_cfg_info_t, name_hash, eng_meta_hash, fold, item);

func_exit:
	if (err != DB_SUCCESS && item) {
		free(item);
		item = NULL;
	}

	return(item);
}

/**********************************************************************//**
This function opens the "containers" table, reads in all rows and
instantiates the metadata hash table.
@return the default configuration setting (whose mapping name is "default") */
meta_cfg_info_t*
innodb_config_meta_hash_init(
/*=========================*/
	hash_table_t*	meta_hash)	/*!< in/out: InnoDB Memcached engine */
{
	ib_trx_t		ib_trx;
	ib_crsr_t		crsr = NULL;
	ib_crsr_t		idx_crsr = NULL;
	ib_tpl_t		tpl = NULL;
	ib_err_t		err = DB_SUCCESS;
	meta_cfg_info_t*        default_item = NULL;

	ib_trx = innodb_cb_trx_begin(IB_TRX_READ_COMMITTED);
	err = innodb_api_begin(NULL, MCI_CFG_DB_NAME,
			       MCI_CFG_CONTAINER_TABLE, NULL, ib_trx,
			       &crsr, &idx_crsr, IB_LOCK_S);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: Please create config table"
				"'%s' in database '%s' by running"
				" 'scripts/innodb_config.sql. error %d'\n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME,
			err);
		err = DB_ERROR;
		goto func_exit;
	}

	tpl = innodb_cb_read_tuple_create(crsr);

	/* If name field is NULL, just read the first row */
	err = innodb_cb_cursor_first(crsr);

	while (err == DB_SUCCESS) {
		meta_cfg_info_t*        item;

		err = innodb_cb_read_row(crsr, tpl);

		if (err != DB_SUCCESS) {
			fprintf(stderr, " InnoDB_Memcached: failed to read row"
					" from config table '%s' in database"
					" '%s' \n",
				MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);
			err = DB_ERROR;
			goto func_exit;
		}

		item = innodb_config_add_item(tpl, meta_hash);

		/* First initialize default setting to be the first row
		of the table */
		/* If there are any setting whose name is "default",
		then set default_item to point to this setting, otherwise
		point it to the first row of the table */
		if (default_item == NULL
		    || (item && strcmp(item->col_info[0].col_name,
				       "default") == 0)) {
			default_item = item;
		}

		err = ib_cb_cursor_next(crsr);
	}

	if (err == DB_END_OF_INDEX) {
		err = DB_SUCCESS;
	}

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate entry in"
				" config table '%s' in database '%s' \n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);
		err = DB_ERROR;
	}

func_exit:

	if (crsr) {
		innodb_cb_cursor_close(crsr);
	}

	if (tpl) {
		innodb_cb_tuple_delete(tpl);
	}

	innodb_cb_trx_commit(ib_trx);

	return(default_item);
}

/**********************************************************************//**
This function opens the "containers" configuration table, and find the
table and column info that used for memcached data
@return true if everything works out fine */
static
meta_cfg_info_t*
innodb_config_container(
/*====================*/
	const char*		name,	/*!< in: option name to look for */
	size_t			name_len,/*!< in: option name length */
	hash_table_t*		meta_hash) /*!< in: engine hash table */
{
	ib_trx_t		ib_trx;
	ib_crsr_t		crsr = NULL;
	ib_crsr_t		idx_crsr = NULL;
	ib_tpl_t		tpl = NULL;
	ib_err_t		err = DB_SUCCESS;
	int			n_cols;
	int			i;
	ib_ulint_t		data_len;
	ib_col_meta_t		col_meta;
	ib_tpl_t		read_tpl = NULL;
	meta_cfg_info_t*	item = NULL;

	if (name != NULL) {
		ib_ulint_t	fold;

		assert(meta_hash);

		fold = ut_fold_string(name);
		HASH_SEARCH(name_hash, meta_hash, fold,
			    meta_cfg_info_t*, item,
			    (name_len == item->col_info[0].col_name_len
			     && strcmp(name, item->col_info[0].col_name) == 0));

		if (item) {
			return(item);
		}
	}

	ib_trx = innodb_cb_trx_begin(IB_TRX_READ_COMMITTED);
	err = innodb_api_begin(NULL, MCI_CFG_DB_NAME,
			       MCI_CFG_CONTAINER_TABLE, NULL, ib_trx,
			       &crsr, &idx_crsr, IB_LOCK_S);

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: Please create config table"
				"'%s' in database '%s' by running"
				" 'scripts/innodb_config.sql. error %d'\n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME,
			err);
		err = DB_ERROR;
		goto func_exit;
	}

	if (!name) {
		tpl = innodb_cb_read_tuple_create(crsr);

		/* If name field is NULL, just read the first row */
		err = innodb_cb_cursor_first(crsr);
	} else {
		/* User supplied a config option name, find it */
		tpl = ib_cb_search_tuple_create(crsr);

		err = ib_cb_col_set_value(tpl, 0, name, name_len, true);

		ib_cb_cursor_set_match_mode(crsr, IB_EXACT_MATCH);
		err = ib_cb_moveto(crsr, tpl, IB_CUR_GE);
	}

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate entry in"
				" config table '%s' in database '%s' \n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);
		err = DB_ERROR;
		goto func_exit;
	}

	if (!name) {
		read_tpl = tpl;
		err = innodb_cb_read_row(crsr, tpl);
	} else {
		read_tpl = ib_cb_read_tuple_create(crsr);

		err = ib_cb_read_row(crsr, read_tpl);
	}

	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to read row from"
				" config table '%s' in database '%s' \n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);
		err = DB_ERROR;
		goto func_exit;
	}

	n_cols = innodb_cb_tuple_get_n_cols(read_tpl);

	if (n_cols < CONTAINER_NUM_COLS) {
		fprintf(stderr, " InnoDB_Memcached: config table '%s' in"
				" database '%s' has only %d column(s),"
				" server is expecting %d columns\n",
			MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME,
			n_cols, CONTAINER_NUM_COLS);
		err = DB_ERROR;
		goto func_exit;
	}

	item = malloc(sizeof(*item));
	memset(item, 0, sizeof(*item));

	/* Get the column mappings (column for each memcached data */
	for (i = 0; i < CONTAINER_NUM_COLS; ++i) {

		data_len = innodb_cb_col_get_meta(read_tpl, i, &col_meta);

		if (data_len == IB_SQL_NULL) {
			fprintf(stderr, " InnoDB_Memcached: column %d in"
					" the entry for config table '%s' in"
					" database '%s' has an invalid"
					" NULL value\n",
				i, MCI_CFG_CONTAINER_TABLE, MCI_CFG_DB_NAME);

			err = DB_ERROR;
			goto func_exit;


		}

		item->col_info[i].col_name_len = data_len;

		item->col_info[i].col_name = my_strdupl(
			(char*)innodb_cb_col_get_value(read_tpl, i), data_len);

		item->col_info[i].field_id = -1;

		if (i == CONTAINER_VALUE) {
			innodb_config_parse_value_col(
				item, item->col_info[i].col_name, data_len);
		}
	}

	/* Last column is about the unique index name on key column */
	data_len = innodb_cb_col_get_meta(read_tpl, i, &col_meta);

	if (data_len == IB_SQL_NULL) {
		fprintf(stderr, " InnoDB_Memcached: There must be a unique"
				" index on memcached table's key column\n");
		err = DB_ERROR;
		goto func_exit;
	}

	item->index_info.idx_name = my_strdupl((char*)innodb_cb_col_get_value(
						read_tpl, i), data_len);

	if (!innodb_verify(item)) {
		err = DB_ERROR;
	}

func_exit:

	if (crsr) {
		innodb_cb_cursor_close(crsr);
	}

	if (tpl) {
		innodb_cb_tuple_delete(tpl);
	}

	innodb_cb_trx_commit(ib_trx);

	if (err != DB_SUCCESS) {
		free(item);
		item = NULL;
	} else {
		ib_ulint_t	fold;

		fold = ut_fold_string(item->col_info[0].col_name);
		HASH_INSERT(
			meta_cfg_info_t, name_hash, meta_hash, fold, item);
	}

	return(item);
}

/**********************************************************************//**
This function verifies "value" column(s) specified by configure table are of
the correct type
@return DB_SUCCESS if everything is verified */
static
ib_err_t
innodb_config_value_col_verify(
/*===========================*/
	char*		name,		/*!< in: column name */
	meta_cfg_info_t*meta_info,	/*!< in: meta info structure */
	ib_col_meta_t*	col_meta,	/*!< in: column metadata */
	int		col_id,		/*!< in: column ID */
	meta_column_t*	col_verify)	/*!< in: verify structure */
{
	ib_err_t	err = DB_NOT_FOUND;
	char            table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];
	char*		dbname;
	char*		tname;

	/* Get table name. */
	dbname = meta_info->col_info[CONTAINER_DB].col_name;
	tname = meta_info->col_info[CONTAINER_TABLE].col_name;
#ifdef __WIN__
	sprintf(table_name, "%s\%s", dbname, tname);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, tname);
#endif

	if (!meta_info->n_extra_col) {
		meta_column_t*	cinfo = meta_info->col_info;

		/* "value" column must be of CHAR, VARCHAR or BLOB type */
		if (strcmp(name, cinfo[CONTAINER_VALUE].col_name) == 0) {
			if (col_meta->type != IB_VARCHAR
			    && col_meta->type != IB_CHAR
			    && col_meta->type != IB_BLOB
			    && col_meta->type != IB_CHAR_ANYCHARSET
			    && col_meta->type != IB_VARCHAR_ANYCHARSET
			    && col_meta->type != IB_INT) {
				fprintf(stderr,
					" InnoDB_Memcached: the value"
					" column %s in table %s"
					" should be INTEGER, CHAR or"
					" VARCHAR.\n",
					name, table_name);
				err = DB_DATA_MISMATCH;
			}

			cinfo[CONTAINER_VALUE].field_id = col_id;
			cinfo[CONTAINER_VALUE].col_meta = *col_meta;
			err = DB_SUCCESS;
		}
	} else {
		int	i;

		for (i = 0; i < meta_info->n_extra_col; i++) {
			if (strcmp(name,
				   meta_info->extra_col_info[i].col_name) == 0)
			{
				if (col_meta->type != IB_VARCHAR
				    && col_meta->type != IB_CHAR
				    && col_meta->type != IB_BLOB
				    && col_meta->type != IB_CHAR_ANYCHARSET
				    && col_meta->type != IB_VARCHAR_ANYCHARSET
				    && col_meta->type != IB_INT) {
					fprintf(stderr,
						" InnoDB_Memcached: the value"
						" column %s in table %s"
						" should be INTEGER, CHAR or"
						" VARCHAR.\n",
						name, table_name);
					err = DB_DATA_MISMATCH;
					break;
				}

				meta_info->extra_col_info[i].field_id = col_id;
				meta_info->extra_col_info[i].col_meta = *col_meta;

				meta_info->col_info[CONTAINER_VALUE].field_id
					= col_id;
				meta_info->col_info[CONTAINER_VALUE].col_meta
					= *col_meta;

				if (col_verify) {
					col_verify[i].field_id = col_id;
				}

				err = DB_SUCCESS;
			}
		}

	}

	return(err);
}

/**********************************************************************//**
This function verifies the table configuration information on an opened
table, and fills in columns used for memcached functionalities (cas, exp etc.)
@return true if everything works out fine */
ib_err_t
innodb_verify_low(
/*==============*/
	meta_cfg_info_t*	info,	/*!< in/out: meta info structure */
	ib_crsr_t		crsr,	/*!< in: crsr */
	bool			runtime)/*!< in: verify at the runtime */
{
	ib_crsr_t	idx_crsr = NULL;
	ib_tpl_t	tpl = NULL;
	ib_col_meta_t	col_meta;
	int		n_cols;
	int		i;
	bool		is_key_col = false;
	bool		is_value_col = false;
	bool		is_flag_col = false;
	bool		is_cas_col = false;
	bool		is_exp_col = false;
	int		index_type;
	ib_id_u64_t	index_id;
	ib_err_t	err = DB_SUCCESS;
	char*		name;
	meta_column_t*	cinfo = info->col_info;
	meta_column_t*	col_verify = NULL;
	char            table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];
	char*		dbname;
	char*		tname;

	tpl = innodb_cb_read_tuple_create(crsr);

	if (runtime && info->n_extra_col) {
		col_verify = malloc(info->n_extra_col * sizeof(meta_column_t));

		if (!col_verify) {
			return(false);
		}

		for (i = 0; i < info->n_extra_col; i++) {
			col_verify[i].field_id = -1;
		}
	}

	/* Get table name. */
	dbname = info->col_info[CONTAINER_DB].col_name;
	tname = info->col_info[CONTAINER_TABLE].col_name;
#ifdef __WIN__
	sprintf(table_name, "%s\%s", dbname, tname);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, tname);
#endif

	n_cols = innodb_cb_tuple_get_n_cols(tpl);

	/* Verify each mapped column */
	for (i = 0; i < n_cols; i++) {
		ib_err_t	result = DB_SUCCESS;

		name = innodb_cb_col_get_name(crsr, i);
		innodb_cb_col_get_meta(tpl, i, &col_meta);

		result = innodb_config_value_col_verify(
			name, info, &col_meta, i, col_verify);

		if (result == DB_SUCCESS) {
			is_value_col = true;
			continue;
		} else if (result == DB_DATA_MISMATCH) {
			err = DB_DATA_MISMATCH;
			goto func_exit;
		}

		if (strcmp(name, cinfo[CONTAINER_KEY].col_name) == 0) {
			/* Key column must be CHAR or VARCHAR type */
			if (col_meta.type != IB_VARCHAR
			    && col_meta.type != IB_CHAR
			    && col_meta.type != IB_VARCHAR_ANYCHARSET
			    && col_meta.type != IB_CHAR_ANYCHARSET
			    && col_meta.type != IB_INT) {
				fprintf(stderr,
					" InnoDB_Memcached: the key"
					" column %s in table %s should"
					" be INTEGER, CHAR or VARCHAR.\n",
					name, table_name);
				err = DB_DATA_MISMATCH;
				goto func_exit;
			}
			cinfo[CONTAINER_KEY].field_id = i;
			cinfo[CONTAINER_KEY].col_meta = col_meta;
			is_key_col = true;
		} else if (strcmp(name, cinfo[CONTAINER_FLAG].col_name) == 0) {
			/* Flag column must be integer type */
			if (col_meta.type != IB_INT) {
				fprintf(stderr, " InnoDB_Memcached: the flag"
						" column %s in table %s should"
						" be INTEGER.\n",
					name, table_name);
				err = DB_DATA_MISMATCH;
				goto func_exit;
			}
			cinfo[CONTAINER_FLAG].field_id = i;
			cinfo[CONTAINER_FLAG].col_meta = col_meta;
			info->flag_enabled = true;
			is_flag_col = true;
		} else if (strcmp(name, cinfo[CONTAINER_CAS].col_name) == 0) {
			/* CAS column must be integer type */
			if (col_meta.type != IB_INT) {
				fprintf(stderr, " InnoDB_Memcached: the cas"
						" column %s in table %s should"
						" be INTEGER.\n",
					name, table_name);
				err = DB_DATA_MISMATCH;
				goto func_exit;
			}
			cinfo[CONTAINER_CAS].field_id = i;
			cinfo[CONTAINER_CAS].col_meta = col_meta;
			info->cas_enabled = true;
			is_cas_col = true;
		} else if (strcmp(name, cinfo[CONTAINER_EXP].col_name) == 0) {
			/* EXP column must be integer type */
			if (col_meta.type != IB_INT) {
				fprintf(stderr, " InnoDB_Memcached: the expire"
						" column %s in table %s should"
						" be INTEGER.\n",
					name, table_name);
				err = DB_DATA_MISMATCH;
				goto func_exit;
			}
			cinfo[CONTAINER_EXP].field_id = i;
			cinfo[CONTAINER_EXP].col_meta = col_meta;
			info->exp_enabled = true;
			is_exp_col = true;
		}
	}

	/* Key column and Value column must present */
	if (!is_key_col || !is_value_col) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate key"
				" column or value column in table"
				" as specified by config table \n");

		err = DB_ERROR;
		goto func_exit;
	}

	if (info->n_extra_col) {
		meta_column_t*	col_check;

		col_check = (runtime && col_verify)
			    ? col_verify
			    : info->extra_col_info;

		for (i = 0; i < info->n_extra_col; i++) {
			if (col_check[i].field_id < 0) {
				fprintf(stderr, " InnoDB_Memcached: failed to"
						" locate value column %s"
						" as specified by config"
						" table \n",
					info->extra_col_info[i].col_name);
				err = DB_ERROR;
				goto func_exit;
			}
		}
	}

	if (info->flag_enabled && !is_flag_col) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate flag"
				" column as specified by config table \n");
		err = DB_ERROR;
		goto func_exit;
	}

	if (info->cas_enabled && !is_cas_col) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate cas"
				" column as specified by config table \n");
		err = DB_ERROR;
		goto func_exit;
	}

	if (info->exp_enabled && !is_exp_col) {
		fprintf(stderr, " InnoDB_Memcached: failed to locate exp"
				" column as specified by config table \n");
		err = DB_ERROR;
		goto func_exit;
	}

	/* Test the specified index */
	innodb_cb_cursor_open_index_using_name(crsr, info->index_info.idx_name,
					       &idx_crsr, &index_type,
					       &index_id);

	if (index_type & IB_CLUSTERED) {
		info->index_info.srch_use_idx = META_USE_CLUSTER;
	} else if (!idx_crsr || !(index_type & IB_UNIQUE)) {
		fprintf(stderr, " InnoDB_Memcached: Index on key column"
				" must be a Unique index\n");
		info->index_info.srch_use_idx = META_USE_NO_INDEX;
		err = DB_ERROR;
	} else {
		info->index_info.idx_id = index_id;
		info->index_info.srch_use_idx = META_USE_SECONDARY;
	}

	if (idx_crsr) {
		ib_tpl_t	idx_tpl = NULL;
		if (index_type & IB_CLUSTERED) {
			idx_tpl = innodb_cb_read_tuple_create(idx_crsr);
		} else {
			idx_tpl = ib_cb_search_tuple_create(idx_crsr);
		}

		n_cols = ib_cb_get_n_user_cols(idx_tpl);

		name = ib_cb_get_idx_field_name(idx_crsr, 0);

		if (strcmp(name, cinfo[CONTAINER_KEY].col_name)) {
			fprintf(stderr, " InnoDB_Memcached: Index used"
					" must be on key column only\n");
			err = DB_ERROR;
		}

		if (!(index_type & IB_CLUSTERED) && n_cols > 1) {
			fprintf(stderr, " InnoDB_Memcached: Index used"
					" must be on key column only\n");
			err = DB_ERROR;
		}

		innodb_cb_tuple_delete(idx_tpl);
		innodb_cb_cursor_close(idx_crsr);
	}
func_exit:

	if (runtime && col_verify) {
		free(col_verify);
	}

	if (tpl) {
		innodb_cb_tuple_delete(tpl);
	}

	return(err);
}

/**********************************************************************//**
This function verifies the table configuration information, and fills
in columns used for memcached functionalities (cas, exp etc.)
@return true if everything works out fine */
bool
innodb_verify(
/*==========*/
	meta_cfg_info_t*       info)	/*!< in: meta info structure */
{
	ib_crsr_t	crsr = NULL;
	char            table_name[MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN];
	char*		dbname;
	char*		name;
	ib_err_t	err = DB_SUCCESS;

	dbname = info->col_info[CONTAINER_DB].col_name;
	name = info->col_info[CONTAINER_TABLE].col_name;
	info->flag_enabled = false;
	info->cas_enabled = false;
	info->exp_enabled = false;

#ifdef __WIN__
	sprintf(table_name, "%s\%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif

	err = innodb_cb_open_table(table_name, NULL, &crsr);

	/* Mapped InnoDB table must be able to open */
	if (err != DB_SUCCESS) {
		fprintf(stderr, " InnoDB_Memcached: failed to open table"
				" '%s' \n", table_name);
		err = DB_ERROR;
		goto func_exit;
	}

	err = innodb_verify_low(info, crsr, false);
func_exit:
	if (crsr) {
		innodb_cb_cursor_close(crsr);
	}

	return(err == DB_SUCCESS);
}

/**********************************************************************//**
If the hash table (meta_hash) is NULL, then initialise the hash table with
data in the configure tables. And return the "default" item.  If there is
no setting named "default" then use the first row in the table. This is
currently only used at the engine initialization time.
If the hash table (meta_hash) is created, then look for the meta-data based on
specified configuration name parameter. If such metadata does not exist in
the hash table, then add such metadata into hash table.
@return meta_cfg_info_t* structure if configure option found, otherwise NULL */
meta_cfg_info_t*
innodb_config(
/*==========*/
	const char*		name,		/*!< in: config option name */
	size_t			name_len,	/*!< in: name length */
	hash_table_t**		meta_hash)	/*!< in/out: engine hash
						table. If NULL, it will be
						created and initialized */
{
	meta_cfg_info_t*	item;
	bool			success;

	if (*meta_hash == NULL) {
		*meta_hash = hash_create(100);
	}

	if (!name) {
		item = innodb_config_meta_hash_init(*meta_hash);
	} else {
		ib_ulint_t	fold;

		fold = ut_fold_string(name);
		HASH_SEARCH(name_hash, *meta_hash, fold,
			    meta_cfg_info_t*, item,
			    (name_len == item->col_info[0].col_name_len
			     && strcmp(name, item->col_info[0].col_name) == 0));

		if (item) {
			return(item);
		}

		item = innodb_config_container(name, name_len, *meta_hash);
	}

	if (!item) {
		return(NULL);
	}

	/* Following two configure operations are optional, and can be
	failed */
	success = innodb_read_cache_policy(item);

	if (!success) {
		return(NULL);
	}

	success = innodb_read_config_option(item);

	if (!success) {
		return(NULL);
	}

	return(item);
}
