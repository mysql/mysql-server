/*****************************************************************************

Copyright (c) 1994, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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
@file btr/btr0btr.cc
The B-tree

Created 6/2/1994 Heikki Tuuri
*******************************************************/

#include "btr0btr.h"

#ifdef UNIV_NONINL
#include "btr0btr.ic"
#endif

#include "fsp0fsp.h"
#include "page0page.h"
#include "page0zip.h"

#ifndef UNIV_HOTBACKUP
#include "btr0cur.h"
#include "btr0sea.h"
#include "btr0pcur.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"
#include "trx0trx.h"
#include "srv0mon.h"

#endif /* UNIV_HOTBACKUP */
/**************************************************************//**
Report that an index page is corrupted. */
UNIV_INTERN
void
btr_corruption_report(
/*==================*/
	const buf_block_t*	block,	/*!< in: corrupted block */
	const dict_index_t*	index)	/*!< in: index tree */
{
	fprintf(stderr, "InnoDB: flag mismatch in space %u page %u"
		" index %s of table %s\n",
		(unsigned) buf_block_get_space(block),
		(unsigned) buf_block_get_page_no(block),
		index->name, index->table_name);
	if (block->page.zip.data) {
		buf_page_print(block->page.zip.data,
			       buf_block_get_zip_size(block),
			       BUF_PAGE_PRINT_NO_CRASH);
	}
	buf_page_print(buf_block_get_frame(block), 0, 0);
}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_BLOB_DEBUG
# include "srv0srv.h"
# include "ut0rbt.h"

/** TRUE when messages about index->blobs modification are enabled. */
static ibool btr_blob_dbg_msg;

/** Issue a message about an operation on index->blobs.
@param op	operation
@param b	the entry being subjected to the operation
@param ctx	the context of the operation */
#define btr_blob_dbg_msg_issue(op, b, ctx)			\
	fprintf(stderr, op " %u:%u:%u->%u %s(%u,%u,%u)\n",	\
		(b)->ref_page_no, (b)->ref_heap_no,		\
		(b)->ref_field_no, (b)->blob_page_no, ctx,	\
		(b)->owner, (b)->always_owner, (b)->del)

/** Insert to index->blobs a reference to an off-page column.
@param index	the index tree
@param b	the reference
@param ctx	context (for logging) */
UNIV_INTERN
void
btr_blob_dbg_rbt_insert(
/*====================*/
	dict_index_t*		index,	/*!< in/out: index tree */
	const btr_blob_dbg_t*	b,	/*!< in: the reference */
	const char*		ctx)	/*!< in: context (for logging) */
{
	if (btr_blob_dbg_msg) {
		btr_blob_dbg_msg_issue("insert", b, ctx);
	}
	mutex_enter(&index->blobs_mutex);
	rbt_insert(index->blobs, b, b);
	mutex_exit(&index->blobs_mutex);
}

/** Remove from index->blobs a reference to an off-page column.
@param index	the index tree
@param b	the reference
@param ctx	context (for logging) */
UNIV_INTERN
void
btr_blob_dbg_rbt_delete(
/*====================*/
	dict_index_t*		index,	/*!< in/out: index tree */
	const btr_blob_dbg_t*	b,	/*!< in: the reference */
	const char*		ctx)	/*!< in: context (for logging) */
{
	if (btr_blob_dbg_msg) {
		btr_blob_dbg_msg_issue("delete", b, ctx);
	}
	mutex_enter(&index->blobs_mutex);
	ut_a(rbt_delete(index->blobs, b));
	mutex_exit(&index->blobs_mutex);
}

/**************************************************************//**
Comparator for items (btr_blob_dbg_t) in index->blobs.
The key in index->blobs is (ref_page_no, ref_heap_no, ref_field_no).
@return negative, 0 or positive if *a<*b, *a=*b, *a>*b */
static
int
btr_blob_dbg_cmp(
/*=============*/
	const void*	a,	/*!< in: first btr_blob_dbg_t to compare */
	const void*	b)	/*!< in: second btr_blob_dbg_t to compare */
{
	const btr_blob_dbg_t*	aa = static_cast<const btr_blob_dbg_t*>(a);
	const btr_blob_dbg_t*	bb = static_cast<const btr_blob_dbg_t*>(b);

	ut_ad(aa != NULL);
	ut_ad(bb != NULL);

	if (aa->ref_page_no != bb->ref_page_no) {
		return(aa->ref_page_no < bb->ref_page_no ? -1 : 1);
	}
	if (aa->ref_heap_no != bb->ref_heap_no) {
		return(aa->ref_heap_no < bb->ref_heap_no ? -1 : 1);
	}
	if (aa->ref_field_no != bb->ref_field_no) {
		return(aa->ref_field_no < bb->ref_field_no ? -1 : 1);
	}
	return(0);
}

/**************************************************************//**
Add a reference to an off-page column to the index->blobs map. */
UNIV_INTERN
void
btr_blob_dbg_add_blob(
/*==================*/
	const rec_t*	rec,		/*!< in: clustered index record */
	ulint		field_no,	/*!< in: off-page column number */
	ulint		page_no,	/*!< in: start page of the column */
	dict_index_t*	index,		/*!< in/out: index tree */
	const char*	ctx)		/*!< in: context (for logging) */
{
	btr_blob_dbg_t	b;
	const page_t*	page	= page_align(rec);

	ut_a(index->blobs);

	b.blob_page_no = page_no;
	b.ref_page_no = page_get_page_no(page);
	b.ref_heap_no = page_rec_get_heap_no(rec);
	b.ref_field_no = field_no;
	ut_a(b.ref_field_no >= index->n_uniq);
	b.always_owner = b.owner = TRUE;
	b.del = FALSE;
	ut_a(!rec_get_deleted_flag(rec, page_is_comp(page)));
	btr_blob_dbg_rbt_insert(index, &b, ctx);
}

/**************************************************************//**
Add to index->blobs any references to off-page columns from a record.
@return number of references added */
UNIV_INTERN
ulint
btr_blob_dbg_add_rec(
/*=================*/
	const rec_t*	rec,	/*!< in: record */
	dict_index_t*	index,	/*!< in/out: index */
	const ulint*	offsets,/*!< in: offsets */
	const char*	ctx)	/*!< in: context (for logging) */
{
	ulint		count	= 0;
	ulint		i;
	btr_blob_dbg_t	b;
	ibool		del;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (!rec_offs_any_extern(offsets)) {
		return(0);
	}

	b.ref_page_no = page_get_page_no(page_align(rec));
	b.ref_heap_no = page_rec_get_heap_no(rec);
	del = (rec_get_deleted_flag(rec, rec_offs_comp(offsets)) != 0);

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		if (rec_offs_nth_extern(offsets, i)) {
			ulint		len;
			const byte*	field_ref = rec_get_nth_field(
				rec, offsets, i, &len);

			ut_a(len != UNIV_SQL_NULL);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
			field_ref += len - BTR_EXTERN_FIELD_REF_SIZE;

			if (!memcmp(field_ref, field_ref_zero,
				    BTR_EXTERN_FIELD_REF_SIZE)) {
				/* the column has not been stored yet */
				continue;
			}

			b.ref_field_no = i;
			b.blob_page_no = mach_read_from_4(
				field_ref + BTR_EXTERN_PAGE_NO);
			ut_a(b.ref_field_no >= index->n_uniq);
			b.always_owner = b.owner
				= !(field_ref[BTR_EXTERN_LEN]
				    & BTR_EXTERN_OWNER_FLAG);
			b.del = del;

			btr_blob_dbg_rbt_insert(index, &b, ctx);
			count++;
		}
	}

	return(count);
}

/**************************************************************//**
Display the references to off-page columns.
This function is to be called from a debugger,
for example when a breakpoint on ut_dbg_assertion_failed is hit. */
UNIV_INTERN
void
btr_blob_dbg_print(
/*===============*/
	const dict_index_t*	index)	/*!< in: index tree */
{
	const ib_rbt_node_t*	node;

	if (!index->blobs) {
		return;
	}

	/* We intentionally do not acquire index->blobs_mutex here.
	This function is to be called from a debugger, and the caller
	should make sure that the index->blobs_mutex is held. */

	for (node = rbt_first(index->blobs);
	     node != NULL; node = rbt_next(index->blobs, node)) {
		const btr_blob_dbg_t*	b
			= rbt_value(btr_blob_dbg_t, node);
		fprintf(stderr, "%u:%u:%u->%u%s%s%s\n",
			b->ref_page_no, b->ref_heap_no, b->ref_field_no,
			b->blob_page_no,
			b->owner ? "" : "(disowned)",
			b->always_owner ? "" : "(has disowned)",
			b->del ? "(deleted)" : "");
	}
}

/**************************************************************//**
Remove from index->blobs any references to off-page columns from a record.
@return number of references removed */
UNIV_INTERN
ulint
btr_blob_dbg_remove_rec(
/*====================*/
	const rec_t*	rec,	/*!< in: record */
	dict_index_t*	index,	/*!< in/out: index */
	const ulint*	offsets,/*!< in: offsets */
	const char*	ctx)	/*!< in: context (for logging) */
{
	ulint		i;
	ulint		count	= 0;
	btr_blob_dbg_t	b;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (!rec_offs_any_extern(offsets)) {
		return(0);
	}

	b.ref_page_no = page_get_page_no(page_align(rec));
	b.ref_heap_no = page_rec_get_heap_no(rec);

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		if (rec_offs_nth_extern(offsets, i)) {
			ulint		len;
			const byte*	field_ref = rec_get_nth_field(
				rec, offsets, i, &len);

			ut_a(len != UNIV_SQL_NULL);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
			field_ref += len - BTR_EXTERN_FIELD_REF_SIZE;

			b.ref_field_no = i;
			b.blob_page_no = mach_read_from_4(
				field_ref + BTR_EXTERN_PAGE_NO);

			switch (b.blob_page_no) {
			case 0:
				/* The column has not been stored yet.
				The BLOB pointer must be all zero.
				There cannot be a BLOB starting at
				page 0, because page 0 is reserved for
				the tablespace header. */
				ut_a(!memcmp(field_ref, field_ref_zero,
					     BTR_EXTERN_FIELD_REF_SIZE));
				/* fall through */
			case FIL_NULL:
				/* the column has been freed already */
				continue;
			}

			btr_blob_dbg_rbt_delete(index, &b, ctx);
			count++;
		}
	}

	return(count);
}

/**************************************************************//**
Check that there are no references to off-page columns from or to
the given page. Invoked when freeing or clearing a page.
@return TRUE when no orphan references exist */
UNIV_INTERN
ibool
btr_blob_dbg_is_empty(
/*==================*/
	dict_index_t*	index,		/*!< in: index */
	ulint		page_no)	/*!< in: page number */
{
	const ib_rbt_node_t*	node;
	ibool			success	= TRUE;

	if (!index->blobs) {
		return(success);
	}

	mutex_enter(&index->blobs_mutex);

	for (node = rbt_first(index->blobs);
	     node != NULL; node = rbt_next(index->blobs, node)) {
		const btr_blob_dbg_t*	b
			= rbt_value(btr_blob_dbg_t, node);

		if (b->ref_page_no != page_no && b->blob_page_no != page_no) {
			continue;
		}

		fprintf(stderr,
			"InnoDB: orphan BLOB ref%s%s%s %u:%u:%u->%u\n",
			b->owner ? "" : "(disowned)",
			b->always_owner ? "" : "(has disowned)",
			b->del ? "(deleted)" : "",
			b->ref_page_no, b->ref_heap_no, b->ref_field_no,
			b->blob_page_no);

		if (b->blob_page_no != page_no || b->owner || !b->del) {
			success = FALSE;
		}
	}

	mutex_exit(&index->blobs_mutex);
	return(success);
}

/**************************************************************//**
Count and process all references to off-page columns on a page.
@return number of references processed */
UNIV_INTERN
ulint
btr_blob_dbg_op(
/*============*/
	const page_t*		page,	/*!< in: B-tree leaf page */
	const rec_t*		rec,	/*!< in: record to start from
					(NULL to process the whole page) */
	dict_index_t*		index,	/*!< in/out: index */
	const char*		ctx,	/*!< in: context (for logging) */
	const btr_blob_dbg_op_f	op)	/*!< in: operation on records */
{
	ulint		count	= 0;
	mem_heap_t*	heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets	= offsets_;
	rec_offs_init(offsets_);

	ut_a(fil_page_get_type(page) == FIL_PAGE_INDEX);
	ut_a(!rec || page_align(rec) == page);

	if (!index->blobs || !page_is_leaf(page)
	    || !dict_index_is_clust(index)) {
		return(0);
	}

	if (rec == NULL) {
		rec = page_get_infimum_rec(page);
	}

	do {
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		count += op(rec, index, offsets, ctx);
		rec = page_rec_get_next_const(rec);
	} while (!page_rec_is_supremum(rec));

	if (heap) {
		mem_heap_free(heap);
	}

	return(count);
}

/**************************************************************//**
Count and add to index->blobs any references to off-page columns
from records on a page.
@return number of references added */
UNIV_INTERN
ulint
btr_blob_dbg_add(
/*=============*/
	const page_t*	page,	/*!< in: rewritten page */
	dict_index_t*	index,	/*!< in/out: index */
	const char*	ctx)	/*!< in: context (for logging) */
{
	btr_blob_dbg_assert_empty(index, page_get_page_no(page));

	return(btr_blob_dbg_op(page, NULL, index, ctx, btr_blob_dbg_add_rec));
}

/**************************************************************//**
Count and remove from index->blobs any references to off-page columns
from records on a page.
Used when reorganizing a page, before copying the records.
@return number of references removed */
UNIV_INTERN
ulint
btr_blob_dbg_remove(
/*================*/
	const page_t*	page,	/*!< in: b-tree page */
	dict_index_t*	index,	/*!< in/out: index */
	const char*	ctx)	/*!< in: context (for logging) */
{
	ulint	count;

	count = btr_blob_dbg_op(page, NULL, index, ctx,
				btr_blob_dbg_remove_rec);

	/* Check that no references exist. */
	btr_blob_dbg_assert_empty(index, page_get_page_no(page));

	return(count);
}

/**************************************************************//**
Restore in index->blobs any references to off-page columns
Used when page reorganize fails due to compressed page overflow. */
UNIV_INTERN
void
btr_blob_dbg_restore(
/*=================*/
	const page_t*	npage,	/*!< in: page that failed to compress  */
	const page_t*	page,	/*!< in: copy of original page */
	dict_index_t*	index,	/*!< in/out: index */
	const char*	ctx)	/*!< in: context (for logging) */
{
	ulint	removed;
	ulint	added;

	ut_a(page_get_page_no(npage) == page_get_page_no(page));
	ut_a(page_get_space_id(npage) == page_get_space_id(page));

	removed = btr_blob_dbg_remove(npage, index, ctx);
	added = btr_blob_dbg_add(page, index, ctx);
	ut_a(added == removed);
}

/**************************************************************//**
Modify the 'deleted' flag of a record. */
UNIV_INTERN
void
btr_blob_dbg_set_deleted_flag(
/*==========================*/
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in/out: index */
	const ulint*		offsets,/*!< in: rec_get_offs(rec, index) */
	ibool			del)	/*!< in: TRUE=deleted, FALSE=exists */
{
	const ib_rbt_node_t*	node;
	btr_blob_dbg_t		b;
	btr_blob_dbg_t*		c;
	ulint			i;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_a(dict_index_is_clust(index));
	ut_a(del == !!del);/* must be FALSE==0 or TRUE==1 */

	if (!rec_offs_any_extern(offsets) || !index->blobs) {

		return;
	}

	b.ref_page_no = page_get_page_no(page_align(rec));
	b.ref_heap_no = page_rec_get_heap_no(rec);

	for (i = 0; i < rec_offs_n_fields(offsets); i++) {
		if (rec_offs_nth_extern(offsets, i)) {
			ulint		len;
			const byte*	field_ref = rec_get_nth_field(
				rec, offsets, i, &len);

			ut_a(len != UNIV_SQL_NULL);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
			field_ref += len - BTR_EXTERN_FIELD_REF_SIZE;

			b.ref_field_no = i;
			b.blob_page_no = mach_read_from_4(
				field_ref + BTR_EXTERN_PAGE_NO);

			switch (b.blob_page_no) {
			case 0:
				ut_a(memcmp(field_ref, field_ref_zero,
					    BTR_EXTERN_FIELD_REF_SIZE));
				/* page number 0 is for the
				page allocation bitmap */
			case FIL_NULL:
				/* the column has been freed already */
				ut_error;
			}

			mutex_enter(&index->blobs_mutex);
			node = rbt_lookup(index->blobs, &b);
			ut_a(node);

			c = rbt_value(btr_blob_dbg_t, node);
			/* The flag should be modified. */
			c->del = del;
			if (btr_blob_dbg_msg) {
				b = *c;
				mutex_exit(&index->blobs_mutex);
				btr_blob_dbg_msg_issue("del_mk", &b, "");
			} else {
				mutex_exit(&index->blobs_mutex);
			}
		}
	}
}

/**************************************************************//**
Change the ownership of an off-page column. */
UNIV_INTERN
void
btr_blob_dbg_owner(
/*===============*/
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in/out: index */
	const ulint*		offsets,/*!< in: rec_get_offs(rec, index) */
	ulint			i,	/*!< in: ith field in rec */
	ibool			own)	/*!< in: TRUE=owned, FALSE=disowned */
{
	const ib_rbt_node_t*	node;
	btr_blob_dbg_t		b;
	const byte*		field_ref;
	ulint			len;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_a(rec_offs_nth_extern(offsets, i));

	field_ref = rec_get_nth_field(rec, offsets, i, &len);
	ut_a(len != UNIV_SQL_NULL);
	ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
	field_ref += len - BTR_EXTERN_FIELD_REF_SIZE;

	b.ref_page_no = page_get_page_no(page_align(rec));
	b.ref_heap_no = page_rec_get_heap_no(rec);
	b.ref_field_no = i;
	b.owner = !(field_ref[BTR_EXTERN_LEN] & BTR_EXTERN_OWNER_FLAG);
	b.blob_page_no = mach_read_from_4(field_ref + BTR_EXTERN_PAGE_NO);

	ut_a(b.owner == own);

	mutex_enter(&index->blobs_mutex);
	node = rbt_lookup(index->blobs, &b);
	/* row_ins_clust_index_entry_by_modify() invokes
	btr_cur_unmark_extern_fields() also for the newly inserted
	references, which are all zero bytes until the columns are stored.
	The node lookup must fail if and only if that is the case. */
	ut_a(!memcmp(field_ref, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE)
	     == !node);

	if (node) {
		btr_blob_dbg_t*	c = rbt_value(btr_blob_dbg_t, node);
		/* Some code sets ownership from TRUE to TRUE.
		We do not allow changing ownership from FALSE to FALSE. */
		ut_a(own || c->owner);

		c->owner = own;
		if (!own) {
			c->always_owner = FALSE;
		}
	}

	mutex_exit(&index->blobs_mutex);
}
#endif /* UNIV_BLOB_DEBUG */

/*
Latching strategy of the InnoDB B-tree
--------------------------------------
A tree latch protects all non-leaf nodes of the tree. Each node of a tree
also has a latch of its own.

A B-tree operation normally first acquires an S-latch on the tree. It
searches down the tree and releases the tree latch when it has the
leaf node latch. To save CPU time we do not acquire any latch on
non-leaf nodes of the tree during a search, those pages are only bufferfixed.

If an operation needs to restructure the tree, it acquires an X-latch on
the tree before searching to a leaf node. If it needs, for example, to
split a leaf,
(1) InnoDB decides the split point in the leaf,
(2) allocates a new page,
(3) inserts the appropriate node pointer to the first non-leaf level,
(4) releases the tree X-latch,
(5) and then moves records from the leaf to the new allocated page.

Node pointers
-------------
Leaf pages of a B-tree contain the index records stored in the
tree. On levels n > 0 we store 'node pointers' to pages on level
n - 1. For each page there is exactly one node pointer stored:
thus the our tree is an ordinary B-tree, not a B-link tree.

A node pointer contains a prefix P of an index record. The prefix
is long enough so that it determines an index record uniquely.
The file page number of the child page is added as the last
field. To the child page we can store node pointers or index records
which are >= P in the alphabetical order, but < P1 if there is
a next node pointer on the level, and P1 is its prefix.

If a node pointer with a prefix P points to a non-leaf child,
then the leftmost record in the child must have the same
prefix P. If it points to a leaf node, the child is not required
to contain any record with a prefix equal to P. The leaf case
is decided this way to allow arbitrary deletions in a leaf node
without touching upper levels of the tree.

We have predefined a special minimum record which we
define as the smallest record in any alphabetical order.
A minimum record is denoted by setting a bit in the record
header. A minimum record acts as the prefix of a node pointer
which points to a leftmost node on any level of the tree.

File page allocation
--------------------
In the root node of a B-tree there are two file segment headers.
The leaf pages of a tree are allocated from one file segment, to
make them consecutive on disk if possible. From the other file segment
we allocate pages for the non-leaf levels of the tree.
*/

#ifdef UNIV_BTR_DEBUG
/**************************************************************//**
Checks a file segment header within a B-tree root page.
@return	TRUE if valid */
static
ibool
btr_root_fseg_validate(
/*===================*/
	const fseg_header_t*	seg_header,	/*!< in: segment header */
	ulint			space)		/*!< in: tablespace identifier */
{
	ulint	offset = mach_read_from_2(seg_header + FSEG_HDR_OFFSET);

	ut_a(mach_read_from_4(seg_header + FSEG_HDR_SPACE) == space);
	ut_a(offset >= FIL_PAGE_DATA);
	ut_a(offset <= UNIV_PAGE_SIZE - FIL_PAGE_DATA_END);
	return(TRUE);
}
#endif /* UNIV_BTR_DEBUG */

/**************************************************************//**
Gets the root node of a tree and x- or s-latches it.
@return	root page, x- or s-latched */
static
buf_block_t*
btr_root_block_get(
/*===============*/
	const dict_index_t*	index,	/*!< in: index tree */
	ulint			mode,	/*!< in: either RW_S_LATCH
					or RW_X_LATCH */
	mtr_t*			mtr)	/*!< in: mtr */
{
	ulint		space;
	ulint		zip_size;
	ulint		root_page_no;
	buf_block_t*	block;

	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);
	root_page_no = dict_index_get_page(index);

	block = btr_block_get(space, zip_size, root_page_no, mode, index, mtr);
	btr_assert_not_corrupted(block, index);
#ifdef UNIV_BTR_DEBUG
	if (!dict_index_is_ibuf(index)) {
		const page_t*	root = buf_block_get_frame(block);

		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
					    + root, space));
		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
					    + root, space));
	}
#endif /* UNIV_BTR_DEBUG */

	return(block);
}

/**************************************************************//**
Gets the root node of a tree and x-latches it.
@return	root page, x-latched */
UNIV_INTERN
page_t*
btr_root_get(
/*=========*/
	const dict_index_t*	index,	/*!< in: index tree */
	mtr_t*			mtr)	/*!< in: mtr */
{
	return(buf_block_get_frame(btr_root_block_get(index, RW_X_LATCH,
						      mtr)));
}

/**************************************************************//**
Gets the height of the B-tree (the level of the root, when the leaf
level is assumed to be 0). The caller must hold an S or X latch on
the index.
@return	tree height (level of the root) */
UNIV_INTERN
ulint
btr_height_get(
/*===========*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		height;
	buf_block_t*	root_block;

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_S_LOCK)
	      || mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));

        /* S latches the page */
        root_block = btr_root_block_get(index, RW_S_LATCH, mtr);

        height = btr_page_get_level(buf_block_get_frame(root_block), mtr);

        /* Release the S latch on the root page. */
        mtr_memo_release(mtr, root_block, MTR_MEMO_PAGE_S_FIX);
#ifdef UNIV_SYNC_DEBUG
        sync_thread_reset_level(&root_block->lock);
#endif /* UNIV_SYNC_DEBUG */

	return(height);
}

/**************************************************************//**
Checks a file segment header within a B-tree root page and updates
the segment header space id.
@return	TRUE if valid */
static
bool
btr_root_fseg_adjust_on_import(
/*===========================*/
	fseg_header_t*	seg_header,	/*!< in/out: segment header */
	page_zip_des_t*	page_zip,	/*!< in/out: compressed page,
					or NULL */
	ulint		space,		/*!< in: tablespace identifier */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint	offset = mach_read_from_2(seg_header + FSEG_HDR_OFFSET);

	if (offset < FIL_PAGE_DATA
	    || offset > UNIV_PAGE_SIZE - FIL_PAGE_DATA_END) {

		return(FALSE);

	} else if (page_zip) {
		mach_write_to_4(seg_header + FSEG_HDR_SPACE, space);
		page_zip_write_header(page_zip, seg_header + FSEG_HDR_SPACE,
				      4, mtr);
	} else {
		mlog_write_ulint(seg_header + FSEG_HDR_SPACE,
				 space, MLOG_4BYTES, mtr);
	}

	return(TRUE);
}

/**************************************************************//**
Checks and adjusts the root node of a tree during IMPORT TABLESPACE.
@return error code, or DB_SUCCESS */
UNIV_INTERN
dberr_t
btr_root_adjust_on_import(
/*======================*/
	const dict_index_t*	index)	/*!< in: index tree */
{
	dberr_t		err;
	mtr_t		mtr;
	page_t*		page;
	buf_block_t*	block;
	page_zip_des_t*	page_zip;
	dict_table_t*	table		= index->table;
	ulint		space_id	= dict_index_get_space(index);
	ulint		zip_size	= dict_table_zip_size(table);
	ulint		root_page_no	= dict_index_get_page(index);

	mtr_start(&mtr);

	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	DBUG_EXECUTE_IF("ib_import_trigger_corruption_3",
			return(DB_CORRUPTION););

	block = btr_block_get(
		space_id, zip_size, root_page_no, RW_X_LATCH, index, &mtr);

	page = buf_block_get_frame(block);
	page_zip = buf_block_get_page_zip(block);

	/* Check that this is a B-tree page and both the PREV and NEXT
	pointers are FIL_NULL, because the root page does not have any
	siblings. */
	if (fil_page_get_type(page) != FIL_PAGE_INDEX
	    || fil_page_get_prev(page) != FIL_NULL
	    || fil_page_get_next(page) != FIL_NULL) {

		err = DB_CORRUPTION;

	} else if (dict_index_is_clust(index)) {
		bool	page_is_compact_format;

		page_is_compact_format = page_is_comp(page) > 0;

		/* Check if the page format and table format agree. */
		if (page_is_compact_format != dict_table_is_comp(table)) {
			err = DB_CORRUPTION;
		} else {

			/* Check that the table flags and the tablespace
			flags match. */
			ulint	flags = fil_space_get_flags(table->space);

			if (flags
			    && flags != dict_tf_to_fsp_flags(table->flags)) {

				err = DB_CORRUPTION;
			} else {
				err = DB_SUCCESS;
			}
		}
	} else {
		err = DB_SUCCESS;
	}

	/* Check and adjust the file segment headers, if all OK so far. */
	if (err == DB_SUCCESS
	    && (!btr_root_fseg_adjust_on_import(
			FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
			+ page, page_zip, space_id, &mtr)
		|| !btr_root_fseg_adjust_on_import(
			FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
			+ page, page_zip, space_id, &mtr))) {

		err = DB_CORRUPTION;
	}

	mtr_commit(&mtr);

	return(err);
}

/*************************************************************//**
Gets pointer to the previous user record in the tree. It is assumed that
the caller has appropriate latches on the page and its neighbor.
@return	previous user record, NULL if there is none */
UNIV_INTERN
rec_t*
btr_get_prev_user_rec(
/*==================*/
	rec_t*	rec,	/*!< in: record on leaf level */
	mtr_t*	mtr)	/*!< in: mtr holding a latch on the page, and if
			needed, also to the previous page */
{
	page_t*	page;
	page_t*	prev_page;
	ulint	prev_page_no;

	if (!page_rec_is_infimum(rec)) {

		rec_t*	prev_rec = page_rec_get_prev(rec);

		if (!page_rec_is_infimum(prev_rec)) {

			return(prev_rec);
		}
	}

	page = page_align(rec);
	prev_page_no = btr_page_get_prev(page, mtr);

	if (prev_page_no != FIL_NULL) {

		ulint		space;
		ulint		zip_size;
		buf_block_t*	prev_block;

		space = page_get_space_id(page);
		zip_size = fil_space_get_zip_size(space);

		prev_block = buf_page_get_with_no_latch(space, zip_size,
							prev_page_no, mtr);
		prev_page = buf_block_get_frame(prev_block);
		/* The caller must already have a latch to the brother */
		ut_ad(mtr_memo_contains(mtr, prev_block,
					MTR_MEMO_PAGE_S_FIX)
		      || mtr_memo_contains(mtr, prev_block,
					   MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(prev_page) == page_is_comp(page));
		ut_a(btr_page_get_next(prev_page, mtr)
		     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

		return(page_rec_get_prev(page_get_supremum_rec(prev_page)));
	}

	return(NULL);
}

/*************************************************************//**
Gets pointer to the next user record in the tree. It is assumed that the
caller has appropriate latches on the page and its neighbor.
@return	next user record, NULL if there is none */
UNIV_INTERN
rec_t*
btr_get_next_user_rec(
/*==================*/
	rec_t*	rec,	/*!< in: record on leaf level */
	mtr_t*	mtr)	/*!< in: mtr holding a latch on the page, and if
			needed, also to the next page */
{
	page_t*	page;
	page_t*	next_page;
	ulint	next_page_no;

	if (!page_rec_is_supremum(rec)) {

		rec_t*	next_rec = page_rec_get_next(rec);

		if (!page_rec_is_supremum(next_rec)) {

			return(next_rec);
		}
	}

	page = page_align(rec);
	next_page_no = btr_page_get_next(page, mtr);

	if (next_page_no != FIL_NULL) {
		ulint		space;
		ulint		zip_size;
		buf_block_t*	next_block;

		space = page_get_space_id(page);
		zip_size = fil_space_get_zip_size(space);

		next_block = buf_page_get_with_no_latch(space, zip_size,
							next_page_no, mtr);
		next_page = buf_block_get_frame(next_block);
		/* The caller must already have a latch to the brother */
		ut_ad(mtr_memo_contains(mtr, next_block, MTR_MEMO_PAGE_S_FIX)
		      || mtr_memo_contains(mtr, next_block,
					   MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(next_page) == page_is_comp(page));
		ut_a(btr_page_get_prev(next_page, mtr)
		     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

		return(page_rec_get_next(page_get_infimum_rec(next_page)));
	}

	return(NULL);
}

/**************************************************************//**
Creates a new index page (not the root, and also not
used in page reorganization).  @see btr_page_empty(). */
static
void
btr_page_create(
/*============*/
	buf_block_t*	block,	/*!< in/out: page to be created */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: the B-tree level of the page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_t*		page = buf_block_get_frame(block);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	btr_blob_dbg_assert_empty(index, buf_block_get_page_no(block));

	if (page_zip) {
		page_create_zip(block, index, level, mtr);
	} else {
		page_create(block, mtr, dict_table_is_comp(index->table));
		/* Set the level of the new index page */
		btr_page_set_level(page, NULL, level, mtr);
	}

	block->check_index_page_at_flush = TRUE;

	btr_page_set_index_id(page, page_zip, index->id, mtr);
}

/**************************************************************//**
Allocates a new file page to be used in an ibuf tree. Takes the page from
the free list of the tree, which must contain pages!
@return	new allocated block, x-latched */
static
buf_block_t*
btr_page_alloc_for_ibuf(
/*====================*/
	dict_index_t*	index,	/*!< in: index tree */
	mtr_t*		mtr)	/*!< in: mtr */
{
	fil_addr_t	node_addr;
	page_t*		root;
	page_t*		new_page;
	buf_block_t*	new_block;

	root = btr_root_get(index, mtr);

	node_addr = flst_get_first(root + PAGE_HEADER
				   + PAGE_BTR_IBUF_FREE_LIST, mtr);
	ut_a(node_addr.page != FIL_NULL);

	new_block = buf_page_get(dict_index_get_space(index),
				 dict_table_zip_size(index->table),
				 node_addr.page, RW_X_LATCH, mtr);
	new_page = buf_block_get_frame(new_block);
	buf_block_dbg_add_level(new_block, SYNC_IBUF_TREE_NODE_NEW);

	flst_remove(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		    new_page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE,
		    mtr);
	ut_ad(flst_validate(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
			    mtr));

	return(new_block);
}

/**************************************************************//**
Allocates a new file page to be used in an index tree. NOTE: we assume
that the caller has made the reservation for free extents!
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
static __attribute__((nonnull, warn_unused_result))
buf_block_t*
btr_page_alloc_low(
/*===============*/
	dict_index_t*	index,		/*!< in: index */
	ulint		hint_page_no,	/*!< in: hint of a good page */
	byte		file_direction,	/*!< in: direction where a possible
					page split is made */
	ulint		level,		/*!< in: level where the page is placed
					in the tree */
	mtr_t*		mtr,		/*!< in/out: mini-transaction
					for the allocation */
	mtr_t*		init_mtr)	/*!< in/out: mtr or another
					mini-transaction in which the
					page should be initialized.
					If init_mtr!=mtr, but the page
					is already X-latched in mtr, do
					not initialize the page. */
{
	fseg_header_t*	seg_header;
	page_t*		root;

	root = btr_root_get(index, mtr);

	if (level == 0) {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_LEAF;
	} else {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_TOP;
	}

	/* Parameter TRUE below states that the caller has made the
	reservation for free extents, and thus we know that a page can
	be allocated: */

	return(fseg_alloc_free_page_general(
		       seg_header, hint_page_no, file_direction,
		       TRUE, mtr, init_mtr));
}

/**************************************************************//**
Allocates a new file page to be used in an index tree. NOTE: we assume
that the caller has made the reservation for free extents!
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
UNIV_INTERN
buf_block_t*
btr_page_alloc(
/*===========*/
	dict_index_t*	index,		/*!< in: index */
	ulint		hint_page_no,	/*!< in: hint of a good page */
	byte		file_direction,	/*!< in: direction where a possible
					page split is made */
	ulint		level,		/*!< in: level where the page is placed
					in the tree */
	mtr_t*		mtr,		/*!< in/out: mini-transaction
					for the allocation */
	mtr_t*		init_mtr)	/*!< in/out: mini-transaction
					for x-latching and initializing
					the page */
{
	buf_block_t*	new_block;

	if (dict_index_is_ibuf(index)) {

		return(btr_page_alloc_for_ibuf(index, mtr));
	}

	new_block = btr_page_alloc_low(
		index, hint_page_no, file_direction, level, mtr, init_mtr);

	if (new_block) {
		buf_block_dbg_add_level(new_block, SYNC_TREE_NODE_NEW);
	}

	return(new_block);
}

/**************************************************************//**
Gets the number of pages in a B-tree.
@return	number of pages, or ULINT_UNDEFINED if the index is unavailable */
UNIV_INTERN
ulint
btr_get_size(
/*=========*/
	dict_index_t*	index,	/*!< in: index */
	ulint		flag,	/*!< in: BTR_N_LEAF_PAGES or BTR_TOTAL_SIZE */
	mtr_t*		mtr)	/*!< in/out: mini-transaction where index
				is s-latched */
{
	fseg_header_t*	seg_header;
	page_t*		root;
	ulint		n;
	ulint		dummy;

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_S_LOCK));

	if (index->page == FIL_NULL || dict_index_is_online_ddl(index)
	    || *index->name == TEMP_INDEX_PREFIX) {
		return(ULINT_UNDEFINED);
	}

	root = btr_root_get(index, mtr);

	if (flag == BTR_N_LEAF_PAGES) {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_LEAF;

		fseg_n_reserved_pages(seg_header, &n, mtr);

	} else if (flag == BTR_TOTAL_SIZE) {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_TOP;

		n = fseg_n_reserved_pages(seg_header, &dummy, mtr);

		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_LEAF;

		n += fseg_n_reserved_pages(seg_header, &dummy, mtr);
	} else {
		ut_error;
	}

	return(n);
}

/**************************************************************//**
Frees a page used in an ibuf tree. Puts the page to the free list of the
ibuf tree. */
static
void
btr_page_free_for_ibuf(
/*===================*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: block to be freed, x-latched */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_t*		root;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	root = btr_root_get(index, mtr);

	flst_add_first(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		       buf_block_get_frame(block)
		       + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, mtr);

	ut_ad(flst_validate(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
			    mtr));
}

/**************************************************************//**
Frees a file page used in an index tree. Can be used also to (BLOB)
external storage pages, because the page level 0 can be given as an
argument. */
UNIV_INTERN
void
btr_page_free_low(
/*==============*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: block to be freed, x-latched */
	ulint		level,	/*!< in: page level */
	mtr_t*		mtr)	/*!< in: mtr */
{
	fseg_header_t*	seg_header;
	page_t*		root;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	/* The page gets invalid for optimistic searches: increment the frame
	modify clock */

	buf_block_modify_clock_inc(block);
	btr_blob_dbg_assert_empty(index, buf_block_get_page_no(block));

	if (dict_index_is_ibuf(index)) {

		btr_page_free_for_ibuf(index, block, mtr);

		return;
	}

	root = btr_root_get(index, mtr);

	if (level == 0) {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_LEAF;
	} else {
		seg_header = root + PAGE_HEADER + PAGE_BTR_SEG_TOP;
	}

	fseg_free_page(seg_header,
		       buf_block_get_space(block),
		       buf_block_get_page_no(block), mtr);

	/* The page was marked free in the allocation bitmap, but it
	should remain buffer-fixed until mtr_commit(mtr) or until it
	is explicitly freed from the mini-transaction. */
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	/* TODO: Discard any operations on the page from the redo log
	and remove the block from the flush list and the buffer pool.
	This would free up buffer pool earlier and reduce writes to
	both the tablespace and the redo log. */
}

/**************************************************************//**
Frees a file page used in an index tree. NOTE: cannot free field external
storage pages because the page must contain info on its level. */
UNIV_INTERN
void
btr_page_free(
/*==========*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: block to be freed, x-latched */
	mtr_t*		mtr)	/*!< in: mtr */
{
	const page_t*	page	= buf_block_get_frame(block);
	ulint		level	= btr_page_get_level(page, mtr);

	ut_ad(fil_page_get_type(block->frame) == FIL_PAGE_INDEX);
	btr_page_free_low(index, block, level, mtr);
}

/**************************************************************//**
Sets the child node file address in a node pointer. */
UNIV_INLINE
void
btr_node_ptr_set_child_page_no(
/*===========================*/
	rec_t*		rec,	/*!< in: node pointer record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		page_no,/*!< in: child node address */
	mtr_t*		mtr)	/*!< in: mtr */
{
	byte*	field;
	ulint	len;

	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_ad(!page_is_leaf(page_align(rec)));
	ut_ad(!rec_offs_comp(offsets) || rec_get_node_ptr_flag(rec));

	/* The child address is in the last field */
	field = rec_get_nth_field(rec, offsets,
				  rec_offs_n_fields(offsets) - 1, &len);

	ut_ad(len == REC_NODE_PTR_SIZE);

	if (page_zip) {
		page_zip_write_node_ptr(page_zip, rec,
					rec_offs_data_size(offsets),
					page_no, mtr);
	} else {
		mlog_write_ulint(field, page_no, MLOG_4BYTES, mtr);
	}
}

/************************************************************//**
Returns the child page of a node pointer and x-latches it.
@return	child page, x-latched */
static
buf_block_t*
btr_node_ptr_get_child(
/*===================*/
	const rec_t*	node_ptr,/*!< in: node pointer */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint	page_no;
	ulint	space;

	ut_ad(rec_offs_validate(node_ptr, index, offsets));
	space = page_get_space_id(page_align(node_ptr));
	page_no = btr_node_ptr_get_child_page_no(node_ptr, offsets);

	return(btr_block_get(space, dict_table_zip_size(index->table),
			     page_no, RW_X_LATCH, index, mtr));
}

/************************************************************//**
Returns the upper level node pointer to a page. It is assumed that mtr holds
an x-latch on the tree.
@return	rec_get_offsets() of the node pointer record */
static
ulint*
btr_page_get_father_node_ptr_func(
/*==============================*/
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	btr_cur_t*	cursor,	/*!< in: cursor pointing to user record,
				out: cursor on node pointer record,
				its page x-latched */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dtuple_t*	tuple;
	rec_t*		user_rec;
	rec_t*		node_ptr;
	ulint		level;
	ulint		page_no;
	dict_index_t*	index;

	page_no = buf_block_get_page_no(btr_cur_get_block(cursor));
	index = btr_cur_get_index(cursor);

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));

	ut_ad(dict_index_get_page(index) != page_no);

	level = btr_page_get_level(btr_cur_get_page(cursor), mtr);

	user_rec = btr_cur_get_rec(cursor);
	ut_a(page_rec_is_user_rec(user_rec));
	tuple = dict_index_build_node_ptr(index, user_rec, 0, heap, level);

	btr_cur_search_to_nth_level(index, level + 1, tuple, PAGE_CUR_LE,
				    BTR_CONT_MODIFY_TREE, cursor, 0,
				    file, line, mtr);

	node_ptr = btr_cur_get_rec(cursor);
	ut_ad(!page_rec_is_comp(node_ptr)
	      || rec_get_status(node_ptr) == REC_STATUS_NODE_PTR);
	offsets = rec_get_offsets(node_ptr, index, offsets,
				  ULINT_UNDEFINED, &heap);

	if (btr_node_ptr_get_child_page_no(node_ptr, offsets) != page_no) {
		rec_t*	print_rec;
		fputs("InnoDB: Dump of the child page:\n", stderr);
		buf_page_print(page_align(user_rec), 0,
			       BUF_PAGE_PRINT_NO_CRASH);
		fputs("InnoDB: Dump of the parent page:\n", stderr);
		buf_page_print(page_align(node_ptr), 0,
			       BUF_PAGE_PRINT_NO_CRASH);

		fputs("InnoDB: Corruption of an index tree: table ", stderr);
		ut_print_name(stderr, NULL, TRUE, index->table_name);
		fputs(", index ", stderr);
		ut_print_name(stderr, NULL, FALSE, index->name);
		fprintf(stderr, ",\n"
			"InnoDB: father ptr page no %lu, child page no %lu\n",
			(ulong)
			btr_node_ptr_get_child_page_no(node_ptr, offsets),
			(ulong) page_no);
		print_rec = page_rec_get_next(
			page_get_infimum_rec(page_align(user_rec)));
		offsets = rec_get_offsets(print_rec, index,
					  offsets, ULINT_UNDEFINED, &heap);
		page_rec_print(print_rec, offsets);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					  ULINT_UNDEFINED, &heap);
		page_rec_print(node_ptr, offsets);

		fputs("InnoDB: You should dump + drop + reimport the table"
		      " to fix the\n"
		      "InnoDB: corruption. If the crash happens at "
		      "the database startup, see\n"
		      "InnoDB: " REFMAN "forcing-innodb-recovery.html about\n"
		      "InnoDB: forcing recovery. "
		      "Then dump + drop + reimport.\n", stderr);

		ut_error;
	}

	return(offsets);
}

#define btr_page_get_father_node_ptr(of,heap,cur,mtr)			\
	btr_page_get_father_node_ptr_func(of,heap,cur,__FILE__,__LINE__,mtr)

/************************************************************//**
Returns the upper level node pointer to a page. It is assumed that mtr holds
an x-latch on the tree.
@return	rec_get_offsets() of the node pointer record */
static
ulint*
btr_page_get_father_block(
/*======================*/
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	dict_index_t*	index,	/*!< in: b-tree index */
	buf_block_t*	block,	/*!< in: child page in the index */
	mtr_t*		mtr,	/*!< in: mtr */
	btr_cur_t*	cursor)	/*!< out: cursor on node pointer record,
				its page x-latched */
{
	rec_t*	rec
		= page_rec_get_next(page_get_infimum_rec(buf_block_get_frame(
								 block)));
	btr_cur_position(index, rec, block, cursor);
	return(btr_page_get_father_node_ptr(offsets, heap, cursor, mtr));
}

/************************************************************//**
Seeks to the upper level node pointer to a page.
It is assumed that mtr holds an x-latch on the tree. */
static
void
btr_page_get_father(
/*================*/
	dict_index_t*	index,	/*!< in: b-tree index */
	buf_block_t*	block,	/*!< in: child page in the index */
	mtr_t*		mtr,	/*!< in: mtr */
	btr_cur_t*	cursor)	/*!< out: cursor on node pointer record,
				its page x-latched */
{
	mem_heap_t*	heap;
	rec_t*		rec
		= page_rec_get_next(page_get_infimum_rec(buf_block_get_frame(
								 block)));
	btr_cur_position(index, rec, block, cursor);

	heap = mem_heap_create(100);
	btr_page_get_father_node_ptr(NULL, heap, cursor, mtr);
	mem_heap_free(heap);
}

/************************************************************//**
Creates the root node for a new index tree.
@return	page number of the created root, FIL_NULL if did not succeed */
UNIV_INTERN
ulint
btr_create(
/*=======*/
	ulint		type,	/*!< in: type of the index */
	ulint		space,	/*!< in: space where created */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	index_id_t	index_id,/*!< in: index id */
	dict_index_t*	index,	/*!< in: index */
	mtr_t*		mtr)	/*!< in: mini-transaction handle */
{
	ulint		page_no;
	buf_block_t*	block;
	buf_frame_t*	frame;
	page_t*		page;
	page_zip_des_t*	page_zip;

	/* Create the two new segments (one, in the case of an ibuf tree) for
	the index tree; the segment headers are put on the allocated root page
	(for an ibuf tree, not in the root, but on a separate ibuf header
	page) */

	if (type & DICT_IBUF) {
		/* Allocate first the ibuf header page */
		buf_block_t*	ibuf_hdr_block = fseg_create(
			space, 0,
			IBUF_HEADER + IBUF_TREE_SEG_HEADER, mtr);

		buf_block_dbg_add_level(
			ibuf_hdr_block, SYNC_IBUF_TREE_NODE_NEW);

		ut_ad(buf_block_get_page_no(ibuf_hdr_block)
		      == IBUF_HEADER_PAGE_NO);
		/* Allocate then the next page to the segment: it will be the
		tree root page */

		block = fseg_alloc_free_page(
			buf_block_get_frame(ibuf_hdr_block)
			+ IBUF_HEADER + IBUF_TREE_SEG_HEADER,
			IBUF_TREE_ROOT_PAGE_NO,
			FSP_UP, mtr);
		ut_ad(buf_block_get_page_no(block) == IBUF_TREE_ROOT_PAGE_NO);
	} else {
#ifdef UNIV_BLOB_DEBUG
		if ((type & DICT_CLUSTERED) && !index->blobs) {
			mutex_create(PFS_NOT_INSTRUMENTED,
				     &index->blobs_mutex, SYNC_ANY_LATCH);
			index->blobs = rbt_create(sizeof(btr_blob_dbg_t),
						  btr_blob_dbg_cmp);
		}
#endif /* UNIV_BLOB_DEBUG */
		block = fseg_create(space, 0,
				    PAGE_HEADER + PAGE_BTR_SEG_TOP, mtr);
	}

	if (block == NULL) {

		return(FIL_NULL);
	}

	page_no = buf_block_get_page_no(block);
	frame = buf_block_get_frame(block);

	if (type & DICT_IBUF) {
		/* It is an insert buffer tree: initialize the free list */
		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE_NEW);

		ut_ad(page_no == IBUF_TREE_ROOT_PAGE_NO);

		flst_init(frame + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST, mtr);
	} else {
		/* It is a non-ibuf tree: create a file segment for leaf
		pages */
		buf_block_dbg_add_level(block, SYNC_TREE_NODE_NEW);

		if (!fseg_create(space, page_no,
				 PAGE_HEADER + PAGE_BTR_SEG_LEAF, mtr)) {
			/* Not enough space for new segment, free root
			segment before return. */
			btr_free_root(space, zip_size, page_no, mtr);

			return(FIL_NULL);
		}

		/* The fseg create acquires a second latch on the page,
		therefore we must declare it: */
		buf_block_dbg_add_level(block, SYNC_TREE_NODE_NEW);
	}

	/* Create a new index page on the allocated segment page */
	page_zip = buf_block_get_page_zip(block);

	if (page_zip) {
		page = page_create_zip(block, index, 0, mtr);
	} else {
		page = page_create(block, mtr,
				   dict_table_is_comp(index->table));
		/* Set the level of the new index page */
		btr_page_set_level(page, NULL, 0, mtr);
	}

	block->check_index_page_at_flush = TRUE;

	/* Set the index id of the page */
	btr_page_set_index_id(page, page_zip, index_id, mtr);

	/* Set the next node and previous node fields */
	btr_page_set_next(page, page_zip, FIL_NULL, mtr);
	btr_page_set_prev(page, page_zip, FIL_NULL, mtr);

	/* We reset the free bits for the page to allow creation of several
	trees in the same mtr, otherwise the latch on a bitmap page would
	prevent it because of the latching order.
	Note: Insert Buffering is disabled for temporary tables. */
	if (!dict_table_is_temporary(index->table)
	    && !(type & DICT_CLUSTERED)) {
		ibuf_reset_free_bits(block);
	}

	/* In the following assertion we test that two records of maximum
	allowed size fit on the root page: this fact is needed to ensure
	correctness of split algorithms */

	ut_ad(page_get_max_insert_size(page, 2) > 2 * BTR_PAGE_MAX_REC_SIZE);

	return(page_no);
}

/************************************************************//**
Frees a B-tree except the root page, which MUST be freed after this
by calling btr_free_root. */
UNIV_INTERN
void
btr_free_but_not_root(
/*==================*/
	ulint			space,		/*!< in: space where created */
	ulint			zip_size,	/*!< in: compressed page size
						in bytes or 0 for uncompressed
						pages */
	ulint			root_page_no,	/*!< in: root page number */
	bool			is_temp_table)	/*!< in: true if temp-table */
{
	ibool	finished;
	page_t*	root;
	mtr_t	mtr;

leaf_loop:
	mtr_start(&mtr);
	if (is_temp_table) {
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
	}

	root = btr_page_get(space, zip_size, root_page_no, RW_X_LATCH,
			    NULL, &mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
				    + root, space));
	ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
				    + root, space));
#endif /* UNIV_BTR_DEBUG */

	/* NOTE: page hash indexes are dropped when a page is freed inside
	fsp0fsp. */

	finished = fseg_free_step(root + PAGE_HEADER + PAGE_BTR_SEG_LEAF,
				  &mtr);
	mtr_commit(&mtr);

	if (!finished) {

		goto leaf_loop;
	}
top_loop:
	mtr_start(&mtr);
	if (is_temp_table) {
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
	}

	root = btr_page_get(space, zip_size, root_page_no, RW_X_LATCH,
			    NULL, &mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
				    + root, space));
#endif /* UNIV_BTR_DEBUG */

	finished = fseg_free_step_not_header(
		root + PAGE_HEADER + PAGE_BTR_SEG_TOP, &mtr);
	mtr_commit(&mtr);

	if (!finished) {

		goto top_loop;
	}
}

/************************************************************//**
Frees the B-tree root page. Other tree MUST already have been freed. */
UNIV_INTERN
void
btr_free_root(
/*==========*/
	ulint	space,		/*!< in: space where created */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	root_page_no,	/*!< in: root page number */
	mtr_t*	mtr)		/*!< in/out: mini-transaction */
{
	buf_block_t*	block;
	fseg_header_t*	header;

	block = btr_block_get(space, zip_size, root_page_no, RW_X_LATCH,
			      NULL, mtr);

	btr_search_drop_page_hash_index(block);

	header = buf_block_get_frame(block) + PAGE_HEADER + PAGE_BTR_SEG_TOP;
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_root_fseg_validate(header, space));
#endif /* UNIV_BTR_DEBUG */

	while (!fseg_free_step(header, mtr)) {
		/* Free the entire segment in small steps. */
	}
}
#endif /* !UNIV_HOTBACKUP */

/*************************************************************//**
Reorganizes an index page. */
static
ibool
btr_page_reorganize_low(
/*====================*/
	ibool		recovery,/*!< in: TRUE if called in recovery:
				locks should not be updated, i.e.,
				there cannot exist locks on the
				page, and a hash index should not be
				dropped: it cannot exist */
	ulint		compression_level,/*!< in: compression level to be used
				if dealing with compressed page */
	buf_block_t*	block,	/*!< in: page to be reorganized */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr */
{
#ifndef UNIV_HOTBACKUP
	buf_pool_t*	buf_pool	= buf_pool_from_bpage(&block->page);
#endif /* !UNIV_HOTBACKUP */
	page_t*		page		= buf_block_get_frame(block);
	page_zip_des_t*	page_zip	= buf_block_get_page_zip(block);
	buf_block_t*	temp_block;
	page_t*		temp_page;
	ulint		log_mode;
	ulint		data_size1;
	ulint		data_size2;
	ulint		max_ins_size1;
	ulint		max_ins_size2;
	ibool		success		= FALSE;
	byte		type;
	byte*		log_ptr;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	btr_assert_not_corrupted(block, index);
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
	data_size1 = page_get_data_size(page);
	max_ins_size1 = page_get_max_insert_size_after_reorganize(page, 1);

#ifndef UNIV_HOTBACKUP
	/* Write the log record */
	if (page_zip) {
		type = MLOG_ZIP_PAGE_REORGANIZE;
	} else if (page_is_comp(page)) {
		type = MLOG_COMP_PAGE_REORGANIZE;
	} else {
		type = MLOG_PAGE_REORGANIZE;
	}

	log_ptr = mlog_open_and_write_index(
		mtr, page, index, type, page_zip ? 1 : 0);

	/* For compressed pages write the compression level. */
	if (log_ptr && page_zip) {
		mach_write_to_1(log_ptr, compression_level);
		mlog_close(mtr, log_ptr + 1);
	}

#endif /* !UNIV_HOTBACKUP */

	/* Turn logging off */
	log_mode = mtr_set_log_mode(mtr, MTR_LOG_NONE);

#ifndef UNIV_HOTBACKUP
	temp_block = buf_block_alloc(buf_pool);
#else /* !UNIV_HOTBACKUP */
	ut_ad(block == back_block1);
	temp_block = back_block2;
#endif /* !UNIV_HOTBACKUP */
	temp_page = temp_block->frame;

	/* Copy the old page to temporary space */
	buf_frame_copy(temp_page, page);

#ifndef UNIV_HOTBACKUP
	if (!recovery) {
		btr_search_drop_page_hash_index(block);
	}

	block->check_index_page_at_flush = TRUE;
#endif /* !UNIV_HOTBACKUP */
	btr_blob_dbg_remove(page, index, "btr_page_reorganize");

	/* Recreate the page: note that global data on page (possible
	segment headers, next page-field, etc.) is preserved intact */

	page_create(block, mtr, dict_table_is_comp(index->table));

	/* Copy the records from the temporary space to the recreated page;
	do not copy the lock bits yet */

	page_copy_rec_list_end_no_locks(block, temp_block,
					page_get_infimum_rec(temp_page),
					index, mtr);

	if (dict_index_is_sec_or_ibuf(index) && page_is_leaf(page)) {
		/* Copy max trx id to recreated page */
		trx_id_t	max_trx_id = page_get_max_trx_id(temp_page);
		page_set_max_trx_id(block, NULL, max_trx_id, mtr);
		/* In crash recovery, dict_index_is_sec_or_ibuf() always
		returns TRUE, even for clustered indexes.  max_trx_id is
		unused in clustered index pages. */
		ut_ad(max_trx_id != 0 || recovery);
	}

	if (page_zip
	    && !page_zip_compress(page_zip, page, index,
				  compression_level, NULL)) {

		/* Restore the old page and exit. */
		btr_blob_dbg_restore(page, temp_page, index,
				     "btr_page_reorganize_compress_fail");

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
		/* Check that the bytes that we skip are identical. */
		ut_a(!memcmp(page, temp_page, PAGE_HEADER));
		ut_a(!memcmp(PAGE_HEADER + PAGE_N_RECS + page,
			     PAGE_HEADER + PAGE_N_RECS + temp_page,
			     PAGE_DATA - (PAGE_HEADER + PAGE_N_RECS)));
		ut_a(!memcmp(UNIV_PAGE_SIZE - FIL_PAGE_DATA_END + page,
			     UNIV_PAGE_SIZE - FIL_PAGE_DATA_END + temp_page,
			     FIL_PAGE_DATA_END));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

		memcpy(PAGE_HEADER + page, PAGE_HEADER + temp_page,
		       PAGE_N_RECS - PAGE_N_DIR_SLOTS);
		memcpy(PAGE_DATA + page, PAGE_DATA + temp_page,
		       UNIV_PAGE_SIZE - PAGE_DATA - FIL_PAGE_DATA_END);

#if defined UNIV_DEBUG || defined UNIV_ZIP_DEBUG
		ut_a(!memcmp(page, temp_page, UNIV_PAGE_SIZE));
#endif /* UNIV_DEBUG || UNIV_ZIP_DEBUG */

		goto func_exit;
	}

#ifndef UNIV_HOTBACKUP
	if (!recovery) {
		/* Update the record lock bitmaps */
		lock_move_reorganize_page(block, temp_block);
	}
#endif /* !UNIV_HOTBACKUP */

	data_size2 = page_get_data_size(page);
	max_ins_size2 = page_get_max_insert_size_after_reorganize(page, 1);

	if (data_size1 != data_size2 || max_ins_size1 != max_ins_size2) {
		buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);
		buf_page_print(temp_page, 0, BUF_PAGE_PRINT_NO_CRASH);

		fprintf(stderr,
			"InnoDB: Error: page old data size %lu"
			" new data size %lu\n"
			"InnoDB: Error: page old max ins size %lu"
			" new max ins size %lu\n"
			"InnoDB: Submit a detailed bug report"
			" to http://bugs.mysql.com\n",
			(unsigned long) data_size1, (unsigned long) data_size2,
			(unsigned long) max_ins_size1,
			(unsigned long) max_ins_size2);
		ut_ad(0);
	} else {
		success = TRUE;
	}

func_exit:
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
#ifndef UNIV_HOTBACKUP
	buf_block_free(temp_block);
#endif /* !UNIV_HOTBACKUP */

	/* Restore logging mode */
	mtr_set_log_mode(mtr, log_mode);

	return(success);
}

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Reorganizes an index page.
IMPORTANT: if btr_page_reorganize() is invoked on a compressed leaf
page of a non-clustered index, the caller must update the insert
buffer free bits in the same mini-transaction in such a way that the
modification will be redo-logged.
@return	TRUE on success, FALSE on failure */
UNIV_INTERN
ibool
btr_page_reorganize(
/*================*/
	buf_block_t*	block,	/*!< in: page to be reorganized */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr */
{
	return(btr_page_reorganize_low(FALSE, page_compression_level,
				       block, index, mtr));
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses a redo log record of reorganizing a page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_parse_page_reorganize(
/*======================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	dict_index_t*	index,	/*!< in: record descriptor */
	bool		compressed,/*!< in: true if compressed page */
	buf_block_t*	block,	/*!< in: page to be reorganized, or NULL */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	ulint	level = page_compression_level;

	ut_ad(ptr && end_ptr);

	/* If dealing with a compressed page the record has the
	compression level used during original compression written in
	one byte. Otherwise record is empty. */
	if (compressed) {
		if (ptr == end_ptr) {
			return(NULL);
		}

		level = (ulint)mach_read_from_1(ptr);

		ut_a(level <= 9);
		++ptr;
	}

	if (block != NULL) {
		btr_page_reorganize_low(TRUE, level, block, index, mtr);
	}

	return(ptr);
}

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Empties an index page.  @see btr_page_create(). */
static
void
btr_page_empty(
/*===========*/
	buf_block_t*	block,	/*!< in: page to be emptied */
	page_zip_des_t*	page_zip,/*!< out: compressed page, or NULL */
	dict_index_t*	index,	/*!< in: index of the page */
	ulint		level,	/*!< in: the B-tree level of the page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_t*	page = buf_block_get_frame(block);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	ut_ad(page_zip == buf_block_get_page_zip(block));
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

	btr_search_drop_page_hash_index(block);
	btr_blob_dbg_remove(page, index, "btr_page_empty");

	/* Recreate the page: note that global data on page (possible
	segment headers, next page-field, etc.) is preserved intact */

	if (page_zip) {
		page_create_zip(block, index, level, mtr);
	} else {
		page_create(block, mtr, dict_table_is_comp(index->table));
		btr_page_set_level(page, NULL, level, mtr);
	}

	block->check_index_page_at_flush = TRUE;
}

/*************************************************************//**
Makes tree one level higher by splitting the root, and inserts
the tuple. It is assumed that mtr contains an x-latch on the tree.
NOTE that the operation of this function must always succeed,
we cannot reverse it: therefore enough free disk space must be
guaranteed to be available before this function is called.
@return	inserted record */
UNIV_INTERN
rec_t*
btr_root_raise_and_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert: must be
				on the root page; when the function returns,
				the cursor is positioned on the predecessor
				of the inserted record */
	ulint**		offsets,/*!< out: offsets on inserted record */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap, or NULL */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dict_index_t*	index;
	page_t*		root;
	page_t*		new_page;
	ulint		new_page_no;
	rec_t*		rec;
	dtuple_t*	node_ptr;
	ulint		level;
	rec_t*		node_ptr_rec;
	page_cur_t*	page_cursor;
	page_zip_des_t*	root_page_zip;
	page_zip_des_t*	new_page_zip;
	buf_block_t*	root_block;
	buf_block_t*	new_block;

	root = btr_cur_get_page(cursor);
	root_block = btr_cur_get_block(cursor);
	root_page_zip = buf_block_get_page_zip(root_block);
	ut_ad(page_get_n_recs(root) > 0);
	index = btr_cur_get_index(cursor);
#ifdef UNIV_ZIP_DEBUG
	ut_a(!root_page_zip || page_zip_validate(root_page_zip, root, index));
#endif /* UNIV_ZIP_DEBUG */
#ifdef UNIV_BTR_DEBUG
	if (!dict_index_is_ibuf(index)) {
		ulint	space = dict_index_get_space(index);

		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
					    + root, space));
		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
					    + root, space));
	}

	ut_a(dict_index_get_page(index) == page_get_page_no(root));
#endif /* UNIV_BTR_DEBUG */
	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, root_block, MTR_MEMO_PAGE_X_FIX));

	/* Allocate a new page to the tree. Root splitting is done by first
	moving the root records to the new page, emptying the root, putting
	a node pointer to the new page, and then splitting the new page. */

	level = btr_page_get_level(root, mtr);

	new_block = btr_page_alloc(index, 0, FSP_NO_DIR, level, mtr, mtr);
	new_page = buf_block_get_frame(new_block);
	new_page_zip = buf_block_get_page_zip(new_block);
	ut_a(!new_page_zip == !root_page_zip);
	ut_a(!new_page_zip
	     || page_zip_get_size(new_page_zip)
	     == page_zip_get_size(root_page_zip));

	btr_page_create(new_block, new_page_zip, index, level, mtr);

	/* Set the next node and previous node fields of new page */
	btr_page_set_next(new_page, new_page_zip, FIL_NULL, mtr);
	btr_page_set_prev(new_page, new_page_zip, FIL_NULL, mtr);

	/* Copy the records from root to the new page one by one. */

	if (0
#ifdef UNIV_ZIP_COPY
	    || new_page_zip
#endif /* UNIV_ZIP_COPY */
	    || !page_copy_rec_list_end(new_block, root_block,
				     page_get_infimum_rec(root),
				     index, mtr)) {
		ut_a(new_page_zip);

		/* Copy the page byte for byte. */
		page_zip_copy_recs(new_page_zip, new_page,
				   root_page_zip, root, index, mtr);

		/* Update the lock table and possible hash index. */

		lock_move_rec_list_end(new_block, root_block,
				       page_get_infimum_rec(root));

		btr_search_move_or_delete_hash_entries(new_block, root_block,
						       index);
	}

	/* If this is a pessimistic insert which is actually done to
	perform a pessimistic update then we have stored the lock
	information of the record to be inserted on the infimum of the
	root page: we cannot discard the lock structs on the root page */

	lock_update_root_raise(new_block, root_block);

	/* Create a memory heap where the node pointer is stored */
	if (!*heap) {
		*heap = mem_heap_create(1000);
	}

	rec = page_rec_get_next(page_get_infimum_rec(new_page));
	new_page_no = buf_block_get_page_no(new_block);

	/* Build the node pointer (= node key and page address) for the
	child */

	node_ptr = dict_index_build_node_ptr(
		index, rec, new_page_no, *heap, level);
	/* The node pointer must be marked as the predefined minimum record,
	as there is no lower alphabetical limit to records in the leftmost
	node of a level: */
	dtuple_set_info_bits(node_ptr,
			     dtuple_get_info_bits(node_ptr)
			     | REC_INFO_MIN_REC_FLAG);

	/* Rebuild the root page to get free space */
	btr_page_empty(root_block, root_page_zip, index, level + 1, mtr);

	/* Set the next node and previous node fields, although
	they should already have been set.  The previous node field
	must be FIL_NULL if root_page_zip != NULL, because the
	REC_INFO_MIN_REC_FLAG (of the first user record) will be
	set if and only if btr_page_get_prev() == FIL_NULL. */
	btr_page_set_next(root, root_page_zip, FIL_NULL, mtr);
	btr_page_set_prev(root, root_page_zip, FIL_NULL, mtr);

	page_cursor = btr_cur_get_page_cur(cursor);

	/* Insert node pointer to the root */

	page_cur_set_before_first(root_block, page_cursor);

	node_ptr_rec = page_cur_tuple_insert(page_cursor, node_ptr,
					     index, offsets, heap, 0, mtr);

	/* The root page should only contain the node pointer
	to new_page at this point.  Thus, the data should fit. */
	ut_a(node_ptr_rec);

	/* We play safe and reset the free bits for the new page */

#if 0
	fprintf(stderr, "Root raise new page no %lu\n", new_page_no);
#endif

	if (!dict_index_is_clust(index)) {
		ibuf_reset_free_bits(new_block);
	}

	/* Reposition the cursor to the child node */
	page_cur_search(new_block, index, tuple,
			PAGE_CUR_LE, page_cursor);

	/* Split the child and insert tuple */
	return(btr_page_split_and_insert(flags, cursor, offsets, heap,
					 tuple, n_ext, mtr));
}

/*************************************************************//**
Decides if the page should be split at the convergence point of inserts
converging to the left.
@return	TRUE if split recommended */
UNIV_INTERN
ibool
btr_page_get_split_rec_to_left(
/*===========================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert */
	rec_t**		split_rec) /*!< out: if split recommended,
				the first record on upper half page,
				or NULL if tuple to be inserted should
				be first */
{
	page_t*	page;
	rec_t*	insert_point;
	rec_t*	infimum;

	page = btr_cur_get_page(cursor);
	insert_point = btr_cur_get_rec(cursor);

	if (page_header_get_ptr(page, PAGE_LAST_INSERT)
	    == page_rec_get_next(insert_point)) {

		infimum = page_get_infimum_rec(page);

		/* If the convergence is in the middle of a page, include also
		the record immediately before the new insert to the upper
		page. Otherwise, we could repeatedly move from page to page
		lots of records smaller than the convergence point. */

		if (infimum != insert_point
		    && page_rec_get_next(infimum) != insert_point) {

			*split_rec = insert_point;
		} else {
			*split_rec = page_rec_get_next(insert_point);
		}

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************//**
Decides if the page should be split at the convergence point of inserts
converging to the right.
@return	TRUE if split recommended */
UNIV_INTERN
ibool
btr_page_get_split_rec_to_right(
/*============================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert */
	rec_t**		split_rec) /*!< out: if split recommended,
				the first record on upper half page,
				or NULL if tuple to be inserted should
				be first */
{
	page_t*	page;
	rec_t*	insert_point;

	page = btr_cur_get_page(cursor);
	insert_point = btr_cur_get_rec(cursor);

	/* We use eager heuristics: if the new insert would be right after
	the previous insert on the same page, we assume that there is a
	pattern of sequential inserts here. */

	if (page_header_get_ptr(page, PAGE_LAST_INSERT) == insert_point) {

		rec_t*	next_rec;

		next_rec = page_rec_get_next(insert_point);

		if (page_rec_is_supremum(next_rec)) {
split_at_new:
			/* Split at the new record to insert */
			*split_rec = NULL;
		} else {
			rec_t*	next_next_rec = page_rec_get_next(next_rec);
			if (page_rec_is_supremum(next_next_rec)) {

				goto split_at_new;
			}

			/* If there are >= 2 user records up from the insert
			point, split all but 1 off. We want to keep one because
			then sequential inserts can use the adaptive hash
			index, as they can do the necessary checks of the right
			search position just by looking at the records on this
			page. */

			*split_rec = next_next_rec;
		}

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************//**
Calculates a split record such that the tuple will certainly fit on
its half-page when the split is performed. We assume in this function
only that the cursor page has at least one user record.
@return split record, or NULL if tuple will be the first record on
the lower or upper half-page (determined by btr_page_tuple_smaller()) */
static
rec_t*
btr_page_get_split_rec(
/*===================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which insert should be made */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext)	/*!< in: number of externally stored columns */
{
	page_t*		page;
	page_zip_des_t*	page_zip;
	ulint		insert_size;
	ulint		free_space;
	ulint		total_data;
	ulint		total_n_recs;
	ulint		total_space;
	ulint		incl_data;
	rec_t*		ins_rec;
	rec_t*		rec;
	rec_t*		next_rec;
	ulint		n;
	mem_heap_t*	heap;
	ulint*		offsets;

	page = btr_cur_get_page(cursor);

	insert_size = rec_get_converted_size(cursor->index, tuple, n_ext);
	free_space  = page_get_free_space_of_empty(page_is_comp(page));

	page_zip = btr_cur_get_page_zip(cursor);
	if (page_zip) {
		/* Estimate the free space of an empty compressed page. */
		ulint	free_space_zip = page_zip_empty_size(
			cursor->index->n_fields,
			page_zip_get_size(page_zip));

		if (free_space > (ulint) free_space_zip) {
			free_space = (ulint) free_space_zip;
		}
	}

	/* free_space is now the free space of a created new page */

	total_data   = page_get_data_size(page) + insert_size;
	total_n_recs = page_get_n_recs(page) + 1;
	ut_ad(total_n_recs >= 2);
	total_space  = total_data + page_dir_calc_reserved_space(total_n_recs);

	n = 0;
	incl_data = 0;
	ins_rec = btr_cur_get_rec(cursor);
	rec = page_get_infimum_rec(page);

	heap = NULL;
	offsets = NULL;

	/* We start to include records to the left half, and when the
	space reserved by them exceeds half of total_space, then if
	the included records fit on the left page, they will be put there
	if something was left over also for the right page,
	otherwise the last included record will be the first on the right
	half page */

	do {
		/* Decide the next record to include */
		if (rec == ins_rec) {
			rec = NULL;	/* NULL denotes that tuple is
					now included */
		} else if (rec == NULL) {
			rec = page_rec_get_next(ins_rec);
		} else {
			rec = page_rec_get_next(rec);
		}

		if (rec == NULL) {
			/* Include tuple */
			incl_data += insert_size;
		} else {
			offsets = rec_get_offsets(rec, cursor->index,
						  offsets, ULINT_UNDEFINED,
						  &heap);
			incl_data += rec_offs_size(offsets);
		}

		n++;
	} while (incl_data + page_dir_calc_reserved_space(n)
		 < total_space / 2);

	if (incl_data + page_dir_calc_reserved_space(n) <= free_space) {
		/* The next record will be the first on
		the right half page if it is not the
		supremum record of page */

		if (rec == ins_rec) {
			rec = NULL;

			goto func_exit;
		} else if (rec == NULL) {
			next_rec = page_rec_get_next(ins_rec);
		} else {
			next_rec = page_rec_get_next(rec);
		}
		ut_ad(next_rec);
		if (!page_rec_is_supremum(next_rec)) {
			rec = next_rec;
		}
	}

func_exit:
	if (heap) {
		mem_heap_free(heap);
	}
	return(rec);
}

/*************************************************************//**
Returns TRUE if the insert fits on the appropriate half-page with the
chosen split_rec.
@return	true if fits */
static __attribute__((nonnull(1,3,4,6), warn_unused_result))
bool
btr_page_insert_fits(
/*=================*/
	btr_cur_t*	cursor,	/*!< in: cursor at which insert
				should be made */
	const rec_t*	split_rec,/*!< in: suggestion for first record
				on upper half-page, or NULL if
				tuple to be inserted should be first */
	ulint**		offsets,/*!< in: rec_get_offsets(
				split_rec, cursor->index); out: garbage */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mem_heap_t**	heap)	/*!< in: temporary memory heap */
{
	page_t*		page;
	ulint		insert_size;
	ulint		free_space;
	ulint		total_data;
	ulint		total_n_recs;
	const rec_t*	rec;
	const rec_t*	end_rec;

	page = btr_cur_get_page(cursor);

	ut_ad(!split_rec
	      || !page_is_comp(page) == !rec_offs_comp(*offsets));
	ut_ad(!split_rec
	      || rec_offs_validate(split_rec, cursor->index, *offsets));

	insert_size = rec_get_converted_size(cursor->index, tuple, n_ext);
	free_space  = page_get_free_space_of_empty(page_is_comp(page));

	/* free_space is now the free space of a created new page */

	total_data   = page_get_data_size(page) + insert_size;
	total_n_recs = page_get_n_recs(page) + 1;

	/* We determine which records (from rec to end_rec, not including
	end_rec) will end up on the other half page from tuple when it is
	inserted. */

	if (split_rec == NULL) {
		rec = page_rec_get_next(page_get_infimum_rec(page));
		end_rec = page_rec_get_next(btr_cur_get_rec(cursor));

	} else if (cmp_dtuple_rec(tuple, split_rec, *offsets) >= 0) {

		rec = page_rec_get_next(page_get_infimum_rec(page));
		end_rec = split_rec;
	} else {
		rec = split_rec;
		end_rec = page_get_supremum_rec(page);
	}

	if (total_data + page_dir_calc_reserved_space(total_n_recs)
	    <= free_space) {

		/* Ok, there will be enough available space on the
		half page where the tuple is inserted */

		return(true);
	}

	while (rec != end_rec) {
		/* In this loop we calculate the amount of reserved
		space after rec is removed from page. */

		*offsets = rec_get_offsets(rec, cursor->index, *offsets,
					   ULINT_UNDEFINED, heap);

		total_data -= rec_offs_size(*offsets);
		total_n_recs--;

		if (total_data + page_dir_calc_reserved_space(total_n_recs)
		    <= free_space) {

			/* Ok, there will be enough available space on the
			half page where the tuple is inserted */

			return(true);
		}

		rec = page_rec_get_next_const(rec);
	}

	return(false);
}

/*******************************************************//**
Inserts a data tuple to a tree on a non-leaf level. It is assumed
that mtr holds an x-latch on the tree. */
UNIV_INTERN
void
btr_insert_on_non_leaf_level_func(
/*==============================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: level, must be > 0 */
	dtuple_t*	tuple,	/*!< in: the record to be inserted */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr */
{
	big_rec_t*	dummy_big_rec;
	btr_cur_t	cursor;
	dberr_t		err;
	rec_t*		rec;
	ulint*		offsets	= NULL;
	mem_heap_t*	heap = NULL;

	ut_ad(level > 0);

	btr_cur_search_to_nth_level(index, level, tuple, PAGE_CUR_LE,
				    BTR_CONT_MODIFY_TREE,
				    &cursor, 0, file, line, mtr);

	ut_ad(cursor.flag == BTR_CUR_BINARY);

	err = btr_cur_optimistic_insert(
		flags
		| BTR_NO_LOCKING_FLAG
		| BTR_KEEP_SYS_FLAG
		| BTR_NO_UNDO_LOG_FLAG,
		&cursor, &offsets, &heap,
		tuple, &rec, &dummy_big_rec, 0, NULL, mtr);

	if (err == DB_FAIL) {
		err = btr_cur_pessimistic_insert(flags
						 | BTR_NO_LOCKING_FLAG
						 | BTR_KEEP_SYS_FLAG
						 | BTR_NO_UNDO_LOG_FLAG,
						 &cursor, &offsets, &heap,
						 tuple, &rec,
						 &dummy_big_rec, 0, NULL, mtr);
		ut_a(err == DB_SUCCESS);
	}
	mem_heap_free(heap);
}

/**************************************************************//**
Attaches the halves of an index page on the appropriate level in an
index tree. */
static __attribute__((nonnull))
void
btr_attach_half_pages(
/*==================*/
	ulint		flags,		/*!< in: undo logging and
					locking flags */
	dict_index_t*	index,		/*!< in: the index tree */
	buf_block_t*	block,		/*!< in/out: page to be split */
	const rec_t*	split_rec,	/*!< in: first record on upper
					half page */
	buf_block_t*	new_block,	/*!< in/out: the new half page */
	ulint		direction,	/*!< in: FSP_UP or FSP_DOWN */
	mtr_t*		mtr)		/*!< in: mtr */
{
	ulint		space;
	ulint		zip_size;
	ulint		prev_page_no;
	ulint		next_page_no;
	ulint		level;
	page_t*		page		= buf_block_get_frame(block);
	page_t*		lower_page;
	page_t*		upper_page;
	ulint		lower_page_no;
	ulint		upper_page_no;
	page_zip_des_t*	lower_page_zip;
	page_zip_des_t*	upper_page_zip;
	dtuple_t*	node_ptr_upper;
	mem_heap_t*	heap;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, new_block, MTR_MEMO_PAGE_X_FIX));

	/* Create a memory heap where the data tuple is stored */
	heap = mem_heap_create(1024);

	/* Based on split direction, decide upper and lower pages */
	if (direction == FSP_DOWN) {

		btr_cur_t	cursor;
		ulint*		offsets;

		lower_page = buf_block_get_frame(new_block);
		lower_page_no = buf_block_get_page_no(new_block);
		lower_page_zip = buf_block_get_page_zip(new_block);
		upper_page = buf_block_get_frame(block);
		upper_page_no = buf_block_get_page_no(block);
		upper_page_zip = buf_block_get_page_zip(block);

		/* Look up the index for the node pointer to page */
		offsets = btr_page_get_father_block(NULL, heap, index,
						    block, mtr, &cursor);

		/* Replace the address of the old child node (= page) with the
		address of the new lower half */

		btr_node_ptr_set_child_page_no(
			btr_cur_get_rec(&cursor),
			btr_cur_get_page_zip(&cursor),
			offsets, lower_page_no, mtr);
		mem_heap_empty(heap);
	} else {
		lower_page = buf_block_get_frame(block);
		lower_page_no = buf_block_get_page_no(block);
		lower_page_zip = buf_block_get_page_zip(block);
		upper_page = buf_block_get_frame(new_block);
		upper_page_no = buf_block_get_page_no(new_block);
		upper_page_zip = buf_block_get_page_zip(new_block);
	}

	/* Get the level of the split pages */
	level = btr_page_get_level(buf_block_get_frame(block), mtr);
	ut_ad(level
	      == btr_page_get_level(buf_block_get_frame(new_block), mtr));

	/* Build the node pointer (= node key and page address) for the upper
	half */

	node_ptr_upper = dict_index_build_node_ptr(index, split_rec,
						   upper_page_no, heap, level);

	/* Insert it next to the pointer to the lower half. Note that this
	may generate recursion leading to a split on the higher level. */

	btr_insert_on_non_leaf_level(flags, index, level + 1,
				     node_ptr_upper, mtr);

	/* Free the memory heap */
	mem_heap_free(heap);

	/* Get the previous and next pages of page */

	prev_page_no = btr_page_get_prev(page, mtr);
	next_page_no = btr_page_get_next(page, mtr);
	space = buf_block_get_space(block);
	zip_size = buf_block_get_zip_size(block);

	/* Update page links of the level */

	if (prev_page_no != FIL_NULL) {
		buf_block_t*	prev_block = btr_block_get(
			space, zip_size, prev_page_no, RW_X_LATCH, index, mtr);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(prev_block->frame) == page_is_comp(page));
		ut_a(btr_page_get_next(prev_block->frame, mtr)
		     == buf_block_get_page_no(block));
#endif /* UNIV_BTR_DEBUG */

		btr_page_set_next(buf_block_get_frame(prev_block),
				  buf_block_get_page_zip(prev_block),
				  lower_page_no, mtr);
	}

	if (next_page_no != FIL_NULL) {
		buf_block_t*	next_block = btr_block_get(
			space, zip_size, next_page_no, RW_X_LATCH, index, mtr);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(next_block->frame) == page_is_comp(page));
		ut_a(btr_page_get_prev(next_block->frame, mtr)
		     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

		btr_page_set_prev(buf_block_get_frame(next_block),
				  buf_block_get_page_zip(next_block),
				  upper_page_no, mtr);
	}

	btr_page_set_prev(lower_page, lower_page_zip, prev_page_no, mtr);
	btr_page_set_next(lower_page, lower_page_zip, upper_page_no, mtr);

	btr_page_set_prev(upper_page, upper_page_zip, lower_page_no, mtr);
	btr_page_set_next(upper_page, upper_page_zip, next_page_no, mtr);
}

/*************************************************************//**
Determine if a tuple is smaller than any record on the page.
@return TRUE if smaller */
static __attribute__((nonnull, warn_unused_result))
bool
btr_page_tuple_smaller(
/*===================*/
	btr_cur_t*	cursor,	/*!< in: b-tree cursor */
	const dtuple_t*	tuple,	/*!< in: tuple to consider */
	ulint**		offsets,/*!< in/out: temporary storage */
	ulint		n_uniq,	/*!< in: number of unique fields
				in the index page records */
	mem_heap_t**	heap)	/*!< in/out: heap for offsets */
{
	buf_block_t*	block;
	const rec_t*	first_rec;
	page_cur_t	pcur;

	/* Read the first user record in the page. */
	block = btr_cur_get_block(cursor);
	page_cur_set_before_first(block, &pcur);
	page_cur_move_to_next(&pcur);
	first_rec = page_cur_get_rec(&pcur);

	*offsets = rec_get_offsets(
		first_rec, cursor->index, *offsets,
		n_uniq, heap);

	return(cmp_dtuple_rec(tuple, first_rec, *offsets) < 0);
}

/*************************************************************//**
Splits an index page to halves and inserts the tuple. It is assumed
that mtr holds an x-latch to the index tree. NOTE: the tree x-latch is
released within this function! NOTE that the operation of this
function must always succeed, we cannot reverse it: therefore enough
free disk space (2 pages) must be guaranteed to be available before
this function is called.

@return inserted record */
UNIV_INTERN
rec_t*
btr_page_split_and_insert(
/*======================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: cursor at which to insert; when the
				function returns, the cursor is positioned
				on the predecessor of the inserted record */
	ulint**		offsets,/*!< out: offsets on inserted record */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap, or NULL */
	const dtuple_t*	tuple,	/*!< in: tuple to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr)	/*!< in: mtr */
{
	buf_block_t*	block;
	page_t*		page;
	page_zip_des_t*	page_zip;
	ulint		page_no;
	byte		direction;
	ulint		hint_page_no;
	buf_block_t*	new_block;
	page_t*		new_page;
	page_zip_des_t*	new_page_zip;
	rec_t*		split_rec;
	buf_block_t*	left_block;
	buf_block_t*	right_block;
	buf_block_t*	insert_block;
	page_cur_t*	page_cursor;
	rec_t*		first_rec;
	byte*		buf = 0; /* remove warning */
	rec_t*		move_limit;
	ibool		insert_will_fit;
	ibool		insert_left;
	ulint		n_iterations = 0;
	rec_t*		rec;
	ulint		n_uniq;

	if (!*heap) {
		*heap = mem_heap_create(1024);
	}
	n_uniq = dict_index_get_n_unique_in_tree(cursor->index);
func_start:
	mem_heap_empty(*heap);
	*offsets = NULL;

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(cursor->index),
				MTR_MEMO_X_LOCK));
	ut_ad(!dict_index_is_online_ddl(cursor->index)
	      || (flags & BTR_CREATE_FLAG)
	      || dict_index_is_clust(cursor->index));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(dict_index_get_lock(cursor->index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	block = btr_cur_get_block(cursor);
	page = buf_block_get_frame(block);
	page_zip = buf_block_get_page_zip(block);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	ut_ad(page_get_n_recs(page) >= 1);

	page_no = buf_block_get_page_no(block);

	/* 1. Decide the split record; split_rec == NULL means that the
	tuple to be inserted should be the first record on the upper
	half-page */
	insert_left = FALSE;

	if (n_iterations > 0) {
		direction = FSP_UP;
		hint_page_no = page_no + 1;
		split_rec = btr_page_get_split_rec(cursor, tuple, n_ext);

		if (split_rec == NULL) {
			insert_left = btr_page_tuple_smaller(
				cursor, tuple, offsets, n_uniq, heap);
		}
	} else if (btr_page_get_split_rec_to_right(cursor, &split_rec)) {
		direction = FSP_UP;
		hint_page_no = page_no + 1;

	} else if (btr_page_get_split_rec_to_left(cursor, &split_rec)) {
		direction = FSP_DOWN;
		hint_page_no = page_no - 1;
		ut_ad(split_rec);
	} else {
		direction = FSP_UP;
		hint_page_no = page_no + 1;

		/* If there is only one record in the index page, we
		can't split the node in the middle by default. We need
		to determine whether the new record will be inserted
		to the left or right. */

		if (page_get_n_recs(page) > 1) {
			split_rec = page_get_middle_rec(page);
		} else if (btr_page_tuple_smaller(cursor, tuple,
						  offsets, n_uniq, heap)) {
			split_rec = page_rec_get_next(
				page_get_infimum_rec(page));
		} else {
			split_rec = NULL;
		}
	}

	/* 2. Allocate a new page to the index */
	new_block = btr_page_alloc(cursor->index, hint_page_no, direction,
				   btr_page_get_level(page, mtr), mtr, mtr);
	new_page = buf_block_get_frame(new_block);
	new_page_zip = buf_block_get_page_zip(new_block);
	btr_page_create(new_block, new_page_zip, cursor->index,
			btr_page_get_level(page, mtr), mtr);

	/* 3. Calculate the first record on the upper half-page, and the
	first record (move_limit) on original page which ends up on the
	upper half */

	if (split_rec) {
		first_rec = move_limit = split_rec;

		*offsets = rec_get_offsets(split_rec, cursor->index, *offsets,
					   n_uniq, heap);

		insert_left = cmp_dtuple_rec(tuple, split_rec, *offsets) < 0;

		if (!insert_left && new_page_zip && n_iterations > 0) {
			/* If a compressed page has already been split,
			avoid further splits by inserting the record
			to an empty page. */
			split_rec = NULL;
			goto insert_empty;
		}
	} else if (insert_left) {
		ut_a(n_iterations > 0);
		first_rec = page_rec_get_next(page_get_infimum_rec(page));
		move_limit = page_rec_get_next(btr_cur_get_rec(cursor));
	} else {
insert_empty:
		ut_ad(!split_rec);
		ut_ad(!insert_left);
		buf = (byte*) mem_alloc(rec_get_converted_size(cursor->index,
							       tuple, n_ext));

		first_rec = rec_convert_dtuple_to_rec(buf, cursor->index,
						      tuple, n_ext);
		move_limit = page_rec_get_next(btr_cur_get_rec(cursor));
	}

	/* 4. Do first the modifications in the tree structure */

	btr_attach_half_pages(flags, cursor->index, block,
			      first_rec, new_block, direction, mtr);

	/* If the split is made on the leaf level and the insert will fit
	on the appropriate half-page, we may release the tree x-latch.
	We can then move the records after releasing the tree latch,
	thus reducing the tree latch contention. */

	if (split_rec) {
		insert_will_fit = !new_page_zip
			&& btr_page_insert_fits(cursor, split_rec,
						offsets, tuple, n_ext, heap);
	} else {
		if (!insert_left) {
			mem_free(buf);
			buf = NULL;
		}

		insert_will_fit = !new_page_zip
			&& btr_page_insert_fits(cursor, NULL,
						offsets, tuple, n_ext, heap);
	}

	if (insert_will_fit && page_is_leaf(page)
	    && !dict_index_is_online_ddl(cursor->index)) {

		mtr_memo_release(mtr, dict_index_get_lock(cursor->index),
				 MTR_MEMO_X_LOCK);
	}

	/* 5. Move then the records to the new page */
	if (direction == FSP_DOWN) {
		/*		fputs("Split left\n", stderr); */

		if (0
#ifdef UNIV_ZIP_COPY
		    || page_zip
#endif /* UNIV_ZIP_COPY */
		    || !page_move_rec_list_start(new_block, block, move_limit,
					       cursor->index, mtr)) {
			/* For some reason, compressing new_page failed,
			even though it should contain fewer records than
			the original page.  Copy the page byte for byte
			and then delete the records from both pages
			as appropriate.  Deleting will always succeed. */
			ut_a(new_page_zip);

			page_zip_copy_recs(new_page_zip, new_page,
					   page_zip, page, cursor->index, mtr);
			page_delete_rec_list_end(move_limit - page + new_page,
						 new_block, cursor->index,
						 ULINT_UNDEFINED,
						 ULINT_UNDEFINED, mtr);

			/* Update the lock table and possible hash index. */

			lock_move_rec_list_start(
				new_block, block, move_limit,
				new_page + PAGE_NEW_INFIMUM);

			btr_search_move_or_delete_hash_entries(
				new_block, block, cursor->index);

			/* Delete the records from the source page. */

			page_delete_rec_list_start(move_limit, block,
						   cursor->index, mtr);
		}

		left_block = new_block;
		right_block = block;

		lock_update_split_left(right_block, left_block);
	} else {
		/*		fputs("Split right\n", stderr); */

		if (0
#ifdef UNIV_ZIP_COPY
		    || page_zip
#endif /* UNIV_ZIP_COPY */
		    || !page_move_rec_list_end(new_block, block, move_limit,
					     cursor->index, mtr)) {
			/* For some reason, compressing new_page failed,
			even though it should contain fewer records than
			the original page.  Copy the page byte for byte
			and then delete the records from both pages
			as appropriate.  Deleting will always succeed. */
			ut_a(new_page_zip);

			page_zip_copy_recs(new_page_zip, new_page,
					   page_zip, page, cursor->index, mtr);
			page_delete_rec_list_start(move_limit - page
						   + new_page, new_block,
						   cursor->index, mtr);

			/* Update the lock table and possible hash index. */

			lock_move_rec_list_end(new_block, block, move_limit);

			btr_search_move_or_delete_hash_entries(
				new_block, block, cursor->index);

			/* Delete the records from the source page. */

			page_delete_rec_list_end(move_limit, block,
						 cursor->index,
						 ULINT_UNDEFINED,
						 ULINT_UNDEFINED, mtr);
		}

		left_block = block;
		right_block = new_block;

		lock_update_split_right(right_block, left_block);
	}

#ifdef UNIV_ZIP_DEBUG
	if (page_zip) {
		ut_a(page_zip_validate(page_zip, page, cursor->index));
		ut_a(page_zip_validate(new_page_zip, new_page, cursor->index));
	}
#endif /* UNIV_ZIP_DEBUG */

	/* At this point, split_rec, move_limit and first_rec may point
	to garbage on the old page. */

	/* 6. The split and the tree modification is now completed. Decide the
	page where the tuple should be inserted */

	if (insert_left) {
		insert_block = left_block;
	} else {
		insert_block = right_block;
	}

	/* 7. Reposition the cursor for insert and try insertion */
	page_cursor = btr_cur_get_page_cur(cursor);

	page_cur_search(insert_block, cursor->index, tuple,
			PAGE_CUR_LE, page_cursor);

	rec = page_cur_tuple_insert(page_cursor, tuple, cursor->index,
				    offsets, heap, n_ext, mtr);

#ifdef UNIV_ZIP_DEBUG
	{
		page_t*		insert_page
			= buf_block_get_frame(insert_block);

		page_zip_des_t*	insert_page_zip
			= buf_block_get_page_zip(insert_block);

		ut_a(!insert_page_zip
		     || page_zip_validate(insert_page_zip, insert_page,
					  cursor->index));
	}
#endif /* UNIV_ZIP_DEBUG */

	if (rec != NULL) {

		goto func_exit;
	}

	/* 8. If insert did not fit, try page reorganization */

	if (!btr_page_reorganize(insert_block, cursor->index, mtr)) {

		goto insert_failed;
	}

	page_cur_search(insert_block, cursor->index, tuple,
			PAGE_CUR_LE, page_cursor);
	rec = page_cur_tuple_insert(page_cursor, tuple, cursor->index,
				    offsets, heap, n_ext, mtr);

	if (rec == NULL) {
		/* The insert did not fit on the page: loop back to the
		start of the function for a new split */
insert_failed:
		/* We play safe and reset the free bits for new_page */
		if (!dict_index_is_clust(cursor->index)) {
			ibuf_reset_free_bits(new_block);
		}

		/* fprintf(stderr, "Split second round %lu\n",
		page_get_page_no(page)); */
		n_iterations++;
		ut_ad(n_iterations < 2
		      || buf_block_get_page_zip(insert_block));
		ut_ad(!insert_will_fit);

		goto func_start;
	}

func_exit:
	/* Insert fit on the page: update the free bits for the
	left and right pages in the same mtr */

	if (!dict_index_is_clust(cursor->index) && page_is_leaf(page)) {
		ibuf_update_free_bits_for_two_pages_low(
			buf_block_get_zip_size(left_block),
			left_block, right_block, mtr);
	}

#if 0
	fprintf(stderr, "Split and insert done %lu %lu\n",
		buf_block_get_page_no(left_block),
		buf_block_get_page_no(right_block));
#endif
	MONITOR_INC(MONITOR_INDEX_SPLIT);

	ut_ad(page_validate(buf_block_get_frame(left_block), cursor->index));
	ut_ad(page_validate(buf_block_get_frame(right_block), cursor->index));

	ut_ad(!rec || rec_offs_validate(rec, cursor->index, *offsets));
	return(rec);
}

#ifdef UNIV_SYNC_DEBUG
/*************************************************************//**
Removes a page from the level list of pages.
@param space	in: space where removed
@param zip_size	in: compressed page size in bytes, or 0 for uncompressed
@param page	in/out: page to remove
@param index	in: index tree
@param mtr	in/out: mini-transaction */
# define btr_level_list_remove(space,zip_size,page,index,mtr)		\
	btr_level_list_remove_func(space,zip_size,page,index,mtr)
#else /* UNIV_SYNC_DEBUG */
/*************************************************************//**
Removes a page from the level list of pages.
@param space	in: space where removed
@param zip_size	in: compressed page size in bytes, or 0 for uncompressed
@param page	in/out: page to remove
@param index	in: index tree
@param mtr	in/out: mini-transaction */
# define btr_level_list_remove(space,zip_size,page,index,mtr)		\
	btr_level_list_remove_func(space,zip_size,page,mtr)
#endif /* UNIV_SYNC_DEBUG */

/*************************************************************//**
Removes a page from the level list of pages. */
static __attribute__((nonnull))
void
btr_level_list_remove_func(
/*=======================*/
	ulint			space,	/*!< in: space where removed */
	ulint			zip_size,/*!< in: compressed page size in bytes
					or 0 for uncompressed pages */
	page_t*			page,	/*!< in/out: page to remove */
#ifdef UNIV_SYNC_DEBUG
	const dict_index_t*	index,	/*!< in: index tree */
#endif /* UNIV_SYNC_DEBUG */
	mtr_t*			mtr)	/*!< in/out: mini-transaction */
{
	ulint	prev_page_no;
	ulint	next_page_no;

	ut_ad(page && mtr);
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));
	ut_ad(space == page_get_space_id(page));
	/* Get the previous and next page numbers of page */

	prev_page_no = btr_page_get_prev(page, mtr);
	next_page_no = btr_page_get_next(page, mtr);

	/* Update page links of the level */

	if (prev_page_no != FIL_NULL) {
		buf_block_t*	prev_block
			= btr_block_get(space, zip_size, prev_page_no,
					RW_X_LATCH, index, mtr);
		page_t*		prev_page
			= buf_block_get_frame(prev_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(prev_page) == page_is_comp(page));
		ut_a(btr_page_get_next(prev_page, mtr)
		     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

		btr_page_set_next(prev_page,
				  buf_block_get_page_zip(prev_block),
				  next_page_no, mtr);
	}

	if (next_page_no != FIL_NULL) {
		buf_block_t*	next_block
			= btr_block_get(space, zip_size, next_page_no,
					RW_X_LATCH, index, mtr);
		page_t*		next_page
			= buf_block_get_frame(next_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(next_page) == page_is_comp(page));
		ut_a(btr_page_get_prev(next_page, mtr)
		     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

		btr_page_set_prev(next_page,
				  buf_block_get_page_zip(next_block),
				  prev_page_no, mtr);
	}
}

/****************************************************************//**
Writes the redo log record for setting an index record as the predefined
minimum record. */
UNIV_INLINE
void
btr_set_min_rec_mark_log(
/*=====================*/
	rec_t*	rec,	/*!< in: record */
	byte	type,	/*!< in: MLOG_COMP_REC_MIN_MARK or MLOG_REC_MIN_MARK */
	mtr_t*	mtr)	/*!< in: mtr */
{
	mlog_write_initial_log_record(rec, type, mtr);

	/* Write rec offset as a 2-byte ulint */
	mlog_catenate_ulint(mtr, page_offset(rec), MLOG_2BYTES);
}
#else /* !UNIV_HOTBACKUP */
# define btr_set_min_rec_mark_log(rec,comp,mtr) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

/****************************************************************//**
Parses the redo log record for setting an index record as the predefined
minimum record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
btr_parse_set_min_rec_mark(
/*=======================*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	ulint	comp,	/*!< in: nonzero=compact page format */
	page_t*	page,	/*!< in: page or NULL */
	mtr_t*	mtr)	/*!< in: mtr or NULL */
{
	rec_t*	rec;

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	if (page) {
		ut_a(!page_is_comp(page) == !comp);

		rec = page + mach_read_from_2(ptr);

		btr_set_min_rec_mark(rec, mtr);
	}

	return(ptr + 2);
}

/****************************************************************//**
Sets a record as the predefined minimum record. */
UNIV_INTERN
void
btr_set_min_rec_mark(
/*=================*/
	rec_t*	rec,	/*!< in: record */
	mtr_t*	mtr)	/*!< in: mtr */
{
	ulint	info_bits;

	if (page_rec_is_comp(rec)) {
		info_bits = rec_get_info_bits(rec, TRUE);

		rec_set_info_bits_new(rec, info_bits | REC_INFO_MIN_REC_FLAG);

		btr_set_min_rec_mark_log(rec, MLOG_COMP_REC_MIN_MARK, mtr);
	} else {
		info_bits = rec_get_info_bits(rec, FALSE);

		rec_set_info_bits_old(rec, info_bits | REC_INFO_MIN_REC_FLAG);

		btr_set_min_rec_mark_log(rec, MLOG_REC_MIN_MARK, mtr);
	}
}

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Deletes on the upper level the node pointer to a page. */
UNIV_INTERN
void
btr_node_ptr_delete(
/*================*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: page whose node pointer is deleted */
	mtr_t*		mtr)	/*!< in: mtr */
{
	btr_cur_t	cursor;
	ibool		compressed;
	dberr_t		err;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));

	/* Delete node pointer on father page */
	btr_page_get_father(index, block, mtr, &cursor);

	compressed = btr_cur_pessimistic_delete(&err, TRUE, &cursor,
						BTR_CREATE_FLAG, RB_NONE, mtr);
	ut_a(err == DB_SUCCESS);

	if (!compressed) {
		btr_cur_compress_if_useful(&cursor, FALSE, mtr);
	}
}

/*************************************************************//**
If page is the only on its level, this function moves its records to the
father page, thus reducing the tree height.
@return father block */
static
buf_block_t*
btr_lift_page_up(
/*=============*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: page which is the only on its level;
				must not be empty: use
				btr_discard_only_page_on_level if the last
				record from the page should be removed */
	mtr_t*		mtr)	/*!< in: mtr */
{
	buf_block_t*	father_block;
	page_t*		father_page;
	ulint		page_level;
	page_zip_des_t*	father_page_zip;
	page_t*		page		= buf_block_get_frame(block);
	ulint		root_page_no;
	buf_block_t*	blocks[BTR_MAX_LEVELS];
	ulint		n_blocks;	/*!< last used index in blocks[] */
	ulint		i;
	bool		lift_father_up;
	buf_block_t*	block_orig	= block;

	ut_ad(btr_page_get_prev(page, mtr) == FIL_NULL);
	ut_ad(btr_page_get_next(page, mtr) == FIL_NULL);
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));

	page_level = btr_page_get_level(page, mtr);
	root_page_no = dict_index_get_page(index);

	{
		btr_cur_t	cursor;
		ulint*		offsets	= NULL;
		mem_heap_t*	heap	= mem_heap_create(
			sizeof(*offsets)
			* (REC_OFFS_HEADER_SIZE + 1 + 1 + index->n_fields));
		buf_block_t*	b;

		offsets = btr_page_get_father_block(offsets, heap, index,
						    block, mtr, &cursor);
		father_block = btr_cur_get_block(&cursor);
		father_page_zip = buf_block_get_page_zip(father_block);
		father_page = buf_block_get_frame(father_block);

		n_blocks = 0;

		/* Store all ancestor pages so we can reset their
		levels later on.  We have to do all the searches on
		the tree now because later on, after we've replaced
		the first level, the tree is in an inconsistent state
		and can not be searched. */
		for (b = father_block;
		     buf_block_get_page_no(b) != root_page_no; ) {
			ut_a(n_blocks < BTR_MAX_LEVELS);

			offsets = btr_page_get_father_block(offsets, heap,
							    index, b,
							    mtr, &cursor);

			blocks[n_blocks++] = b = btr_cur_get_block(&cursor);
		}

		lift_father_up = (n_blocks && page_level == 0);
		if (lift_father_up) {
			/* The father page also should be the only on its level (not
			root). We should lift up the father page at first.
			Because the leaf page should be lifted up only for root page.
			The freeing page is based on page_level (==0 or !=0)
			to choose segment. If the page_level is changed ==0 from !=0,
			later freeing of the page doesn't find the page allocation
			to be freed.*/

			block = father_block;
			page = buf_block_get_frame(block);
			page_level = btr_page_get_level(page, mtr);

			ut_ad(btr_page_get_prev(page, mtr) == FIL_NULL);
			ut_ad(btr_page_get_next(page, mtr) == FIL_NULL);
			ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));

			father_block = blocks[0];
			father_page_zip = buf_block_get_page_zip(father_block);
			father_page = buf_block_get_frame(father_block);
		}

		mem_heap_free(heap);
	}

	btr_search_drop_page_hash_index(block);

	/* Make the father empty */
	btr_page_empty(father_block, father_page_zip, index, page_level, mtr);
	page_level++;

	/* Copy the records to the father page one by one. */
	if (0
#ifdef UNIV_ZIP_COPY
	    || father_page_zip
#endif /* UNIV_ZIP_COPY */
	    || !page_copy_rec_list_end(father_block, block,
				       page_get_infimum_rec(page),
				       index, mtr)) {
		const page_zip_des_t*	page_zip
			= buf_block_get_page_zip(block);
		ut_a(father_page_zip);
		ut_a(page_zip);

		/* Copy the page byte for byte. */
		page_zip_copy_recs(father_page_zip, father_page,
				   page_zip, page, index, mtr);

		/* Update the lock table and possible hash index. */

		lock_move_rec_list_end(father_block, block,
				       page_get_infimum_rec(page));

		btr_search_move_or_delete_hash_entries(father_block, block,
						       index);
	}

	btr_blob_dbg_remove(page, index, "btr_lift_page_up");
	lock_update_copy_and_discard(father_block, block);

	/* Go upward to root page, decrementing levels by one. */
	for (i = lift_father_up ? 1 : 0; i < n_blocks; i++, page_level++) {
		page_t*		page	= buf_block_get_frame(blocks[i]);
		page_zip_des_t*	page_zip= buf_block_get_page_zip(blocks[i]);

		ut_ad(btr_page_get_level(page, mtr) == page_level + 1);

		btr_page_set_level(page, page_zip, page_level, mtr);
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
	}

	/* Free the file page */
	btr_page_free(index, block, mtr);

	/* We play it safe and reset the free bits for the father */
	if (!dict_index_is_clust(index)) {
		ibuf_reset_free_bits(father_block);
	}
	ut_ad(page_validate(father_page, index));
	ut_ad(btr_check_node_ptr(index, father_block, mtr));

	return(lift_father_up ? block_orig : father_block);
}

/*************************************************************//**
Tries to merge the page first to the left immediate brother if such a
brother exists, and the node pointers to the current page and to the brother
reside on the same page. If the left brother does not satisfy these
conditions, looks at the right brother. If the page is the only one on that
level lifts the records of the page to the father page, thus reducing the
tree height. It is assumed that mtr holds an x-latch on the tree and on the
page. If cursor is on the leaf level, mtr must also hold x-latches to the
brothers, if they exist.
@return	TRUE on success */
UNIV_INTERN
ibool
btr_compress(
/*=========*/
	btr_cur_t*	cursor,	/*!< in/out: cursor on the page to merge
				or lift; the page must not be empty:
				when deleting records, use btr_discard_page()
				if the page would become empty */
	ibool		adjust,	/*!< in: TRUE if should adjust the
				cursor position even if compression occurs */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	dict_index_t*	index;
	ulint		space;
	ulint		zip_size;
	ulint		left_page_no;
	ulint		right_page_no;
	buf_block_t*	merge_block;
	page_t*		merge_page;
	page_zip_des_t*	merge_page_zip;
	ibool		is_left;
	buf_block_t*	block;
	page_t*		page;
	btr_cur_t	father_cursor;
	mem_heap_t*	heap;
	ulint*		offsets;
	ulint		data_size;
	ulint		n_recs;
	ulint		nth_rec = 0; /* remove bogus warning */
	ulint		max_ins_size;
	ulint		max_ins_size_reorg;

	block = btr_cur_get_block(cursor);
	page = btr_cur_get_page(cursor);
	index = btr_cur_get_index(cursor);

	btr_assert_not_corrupted(block, index);

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);

	left_page_no = btr_page_get_prev(page, mtr);
	right_page_no = btr_page_get_next(page, mtr);

#if 0
	fprintf(stderr, "Merge left page %lu right %lu \n",
		left_page_no, right_page_no);
#endif

	heap = mem_heap_create(100);
	offsets = btr_page_get_father_block(NULL, heap, index, block, mtr,
					    &father_cursor);

	if (adjust) {
		nth_rec = page_rec_get_n_recs_before(btr_cur_get_rec(cursor));
		ut_ad(nth_rec > 0);
	}

	/* Decide the page to which we try to merge and which will inherit
	the locks */

	is_left = left_page_no != FIL_NULL;

	if (is_left) {

		merge_block = btr_block_get(space, zip_size, left_page_no,
					    RW_X_LATCH, index, mtr);
		merge_page = buf_block_get_frame(merge_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(btr_page_get_next(merge_page, mtr)
		     == buf_block_get_page_no(block));
#endif /* UNIV_BTR_DEBUG */
	} else if (right_page_no != FIL_NULL) {

		merge_block = btr_block_get(space, zip_size, right_page_no,
					    RW_X_LATCH, index, mtr);
		merge_page = buf_block_get_frame(merge_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(btr_page_get_prev(merge_page, mtr)
		     == buf_block_get_page_no(block));
#endif /* UNIV_BTR_DEBUG */
	} else {
		/* The page is the only one on the level, lift the records
		to the father */

		merge_block = btr_lift_page_up(index, block, mtr);
		goto func_exit;
	}

	n_recs = page_get_n_recs(page);
	data_size = page_get_data_size(page);
#ifdef UNIV_BTR_DEBUG
	ut_a(page_is_comp(merge_page) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */

	max_ins_size_reorg = page_get_max_insert_size_after_reorganize(
		merge_page, n_recs);
	if (data_size > max_ins_size_reorg) {

		/* No space for merge */
err_exit:
		/* We play it safe and reset the free bits. */
		if (zip_size
		    && page_is_leaf(merge_page)
		    && !dict_index_is_clust(index)) {
			ibuf_reset_free_bits(merge_block);
		}

		mem_heap_free(heap);
		return(FALSE);
	}

	/* If compression padding tells us that merging will result in
	too packed up page i.e.: which is likely to cause compression
	failure then don't merge the pages. */
	if (zip_size && page_is_leaf(merge_page)
	    && (page_get_data_size(merge_page) + data_size
		>= dict_index_zip_pad_optimal_page_size(index))) {

		goto err_exit;
	}

	ut_ad(page_validate(merge_page, index));

	max_ins_size = page_get_max_insert_size(merge_page, n_recs);

	if (data_size > max_ins_size) {

		/* We have to reorganize merge_page */

		if (!btr_page_reorganize(merge_block, index, mtr)) {

			goto err_exit;
		}

		max_ins_size = page_get_max_insert_size(merge_page, n_recs);

		ut_ad(page_validate(merge_page, index));
		ut_ad(max_ins_size == max_ins_size_reorg);

		if (data_size > max_ins_size) {

			/* Add fault tolerance, though this should
			never happen */

			goto err_exit;
		}
	}

	merge_page_zip = buf_block_get_page_zip(merge_block);
#ifdef UNIV_ZIP_DEBUG
	if (merge_page_zip) {
		const page_zip_des_t*	page_zip
			= buf_block_get_page_zip(block);
		ut_a(page_zip);
		ut_a(page_zip_validate(merge_page_zip, merge_page, index));
		ut_a(page_zip_validate(page_zip, page, index));
	}
#endif /* UNIV_ZIP_DEBUG */

	/* Move records to the merge page */
	if (is_left) {
		rec_t*	orig_pred = page_copy_rec_list_start(
			merge_block, block, page_get_supremum_rec(page),
			index, mtr);

		if (!orig_pred) {
			goto err_exit;
		}

		btr_search_drop_page_hash_index(block);

		/* Remove the page from the level list */
		btr_level_list_remove(space, zip_size, page, index, mtr);

		btr_node_ptr_delete(index, block, mtr);
		lock_update_merge_left(merge_block, orig_pred, block);

		if (adjust) {
			nth_rec += page_rec_get_n_recs_before(orig_pred);
		}
	} else {
		rec_t*		orig_succ;
#ifdef UNIV_BTR_DEBUG
		byte		fil_page_prev[4];
#endif /* UNIV_BTR_DEBUG */

		if (merge_page_zip) {
			/* The function page_zip_compress(), which will be
			invoked by page_copy_rec_list_end() below,
			requires that FIL_PAGE_PREV be FIL_NULL.
			Clear the field, but prepare to restore it. */
#ifdef UNIV_BTR_DEBUG
			memcpy(fil_page_prev, merge_page + FIL_PAGE_PREV, 4);
#endif /* UNIV_BTR_DEBUG */
#if FIL_NULL != 0xffffffff
# error "FIL_NULL != 0xffffffff"
#endif
			memset(merge_page + FIL_PAGE_PREV, 0xff, 4);
		}

		orig_succ = page_copy_rec_list_end(merge_block, block,
						   page_get_infimum_rec(page),
						   cursor->index, mtr);

		if (!orig_succ) {
			ut_a(merge_page_zip);
#ifdef UNIV_BTR_DEBUG
			/* FIL_PAGE_PREV was restored from merge_page_zip. */
			ut_a(!memcmp(fil_page_prev,
				     merge_page + FIL_PAGE_PREV, 4));
#endif /* UNIV_BTR_DEBUG */
			goto err_exit;
		}

		btr_search_drop_page_hash_index(block);

#ifdef UNIV_BTR_DEBUG
		if (merge_page_zip) {
			/* Restore FIL_PAGE_PREV in order to avoid an assertion
			failure in btr_level_list_remove(), which will set
			the field again to FIL_NULL.  Even though this makes
			merge_page and merge_page_zip inconsistent for a
			split second, it is harmless, because the pages
			are X-latched. */
			memcpy(merge_page + FIL_PAGE_PREV, fil_page_prev, 4);
		}
#endif /* UNIV_BTR_DEBUG */

		/* Remove the page from the level list */
		btr_level_list_remove(space, zip_size, page, index, mtr);

		/* Replace the address of the old child node (= page) with the
		address of the merge page to the right */

		btr_node_ptr_set_child_page_no(
			btr_cur_get_rec(&father_cursor),
			btr_cur_get_page_zip(&father_cursor),
			offsets, right_page_no, mtr);
		btr_node_ptr_delete(index, merge_block, mtr);

		lock_update_merge_right(merge_block, orig_succ, block);
	}

	btr_blob_dbg_remove(page, index, "btr_compress");

	if (!dict_index_is_clust(index) && page_is_leaf(merge_page)) {
		/* Update the free bits of the B-tree page in the
		insert buffer bitmap.  This has to be done in a
		separate mini-transaction that is committed before the
		main mini-transaction.  We cannot update the insert
		buffer bitmap in this mini-transaction, because
		btr_compress() can be invoked recursively without
		committing the mini-transaction in between.  Since
		insert buffer bitmap pages have a lower rank than
		B-tree pages, we must not access other pages in the
		same mini-transaction after accessing an insert buffer
		bitmap page. */

		/* The free bits in the insert buffer bitmap must
		never exceed the free space on a page.  It is safe to
		decrement or reset the bits in the bitmap in a
		mini-transaction that is committed before the
		mini-transaction that affects the free space. */

		/* It is unsafe to increment the bits in a separately
		committed mini-transaction, because in crash recovery,
		the free bits could momentarily be set too high. */

		if (zip_size) {
			/* Because the free bits may be incremented
			and we cannot update the insert buffer bitmap
			in the same mini-transaction, the only safe
			thing we can do here is the pessimistic
			approach: reset the free bits. */
			ibuf_reset_free_bits(merge_block);
		} else {
			/* On uncompressed pages, the free bits will
			never increase here.  Thus, it is safe to
			write the bits accurately in a separate
			mini-transaction. */
			ibuf_update_free_bits_if_full(merge_block,
						      UNIV_PAGE_SIZE,
						      ULINT_UNDEFINED);
		}
	}

	ut_ad(page_validate(merge_page, index));
#ifdef UNIV_ZIP_DEBUG
	ut_a(!merge_page_zip || page_zip_validate(merge_page_zip, merge_page,
						  index));
#endif /* UNIV_ZIP_DEBUG */

	/* Free the file page */
	btr_page_free(index, block, mtr);

	ut_ad(btr_check_node_ptr(index, merge_block, mtr));
func_exit:
	mem_heap_free(heap);

	if (adjust) {
		ut_ad(nth_rec > 0);
		btr_cur_position(
			index,
			page_rec_get_nth(merge_block->frame, nth_rec),
			merge_block, cursor);
	}

	return(TRUE);
}

/*************************************************************//**
Discards a page that is the only page on its level.  This will empty
the whole B-tree, leaving just an empty root page.  This function
should never be reached, because btr_compress(), which is invoked in
delete operations, calls btr_lift_page_up() to flatten the B-tree. */
static
void
btr_discard_only_page_on_level(
/*===========================*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: page which is the only on its level */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint		page_level = 0;
	trx_id_t	max_trx_id;

	/* Save the PAGE_MAX_TRX_ID from the leaf page. */
	max_trx_id = page_get_max_trx_id(buf_block_get_frame(block));

	while (buf_block_get_page_no(block) != dict_index_get_page(index)) {
		btr_cur_t	cursor;
		buf_block_t*	father;
		const page_t*	page	= buf_block_get_frame(block);

		ut_a(page_get_n_recs(page) == 1);
		ut_a(page_level == btr_page_get_level(page, mtr));
		ut_a(btr_page_get_prev(page, mtr) == FIL_NULL);
		ut_a(btr_page_get_next(page, mtr) == FIL_NULL);

		ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
		btr_search_drop_page_hash_index(block);

		btr_page_get_father(index, block, mtr, &cursor);
		father = btr_cur_get_block(&cursor);

		lock_update_discard(father, PAGE_HEAP_NO_SUPREMUM, block);

		/* Free the file page */
		btr_page_free(index, block, mtr);

		block = father;
		page_level++;
	}

	/* block is the root page, which must be empty, except
	for the node pointer to the (now discarded) block(s). */

#ifdef UNIV_BTR_DEBUG
	if (!dict_index_is_ibuf(index)) {
		const page_t*	root	= buf_block_get_frame(block);
		const ulint	space	= dict_index_get_space(index);
		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
					    + root, space));
		ut_a(btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
					    + root, space));
	}
#endif /* UNIV_BTR_DEBUG */

	btr_page_empty(block, buf_block_get_page_zip(block), index, 0, mtr);

	if (!dict_index_is_clust(index)) {
		/* We play it safe and reset the free bits for the root */
		ibuf_reset_free_bits(block);

		if (page_is_leaf(buf_block_get_frame(block))) {
			ut_a(max_trx_id);
			page_set_max_trx_id(block,
					    buf_block_get_page_zip(block),
					    max_trx_id, mtr);
		}
	}
}

/*************************************************************//**
Discards a page from a B-tree. This is used to remove the last record from
a B-tree page: the whole page must be removed at the same time. This cannot
be used for the root page, which is allowed to be empty. */
UNIV_INTERN
void
btr_discard_page(
/*=============*/
	btr_cur_t*	cursor,	/*!< in: cursor on the page to discard: not on
				the root page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dict_index_t*	index;
	ulint		space;
	ulint		zip_size;
	ulint		left_page_no;
	ulint		right_page_no;
	buf_block_t*	merge_block;
	page_t*		merge_page;
	buf_block_t*	block;
	page_t*		page;
	rec_t*		node_ptr;

	block = btr_cur_get_block(cursor);
	index = btr_cur_get_index(cursor);

	ut_ad(dict_index_get_page(index) != buf_block_get_page_no(block));
	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);

	/* Decide the page which will inherit the locks */

	left_page_no = btr_page_get_prev(buf_block_get_frame(block), mtr);
	right_page_no = btr_page_get_next(buf_block_get_frame(block), mtr);

	if (left_page_no != FIL_NULL) {
		merge_block = btr_block_get(space, zip_size, left_page_no,
					    RW_X_LATCH, index, mtr);
		merge_page = buf_block_get_frame(merge_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(btr_page_get_next(merge_page, mtr)
		     == buf_block_get_page_no(block));
#endif /* UNIV_BTR_DEBUG */
	} else if (right_page_no != FIL_NULL) {
		merge_block = btr_block_get(space, zip_size, right_page_no,
					    RW_X_LATCH, index, mtr);
		merge_page = buf_block_get_frame(merge_block);
#ifdef UNIV_BTR_DEBUG
		ut_a(btr_page_get_prev(merge_page, mtr)
		     == buf_block_get_page_no(block));
#endif /* UNIV_BTR_DEBUG */
	} else {
		btr_discard_only_page_on_level(index, block, mtr);

		return;
	}

	page = buf_block_get_frame(block);
	ut_a(page_is_comp(merge_page) == page_is_comp(page));
	btr_search_drop_page_hash_index(block);

	if (left_page_no == FIL_NULL && !page_is_leaf(page)) {

		/* We have to mark the leftmost node pointer on the right
		side page as the predefined minimum record */
		node_ptr = page_rec_get_next(page_get_infimum_rec(merge_page));

		ut_ad(page_rec_is_user_rec(node_ptr));

		/* This will make page_zip_validate() fail on merge_page
		until btr_level_list_remove() completes.  This is harmless,
		because everything will take place within a single
		mini-transaction and because writing to the redo log
		is an atomic operation (performed by mtr_commit()). */
		btr_set_min_rec_mark(node_ptr, mtr);
	}

	btr_node_ptr_delete(index, block, mtr);

	/* Remove the page from the level list */
	btr_level_list_remove(space, zip_size, page, index, mtr);
#ifdef UNIV_ZIP_DEBUG
	{
		page_zip_des_t*	merge_page_zip
			= buf_block_get_page_zip(merge_block);
		ut_a(!merge_page_zip
		     || page_zip_validate(merge_page_zip, merge_page, index));
	}
#endif /* UNIV_ZIP_DEBUG */

	if (left_page_no != FIL_NULL) {
		lock_update_discard(merge_block, PAGE_HEAP_NO_SUPREMUM,
				    block);
	} else {
		lock_update_discard(merge_block,
				    lock_get_min_heap_no(merge_block),
				    block);
	}

	btr_blob_dbg_remove(page, index, "btr_discard_page");

	/* Free the file page */
	btr_page_free(index, block, mtr);

	ut_ad(btr_check_node_ptr(index, merge_block, mtr));
}

#ifdef UNIV_BTR_PRINT
/*************************************************************//**
Prints size info of a B-tree. */
UNIV_INTERN
void
btr_print_size(
/*===========*/
	dict_index_t*	index)	/*!< in: index tree */
{
	page_t*		root;
	fseg_header_t*	seg;
	mtr_t		mtr;

	if (dict_index_is_ibuf(index)) {
		fputs("Sorry, cannot print info of an ibuf tree:"
		      " use ibuf functions\n", stderr);

		return;
	}

	mtr_start(&mtr);

	root = btr_root_get(index, &mtr);

	seg = root + PAGE_HEADER + PAGE_BTR_SEG_TOP;

	fputs("INFO OF THE NON-LEAF PAGE SEGMENT\n", stderr);
	fseg_print(seg, &mtr);

	if (!dict_index_is_univ(index)) {

		seg = root + PAGE_HEADER + PAGE_BTR_SEG_LEAF;

		fputs("INFO OF THE LEAF PAGE SEGMENT\n", stderr);
		fseg_print(seg, &mtr);
	}

	mtr_commit(&mtr);
}

/************************************************************//**
Prints recursively index tree pages. */
static
void
btr_print_recursive(
/*================*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: index page */
	ulint		width,	/*!< in: print this many entries from start
				and end */
	mem_heap_t**	heap,	/*!< in/out: heap for rec_get_offsets() */
	ulint**		offsets,/*!< in/out: buffer for rec_get_offsets() */
	mtr_t*		mtr)	/*!< in: mtr */
{
	const page_t*	page	= buf_block_get_frame(block);
	page_cur_t	cursor;
	ulint		n_recs;
	ulint		i	= 0;
	mtr_t		mtr2;

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	fprintf(stderr, "NODE ON LEVEL %lu page number %lu\n",
		(ulong) btr_page_get_level(page, mtr),
		(ulong) buf_block_get_page_no(block));

	page_print(block, index, width, width);

	n_recs = page_get_n_recs(page);

	page_cur_set_before_first(block, &cursor);
	page_cur_move_to_next(&cursor);

	while (!page_cur_is_after_last(&cursor)) {

		if (page_is_leaf(page)) {

			/* If this is the leaf level, do nothing */

		} else if ((i <= width) || (i >= n_recs - width)) {

			const rec_t*	node_ptr;

			mtr_start(&mtr2);

			node_ptr = page_cur_get_rec(&cursor);

			*offsets = rec_get_offsets(node_ptr, index, *offsets,
						   ULINT_UNDEFINED, heap);
			btr_print_recursive(index,
					    btr_node_ptr_get_child(node_ptr,
								   index,
								   *offsets,
								   &mtr2),
					    width, heap, offsets, &mtr2);
			mtr_commit(&mtr2);
		}

		page_cur_move_to_next(&cursor);
		i++;
	}
}

/**************************************************************//**
Prints directories and other info of all nodes in the tree. */
UNIV_INTERN
void
btr_print_index(
/*============*/
	dict_index_t*	index,	/*!< in: index */
	ulint		width)	/*!< in: print this many entries from start
				and end */
{
	mtr_t		mtr;
	buf_block_t*	root;
	mem_heap_t*	heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets	= offsets_;
	rec_offs_init(offsets_);

	fputs("--------------------------\n"
	      "INDEX TREE PRINT\n", stderr);

	mtr_start(&mtr);

	root = btr_root_block_get(index, RW_X_LATCH, &mtr);

	btr_print_recursive(index, root, width, &heap, &offsets, &mtr);
	if (heap) {
		mem_heap_free(heap);
	}

	mtr_commit(&mtr);

	btr_validate_index(index, 0);
}
#endif /* UNIV_BTR_PRINT */

#ifdef UNIV_DEBUG
/************************************************************//**
Checks that the node pointer to a page is appropriate.
@return	TRUE */
UNIV_INTERN
ibool
btr_check_node_ptr(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	buf_block_t*	block,	/*!< in: index page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	mem_heap_t*	heap;
	dtuple_t*	tuple;
	ulint*		offsets;
	btr_cur_t	cursor;
	page_t*		page = buf_block_get_frame(block);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	if (dict_index_get_page(index) == buf_block_get_page_no(block)) {

		return(TRUE);
	}

	heap = mem_heap_create(256);
	offsets = btr_page_get_father_block(NULL, heap, index, block, mtr,
					    &cursor);

	if (page_is_leaf(page)) {

		goto func_exit;
	}

	tuple = dict_index_build_node_ptr(
		index, page_rec_get_next(page_get_infimum_rec(page)), 0, heap,
		btr_page_get_level(page, mtr));

	ut_a(!cmp_dtuple_rec(tuple, btr_cur_get_rec(&cursor), offsets));
func_exit:
	mem_heap_free(heap);

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/************************************************************//**
Display identification information for a record. */
static
void
btr_index_rec_validate_report(
/*==========================*/
	const page_t*		page,	/*!< in: index page */
	const rec_t*		rec,	/*!< in: index record */
	const dict_index_t*	index)	/*!< in: index */
{
	fputs("InnoDB: Record in ", stderr);
	dict_index_name_print(stderr, NULL, index);
	fprintf(stderr, ", page %lu, at offset %lu\n",
		page_get_page_no(page), (ulint) page_offset(rec));
}

/************************************************************//**
Checks the size and number of fields in a record based on the definition of
the index.
@return	TRUE if ok */
UNIV_INTERN
ibool
btr_index_rec_validate(
/*===================*/
	const rec_t*		rec,		/*!< in: index record */
	const dict_index_t*	index,		/*!< in: index */
	ibool			dump_on_error)	/*!< in: TRUE if the function
						should print hex dump of record
						and page on error */
{
	ulint		len;
	ulint		n;
	ulint		i;
	const page_t*	page;
	mem_heap_t*	heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets	= offsets_;
	rec_offs_init(offsets_);

	page = page_align(rec);

	if (dict_index_is_univ(index)) {
		/* The insert buffer index tree can contain records from any
		other index: we cannot check the number of fields or
		their length */

		return(TRUE);
	}

	if ((ibool)!!page_is_comp(page) != dict_table_is_comp(index->table)) {
		btr_index_rec_validate_report(page, rec, index);
		fprintf(stderr, "InnoDB: compact flag=%lu, should be %lu\n",
			(ulong) !!page_is_comp(page),
			(ulong) dict_table_is_comp(index->table));

		return(FALSE);
	}

	n = dict_index_get_n_fields(index);

	if (!page_is_comp(page) && rec_get_n_fields_old(rec) != n) {
		btr_index_rec_validate_report(page, rec, index);
		fprintf(stderr, "InnoDB: has %lu fields, should have %lu\n",
			(ulong) rec_get_n_fields_old(rec), (ulong) n);

		if (dump_on_error) {
			buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);

			fputs("InnoDB: corrupt record ", stderr);
			rec_print_old(stderr, rec);
			putc('\n', stderr);
		}
		return(FALSE);
	}

	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

	for (i = 0; i < n; i++) {
		ulint	fixed_size = dict_col_get_fixed_size(
			dict_index_get_nth_col(index, i), page_is_comp(page));

		rec_get_nth_field_offs(offsets, i, &len);

		/* Note that if fixed_size != 0, it equals the
		length of a fixed-size column in the clustered index.
		A prefix index of the column is of fixed, but different
		length.  When fixed_size == 0, prefix_len is the maximum
		length of the prefix index column. */

		if ((dict_index_get_nth_field(index, i)->prefix_len == 0
		     && len != UNIV_SQL_NULL && fixed_size
		     && len != fixed_size)
		    || (dict_index_get_nth_field(index, i)->prefix_len > 0
			&& len != UNIV_SQL_NULL
			&& len
			> dict_index_get_nth_field(index, i)->prefix_len)) {

			btr_index_rec_validate_report(page, rec, index);
			fprintf(stderr,
				"InnoDB: field %lu len is %lu,"
				" should be %lu\n",
				(ulong) i, (ulong) len, (ulong) fixed_size);

			if (dump_on_error) {
				buf_page_print(page, 0,
					       BUF_PAGE_PRINT_NO_CRASH);

				fputs("InnoDB: corrupt record ", stderr);
				rec_print_new(stderr, rec, offsets);
				putc('\n', stderr);
			}
			if (heap) {
				mem_heap_free(heap);
			}
			return(FALSE);
		}
	}

	if (heap) {
		mem_heap_free(heap);
	}
	return(TRUE);
}

/************************************************************//**
Checks the size and number of fields in records based on the definition of
the index.
@return	TRUE if ok */
static
ibool
btr_index_page_validate(
/*====================*/
	buf_block_t*	block,	/*!< in: index page */
	dict_index_t*	index)	/*!< in: index */
{
	page_cur_t	cur;
	ibool		ret	= TRUE;
#ifndef DBUG_OFF
	ulint		nth	= 1;
#endif /* !DBUG_OFF */

	page_cur_set_before_first(block, &cur);

	/* Directory slot 0 should only contain the infimum record. */
	DBUG_EXECUTE_IF("check_table_rec_next",
			ut_a(page_rec_get_nth_const(
				     page_cur_get_page(&cur), 0)
			     == cur.rec);
			ut_a(page_dir_slot_get_n_owned(
				     page_dir_get_nth_slot(
					     page_cur_get_page(&cur), 0))
			     == 1););

	page_cur_move_to_next(&cur);

	for (;;) {
		if (page_cur_is_after_last(&cur)) {

			break;
		}

		if (!btr_index_rec_validate(cur.rec, index, TRUE)) {

			return(FALSE);
		}

		/* Verify that page_rec_get_nth_const() is correctly
		retrieving each record. */
		DBUG_EXECUTE_IF("check_table_rec_next",
				ut_a(cur.rec == page_rec_get_nth_const(
					     page_cur_get_page(&cur),
					     page_rec_get_n_recs_before(
						     cur.rec)));
				ut_a(nth++ == page_rec_get_n_recs_before(
					     cur.rec)););

		page_cur_move_to_next(&cur);
	}

	return(ret);
}

/************************************************************//**
Report an error on one page of an index tree. */
static
void
btr_validate_report1(
/*=================*/
	dict_index_t*		index,	/*!< in: index */
	ulint			level,	/*!< in: B-tree level */
	const buf_block_t*	block)	/*!< in: index page */
{
	fprintf(stderr, "InnoDB: Error in page %lu of ",
		buf_block_get_page_no(block));
	dict_index_name_print(stderr, NULL, index);
	if (level) {
		fprintf(stderr, ", index tree level %lu", level);
	}
	putc('\n', stderr);
}

/************************************************************//**
Report an error on two pages of an index tree. */
static
void
btr_validate_report2(
/*=================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			level,	/*!< in: B-tree level */
	const buf_block_t*	block1,	/*!< in: first index page */
	const buf_block_t*	block2)	/*!< in: second index page */
{
	fprintf(stderr, "InnoDB: Error in pages %lu and %lu of ",
		buf_block_get_page_no(block1),
		buf_block_get_page_no(block2));
	dict_index_name_print(stderr, NULL, index);
	if (level) {
		fprintf(stderr, ", index tree level %lu", level);
	}
	putc('\n', stderr);
}

/************************************************************//**
Validates index tree level.
@return	TRUE if ok */
static
bool
btr_validate_level(
/*===============*/
	dict_index_t*	index,	/*!< in: index tree */
	const trx_t*	trx,	/*!< in: transaction or NULL */
	ulint		level)	/*!< in: level number */
{
	ulint		space;
	ulint		space_flags;
	ulint		zip_size;
	buf_block_t*	block;
	page_t*		page;
	buf_block_t*	right_block = 0; /* remove warning */
	page_t*		right_page = 0; /* remove warning */
	page_t*		father_page;
	btr_cur_t	node_cur;
	btr_cur_t	right_node_cur;
	rec_t*		rec;
	ulint		right_page_no;
	ulint		left_page_no;
	page_cur_t	cursor;
	dtuple_t*	node_ptr_tuple;
	bool		ret	= true;
	mtr_t		mtr;
	mem_heap_t*	heap	= mem_heap_create(256);
	fseg_header_t*	seg;
	ulint*		offsets	= NULL;
	ulint*		offsets2= NULL;
#ifdef UNIV_ZIP_DEBUG
	page_zip_des_t*	page_zip;
#endif /* UNIV_ZIP_DEBUG */

	mtr_start(&mtr);

	mtr_x_lock(dict_index_get_lock(index), &mtr);

	block = btr_root_block_get(index, RW_X_LATCH, &mtr);
	page = buf_block_get_frame(block);
	seg = page + PAGE_HEADER + PAGE_BTR_SEG_TOP;

	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);

	fil_space_get_latch(space, &space_flags);

	if (zip_size != dict_tf_get_zip_size(space_flags)) {

		ib_logf(IB_LOG_LEVEL_WARN,
			"Flags mismatch: table=%lu, tablespace=%lu",
			(ulint) index->table->flags, (ulint) space_flags);

		mtr_commit(&mtr);

		return(false);
	}

	while (level != btr_page_get_level(page, &mtr)) {
		const rec_t*	node_ptr;

		if (fseg_page_is_free(seg,
				      block->page.space, block->page.offset)) {

			btr_validate_report1(index, level, block);

			ib_logf(IB_LOG_LEVEL_WARN, "page is free");

			ret = false;
		}

		ut_a(space == buf_block_get_space(block));
		ut_a(space == page_get_space_id(page));
#ifdef UNIV_ZIP_DEBUG
		page_zip = buf_block_get_page_zip(block);
		ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */
		ut_a(!page_is_leaf(page));

		page_cur_set_before_first(block, &cursor);
		page_cur_move_to_next(&cursor);

		node_ptr = page_cur_get_rec(&cursor);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					  ULINT_UNDEFINED, &heap);
		block = btr_node_ptr_get_child(node_ptr, index, offsets, &mtr);
		page = buf_block_get_frame(block);
	}

	/* Now we are on the desired level. Loop through the pages on that
	level. */

	if (level == 0) {
		/* Leaf pages are managed in their own file segment. */
		seg -= PAGE_BTR_SEG_TOP - PAGE_BTR_SEG_LEAF;
	}

loop:
	mem_heap_empty(heap);
	offsets = offsets2 = NULL;
	mtr_x_lock(dict_index_get_lock(index), &mtr);

#ifdef UNIV_ZIP_DEBUG
	page_zip = buf_block_get_page_zip(block);
	ut_a(!page_zip || page_zip_validate(page_zip, page, index));
#endif /* UNIV_ZIP_DEBUG */

	ut_a(block->page.space == space);

	if (fseg_page_is_free(seg, block->page.space, block->page.offset)) {

		btr_validate_report1(index, level, block);

		ib_logf(IB_LOG_LEVEL_WARN, "Page is marked as free");
		ret = false;

	} else if (btr_page_get_index_id(page) != index->id) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Page index id " IB_ID_FMT " != data dictionary "
			"index id " IB_ID_FMT,
			btr_page_get_index_id(page), index->id);

		ret = false;

	} else if (!page_validate(page, index)) {

		btr_validate_report1(index, level, block);
		ret = false;

	} else if (level == 0 && !btr_index_page_validate(block, index)) {

		/* We are on level 0. Check that the records have the right
		number of fields, and field lengths are right. */

		ret = false;
	}

	ut_a(btr_page_get_level(page, &mtr) == level);

	right_page_no = btr_page_get_next(page, &mtr);
	left_page_no = btr_page_get_prev(page, &mtr);

	ut_a(page_get_n_recs(page) > 0 || (level == 0
					   && page_get_page_no(page)
					   == dict_index_get_page(index)));

	if (right_page_no != FIL_NULL) {
		const rec_t*	right_rec;
		right_block = btr_block_get(space, zip_size, right_page_no,
					    RW_X_LATCH, index, &mtr);
		right_page = buf_block_get_frame(right_block);
		if (btr_page_get_prev(right_page, &mtr)
		    != page_get_page_no(page)) {

			btr_validate_report2(index, level, block, right_block);
			fputs("InnoDB: broken FIL_PAGE_NEXT"
			      " or FIL_PAGE_PREV links\n", stderr);
			buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);
			buf_page_print(right_page, 0, BUF_PAGE_PRINT_NO_CRASH);

			ret = false;
		}

		if (page_is_comp(right_page) != page_is_comp(page)) {
			btr_validate_report2(index, level, block, right_block);
			fputs("InnoDB: 'compact' flag mismatch\n", stderr);
			buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);
			buf_page_print(right_page, 0, BUF_PAGE_PRINT_NO_CRASH);

			ret = false;

			goto node_ptr_fails;
		}

		rec = page_rec_get_prev(page_get_supremum_rec(page));
		right_rec = page_rec_get_next(page_get_infimum_rec(
						      right_page));
		offsets = rec_get_offsets(rec, index,
					  offsets, ULINT_UNDEFINED, &heap);
		offsets2 = rec_get_offsets(right_rec, index,
					   offsets2, ULINT_UNDEFINED, &heap);
		if (cmp_rec_rec(rec, right_rec, offsets, offsets2,
			        index) >= 0) {

			btr_validate_report2(index, level, block, right_block);

			fputs("InnoDB: records in wrong order"
			      " on adjacent pages\n", stderr);

			buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);
			buf_page_print(right_page, 0, BUF_PAGE_PRINT_NO_CRASH);

			fputs("InnoDB: record ", stderr);
			rec = page_rec_get_prev(page_get_supremum_rec(page));
			rec_print(stderr, rec, index);
			putc('\n', stderr);
			fputs("InnoDB: record ", stderr);
			rec = page_rec_get_next(
				page_get_infimum_rec(right_page));
			rec_print(stderr, rec, index);
			putc('\n', stderr);

			ret = false;
		}
	}

	if (level > 0 && left_page_no == FIL_NULL) {
		ut_a(REC_INFO_MIN_REC_FLAG & rec_get_info_bits(
			     page_rec_get_next(page_get_infimum_rec(page)),
			     page_is_comp(page)));
	}

	if (buf_block_get_page_no(block) != dict_index_get_page(index)) {

		/* Check father node pointers */

		rec_t*	node_ptr;

		offsets = btr_page_get_father_block(offsets, heap, index,
						    block, &mtr, &node_cur);
		father_page = btr_cur_get_page(&node_cur);
		node_ptr = btr_cur_get_rec(&node_cur);

		btr_cur_position(
			index, page_rec_get_prev(page_get_supremum_rec(page)),
			block, &node_cur);
		offsets = btr_page_get_father_node_ptr(offsets, heap,
						       &node_cur, &mtr);

		if (node_ptr != btr_cur_get_rec(&node_cur)
		    || btr_node_ptr_get_child_page_no(node_ptr, offsets)
				     != buf_block_get_page_no(block)) {

			btr_validate_report1(index, level, block);

			fputs("InnoDB: node pointer to the page is wrong\n",
			      stderr);

			buf_page_print(father_page, 0, BUF_PAGE_PRINT_NO_CRASH);
			buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);

			fputs("InnoDB: node ptr ", stderr);
			rec_print(stderr, node_ptr, index);

			rec = btr_cur_get_rec(&node_cur);
			fprintf(stderr, "\n"
				"InnoDB: node ptr child page n:o %lu\n",
				(ulong) btr_node_ptr_get_child_page_no(
					rec, offsets));

			fputs("InnoDB: record on page ", stderr);
			rec_print_new(stderr, rec, offsets);
			putc('\n', stderr);
			ret = false;

			goto node_ptr_fails;
		}

		if (!page_is_leaf(page)) {
			node_ptr_tuple = dict_index_build_node_ptr(
				index,
				page_rec_get_next(page_get_infimum_rec(page)),
				0, heap, btr_page_get_level(page, &mtr));

			if (cmp_dtuple_rec(node_ptr_tuple, node_ptr,
					   offsets)) {
				const rec_t* first_rec = page_rec_get_next(
					page_get_infimum_rec(page));

				btr_validate_report1(index, level, block);

				buf_page_print(father_page, 0,
					       BUF_PAGE_PRINT_NO_CRASH);
				buf_page_print(page, 0,
					       BUF_PAGE_PRINT_NO_CRASH);

				fputs("InnoDB: Error: node ptrs differ"
				      " on levels > 0\n"
				      "InnoDB: node ptr ", stderr);
				rec_print_new(stderr, node_ptr, offsets);
				fputs("InnoDB: first rec ", stderr);
				rec_print(stderr, first_rec, index);
				putc('\n', stderr);
				ret = false;

				goto node_ptr_fails;
			}
		}

		if (left_page_no == FIL_NULL) {
			ut_a(node_ptr == page_rec_get_next(
				     page_get_infimum_rec(father_page)));
			ut_a(btr_page_get_prev(father_page, &mtr) == FIL_NULL);
		}

		if (right_page_no == FIL_NULL) {
			ut_a(node_ptr == page_rec_get_prev(
				     page_get_supremum_rec(father_page)));
			ut_a(btr_page_get_next(father_page, &mtr) == FIL_NULL);
		} else {
			const rec_t*	right_node_ptr
				= page_rec_get_next(node_ptr);

			offsets = btr_page_get_father_block(
				offsets, heap, index, right_block,
				&mtr, &right_node_cur);
			if (right_node_ptr
			    != page_get_supremum_rec(father_page)) {

				if (btr_cur_get_rec(&right_node_cur)
				    != right_node_ptr) {
					ret = false;
					fputs("InnoDB: node pointer to"
					      " the right page is wrong\n",
					      stderr);

					btr_validate_report1(index, level,
							     block);

					buf_page_print(
						father_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						right_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
				}
			} else {
				page_t*	right_father_page
					= btr_cur_get_page(&right_node_cur);

				if (btr_cur_get_rec(&right_node_cur)
				    != page_rec_get_next(
					    page_get_infimum_rec(
						    right_father_page))) {
					ret = false;
					fputs("InnoDB: node pointer 2 to"
					      " the right page is wrong\n",
					      stderr);

					btr_validate_report1(index, level,
							     block);

					buf_page_print(
						father_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						right_father_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						right_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
				}

				if (page_get_page_no(right_father_page)
				    != btr_page_get_next(father_page, &mtr)) {

					ret = false;
					fputs("InnoDB: node pointer 3 to"
					      " the right page is wrong\n",
					      stderr);

					btr_validate_report1(index, level,
							     block);

					buf_page_print(
						father_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						right_father_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					buf_page_print(
						right_page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
				}
			}
		}
	}

node_ptr_fails:
	/* Commit the mini-transaction to release the latch on 'page'.
	Re-acquire the latch on right_page, which will become 'page'
	on the next loop.  The page has already been checked. */
	mtr_commit(&mtr);

	if (trx_is_interrupted(trx)) {
		/* On interrupt, return the current status. */
	} else if (right_page_no != FIL_NULL) {

		mtr_start(&mtr);

		block = btr_block_get(
			space, zip_size, right_page_no,
			RW_X_LATCH, index, &mtr);

		page = buf_block_get_frame(block);

		goto loop;
	}

	mem_heap_free(heap);

	return(ret);
}

/**************************************************************//**
Checks the consistency of an index tree.
@return	TRUE if ok */
UNIV_INTERN
bool
btr_validate_index(
/*===============*/
	dict_index_t*	index,	/*!< in: index */
	const trx_t*	trx)	/*!< in: transaction or NULL */
{
	/* Full Text index are implemented by auxiliary tables,
	not the B-tree */
	if (dict_index_is_online_ddl(index) || (index->type & DICT_FTS)) {
		return(true);
	}

	mtr_t		mtr;

	mtr_start(&mtr);

	mtr_x_lock(dict_index_get_lock(index), &mtr);

	bool	ok = true;
	page_t*	root = btr_root_get(index, &mtr);
	ulint	n = btr_page_get_level(root, &mtr);

	for (ulint i = 0; i <= n; ++i) {

		if (!btr_validate_level(index, trx, n - i)) {
			ok = false;
			break;
		}
	}

	mtr_commit(&mtr);

	return(ok);
}

#endif /* !UNIV_HOTBACKUP */
