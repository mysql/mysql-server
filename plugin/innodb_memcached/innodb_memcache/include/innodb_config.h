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


/* Database name and table name for our metadata "system" table for
memcached */
#define	INNODB_META_DB			"innodb_memcache"
#define INNODB_META_CONTAINER_TABLE	"containers"

/** structure describes each column's basic info (name, field_id etc.) */
typedef struct meta_columns {
	char*		m_str;
	int		m_len;
	int		m_field_id;
	ib_col_meta_t	m_col;
} meta_column_t;

#define	META_CONTAINER_TO_GET		8

/** ID into the meta_info_t->m_item, describes metadata info for table and its
columns corresponding to each memcached field */
enum meta_container_idx {
	META_NAME,
	META_DB,
	META_TABLE,
	META_KEY,
	META_VALUE,
	META_FLAG,
	META_CAS,
	META_EXP
};

/** Indicate whether type of index on "key" column of the table. */
typedef enum meta_use_idx {
	META_NO_INDEX = 1,
	META_CLUSTER,
	META_SECONDARY
} meta_use_idx_t;

/** Describes the index's name and ID of the index on the "key" column */
typedef struct meta_index {
	char*		m_name;
	int		m_id;
	meta_use_idx_t	m_use_idx;
	ib_crsr_t	m_idx_crsr;
} meta_index_t;

typedef struct meta_container_info {
	meta_column_t	m_item[META_CONTAINER_TO_GET];
	meta_column_t*	m_add_item;
	int		m_num_add;
	meta_index_t	m_index;
	bool		flag_enabled;
	bool		cas_enabled;
	bool		exp_enabled;
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

#endif
