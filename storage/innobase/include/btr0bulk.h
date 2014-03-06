/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/********************************************************************//**
@file include/btr0bulk.h
The B-tree bulk load

Created 11/21/2013 Shaohua Wang
*************************************************************************/

#ifndef btr0bulk_h
#define btr0bulk_h

#include "dict0dict.h"
#include "page0cur.h"

#include <vector>

extern	char	innobase_enable_bulk_load;

extern	char	innobase_enable_bulk_load_redo_log;

/* Innodb index fill factor during index build. */
extern	long	innobase_index_fill_factor;

/* Page bulk load structure. */
struct page_bulk_t {
	mem_heap_t*	heap;		/*!< heap for insert in page */
	dict_index_t*	index;		/*!< index */
	mtr_t*		mtr;		/*!< mtr for page_bulk */
	bool		logging;	/*!< flag whether need redo logging */
	buf_block_t*	block;		/*!< pointer to the block containing rec */
	page_t*		page;		/*!< page */
	page_zip_des_t*	page_zip;	/*!< page zip */
	rec_t*		cur_rec;	/*!< current rec */
	ulint		page_no;	/*!< page no */
	ulint		level;		/*!< page level in btree */
	bool		is_comp;	/*!< page is compact format */
	byte*		heap_top;	/*!< heap top for next insert */
	ulint		free_space;	/*!< free space left in the page */
	ulint		fill_space;	/*!< reserved space for fill factor */
	ulint		pad_space;	/*!< pad space for uncompressed page */
	ulint		heap_no;	/*!< heap no */
	ulint		rec_no;		/*!< rec_no */
};

typedef std::vector<page_bulk_t*>	page_bulk_vector;
typedef std::vector<ulint>		page_stat_vector;

/** Btree bulk load structure. */
struct btr_bulk_t {
	mem_heap_t*	heap;		/*!< heap for page cursor allocation */
        dict_index_t*   index;          /*!< index */
	trx_id_t	trx_id;		/*!< trx id */
	ulint		root_level;	/*!< root page level */

	page_bulk_vector*	page_bulks;
                                        /*!< page cursor vector for all level */
};

/** Btree bulk load init
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	index		index dict
@param[in]	trx_id		trx id
*/
void
btr_bulk_load_init(
	btr_bulk_t*	btr_bulk,
	dict_index_t*	index,
	trx_id_t	trx_id);

/** Btree bulkd load deinit
@param[in,out]	btr_bulk	btr bulk load state
@param[in]	success		whether bulk load is successful
@return error code */
dberr_t
btr_bulk_load_deinit(
	btr_bulk_t*	btr_bulk,
	bool		success);

/** Insert a tuple to btree
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	tuple		tuple to insert
@return error code */
dberr_t
btr_bulk_load_insert(
	btr_bulk_t*	btr_bulk,
	dtuple_t*	tuple);


#endif
