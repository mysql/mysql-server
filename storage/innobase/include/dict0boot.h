/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/dict0boot.h
Data dictionary creation and booting

Created 4/18/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0boot_h
#define dict0boot_h

#include "univ.i"

#include "mtr0mtr.h"
#include "mtr0log.h"
#include "ut0byte.h"
#include "buf0buf.h"
#include "fsp0fsp.h"
#include "dict0dict.h"

typedef	byte	dict_hdr_t;

/**********************************************************************//**
Gets a pointer to the dictionary header and x-latches its page.
@return	pointer to the dictionary header, page x-latched */
UNIV_INTERN
dict_hdr_t*
dict_hdr_get(
/*=========*/
	mtr_t*	mtr);	/*!< in: mtr */
/**********************************************************************//**
Returns a new table, index, or space id. */
UNIV_INTERN
void
dict_hdr_get_new_id(
/*================*/
	table_id_t*	table_id,	/*!< out: table id
					(not assigned if NULL) */
	index_id_t*	index_id,	/*!< out: index id
					(not assigned if NULL) */
	ulint*		space_id);	/*!< out: space id
					(not assigned if NULL) */
/**********************************************************************//**
Returns a new row id.
@return	the new id */
UNIV_INLINE
row_id_t
dict_sys_get_new_row_id(void);
/*=========================*/
/**********************************************************************//**
Reads a row id from a record or other 6-byte stored form.
@return	row id */
UNIV_INLINE
row_id_t
dict_sys_read_row_id(
/*=================*/
	const byte*	field);	/*!< in: record field */
/**********************************************************************//**
Writes a row id to a record or other 6-byte stored form. */
UNIV_INLINE
void
dict_sys_write_row_id(
/*==================*/
	byte*		field,	/*!< in: record field */
	row_id_t	row_id);/*!< in: row id */
/*****************************************************************//**
Initializes the data dictionary memory structures when the database is
started. This function is also called when the data dictionary is created. */
UNIV_INTERN
void
dict_boot(void);
/*===========*/
/*****************************************************************//**
Creates and initializes the data dictionary at the database creation. */
UNIV_INTERN
void
dict_create(void);
/*=============*/


/* Space id and page no where the dictionary header resides */
#define	DICT_HDR_SPACE		0	/* the SYSTEM tablespace */
#define	DICT_HDR_PAGE_NO	FSP_DICT_HDR_PAGE_NO

/* The ids for the basic system tables and their indexes */
#define DICT_TABLES_ID		1
#define DICT_COLUMNS_ID		2
#define DICT_INDEXES_ID		3
#define DICT_FIELDS_ID		4
/* The following is a secondary index on SYS_TABLES */
#define DICT_TABLE_IDS_ID	5

#define	DICT_HDR_FIRST_ID	10	/* the ids for tables etc. start
					from this number, except for basic
					system tables and their above defined
					indexes; ibuf tables and indexes are
					assigned as the id the number
					DICT_IBUF_ID_MIN plus the space id */
#define DICT_IBUF_ID_MIN	0xFFFFFFFF00000000ULL

/* The offset of the dictionary header on the page */
#define	DICT_HDR		FSEG_PAGE_DATA

/*-------------------------------------------------------------*/
/* Dictionary header offsets */
#define DICT_HDR_ROW_ID		0	/* The latest assigned row id */
#define	DICT_HDR_TABLE_ID	8	/* The latest assigned table id */
#define	DICT_HDR_INDEX_ID	16	/* The latest assigned index id */
#define DICT_HDR_MAX_SPACE_ID	24	/* The latest assigned space id, or 0*/
#define	DICT_HDR_MIX_ID_LOW	28	/* Obsolete,always DICT_HDR_FIRST_ID */
#define	DICT_HDR_TABLES		32	/* Root of the table index tree */
#define	DICT_HDR_TABLE_IDS	36	/* Root of the table index tree */
#define	DICT_HDR_COLUMNS	40	/* Root of the column index tree */
#define	DICT_HDR_INDEXES	44	/* Root of the index index tree */
#define	DICT_HDR_FIELDS		48	/* Root of the index field
					index tree */

#define DICT_HDR_FSEG_HEADER	56	/* Segment header for the tablespace
					segment into which the dictionary
					header is created */
/*-------------------------------------------------------------*/

/* The field numbers in the SYS_TABLES clustered index */
#define DICT_SYS_TABLES_TYPE_FIELD		5

/* The field numbers in the SYS_INDEXES clustered index */
#define DICT_SYS_INDEXES_PAGE_NO_FIELD	 8
#define DICT_SYS_INDEXES_SPACE_NO_FIELD	 7
#define DICT_SYS_INDEXES_TYPE_FIELD	 6
#define DICT_SYS_INDEXES_NAME_FIELD	 4

/* When a row id which is zero modulo this number (which must be a power of
two) is assigned, the field DICT_HDR_ROW_ID on the dictionary header page is
updated */
#define DICT_HDR_ROW_ID_WRITE_MARGIN	256

#ifndef UNIV_NONINL
#include "dict0boot.ic"
#endif

#endif
