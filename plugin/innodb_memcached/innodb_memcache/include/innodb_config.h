/***********************************************************************

Copyright (c) 2010, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

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
NDB memcache, so to make the memcache setup compatible between the two.*/
#define	INNODB_META_DB			"innodb_memcache"
#define INNODB_META_CONTAINER_TABLE	"containers"
#define INNODB_CACHE_POLICIES		"cache_policies"
#define INNODB_CONFIG_OPTIONS		"config_options"

/** structure describes each column's basic info (name, field_id etc.) */
typedef struct meta_columns {
	char*		m_str;			/*!< column name */
	int		m_len;			/*!< column name length */
	int		m_field_id;		/*!< column field id in
						the table */
	ib_col_meta_t	m_col;			/*!< column  meta info */
} meta_column_t;


/** Columns in the "containers" system table */

enum meta_container_idx {
	META_NAME,
	META_DB,
	META_TABLE,
	META_KEY,
	META_VALUE,
	META_FLAG,
	META_CAS,
	META_EXP,
	META_CONTAINER_TO_GET
};

/** Indicate whether we have cluster or secondary index on "key" column
of the table. Please note the index must be unique index */
typedef enum meta_use_idx {
	META_NO_INDEX = 1,
	META_CLUSTER,
	META_SECONDARY
} meta_use_idx_t;

/** Describes the index's name and ID of the index on the "key" column */
typedef struct meta_index {
	char*		m_name;			/*!< index name */
	int		m_id;			/*!< index id */
	meta_use_idx_t	m_use_idx;		/*!< has cluster or secondary
						index on the key column */
} meta_index_t;

typedef enum meta_cache_option {
	META_INNODB = 1,
	META_CACHE,
	META_MIX,
	META_DISABLED
} meta_cache_option_t;

/** columns in the "cache_policy" table */
enum meta_cache_cols {
	CACHE_OPT_NAME,
	CACHE_OPT_GET,
	CACHE_OPT_SET,
	CACHE_OPT_DEL,
	CACHE_OPT_FLUSH,
	CACHE_OPT_NUM_COLS	
};

/** columns in the "config_options" table */
enum meta_config_cols {
	CONFIG_KEY,
	CONFIG_VALUE,
	CONFIG_NUM_COLS
};

typedef struct meta_container_info {
	meta_column_t	m_item[META_CONTAINER_TO_GET]; /*!< column info */
	meta_column_t*	m_add_item;		/*!< additional columns
						specified for the value field */
	int		m_num_add;		/*!< number of additional
						value columns */
	meta_index_t	m_index;		/*!< Index info */
	bool		flag_enabled;		/*!< whether flag is enabled */
	bool		cas_enabled;		/*!< whether cas is enabled */
	bool		exp_enabled;		/*!< whether exp is enabled */
	char*		m_separator;		/*!< separator that separates
						incoming "value" string for
						multiple columns */
	int		m_sep_len;		/*!< separator length */
	meta_cache_option_t m_set_option;	/*!< cache option for "set" */
	meta_cache_option_t m_get_option;	/*!< cache option for "get" */
	meta_cache_option_t m_del_option;	/*!< cache option for
						"delete" */
	meta_cache_option_t m_flush_option;	/*!< cache option for
						"delete" */
} meta_info_t;

/**********************************************************************//**
This function opens the default configuration table, and find the
table and column info that used for memcached data 
@return TRUE if everything works out fine */
bool
innodb_config(
/*==========*/
	meta_info_t*	item);

/**********************************************************************//**
This function verify the table configuration information, and fill
in columns used for memcached functionalities (cas, exp etc.)
@return TRUE if everything works out fine */
bool
innodb_verify(
/*==========*/
	meta_info_t*	container);

/**********************************************************************//**
This function frees meta info structure */
void
innodb_config_free(
/*===============*/
        meta_info_t*	item);

/*******************************************************************//**
Removes a node from a linked list. */
#define MCI_LIST_REMOVE(NAME, BASE, N)					\
do {									\
        ((BASE).count)--;						\
        if (((N)->NAME).next != NULL) {					\
                ((((N)->NAME).next)->NAME).prev = ((N)->NAME).prev;	\
        } else {							\
                (BASE).end = ((N)->NAME).prev;				\
        }								\
        if (((N)->NAME).prev != NULL) {					\
                ((((N)->NAME).prev)->NAME).next = ((N)->NAME).next;	\
        } else {							\
                (BASE).start = ((N)->NAME).next;			\
        }								\
        UT_LIST_REMOVE_CLEAR(NAME, N);					\
} while (0)

#endif
