/*****************************************************************************

Copyright (c) 2014, 2015, Oracle and/or its affiliates. All Rights Reserved.

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

Created 03/11/2014 Shaohua Wang
*************************************************************************/

#ifndef btr0bulk_h
#define btr0bulk_h

#include "dict0dict.h"
#include "page0cur.h"
#include "ut0new.h"

#include <vector>

/** Innodb B-tree index fill factor for bulk load. */
extern	long	innobase_fill_factor;

/*
The proper function call sequence of PageBulk is as below:
-- PageBulk::init
-- PageBulk::insert
-- PageBulk::finish
-- PageBulk::compress(COMPRESSED table only)
-- PageBulk::pageSplit(COMPRESSED table only)
-- PageBulk::commit
*/

class PageBulk
{
public:
	/** Constructor
	@param[in]	index		B-tree index
	@param[in]	page_no		page number
	@param[in]	level		page level
	@param[in]	trx_id		transaction id
	@param[in]	observer	flush observer */
	PageBulk(
		dict_index_t*	index,
		trx_id_t	trx_id,
		ulint		page_no,
		ulint		level,
		FlushObserver*	observer)
		:
		m_heap(NULL),
		m_index(index),
		m_mtr(NULL),
		m_trx_id(trx_id),
		m_block(NULL),
		m_page(NULL),
		m_page_zip(NULL),
		m_cur_rec(NULL),
		m_page_no(page_no),
		m_level(level),
		m_is_comp(dict_table_is_comp(index->table)),
		m_heap_top(NULL),
		m_rec_no(0),
		m_free_space(0),
		m_reserved_space(0),
#ifdef UNIV_DEBUG
		m_total_data(0),
#endif /* UNIV_DEBUG */
		m_modify_clock(0),
		m_flush_observer(observer)
	{
		ut_ad(!dict_index_is_spatial(m_index));
	}

	/** Deconstructor */
	~PageBulk()
	{
		mem_heap_free(m_heap);
	}

	/** Initialize members and allocate page if needed and start mtr.
	Note: must be called and only once right after constructor.
	@return error code */
	dberr_t init();

	/** Insert a record in the page.
	@param[in]	rec		record
	@param[in]	offsets		record offsets */
	void insert(const rec_t* rec, ulint* offsets);

	/** Mark end of insertion to the page. Scan all records to set page
	dirs, and set page header members. */
	void finish();

	/** Commit mtr for a page
	@param[in]	success		Flag whether all inserts succeed. */
	void commit(bool success);

	/** Compress if it is compressed table
	@return	true	compress successfully or no need to compress
	@return	false	compress failed. */
	bool compress();

	/** Check whether the record needs to be stored externally.
	@return	true
	@return	false */
	bool needExt(const dtuple_t* tuple, ulint rec_size);

	/** Store external record
	@param[in]	big_rec		external recrod
	@param[in]	offsets		record offsets
	@return	error code */
	dberr_t storeExt(const big_rec_t* big_rec, ulint* offsets);

	/** Get node pointer
	@return node pointer */
	dtuple_t* getNodePtr();

	/** Get split rec in the page. We split a page in half when compresssion
	fails, and the split rec should be copied to the new page.
	@return split rec */
	rec_t*	getSplitRec();

	/** Copy all records after split rec including itself.
	@param[in]	rec	split rec */
	void copyIn(rec_t*	split_rec);

	/** Remove all records after split rec including itself.
	@param[in]	rec	split rec	*/
	void copyOut(rec_t*	split_rec);

	/** Set next page
	@param[in]	next_page_no	next page no */
	void setNext(ulint	next_page_no);

	/** Set previous page
	@param[in]	prev_page_no	previous page no */
	void setPrev(ulint	prev_page_no);

	/** Release block by commiting mtr */
	inline void release();

	/** Start mtr and latch block */
	inline void latch();

	/** Check if required space is available in the page for the rec
	to be inserted.	We check fill factor & padding here.
	@param[in]	length		required length
	@return true	if space is available */
	inline bool isSpaceAvailable(ulint	rec_size);

	/** Get page no */
	ulint	getPageNo()
	{
		return(m_page_no);
	}

	/** Get page level */
	ulint	getLevel()
	{
		return(m_level);
	}

	/** Get record no */
	ulint	getRecNo()
	{
		return(m_rec_no);
	}

	/** Get page */
	page_t*	getPage()
	{
		return(m_page);
	}

	/** Get page zip */
	page_zip_des_t*	getPageZip()
	{
		return(m_page_zip);
	}

	/* Memory heap for internal allocation */
	mem_heap_t*	m_heap;

private:
	/** The index B-tree */
	dict_index_t*	m_index;

	/** The min-transaction */
	mtr_t*		m_mtr;

	/** The transaction id */
	trx_id_t	m_trx_id;

	/** The buffer block */
	buf_block_t*	m_block;

	/** The page */
	page_t*		m_page;

	/** The page zip descriptor */
	page_zip_des_t*	m_page_zip;

	/** The current rec, just before the next insert rec */
	rec_t*		m_cur_rec;

	/** The page no */
	ulint		m_page_no;

	/** The page level in B-tree */
	ulint		m_level;

	/** Flag: is page in compact format */
	const bool	m_is_comp;

	/** The heap top in page for next insert */
	byte*		m_heap_top;

	/** User record no */
	ulint		m_rec_no;

	/** The free space left in the page */
	ulint		m_free_space;

	/** The reserved space for fill factor */
	ulint		m_reserved_space;

	/** The padding space for compressed page */
	ulint		m_padding_space;

#ifdef UNIV_DEBUG
	/** Total data in the page */
	ulint		m_total_data;
#endif /* UNIV_DEBUG */

	/** The modify clock value of the buffer block
	when the block is re-pinned */
	ib_uint64_t     m_modify_clock;

	/** Flush observer */
	FlushObserver*	m_flush_observer;
};

typedef std::vector<PageBulk*, ut_allocator<PageBulk*> >
	page_bulk_vector;

class BtrBulk
{
public:
	/** Constructor
	@param[in]	index		B-tree index
	@param[in]	trx_id		transaction id
	@param[in]	observer	flush observer */
	BtrBulk(
		dict_index_t*	index,
		trx_id_t	trx_id,
		FlushObserver*	observer)
		:
		m_heap(NULL),
		m_index(index),
		m_trx_id(trx_id),
		m_flush_observer(observer)
	{
		ut_ad(m_flush_observer != NULL);
#ifdef UNIV_DEBUG
		fil_space_inc_redo_skipped_count(m_index->space);
#endif /* UNIV_DEBUG */
	}

	/** Destructor */
	~BtrBulk()
	{
		mem_heap_free(m_heap);
		UT_DELETE(m_page_bulks);

#ifdef UNIV_DEBUG
		fil_space_dec_redo_skipped_count(m_index->space);
#endif /* UNIV_DEBUG */
	}

	/** Initialization
	Note: must be called right after constructor. */
	void init()
	{
		ut_ad(m_heap == NULL);
		m_heap = mem_heap_create(1000);

		m_page_bulks = UT_NEW_NOKEY(page_bulk_vector());
	}

	/** Insert a tuple
	@param[in]	tuple	tuple to insert.
	@return error code */
	dberr_t	insert(dtuple_t*	tuple)
	{
		return(insert(tuple, 0));
	}

	/** Btree bulk load finish. We commit the last page in each level
	and copy the last page in top level to the root page of the index
	if no error occurs.
	@param[in]	err	whether bulk load was successful until now
	@return error code  */
	dberr_t finish(dberr_t	err);

	/** Release all latches */
	void release();

	/** Re-latch all latches */
	void latch();

private:
	/** Insert a tuple to a page in a level
	@param[in]	tuple	tuple to insert
	@param[in]	level	B-tree level
	@return error code */
	dberr_t insert(dtuple_t* tuple, ulint level);

	/** Split a page
	@param[in]	page_bulk	page to split
	@param[in]	next_page_bulk	next page
	@return	error code */
	dberr_t pageSplit(PageBulk* page_bulk,
			  PageBulk* next_page_bulk);

	/** Commit(finish) a page. We set next/prev page no, compress a page of
	compressed table and split the page if compression fails, insert a node
	pointer to father page if needed, and commit mini-transaction.
	@param[in]	page_bulk	page to commit
	@param[in]	next_page_bulk	next page
	@param[in]	insert_father	flag whether need to insert node ptr
	@return	error code */
	dberr_t pageCommit(PageBulk* page_bulk,
			   PageBulk* next_page_bulk,
			   bool insert_father);

	/** Abort a page when an error occurs
	@param[in]	page_bulk	page bulk object
	Note: we should call pageAbort for a PageBulk object, which is not in
	m_page_bulks after pageCommit, and we will commit or abort PageBulk
	objects in function "finish". */
	void	pageAbort(PageBulk* page_bulk)
	{
		page_bulk->commit(false);
	}

	/** Log free check */
	void logFreeCheck();

private:
	/** Memory heap for allocation */
	mem_heap_t*		m_heap;

	/** B-tree index */
	dict_index_t*		m_index;

	/** Transaction id */
	trx_id_t		m_trx_id;

	/** Root page level */
	ulint			m_root_level;

	/** Flush observer */
	FlushObserver*		m_flush_observer;

	/** Page cursor vector for all level */
	page_bulk_vector*	m_page_bulks;
};

#endif
