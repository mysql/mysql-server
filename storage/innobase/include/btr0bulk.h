/*****************************************************************************

Copyright (c) 2014, Oracle and/or its affiliates. All Rights Reserved.

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

#include <vector>

extern	char	innobase_enable_bulk_load;

/* Innodb index fill factor during index build. */
extern	long	innobase_index_fill_factor;

class PageBulk
{
public:
	/** Constructor
	@param[in]	index	B-tree index
	@param[in]	page_no	page number
	@param[in]	level	page level
	@param[in]	trx_id	transaction id */
	PageBulk(dict_index_t* index, ulint trx_id,
		 ulint page_no, ulint level):
		 m_index(index), m_trx_id(trx_id),
		 m_page_no(page_no), m_level(level)
	{
		m_heap = mem_heap_create(1000);

		init();
	}

	/** Deconstructor */
	~PageBulk()
	{
		mem_heap_free(m_heap);
	}

	/** Insert a record in the page.
	We insert a record in the page and update releated members,
	it should succeed.
	@param[in]	rec		record
	@param[in]	offsets		record offsets */
	void insert(rec_t* rec, ulint* offsets);

	/** Finish a page
	Scan all records to set page dirs, and set page header members,
	redo log all inserts. */
	void finish();

	/** Commit mtr for a page
	@param[in]	success		Flag whether all inserts succeed.
	@return error code */
	void commit(bool success);

	/** Compress if it is compressed table
	@return	true	compress successfully or no need to compress
	@return	false	compress failed. */
	bool compress();

	/** Check whether the record needs to be stored externally.
	@return	true
	@return	false */
	bool needExt(dtuple_t* tuple, ulint rec_size);

	/** Store external record
	@param[in]	big_rec		external recrod
	@param[in]	offsets		record offsets
	@return	error code */
	dberr_t storeExt(const big_rec_t* big_rec, const ulint* offsets);

	/** Get node pointer
	Note: should before mtr commit */
	dtuple_t* getNodePtr();

	/** Get split rec in the page.
	We split a page in half when compresssion fails, and the split rec
	should be copied to the new page.
	@return split rec */
	rec_t*	getSplitRec();

	/** Copy all records after split rec including itself.
	@param[in]	rec	split rec
	Note: the page where split rec resizes is locked by another mtr.*/
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

	/** Start mtr and lock block */
	inline void lock();

	/** Check if required length is available in the page.
	We check fill factor & padding here.
	@param[in]	length		required length
	@retval true	if space is available
	@retval false	if no space is available */
	inline bool spaceAvailable(ulint	rec_size);

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
	/** Initialize members. */
	void init();

	/* The index B-tree */
	dict_index_t*	m_index;

	/* The min-transaction */
	mtr_t*		m_mtr;

	/* The transaction id */
	ulint		m_trx_id;

	/* Flag: is the mtr need redo logging */
	bool		m_log;

	/* The buffer block */
	buf_block_t*	m_block;

	/* The page */
	page_t*		m_page;

	/* The page zip descriptor */
	page_zip_des_t*	m_page_zip;

	/* The current rec, just before the next insert rec */
	rec_t*		m_cur_rec;

	/* The page no */
	ulint		m_page_no;

	/* The page level in B-tree */
	ulint		m_level;

	/* Flag: is page in compact format */
	bool		m_is_comp;

	/* The heap top in page for next insert */
	byte*		m_heap_top;

	/* Heap no */
	ulint		m_heap_no;

	/* User record no */
	ulint		m_rec_no;

	/* The free space left in the page */
	ulint		m_free_space;

	/* The reserved space for fill factor */
	ulint		m_fill_space;

	/* The pad space for compressed page */
	ulint		m_pad_space;

#ifdef UNIV_DEBUG
	/* Total data in the page */
	ulint		m_total_data;
#endif
};

typedef std::vector<PageBulk*>	page_bulk_vector;

class BtrBulk
{
public:
	/** Constructor */
	BtrBulk(dict_index_t* index, ulint trx_id):
		m_index(index), m_trx_id(trx_id), m_root_level(0)
	{
		m_heap = mem_heap_create(1000);

		m_page_bulks = new page_bulk_vector();
	}

	/** Destructor */
	~BtrBulk()
	{
		mem_heap_free(m_heap);
		delete m_page_bulks;
	}

	/** Insert a tuple
	@param[in]	tuple	tuple to insert.
	@return error code */
	dberr_t	insert(dtuple_t*	tuple) {
		return(insert(tuple, 0));
	}

	/** Finish bulk load
	@param[in]	err	error code of insert.
	@return	error code */
	dberr_t finish(dberr_t	err);

private:
	/** Insert a tuple to page in a level
	@param[in]	tuple	tuple to insert
	@param[in]	level	B-tree level
	@return error code */
	dberr_t insert(dtuple_t* tuple, ulint level);

	/** Split a page
	@param[in]	page_bulk	page to split
	@param[in]	next_page_bulk	next page
	@return	error code */
	dberr_t pageSplit(PageBulk* page_bulk, PageBulk* next_page_bulk);

	/** Commit(finish) a page
	@param[in]	page_bulk	page to commit
	@param[in]	next_page_bulk	next page
	@param[in]	insert_father	flag whether need to insert node ptr
	@return	error code */
	dberr_t pageCommit(PageBulk* page_bulk, PageBulk* next_page_bulk,
			   bool insert_father);

	/** Abort a page
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
	/* Memory heap for allocation */
	mem_heap_t*	m_heap;

	/* B-tree index */
	dict_index_t*   m_index;

	/* Transaction id */
	trx_id_t	m_trx_id;

	/* Root page level */
	ulint		m_root_level;

	/* Page cursor vector for all level */
	page_bulk_vector*	m_page_bulks;
};

#endif
