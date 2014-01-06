/***********************************************************************

Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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
@file innodb_config.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef innodb_config_h
#define innodb_config_h

#include "api0api.h"
#include "innodb_utility.h"

typedef void*   hash_node_t;

/* Database name and table name for our metadata "system" tables for
InnoDB memcache. The table names are the same as those for the
NDB memcache, to make the memcache setup compatible between the two.
There are 3 "system tables":
1) containers - main configure table contains row describing which InnoDB
		table is used to store/retrieve Memcached key/value if InnoDB
		Memcached engine is used
2) cache_policies - decide whether to use "Memcached Default Engine" or "InnoDB
		    Memcached Engine" to handler the requests
3) config_options - for miscellaneous configuration options */
#define MCI_CFG_DB_NAME			"innodb_memcache"
#define MCI_CFG_CONTAINER_TABLE		"containers"
#define MCI_CFG_CACHE_POLICIES		"cache_policies"
#define MCI_CFG_CONFIG_OPTIONS		"config_options"

/** Max table name length as defined in univ.i */
#define MAX_TABLE_NAME_LEN      192
#define MAX_DATABASE_NAME_LEN   MAX_TABLE_NAME_LEN
#define MAX_FULL_NAME_LEN                               \
        (MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 14)

/** structure describes each column's basic info (name, field_id etc.) */
typedef struct meta_column {
	char*		col_name;		/*!< column name */
	size_t		col_name_len;		/*!< column name length */
	int		field_id;		/*!< column field id in
						the table */
	ib_col_meta_t	col_meta;		/*!< column  meta info */
} meta_column_t;

/** Following are enums defining column IDs indexing into each of three
system tables */

/** Columns in the "containers" system table, this maps the Memcached
operation to a consistent InnoDB table */
typedef enum container {
	CONTAINER_NAME,		/*!< name for this mapping */
	CONTAINER_DB,		/*!< database name */
	CONTAINER_TABLE,	/*!< table name */
	CONTAINER_KEY,		/*!< column name for column maps to
				memcached "key" */
	CONTAINER_VALUE,	/*!< column name for column maps to
				memcached "value" */
	CONTAINER_FLAG,		/*!< column name for column maps to
				memcached "flag" value */
	CONTAINER_CAS,		/*!< column name for column maps to
				memcached "cas" value */
	CONTAINER_EXP,		/*!< column name for column maps to
				"expiration" value */
	CONTAINER_NUM_COLS	/*!< number of columns */
} container_t;

/** columns in the "cache_policy" table */
typedef enum cache_policy {
	CACHE_POLICY_NAME,	/*!< "name" column, for the "cache_policy"
				name */
	CACHE_POLICY_GET,	/*!< "get" column, specifies the cache policy
				for "get" command */
	CACHE_POLICY_SET,	/*!< "set" column, specifies the cache policy
				for "set" command */
	CACHE_POLICY_DEL,	/*!< "delete" column, specifies the cache
				policy for "delete" command */
	CACHE_POLICY_FLUSH,	/*!< "flush_all" column, specifies the
				cache policy for "flush_all" command */
	CACHE_POLICY_NUM_COLS	/*!< total 5 columns */
} cache_policy_t;

/** columns in the "config_options" table */
typedef enum config_opt {
	CONFIG_OPT_KEY,		/*!< key column in the "config_option" table */
	CONFIG_OPT_VALUE,	/*!< value column */
	CONFIG_OPT_NUM_COLS	/*!< number of columns (currently 2) in table */
} config_opt_t;

/** Following are some value defines describes the options that configures
the InnoDB Memcached */

/** Values to set up "m_use_idx" field of "meta_index_t" structure,
indicating whether we will use cluster or secondary index on the
"key" column to perform the search. Please note the index must
be unique index */
typedef enum meta_use_idx {
	META_USE_NO_INDEX = 1,	/*!< no cluster or unique secondary index
				on the key column. This is an error, will
				cause setup to fail */
	META_USE_CLUSTER,	/*!< have cluster index on the key column */
	META_USE_SECONDARY	/*!< have unique secondary index on the
				key column */
} meta_use_idx_t;

/** Describes the index's name and ID of the index on the "key" column */
typedef struct meta_index {
	char*		idx_name;	/*!< index name */
	int		idx_id;		/*!< index id */
	meta_use_idx_t	srch_use_idx;	/*!< use cluster or secondary
					index for the search */
} meta_index_t;

/** Cache options, tells if we will used Memcached default engine or InnoDB
Memcached engine to handle the request */
typedef enum meta_cache_opt {
	META_CACHE_OPT_INNODB = 1,	/*!< Use InnoDB Memcached Engine only */
	META_CACHE_OPT_DEFAULT,		/*!< Use Default Memcached Engine
					only */
	META_CACHE_OPT_MIX,		/*!< Use both, first use default
					memcached engine */
	META_CACHE_OPT_DISABLE,		/*!< This operation is disabled */
	META_CACHE_NUM_OPT		/*!< Number of options */
} meta_cache_opt_t;

/** The "names" in the "config_option" table to identify possible
config options. Both are optional.
"COLUMN_SEPARATOR" is the delimiter that separates multiple columns and
"TABLE_MAP_SEPARATOR" is the delimiter that separates table map name
and key value */
#define COLUMN_SEPARATOR        "separator"
#define TABLE_MAP_SEPARATOR     "table_map_delimiter"

/* list of configure options we support */
typedef enum option_id {
	OPTION_ID_COL_SEP,		/*!< ID for character(s) separating
					multiple column mapping */
	OPTION_ID_TBL_MAP_SEP,		/*!< ID for character(s) separating
					table map name and key */
	OPTION_ID_NUM_OPTIONS		/*!< number of options */
} option_id_t;

/** Maximum delimiter length */
#define MAX_DELIMITER_LEN	32

typedef struct option_value {
	char		value[MAX_DELIMITER_LEN + 1];
					/* option value */
	int		value_len;	/* value length */
} option_value_t;

/** structure to define some default "config_option" option settings */
typedef struct option {
	option_id_t	id;		/*!< option id as in enum option_id */
	const char*	name;		/*!< option name for above option ID,
					currently they can be "COLUMN_SEPARATOR"
					and "TABLE_MAP_SEPARATOR" */
	option_value_t	default_value;	/*!< default value */
} option_t;

/** Get configure option value. If the value is not configured by
user, obtain its default value from "config_option_names"
@param meta_info	metadata structure contains configure options
@param option		option whose value to get
@param val		value to fetch
@param val_len		value length */
#define GET_OPTION(meta_info, option, val, val_len)			\
do {									\
	val_len = meta_info->options[option].value_len;			\
									\
	if (val_len == 0) {						\
		val = config_option_names[option].default_value.value;	\
		val_len = config_option_names[option].default_value.value_len;\
	} else {							\
		val = meta_info->options[option].value;			\
	}								\
} while (0)

/** In memory structure contains most necessary metadata info
to configure an InnoDB Memcached engine */
typedef struct meta_cfg_info {
	meta_column_t	col_info[CONTAINER_NUM_COLS]; /*!< column info */
	meta_column_t*	extra_col_info;		/*!< additional columns
						specified for the value field */
	int		n_extra_col;		/*!< number of additional
						value columns */
	meta_index_t	index_info;		/*!< Index info */
	bool		flag_enabled;		/*!< whether flag is enabled */
	bool		cas_enabled;		/*!< whether cas is enabled */
	bool		exp_enabled;		/*!< whether exp is enabled */
	option_value_t	options[OPTION_ID_NUM_OPTIONS];
						/*!< configure options, mostly
						are configured delimiters */
	meta_cache_opt_t set_option;		/*!< cache option for "set" */
	meta_cache_opt_t get_option;		/*!< cache option for "get" */
	meta_cache_opt_t del_option;		/*!< cache option for
						"delete" */
	meta_cache_opt_t flush_option;		/*!< cache option for
						"delete" */
	hash_node_t	name_hash;		/*!< name hash chain node */
} meta_cfg_info_t;


/**********************************************************************//**
This function opens the default configuration table, and find the
table and column info that used for InnoDB Memcached, and set up
InnoDB Memcached's meta_cfg_info_t structure. If the "name" parameter
is not NULL, it will find the specified setting in the "container" table.
If "name" field is NULL, it will then look for setting with the name of
"default". Otherwise, it returns the setting corresponding to the
first row of the configure table.
@return meta_cfg_info_t* structure if configure option found, otherwise NULL */
meta_cfg_info_t*
innodb_config(
/*==========*/
	const char*		name,		/*!< in: config option name */
	size_t			name_len,	/*!< in: option name length */
	hash_table_t**		meta_hash);	/*!< in: engine hash table */

/**********************************************************************//**
This function verifies the table configuration information, and fills
in columns used for memcached functionalities (cas, exp etc.)
@return true if everything works out fine */
bool
innodb_verify(
/*==========*/
	meta_cfg_info_t*	info);		/*!< in: meta info structure */

/**********************************************************************//**
This function frees meta info structure */
void
innodb_config_free(
/*===============*/
        meta_cfg_info_t*	item);		/*!< in/own: meta info
						structure */

/**********************************************************************//**
This function opens the "containers" table, reads in all rows
and instantiates the metadata hash table.
@return the default configuration setting (whose mapping name is "default") */
meta_cfg_info_t*
innodb_config_meta_hash_init(
/*=========================*/
	hash_table_t*		meta_hash);	/*!< in/out: InnoDB Memcached
						engine */
#endif
