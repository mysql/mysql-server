/******************************************************
Data dictionary creation and booting

(c) 1996 Innobase Oy

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

/**************************************************************************
Gets a pointer to the dictionary header and x-latches its page. */

dict_hdr_t*
dict_hdr_get(
/*=========*/
			/* out: pointer to the dictionary header, 
			page x-latched */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
Returns a new row, table, index, or tree id. */

dulint
dict_hdr_get_new_id(
/*================*/
			/* out: the new id */
	ulint	type);	/* in: DICT_HDR_ROW_ID, ... */
/**************************************************************************
Returns a new row id. */
UNIV_INLINE
dulint
dict_sys_get_new_row_id(void);
/*=========================*/
			/* out: the new id */
/**************************************************************************
Reads a row id from a record or other 6-byte stored form. */
UNIV_INLINE
dulint
dict_sys_read_row_id(
/*=================*/
			/* out: row id */
	byte*	field);	/* in: record field */
/**************************************************************************
Writes a row id to a record or other 6-byte stored form. */
UNIV_INLINE
void
dict_sys_write_row_id(
/*==================*/
	byte*	field,	/* in: record field */
	dulint	row_id);/* in: row id */
/*********************************************************************
Initializes the data dictionary memory structures when the database is
started. This function is also called when the data dictionary is created. */

void
dict_boot(void);
/*===========*/
/*********************************************************************
Creates and initializes the data dictionary at the database creation. */

void
dict_create(void);
/*=============*/


/* Space id and page no where the dictionary header resides */
#define	DICT_HDR_SPACE		0	/* the SYSTEM tablespace */
#define	DICT_HDR_PAGE_NO	FSP_DICT_HDR_PAGE_NO

/* The ids for the basic system tables and their indexes */
#define DICT_TABLES_ID		ut_dulint_create(0, 1)
#define DICT_COLUMNS_ID		ut_dulint_create(0, 2)
#define DICT_INDEXES_ID		ut_dulint_create(0, 3)
#define DICT_FIELDS_ID		ut_dulint_create(0, 4)
/* The following is a secondary index on SYS_TABLES */
#define DICT_TABLE_IDS_ID	ut_dulint_create(0, 5)

#define	DICT_HDR_FIRST_ID	10	/* the ids for tables etc. start
					from this number, except for basic
					system tables and their above defined
					indexes; ibuf tables and indexes are
					assigned as the id the number
					DICT_IBUF_ID_MIN plus the space id */
#define DICT_IBUF_ID_MIN	ut_dulint_create(0xFFFFFFFFUL, 0)
					
/* The offset of the dictionary header on the page */
#define	DICT_HDR		FSEG_PAGE_DATA

/*-------------------------------------------------------------*/
/* Dictionary header offsets */
#define DICT_HDR_ROW_ID		0	/* The latest assigned row id */
#define	DICT_HDR_TABLE_ID	8	/* The latest assigned table id */
#define	DICT_HDR_INDEX_ID	16	/* The latest assigned index id */
#define	DICT_HDR_MIX_ID		24	/* The latest assigned mix id */
#define	DICT_HDR_TABLES		32	/* Root of the table index tree */
#define	DICT_HDR_TABLE_IDS	36	/* Root of the table index tree */
#define	DICT_HDR_COLUMNS	40	/* Root of the column index tree */
#define	DICT_HDR_INDEXES	44	/* Root of the index index tree */
#define	DICT_HDR_FIELDS		48	/* Root of the index field index tree */

#define DICT_HDR_FSEG_HEADER	56	/* Segment header for the tablespace
					segment into which the dictionary
					header is created */
/*-------------------------------------------------------------*/

/* The field number of the page number field in the sys_indexes table
clustered index */
#define DICT_SYS_INDEXES_PAGE_NO_FIELD	 8
#define DICT_SYS_INDEXES_SPACE_NO_FIELD	 7
#define DICT_SYS_INDEXES_TYPE_FIELD	 6
					
/* When a row id which is zero modulo this number (which must be a power of
two) is assigned, the field DICT_HDR_ROW_ID on the dictionary header page is
updated */
#define DICT_HDR_ROW_ID_WRITE_MARGIN	256

#ifndef UNIV_NONINL
#include "dict0boot.ic"
#endif

#endif 
