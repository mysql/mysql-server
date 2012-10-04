/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
Writes the current value of the row id counter to the dictionary header file
page. */
UNIV_INTERN
void
dict_hdr_flush_row_id(void);
/*=======================*/
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
started. This function is also called when the data dictionary is created.
@return DB_SUCCESS or error code. */
UNIV_INTERN
dberr_t
dict_boot(void)
/*===========*/
	__attribute__((warn_unused_result));

/*****************************************************************//**
Creates and initializes the data dictionary at the server bootstrap.
@return DB_SUCCESS or error code. */
UNIV_INTERN
dberr_t
dict_create(void)
/*=============*/
	__attribute__((warn_unused_result));

/*********************************************************************//**
Check if a table id belongs to  system table.
@return true if the table id belongs to a system table. */
UNIV_INLINE
bool
dict_is_sys_table(
/*==============*/
	table_id_t	id)		/*!< in: table id to check */
	__attribute__((warn_unused_result));

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

/* The offset of the dictionary header on the page */
#define	DICT_HDR		FSEG_PAGE_DATA

/*-------------------------------------------------------------*/
/* Dictionary header offsets */
#define DICT_HDR_ROW_ID		0	/* The latest assigned row id */
#define DICT_HDR_TABLE_ID	8	/* The latest assigned table id */
#define DICT_HDR_INDEX_ID	16	/* The latest assigned index id */
#define DICT_HDR_MAX_SPACE_ID	24	/* The latest assigned space id,or 0*/
#define DICT_HDR_MIX_ID_LOW	28	/* Obsolete,always DICT_HDR_FIRST_ID*/
#define DICT_HDR_TABLES		32	/* Root of SYS_TABLES clust index */
#define DICT_HDR_TABLE_IDS	36	/* Root of SYS_TABLE_IDS sec index */
#define DICT_HDR_COLUMNS	40	/* Root of SYS_COLUMNS clust index */
#define DICT_HDR_INDEXES	44	/* Root of SYS_INDEXES clust index */
#define DICT_HDR_FIELDS		48	/* Root of SYS_FIELDS clust index */

#define DICT_HDR_FSEG_HEADER	56	/* Segment header for the tablespace
					segment into which the dictionary
					header is created */
/*-------------------------------------------------------------*/

/* The columns in SYS_TABLES */
enum dict_col_sys_tables_enum {
	DICT_COL__SYS_TABLES__NAME		= 0,
	DICT_COL__SYS_TABLES__ID		= 1,
	DICT_COL__SYS_TABLES__N_COLS		= 2,
	DICT_COL__SYS_TABLES__TYPE		= 3,
	DICT_COL__SYS_TABLES__MIX_ID		= 4,
	DICT_COL__SYS_TABLES__MIX_LEN		= 5,
	DICT_COL__SYS_TABLES__CLUSTER_ID	= 6,
	DICT_COL__SYS_TABLES__SPACE		= 7,
	DICT_NUM_COLS__SYS_TABLES		= 8
};
/* The field numbers in the SYS_TABLES clustered index */
enum dict_fld_sys_tables_enum {
	DICT_FLD__SYS_TABLES__NAME		= 0,
	DICT_FLD__SYS_TABLES__DB_TRX_ID		= 1,
	DICT_FLD__SYS_TABLES__DB_ROLL_PTR	= 2,
	DICT_FLD__SYS_TABLES__ID		= 3,
	DICT_FLD__SYS_TABLES__N_COLS		= 4,
	DICT_FLD__SYS_TABLES__TYPE		= 5,
	DICT_FLD__SYS_TABLES__MIX_ID		= 6,
	DICT_FLD__SYS_TABLES__MIX_LEN		= 7,
	DICT_FLD__SYS_TABLES__CLUSTER_ID	= 8,
	DICT_FLD__SYS_TABLES__SPACE		= 9,
	DICT_NUM_FIELDS__SYS_TABLES		= 10
};
/* The field numbers in the SYS_TABLE_IDS index */
enum dict_fld_sys_table_ids_enum {
	DICT_FLD__SYS_TABLE_IDS__ID		= 0,
	DICT_FLD__SYS_TABLE_IDS__NAME		= 1,
	DICT_NUM_FIELDS__SYS_TABLE_IDS		= 2
};
/* The columns in SYS_COLUMNS */
enum dict_col_sys_columns_enum {
	DICT_COL__SYS_COLUMNS__TABLE_ID		= 0,
	DICT_COL__SYS_COLUMNS__POS		= 1,
	DICT_COL__SYS_COLUMNS__NAME		= 2,
	DICT_COL__SYS_COLUMNS__MTYPE		= 3,
	DICT_COL__SYS_COLUMNS__PRTYPE		= 4,
	DICT_COL__SYS_COLUMNS__LEN		= 5,
	DICT_COL__SYS_COLUMNS__PREC		= 6,
	DICT_NUM_COLS__SYS_COLUMNS		= 7
};
/* The field numbers in the SYS_COLUMNS clustered index */
enum dict_fld_sys_columns_enum {
	DICT_FLD__SYS_COLUMNS__TABLE_ID		= 0,
	DICT_FLD__SYS_COLUMNS__POS		= 1,
	DICT_FLD__SYS_COLUMNS__DB_TRX_ID	= 2,
	DICT_FLD__SYS_COLUMNS__DB_ROLL_PTR	= 3,
	DICT_FLD__SYS_COLUMNS__NAME		= 4,
	DICT_FLD__SYS_COLUMNS__MTYPE		= 5,
	DICT_FLD__SYS_COLUMNS__PRTYPE		= 6,
	DICT_FLD__SYS_COLUMNS__LEN		= 7,
	DICT_FLD__SYS_COLUMNS__PREC		= 8,
	DICT_NUM_FIELDS__SYS_COLUMNS		= 9
};
/* The columns in SYS_INDEXES */
enum dict_col_sys_indexes_enum {
	DICT_COL__SYS_INDEXES__TABLE_ID		= 0,
	DICT_COL__SYS_INDEXES__ID		= 1,
	DICT_COL__SYS_INDEXES__NAME		= 2,
	DICT_COL__SYS_INDEXES__N_FIELDS		= 3,
	DICT_COL__SYS_INDEXES__TYPE		= 4,
	DICT_COL__SYS_INDEXES__SPACE		= 5,
	DICT_COL__SYS_INDEXES__PAGE_NO		= 6,
	DICT_NUM_COLS__SYS_INDEXES		= 7
};
/* The field numbers in the SYS_INDEXES clustered index */
enum dict_fld_sys_indexes_enum {
	DICT_FLD__SYS_INDEXES__TABLE_ID		= 0,
	DICT_FLD__SYS_INDEXES__ID		= 1,
	DICT_FLD__SYS_INDEXES__DB_TRX_ID	= 2,
	DICT_FLD__SYS_INDEXES__DB_ROLL_PTR	= 3,
	DICT_FLD__SYS_INDEXES__NAME		= 4,
	DICT_FLD__SYS_INDEXES__N_FIELDS		= 5,
	DICT_FLD__SYS_INDEXES__TYPE		= 6,
	DICT_FLD__SYS_INDEXES__SPACE		= 7,
	DICT_FLD__SYS_INDEXES__PAGE_NO		= 8,
	DICT_NUM_FIELDS__SYS_INDEXES		= 9
};
/* The columns in SYS_FIELDS */
enum dict_col_sys_fields_enum {
	DICT_COL__SYS_FIELDS__INDEX_ID		= 0,
	DICT_COL__SYS_FIELDS__POS		= 1,
	DICT_COL__SYS_FIELDS__COL_NAME		= 2,
	DICT_NUM_COLS__SYS_FIELDS		= 3
};
/* The field numbers in the SYS_FIELDS clustered index */
enum dict_fld_sys_fields_enum {
	DICT_FLD__SYS_FIELDS__INDEX_ID		= 0,
	DICT_FLD__SYS_FIELDS__POS		= 1,
	DICT_FLD__SYS_FIELDS__DB_TRX_ID		= 2,
	DICT_FLD__SYS_FIELDS__DB_ROLL_PTR	= 3,
	DICT_FLD__SYS_FIELDS__COL_NAME		= 4,
	DICT_NUM_FIELDS__SYS_FIELDS		= 5
};
/* The columns in SYS_FOREIGN */
enum dict_col_sys_foreign_enum {
	DICT_COL__SYS_FOREIGN__ID		= 0,
	DICT_COL__SYS_FOREIGN__FOR_NAME		= 1,
	DICT_COL__SYS_FOREIGN__REF_NAME		= 2,
	DICT_COL__SYS_FOREIGN__N_COLS		= 3,
	DICT_NUM_COLS__SYS_FOREIGN		= 4
};
/* The field numbers in the SYS_FOREIGN clustered index */
enum dict_fld_sys_foreign_enum {
	DICT_FLD__SYS_FOREIGN__ID		= 0,
	DICT_FLD__SYS_FOREIGN__DB_TRX_ID	= 1,
	DICT_FLD__SYS_FOREIGN__DB_ROLL_PTR	= 2,
	DICT_FLD__SYS_FOREIGN__FOR_NAME		= 3,
	DICT_FLD__SYS_FOREIGN__REF_NAME		= 4,
	DICT_FLD__SYS_FOREIGN__N_COLS		= 5,
	DICT_NUM_FIELDS__SYS_FOREIGN		= 6
};
/* The field numbers in the SYS_FOREIGN_FOR_NAME secondary index */
enum dict_fld_sys_foreign_for_name_enum {
	DICT_FLD__SYS_FOREIGN_FOR_NAME__NAME	= 0,
	DICT_FLD__SYS_FOREIGN_FOR_NAME__ID	= 1,
	DICT_NUM_FIELDS__SYS_FOREIGN_FOR_NAME	= 2
};
/* The columns in SYS_FOREIGN_COLS */
enum dict_col_sys_foreign_cols_enum {
	DICT_COL__SYS_FOREIGN_COLS__ID			= 0,
	DICT_COL__SYS_FOREIGN_COLS__POS			= 1,
	DICT_COL__SYS_FOREIGN_COLS__FOR_COL_NAME	= 2,
	DICT_COL__SYS_FOREIGN_COLS__REF_COL_NAME	= 3,
	DICT_NUM_COLS__SYS_FOREIGN_COLS			= 4
};
/* The field numbers in the SYS_FOREIGN_COLS clustered index */
enum dict_fld_sys_foreign_cols_enum {
	DICT_FLD__SYS_FOREIGN_COLS__ID			= 0,
	DICT_FLD__SYS_FOREIGN_COLS__POS			= 1,
	DICT_FLD__SYS_FOREIGN_COLS__DB_TRX_ID		= 2,
	DICT_FLD__SYS_FOREIGN_COLS__DB_ROLL_PTR		= 3,
	DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME	= 4,
	DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME	= 5,
	DICT_NUM_FIELDS__SYS_FOREIGN_COLS		= 6
};
/* The columns in SYS_TABLESPACES */
enum dict_col_sys_tablespaces_enum {
	DICT_COL__SYS_TABLESPACES__SPACE		= 0,
	DICT_COL__SYS_TABLESPACES__NAME			= 1,
	DICT_COL__SYS_TABLESPACES__FLAGS		= 2,
	DICT_NUM_COLS__SYS_TABLESPACES			= 3
};
/* The field numbers in the SYS_TABLESPACES clustered index */
enum dict_fld_sys_tablespaces_enum {
	DICT_FLD__SYS_TABLESPACES__SPACE		= 0,
	DICT_FLD__SYS_TABLESPACES__DB_TRX_ID		= 1,
	DICT_FLD__SYS_TABLESPACES__DB_ROLL_PTR		= 2,
	DICT_FLD__SYS_TABLESPACES__NAME			= 3,
	DICT_FLD__SYS_TABLESPACES__FLAGS		= 4,
	DICT_NUM_FIELDS__SYS_TABLESPACES		= 5
};
/* The columns in SYS_DATAFILES */
enum dict_col_sys_datafiles_enum {
	DICT_COL__SYS_DATAFILES__SPACE			= 0,
	DICT_COL__SYS_DATAFILES__PATH			= 1,
	DICT_NUM_COLS__SYS_DATAFILES			= 2
};
/* The field numbers in the SYS_DATAFILES clustered index */
enum dict_fld_sys_datafiles_enum {
	DICT_FLD__SYS_DATAFILES__SPACE			= 0,
	DICT_FLD__SYS_DATAFILES__DB_TRX_ID		= 1,
	DICT_FLD__SYS_DATAFILES__DB_ROLL_PTR		= 2,
	DICT_FLD__SYS_DATAFILES__PATH			= 3,
	DICT_NUM_FIELDS__SYS_DATAFILES			= 4
};

/* A number of the columns above occur in multiple tables.  These are the
length of thos fields. */
#define	DICT_FLD_LEN_SPACE	4
#define	DICT_FLD_LEN_FLAGS	4

/* When a row id which is zero modulo this number (which must be a power of
two) is assigned, the field DICT_HDR_ROW_ID on the dictionary header page is
updated */
#define DICT_HDR_ROW_ID_WRITE_MARGIN	256

#ifndef UNIV_NONINL
#include "dict0boot.ic"
#endif

#endif
