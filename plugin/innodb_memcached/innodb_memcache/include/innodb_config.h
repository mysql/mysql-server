/***********************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

/** Max table name length as defined in univ.i */
#define MAX_TABLE_NAME_LEN      192
#define MAX_DATABASE_NAME_LEN   MAX_TABLE_NAME_LEN
#define MAX_FULL_NAME_LEN                               \
        (MAX_TABLE_NAME_LEN + MAX_DATABASE_NAME_LEN + 14)

/** structure describes each column's basic info (name, field_id etc.) */
typedef struct meta_columns {
	char*		m_str;			/*!< column name */
	int		m_len;			/*!< column name length */
	int		m_field_id;		/*!< column field id in
						the table */
	ib_col_meta_t	m_col;			/*!< column  meta info */
} meta_column_t;

/** Following are enums defining column IDs indexing into each of three
system tables */

/** Columns in the "containers" system table, this maps the Memcached
operation to a consistent InnoDB table */
enum container {
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
};

/** columns in the "cache_policy" table */
enum cache_policy {
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
};

/** columns in the "config_options" table */
enum config_opt {
	CONFIG_OPT_KEY,		/*!< key column in the "config_option" table */
	CONFIG_OPT_VALUE,	/*!< value column */
	CONFIG_OPT_NUM_COLS	/*!< number of columns (currently 2) in table */
};

/** Following are some value defines describes the options that configures
the InnoDB Memcached */

/** Values to set up "m_use_idx" field of "meta_index_t" structure,
indicating whether we will use cluster or secondary index on the
"key" column to perform the search. Please note the index must
be unique index */
typedef enum meta_use_idx {
	META_NO_INDEX = 1,	/*!< no cluster or unique secondary index
				on the key column. This is an error, will
				cause setup to fail */
	META_CLUSTER,		/*!< have cluster index on the key column */
	META_SECONDARY		/*!< have unique secondary index on the
				key column */
} meta_use_idx_t;

/** Describes the index's name and ID of the index on the "key" column */
typedef struct meta_index {
	char*		m_name;		/*!< index name */
	int		m_id;		/*!< index id */
	meta_use_idx_t	m_use_idx;	/*!< has cluster or secondary
					index on the key column */
} meta_index_t;

/** Cache options, tells if we will used Memcached default engine or InnoDB
Memcached engine to handle the request */
typedef enum meta_cache_opt {
	META_CACHE_OPT_INNODB = 1,	/*!< Use InnoDB Memcached Engine only */
	META_CACHE_OPT_DEFAULT,		/*!< Use Default Memcached Engine
					only */
	META_CACHE_OPT_MIX		/*!< Use both, first use default
					memcached engine */
} meta_cache_opt_t;

/** In memory structure contains most necessary metadata info
to configure an InnoDB Memcached engine */
typedef struct meta_cfg_info {
	meta_column_t	m_item[CONTAINER_NUM_COLS]; /*!< column info */
	meta_column_t*	m_add_item;		/*!< additional columns
						specified for the value field */
	int		m_num_add;		/*!< number of additional
						value columns */
	meta_index_t	m_index;		/*!< Index info */
	bool		m_flag_enabled;		/*!< whether flag is enabled */
	bool		m_cas_enabled;		/*!< whether cas is enabled */
	bool		m_exp_enabled;		/*!< whether exp is enabled */
	char*		m_separator;		/*!< separator that separates
						incoming "value" string for
						multiple columns */
	int		m_sep_len;		/*!< separator length */
	meta_cache_opt_t m_set_option;		/*!< cache option for "set" */
	meta_cache_opt_t m_get_option;		/*!< cache option for "get" */
	meta_cache_opt_t m_del_option;		/*!< cache option for
						"delete" */
	meta_cache_opt_t m_flush_option;	/*!< cache option for
						"delete" */
} meta_cfg_info_t;

/**********************************************************************//**
This function opens the default configuration table, and find the
table and column info that used for InnoDB Memcached, and set up
InnoDB Memcached's meta_cfg_info_t structure
@return true if everything works out fine */
bool
innodb_config(
/*==========*/
	meta_cfg_info_t*	item);		/*!< out: meta info structure */

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

#endif
