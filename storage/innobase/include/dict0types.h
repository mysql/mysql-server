/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/dict0types.h
Data dictionary global types

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0types_h
#define dict0types_h

typedef struct dict_sys_struct		dict_sys_t;
typedef struct dict_col_struct		dict_col_t;
typedef struct dict_field_struct	dict_field_t;
typedef struct dict_index_struct	dict_index_t;
typedef struct dict_table_struct	dict_table_t;
typedef struct dict_foreign_struct	dict_foreign_t;

typedef struct ind_node_struct		ind_node_t;
typedef struct tab_node_struct		tab_node_t;

/* Space id and page no where the dictionary header resides */
#define	DICT_HDR_SPACE		0	/* the SYSTEM tablespace */
#define	DICT_HDR_PAGE_NO	FSP_DICT_HDR_PAGE_NO

typedef ib_id_t		table_id_t;
typedef ib_id_t		index_id_t;

/** Error to ignore when we load table dictionary into memory. However,
the table and index will be marked as "corrupted", and caller will
be responsible to deal with corrupted table or index.
Note: please define the IGNORE_ERR_* as bits, so their value can
be or-ed together */
enum dict_err_ignore {
        DICT_ERR_IGNORE_NONE = 0,        /*!< no error to ignore */
        DICT_ERR_IGNORE_INDEX_ROOT = 1, /*!< ignore error if index root
					page is FIL_NULL or incorrect value */
	DICT_ERR_IGNORE_CORRUPT = 2,	/*!< skip corrupted indexes */
	DICT_ERR_IGNORE_FK_NOKEY = 4,	/*!< ignore error if any foreign
					key is missing */
        DICT_ERR_IGNORE_ALL = 0xFFFF	/*!< ignore all errors */
};

typedef enum dict_err_ignore		dict_err_ignore_t;

#define TEMP_TABLE_PREFIX                "#sql"
#define TEMP_TABLE_PATH_PREFIX           "/" TEMP_TABLE_PREFIX

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
extern uint		ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

#endif
