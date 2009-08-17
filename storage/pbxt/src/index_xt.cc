/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-09-30	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#ifndef XT_WIN
#include <strings.h>
#endif

#ifdef DRIZZLED
#include <drizzled/base.h>
#else
#include "mysql_priv.h"
#endif

#include "pthread_xt.h"
#include "memory_xt.h"
#include "index_xt.h"
#include "heap_xt.h"
#include "database_xt.h"
#include "strutil_xt.h"
#include "cache_xt.h"
#include "myxt_xt.h"
#include "trace_xt.h"
#include "table_xt.h"

#ifdef DEBUG
#define MAX_SEARCH_DEPTH			32
//#define CHECK_AND_PRINT
//#define CHECK_NODE_REFERENCE
//#define TRACE_FLUSH
#define CHECK_PRINTS_RECORD_REFERENCES
#else
#define MAX_SEARCH_DEPTH			100
#endif

#define IND_FLUSH_BUFFER_SIZE		200

typedef struct IdxStackItem {
	XTIdxItemRec			i_pos;
	xtIndexNodeID			i_branch;
} IdxStackItemRec, *IdxStackItemPtr;

typedef struct IdxBranchStack {
	int						s_top;
	IdxStackItemRec			s_elements[MAX_SEARCH_DEPTH];
} IdxBranchStackRec, *IdxBranchStackPtr;

#ifdef DEBUG
#ifdef TEST_CODE
static void idx_check_on_key(XTOpenTablePtr ot);
#endif
static u_int idx_check_index(XTOpenTablePtr ot, XTIndexPtr ind, xtBool with_lock);
#endif

static xtBool idx_insert_node(XTOpenTablePtr ot, XTIndexPtr ind, IdxBranchStackPtr stack, XTIdxKeyValuePtr key_value, xtIndexNodeID branch);
static xtBool idx_remove_lazy_deleted_item_in_node(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID current, XTIndReferencePtr iref, XTIdxKeyValuePtr key_value);

#ifdef XT_TRACK_INDEX_UPDATES

static xtBool ind_track_write(struct XTOpenTable *ot, struct XTIndex *ind, xtIndexNodeID offset, size_t size, xtWord1 *data)
{
	ot->ot_ind_reads++;
	return xt_ind_write(ot, ind, offset, size, data);
}

#define XT_IND_WRITE					ind_track_write

#else

#define XT_IND_WRITE					xt_ind_write

#endif


#ifdef CHECK_NODE_REFERENCE
#define IDX_GET_NODE_REF(t, x, o)		idx_get_node_ref(t, x, o)
#else
#define IDX_GET_NODE_REF(t, x, o)		XT_GET_NODE_REF(t, (x) - (o))
#endif

/*
 * -----------------------------------------------------------------------
 * DEBUG ACTIVITY
 */

//#define TRACK_ACTIVITY

#ifdef TRACK_ACTIVITY
#define TRACK_MAX_BLOCKS			2000

typedef struct TrackBlock {
	xtWord1				exists;
	char				*activity;
} TrackBlockRec, *TrackBlockPtr;

TrackBlockRec		blocks[TRACK_MAX_BLOCKS];

xtPublic void track_work(u_int block, char *what)
{
	int len = 0, len2;

	ASSERT_NS(block > 0 && block <= TRACK_MAX_BLOCKS);
	block--;
	if (blocks[block].activity)
		len = strlen(blocks[block].activity);
	len2 = strlen(what);
	xt_realloc_ns((void **) &blocks[block].activity, len + len2 + 1);
	memcpy(blocks[block].activity + len, what, len2 + 1);
}

static void track_block_exists(xtIndexNodeID block)
{
	if (XT_NODE_ID(block) > 0 && XT_NODE_ID(block) <= TRACK_MAX_BLOCKS)
		blocks[XT_NODE_ID(block)-1].exists = TRUE;
}

static void track_reset_missing()
{
	for (u_int i=0; i<TRACK_MAX_BLOCKS; i++)
		blocks[i].exists = FALSE;
}

static void track_dump_missing(xtIndexNodeID eof_block)
{
	for (u_int i=0; i<XT_NODE_ID(eof_block)-1; i++) {
		if (!blocks[i].exists)
			printf("block missing = %04d %s\n", i+1, blocks[i].activity);
	}
}

static void track_dump_all(u_int max_block)
{
	for (u_int i=0; i<max_block; i++) {
		if (blocks[i].exists)
			printf(" %04d %s\n", i+1, blocks[i].activity);
		else
			printf("-%04d %s\n", i+1, blocks[i].activity);
	}
}

#endif

xtPublic void xt_ind_track_dump_block(XTTableHPtr XT_UNUSED(tab), xtIndexNodeID XT_UNUSED(address))
{
#ifdef TRACK_ACTIVITY
	u_int i = XT_NODE_ID(address)-1;

	printf("BLOCK %04d %s\n", i+1, blocks[i].activity);
#endif
}

#ifdef CHECK_NODE_REFERENCE
static xtIndexNodeID idx_get_node_ref(XTTableHPtr tab, xtWord1 *ref, u_int node_ref_size)
{
	xtIndexNodeID node;

	/* Node is invalid by default: */
	XT_NODE_ID(node) = 0xFFFFEEEE;
	if (node_ref_size) {
		ref -= node_ref_size;
		node = XT_RET_NODE_ID(XT_GET_DISK_4(ref));
		if (node >= tab->tab_ind_eof) {
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_CORRUPTED, tab->tab_name);
		}
	}
	return node;
}
#endif

/*
 * -----------------------------------------------------------------------
 * Stack functions
 */

static void idx_newstack(IdxBranchStackPtr stack)
{
	stack->s_top = 0;
}

static xtBool idx_push(IdxBranchStackPtr stack, xtIndexNodeID n, XTIdxItemPtr pos)
{
	if (stack->s_top == MAX_SEARCH_DEPTH) {
		xt_register_error(XT_REG_CONTEXT, XT_ERR_STACK_OVERFLOW, 0, "Index node stack overflow");
		return FAILED;
	}
	stack->s_elements[stack->s_top].i_branch = n;
	if (pos)
		stack->s_elements[stack->s_top].i_pos = *pos;
	stack->s_top++;
	return OK;
}

static IdxStackItemPtr idx_pop(IdxBranchStackPtr stack)
{
	if (stack->s_top == 0)
		return NULL;
	stack->s_top--;
	return &stack->s_elements[stack->s_top];
}

static IdxStackItemPtr idx_top(IdxBranchStackPtr stack)
{
	if (stack->s_top == 0)
		return NULL;
	return &stack->s_elements[stack->s_top-1];
}

/*
 * -----------------------------------------------------------------------
 * Allocation of nodes
 */

static xtBool idx_new_branch(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID *address)
{
	register XTTableHPtr	tab;
	xtIndexNodeID			wrote_pos;
	XTIndFreeBlockRec		free_block;
	XTIndFreeListPtr		list_ptr;

	tab = ot->ot_table;

	//ASSERT_NS(XT_INDEX_HAVE_XLOCK(ind, ot));
	if (ind->mi_free_list && ind->mi_free_list->fl_free_count) {
		ind->mi_free_list->fl_free_count--;
		*address = ind->mi_free_list->fl_page_id[ind->mi_free_list->fl_free_count];
		TRACK_BLOCK_ALLOC(*address);
		return OK;
	}

	xt_lock_mutex_ns(&tab->tab_ind_lock);

	/* Check the cached free list: */
	while ((list_ptr = tab->tab_ind_free_list)) {
		if (list_ptr->fl_start < list_ptr->fl_free_count) {
			wrote_pos = list_ptr->fl_page_id[list_ptr->fl_start];
			list_ptr->fl_start++;
			xt_unlock_mutex_ns(&tab->tab_ind_lock);
			*address = wrote_pos;
			TRACK_BLOCK_ALLOC(wrote_pos);
			return OK;
		}
		tab->tab_ind_free_list = list_ptr->fl_next_list;
		xt_free_ns(list_ptr);
	}

	if ((XT_NODE_ID(wrote_pos) = XT_NODE_ID(tab->tab_ind_free))) {
		/* Use the block on the free list: */
		if (!xt_ind_read_bytes(ot, ind, wrote_pos, sizeof(XTIndFreeBlockRec), (xtWord1 *) &free_block))
			goto failed;
		XT_NODE_ID(tab->tab_ind_free) = (xtIndexNodeID) XT_GET_DISK_8(free_block.if_next_block_8);
		xt_unlock_mutex_ns(&tab->tab_ind_lock);
		*address = wrote_pos;
		TRACK_BLOCK_ALLOC(wrote_pos);
		return OK;
	}

	/* PMC - Dont allow overflow! */
	if (XT_NODE_ID(tab->tab_ind_eof) >= 0xFFFFFFF) {
		xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_INDEX_FILE_TO_LARGE, xt_file_path(ot->ot_ind_file));
		goto failed;
	}
	*address = tab->tab_ind_eof;
	XT_NODE_ID(tab->tab_ind_eof)++;
	xt_unlock_mutex_ns(&tab->tab_ind_lock);
	TRACK_BLOCK_ALLOC(*address);
	return OK;

	failed:
	xt_unlock_mutex_ns(&tab->tab_ind_lock);
	return FAILED;
}

/* Add the block to the private free list of the index.
 * On flush, this list will be transfered to the global list.
 */
static xtBool idx_free_branch(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID node_id)
{
	register u_int		count;
	register u_int		i;
	register u_int		guess;

	TRACK_BLOCK_FREE(node_id);
	//ASSERT_NS(XT_INDEX_HAVE_XLOCK(ind, ot));
	if (!ind->mi_free_list) {
		count = 0;
		if (!(ind->mi_free_list = (XTIndFreeListPtr) xt_calloc_ns(offsetof(XTIndFreeListRec, fl_page_id) + 10 * sizeof(xtIndexNodeID))))
			return FAILED;
	}
	else {
		count = ind->mi_free_list->fl_free_count;
		if (!xt_realloc_ns((void **) &ind->mi_free_list, offsetof(XTIndFreeListRec, fl_page_id) + (count + 1) * sizeof(xtIndexNodeID)))
			return FAILED;
	}
 
	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		if (XT_NODE_ID(node_id) == XT_NODE_ID(ind->mi_free_list->fl_page_id[guess])) {
			// Should not happen...
			ASSERT_NS(FALSE);
			return OK;
		}
		if (XT_NODE_ID(node_id) < XT_NODE_ID(ind->mi_free_list->fl_page_id[guess]))
			count = guess;
		else
			i = guess + 1;
	}

	/* Insert at position i */
	memmove(ind->mi_free_list->fl_page_id + i + 1, ind->mi_free_list->fl_page_id + i, (ind->mi_free_list->fl_free_count - i) * sizeof(xtIndexNodeID));
	ind->mi_free_list->fl_page_id[i] = node_id;
	ind->mi_free_list->fl_free_count++;

	/* Set the cache page to clean: */
	return xt_ind_clean(ot, ind, node_id);
}

/*
 * -----------------------------------------------------------------------
 * Simple compare functions
 */

xtPublic int xt_compare_2_int4(XTIndexPtr XT_UNUSED(ind), uint key_length, xtWord1 *key_value, xtWord1 *b_value)
{
	int r;

	ASSERT_NS(key_length == 4 || key_length == 8);
	r = (xtInt4) XT_GET_DISK_4(key_value) - (xtInt4) XT_GET_DISK_4(b_value);
	if (r == 0 && key_length > 4) {
		key_value += 4;
		b_value += 4;
		r = (xtInt4) XT_GET_DISK_4(key_value) - (xtInt4) XT_GET_DISK_4(b_value);
	}
	return r;
}

xtPublic int xt_compare_3_int4(XTIndexPtr XT_UNUSED(ind), uint key_length, xtWord1 *key_value, xtWord1 *b_value)
{
	int r;

	ASSERT_NS(key_length == 4 || key_length == 8 || key_length == 12);
	r = (xtInt4) XT_GET_DISK_4(key_value) - (xtInt4) XT_GET_DISK_4(b_value);
	if (r == 0 && key_length > 4) {
		key_value += 4;
		b_value += 4;
		r = (xtInt4) XT_GET_DISK_4(key_value) - (xtInt4) XT_GET_DISK_4(b_value);
		if (r == 0 && key_length > 8) {
			key_value += 4;
			b_value += 4;
			r = (xtInt4) XT_GET_DISK_4(key_value) - (xtInt4) XT_GET_DISK_4(b_value);
		}
	}
	return r;
}

/*
 * -----------------------------------------------------------------------
 * Tree branch sanning (searching nodes and leaves)
 */

xtPublic void xt_scan_branch_single(struct XTTable *XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxKeyValuePtr value, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	u_int				branch_size;
	u_int				node_ref_size;
	u_int				full_item_size;
	int					search_flags;
	register xtWord1	*base;
	register u_int		i;
	register xtWord1	*bitem;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_item_size = ind->mi_key_size + XT_RECORD_REF_SIZE;
	full_item_size = result->sr_item.i_item_size + node_ref_size;
	result->sr_item.i_node_ref_size = node_ref_size;

	search_flags = value->sv_flags;
	base = branch->tb_data + node_ref_size;
	if (search_flags & XT_SEARCH_FIRST_FLAG)
		i = 0;
	else if (search_flags & XT_SEARCH_AFTER_LAST_FLAG)
		i = (result->sr_item.i_total_size - node_ref_size) / full_item_size;
	else {
		register u_int		guess;
		register u_int		count;
		register xtInt4		r;
		xtRecordID			key_record;

		key_record = value->sv_rec_id;
		count = (result->sr_item.i_total_size - node_ref_size) / full_item_size;

		ASSERT_NS(ind);
		i = 0;
		while (i < count) {
			guess = (i + count - 1) >> 1;

			bitem = base + guess * full_item_size;

			switch (ind->mi_single_type) {
				case HA_KEYTYPE_LONG_INT: {
					register xtInt4 a, b;
					
					a = XT_GET_DISK_4(value->sv_key);
					b = XT_GET_DISK_4(bitem);
					r = (a < b) ? -1 : (a == b ? 0 : 1);
					break;
				}
				case HA_KEYTYPE_ULONG_INT: {
					register xtWord4 a, b;
					
					a = XT_GET_DISK_4(value->sv_key);
					b = XT_GET_DISK_4(bitem);
					r = (a < b) ? -1 : (a == b ? 0 : 1);
					break;
				}
				default:
					/* Should not happen: */
					r = 1;
					break;
			}
			if (r == 0) {
				if (search_flags & XT_SEARCH_WHOLE_KEY) {
					xtRecordID	item_record;
					xtRowID		row_id;
					
					xt_get_record_ref(bitem + ind->mi_key_size, &item_record, &row_id);

					/* This should not happen because we should never
					 * try to insert the same record twice into the 
					 * index!
					 */
					result->sr_duplicate = TRUE;
					if (key_record == item_record) {
						result->sr_found = TRUE;
						result->sr_rec_id = item_record;
						result->sr_row_id = row_id;
						result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);
						result->sr_item.i_item_offset = node_ref_size + guess * full_item_size;
						return;
					}
					if (key_record < item_record)
						r = -1;
					else
						r = 1;
				}
				else {
					result->sr_found = TRUE;
					/* -1 causes a search to the beginning of the duplicate list of keys.
					 * 1 causes a search to just after the key.
				 	*/
					if (search_flags & XT_SEARCH_AFTER_KEY)
						r = 1;
					else
						r = -1;
				}
			}

			if (r < 0)
				count = guess;
			else
				i = guess + 1;
		}
	}

	bitem = base + i * full_item_size;
	xt_get_res_record_ref(bitem + ind->mi_key_size, result);
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);			/* Only valid if this is a node. */
	result->sr_item.i_item_offset = node_ref_size + i * full_item_size;
}

/*
 * We use a special binary search here. It basically assumes that the values
 * in the index are not unique.
 *
 * Even if they are unique, when we search for part of a key, then it is
 * effectively the case.
 *
 * So in the situation where we find duplicates in the index we usually
 * want to position ourselves at the beginning of the duplicate list.
 *
 * Alternatively a search can find the position just after a given key.
 *
 * To achieve this we make the following modifications:
 * - The result of the comparison is always returns 1 or -1. We only stop
 *   the search early in the case an exact match when inserting (but this
 *   should not happen anyway).
 * - The search never actually fails, but sets 'found' to TRUE if it
 *   sees the search key in the index.
 *
 * If the search value exists in the index we know that
 * this method will take us to the first occurrence of the key in the
 * index (in the case of -1) or to the first value after the
 * the search key in the case of 1.
 */
xtPublic void xt_scan_branch_fix(struct XTTable *XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxKeyValuePtr value, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	u_int				branch_size;
	u_int				node_ref_size;
	u_int				full_item_size;
	int					search_flags;
	xtWord1				*base;
	register u_int		i;
	xtWord1				*bitem;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_item_size = ind->mi_key_size + XT_RECORD_REF_SIZE;
	full_item_size = result->sr_item.i_item_size + node_ref_size;
	result->sr_item.i_node_ref_size = node_ref_size;

	search_flags = value->sv_flags;
	base = branch->tb_data + node_ref_size;
	if (search_flags & XT_SEARCH_FIRST_FLAG)
		i = 0;
	else if (search_flags & XT_SEARCH_AFTER_LAST_FLAG)
		i = (result->sr_item.i_total_size - node_ref_size) / full_item_size;
	else {
		register u_int		guess;
		register u_int		count;
		xtRecordID			key_record;
		int					r;

		key_record = value->sv_rec_id;
		count = (result->sr_item.i_total_size - node_ref_size) / full_item_size;

		ASSERT_NS(ind);
		i = 0;
		while (i < count) {
			guess = (i + count - 1) >> 1;

			bitem = base + guess * full_item_size;

			r = myxt_compare_key(ind, search_flags, value->sv_length, value->sv_key, bitem);

			if (r == 0) {
				if (search_flags & XT_SEARCH_WHOLE_KEY) {
					xtRecordID	item_record;
					xtRowID		row_id;

					xt_get_record_ref(bitem + ind->mi_key_size, &item_record, &row_id);

					/* This should not happen because we should never
					 * try to insert the same record twice into the 
					 * index!
					 */
					result->sr_duplicate = TRUE;
					if (key_record == item_record) {
						result->sr_found = TRUE;
						result->sr_rec_id = item_record;
						result->sr_row_id = row_id;
						result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);
						result->sr_item.i_item_offset = node_ref_size + guess * full_item_size;
						return;
					}
					if (key_record < item_record)
						r = -1;
					else
						r = 1;
				}
				else {
					result->sr_found = TRUE;
					/* -1 causes a search to the beginning of the duplicate list of keys.
					 * 1 causes a search to just after the key.
				 	*/
					if (search_flags & XT_SEARCH_AFTER_KEY)
						r = 1;
					else
						r = -1;
				}
			}

			if (r < 0)
				count = guess;
			else
				i = guess + 1;
		}
	}

	bitem = base + i * full_item_size;
	xt_get_res_record_ref(bitem + ind->mi_key_size, result);
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);			/* Only valid if this is a node. */
	result->sr_item.i_item_offset = node_ref_size + i * full_item_size;
}

xtPublic void xt_scan_branch_fix_simple(struct XTTable *XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxKeyValuePtr value, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	u_int				branch_size;
	u_int				node_ref_size;
	u_int				full_item_size;
	int					search_flags;
	xtWord1				*base;
	register u_int		i;
	xtWord1				*bitem;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_item_size = ind->mi_key_size + XT_RECORD_REF_SIZE;
	full_item_size = result->sr_item.i_item_size + node_ref_size;
	result->sr_item.i_node_ref_size = node_ref_size;

	search_flags = value->sv_flags;
	base = branch->tb_data + node_ref_size;
	if (search_flags & XT_SEARCH_FIRST_FLAG)
		i = 0;
	else if (search_flags & XT_SEARCH_AFTER_LAST_FLAG)
		i = (result->sr_item.i_total_size - node_ref_size) / full_item_size;
	else {
		register u_int		guess;
		register u_int		count;
		xtRecordID			key_record;
		int					r;

		key_record = value->sv_rec_id;
		count = (result->sr_item.i_total_size - node_ref_size) / full_item_size;

		ASSERT_NS(ind);
		i = 0;
		while (i < count) {
			guess = (i + count - 1) >> 1;

			bitem = base + guess * full_item_size;

			r = ind->mi_simple_comp_key(ind, value->sv_length, value->sv_key, bitem);

			if (r == 0) {
				if (search_flags & XT_SEARCH_WHOLE_KEY) {
					xtRecordID	item_record;
					xtRowID		row_id;

					xt_get_record_ref(bitem + ind->mi_key_size, &item_record, &row_id);

					/* This should not happen because we should never
					 * try to insert the same record twice into the 
					 * index!
					 */
					result->sr_duplicate = TRUE;
					if (key_record == item_record) {
						result->sr_found = TRUE;
						result->sr_rec_id = item_record;
						result->sr_row_id = row_id;
						result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);
						result->sr_item.i_item_offset = node_ref_size + guess * full_item_size;
						return;
					}
					if (key_record < item_record)
						r = -1;
					else
						r = 1;
				}
				else {
					result->sr_found = TRUE;
					/* -1 causes a search to the beginning of the duplicate list of keys.
					 * 1 causes a search to just after the key.
				 	*/
					if (search_flags & XT_SEARCH_AFTER_KEY)
						r = 1;
					else
						r = -1;
				}
			}

			if (r < 0)
				count = guess;
			else
				i = guess + 1;
		}
	}

	bitem = base + i * full_item_size;
	xt_get_res_record_ref(bitem + ind->mi_key_size, result);
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);			/* Only valid if this is a node. */
	result->sr_item.i_item_offset = node_ref_size + i * full_item_size;
}

/*
 * Variable length key values are stored as a sorted list. Since each list item has a variable length, we
 * must scan the list sequentially in order to find a key.
 */
xtPublic void xt_scan_branch_var(struct XTTable *XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxKeyValuePtr value, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	u_int			branch_size;
	u_int			node_ref_size;
	int				search_flags;
	xtWord1			*base;
	xtWord1			*bitem;
	u_int			ilen;
	xtWord1			*bend;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_node_ref_size = node_ref_size;

	search_flags = value->sv_flags;
	base = branch->tb_data + node_ref_size;
	bitem = base;
	bend = &branch->tb_data[result->sr_item.i_total_size];
	ilen = 0;
	if (bitem >= bend)
		goto done_ok;

	if (search_flags & XT_SEARCH_FIRST_FLAG)
		ilen = myxt_get_key_length(ind, bitem);
	else if (search_flags & XT_SEARCH_AFTER_LAST_FLAG) {
		bitem = bend;
		ilen = 0;
	}
	else {
		xtRecordID	key_record;
		int			r;

		key_record = value->sv_rec_id;

		ASSERT_NS(ind);
		while (bitem < bend) {
			ilen = myxt_get_key_length(ind, bitem);
			r = myxt_compare_key(ind, search_flags, value->sv_length, value->sv_key, bitem);
			if (r == 0) {
				if (search_flags & XT_SEARCH_WHOLE_KEY) {
					xtRecordID	item_record;
					xtRowID		row_id;

					xt_get_record_ref(bitem + ilen, &item_record, &row_id);

					/* This should not happen because we should never
					 * try to insert the same record twice into the 
					 * index!
					 */
					result->sr_duplicate = TRUE;
					if (key_record == item_record) {
						result->sr_found = TRUE;
						result->sr_item.i_item_size = ilen + XT_RECORD_REF_SIZE;
						result->sr_rec_id = item_record;
						result->sr_row_id = row_id;
						result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);
						result->sr_item.i_item_offset = bitem - branch->tb_data;
						return;
					}
					if (key_record < item_record)
						r = -1;
					else
						r = 1;
				}
				else {
					result->sr_found = TRUE;
					/* -1 causes a search to the beginning of the duplicate list of keys.
					 * 1 causes a search to just after the key.
				 	*/
					if (search_flags & XT_SEARCH_AFTER_KEY)
						r = 1;
					else
						r = -1;
				}
			}
			if (r <= 0)
				break;
			bitem += ilen + XT_RECORD_REF_SIZE + node_ref_size;
		}
	}

	done_ok:
	result->sr_item.i_item_size = ilen + XT_RECORD_REF_SIZE;
	xt_get_res_record_ref(bitem + ilen, result);
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, node_ref_size);			/* Only valid if this is a node. */
	result->sr_item.i_item_offset = bitem - branch->tb_data;
}

/* Go to the next item in the node. */
static void idx_next_branch_item(XTTableHPtr XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	xtWord1	*bitem;
	u_int	ilen;

	result->sr_item.i_item_offset += result->sr_item.i_item_size + result->sr_item.i_node_ref_size;
	bitem = branch->tb_data + result->sr_item.i_item_offset;
	if (ind->mi_fix_key)
		ilen = result->sr_item.i_item_size;
	else {
		ilen = myxt_get_key_length(ind, bitem) + XT_RECORD_REF_SIZE;
		result->sr_item.i_item_size = ilen;
	}
	xt_get_res_record_ref(bitem + ilen - XT_RECORD_REF_SIZE, result); /* (Only valid if i_item_offset < i_total_size) */
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, result->sr_item.i_node_ref_size);
}

xtPublic void xt_prev_branch_item_fix(XTTableHPtr XT_UNUSED(tab), XTIndexPtr XT_UNUSED(ind), XTIdxBranchDPtr branch, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	ASSERT_NS(result->sr_item.i_item_offset >= result->sr_item.i_item_size + result->sr_item.i_node_ref_size + result->sr_item.i_node_ref_size);
	result->sr_item.i_item_offset -= (result->sr_item.i_item_size + result->sr_item.i_node_ref_size);
	xt_get_res_record_ref(branch->tb_data + result->sr_item.i_item_offset + result->sr_item.i_item_size - XT_RECORD_REF_SIZE, result); /* (Only valid if i_item_offset < i_total_size) */
	result->sr_branch = IDX_GET_NODE_REF(tab, branch->tb_data + result->sr_item.i_item_offset, result->sr_item.i_node_ref_size);
}

xtPublic void xt_prev_branch_item_var(XTTableHPtr XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxResultRec *result)
{
	XT_NODE_TEMP;
	xtWord1	*bitem;
	xtWord1	*bend;
	u_int	ilen;

	bitem = branch->tb_data + result->sr_item.i_node_ref_size;
	bend = &branch->tb_data[result->sr_item.i_item_offset];
	for (;;) {
		ilen = myxt_get_key_length(ind, bitem);
		if (bitem + ilen + XT_RECORD_REF_SIZE + result->sr_item.i_node_ref_size >= bend)
			break;
		bitem += ilen + XT_RECORD_REF_SIZE + result->sr_item.i_node_ref_size;
	}

	result->sr_item.i_item_size = ilen + XT_RECORD_REF_SIZE;
	xt_get_res_record_ref(bitem + ilen, result); /* (Only valid if i_item_offset < i_total_size) */
	result->sr_branch = IDX_GET_NODE_REF(tab, bitem, result->sr_item.i_node_ref_size);
	result->sr_item.i_item_offset = bitem - branch->tb_data;
}

static void idx_reload_item_fix(XTIndexPtr XT_NDEBUG_UNUSED(ind), XTIdxBranchDPtr branch, register XTIdxResultPtr result)
{
	u_int branch_size;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	ASSERT_NS(result->sr_item.i_node_ref_size == (XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0));
	ASSERT_NS(result->sr_item.i_item_size == ind->mi_key_size + XT_RECORD_REF_SIZE);
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	if (result->sr_item.i_item_offset > result->sr_item.i_total_size)
		result->sr_item.i_item_offset = result->sr_item.i_total_size;
	xt_get_res_record_ref(&branch->tb_data[result->sr_item.i_item_offset + result->sr_item.i_item_size - XT_RECORD_REF_SIZE], result); 
}

static void idx_first_branch_item(XTTableHPtr XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxResultPtr result)
{
	XT_NODE_TEMP;
	u_int branch_size;
	u_int node_ref_size;
	u_int key_data_size;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	if (ind->mi_fix_key)
		key_data_size = ind->mi_key_size;
	else {
		xtWord1 *bitem;

		bitem = branch->tb_data + node_ref_size;
		if (bitem < &branch->tb_data[result->sr_item.i_total_size])
			key_data_size = myxt_get_key_length(ind, bitem);
		else
			key_data_size = 0;
	}

	result->sr_item.i_item_size = key_data_size + XT_RECORD_REF_SIZE;
	result->sr_item.i_node_ref_size = node_ref_size;

	xt_get_res_record_ref(branch->tb_data + node_ref_size + key_data_size, result);
	result->sr_branch = IDX_GET_NODE_REF(tab, branch->tb_data + node_ref_size, node_ref_size); /* Only valid if this is a node. */
	result->sr_item.i_item_offset = node_ref_size;
}

/*
 * Last means different things for leaf or node!
 */
xtPublic void xt_last_branch_item_fix(XTTableHPtr XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxResultPtr result)
{
	XT_NODE_TEMP;
	u_int branch_size;
	u_int node_ref_size;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_item_size = ind->mi_key_size + XT_RECORD_REF_SIZE;
	result->sr_item.i_node_ref_size = node_ref_size;

	if (node_ref_size) {
		result->sr_item.i_item_offset = result->sr_item.i_total_size;
		result->sr_branch = IDX_GET_NODE_REF(tab, branch->tb_data + result->sr_item.i_item_offset, node_ref_size);
	}
	else {
		if (result->sr_item.i_total_size) {
			result->sr_item.i_item_offset = result->sr_item.i_total_size - result->sr_item.i_item_size;
			xt_get_res_record_ref(branch->tb_data + result->sr_item.i_item_offset + ind->mi_key_size, result);
		}
		else
			/* Leaf is empty: */
			result->sr_item.i_item_offset = 0;
	}
}

xtPublic void xt_last_branch_item_var(XTTableHPtr XT_UNUSED(tab), XTIndexPtr ind, XTIdxBranchDPtr branch, register XTIdxResultPtr result)
{
	XT_NODE_TEMP;
	u_int	branch_size;
	u_int	node_ref_size;

	branch_size = XT_GET_DISK_2(branch->tb_size_2);
	node_ref_size = XT_IS_NODE(branch_size) ? XT_NODE_REF_SIZE : 0;

	result->sr_found = FALSE;
	result->sr_duplicate = FALSE;
	result->sr_item.i_total_size = XT_GET_BRANCH_DATA_SIZE(branch_size);
	ASSERT_NS((int) result->sr_item.i_total_size >= 0 && result->sr_item.i_total_size <= XT_INDEX_PAGE_SIZE-2);

	result->sr_item.i_node_ref_size = node_ref_size;

	if (node_ref_size) {
		result->sr_item.i_item_offset = result->sr_item.i_total_size;
		result->sr_branch = IDX_GET_NODE_REF(tab, branch->tb_data + result->sr_item.i_item_offset, node_ref_size);
		result->sr_item.i_item_size = 0;
	}
	else {
		if (result->sr_item.i_total_size) {
			xtWord1	*bitem;
			u_int	ilen;
			xtWord1	*bend;

			bitem = branch->tb_data + node_ref_size;;
			bend = &branch->tb_data[result->sr_item.i_total_size];
			ilen = 0;
			if (bitem < bend) {
				for (;;) {
					ilen = myxt_get_key_length(ind, bitem);
					if (bitem + ilen + XT_RECORD_REF_SIZE + node_ref_size >= bend)
						break;
					bitem += ilen + XT_RECORD_REF_SIZE + node_ref_size;
				}
			}

			result->sr_item.i_item_offset = bitem - branch->tb_data;
			xt_get_res_record_ref(bitem + ilen, result);
			result->sr_item.i_item_size = ilen + XT_RECORD_REF_SIZE;
		}
		else {
			/* Leaf is empty: */
			result->sr_item.i_item_offset = 0;
			result->sr_item.i_item_size = 0;
		}
	}
}

xtPublic xtBool xt_idx_lazy_delete_on_leaf(XTIndexPtr ind, XTIndBlockPtr block, xtWord2 branch_size)
{
	ASSERT_NS(ind->mi_fix_key);
	
	/* Compact the leaf if more than half the items that fit on the page
	 * are deleted: */
	if (block->cp_del_count >= ind->mi_max_items/2)
		return FALSE;

	/* Compact the page if there is only 1 (or less) valid item left: */
	if ((u_int) block->cp_del_count+1 >= ((u_int) branch_size - 2)/(ind->mi_key_size + XT_RECORD_REF_SIZE))
		return FALSE;

	return OK;
}

static xtBool idx_lazy_delete_on_node(XTIndexPtr ind, XTIndBlockPtr block, register XTIdxItemPtr item)
{
	ASSERT_NS(ind->mi_fix_key);
	
	/* Compact the node if more than 1/4 of the items that fit on the page
	 * are deleted: */
	if (block->cp_del_count >= ind->mi_max_items/4)
		return FALSE;

	/* Compact the page if there is only 1 (or less) valid item left: */
	if ((u_int) block->cp_del_count+1 >= (item->i_total_size - item->i_node_ref_size)/(item->i_item_size + item->i_node_ref_size))
		return FALSE;

	return OK;
}

inline static xtBool idx_cmp_item_key_fix(XTIndReferencePtr iref, register XTIdxItemPtr item, XTIdxKeyValuePtr value)
{
	xtWord1 *data;

	data = &iref->ir_branch->tb_data[item->i_item_offset];
	return memcmp(data, value->sv_key, value->sv_length) == 0;
}

inline static void idx_set_item_key_fix(XTIndReferencePtr iref, register XTIdxItemPtr item, XTIdxKeyValuePtr value)
{
	xtWord1 *data;

	data = &iref->ir_branch->tb_data[item->i_item_offset];
	memcpy(data, value->sv_key, value->sv_length);
	xt_set_val_record_ref(data + value->sv_length, value);
	iref->ir_updated = TRUE;
}

inline static void idx_set_item_reference(XTIndReferencePtr iref, register XTIdxItemPtr item, xtRowID rec_id, xtRowID row_id)
{
	size_t	offset;
	xtWord1	*data;

	/* This is the offset of the reference in the item we found: */
	offset = item->i_item_offset +item->i_item_size - XT_RECORD_REF_SIZE;
	data = &iref->ir_branch->tb_data[offset];

	xt_set_record_ref(data, rec_id, row_id);
	iref->ir_updated = TRUE;
}

inline static void idx_set_item_row_id(XTIndReferencePtr iref, register XTIdxItemPtr item, xtRowID row_id)
{
	size_t	offset;
	xtWord1	*data;

	offset = 
		/* This is the offset of the reference in the item we found: */
		item->i_item_offset +item->i_item_size - XT_RECORD_REF_SIZE +
		/* This is the offset of the row id in the reference: */
		XT_RECORD_ID_SIZE;
	data = &iref->ir_branch->tb_data[offset];

	/* This update does not change the structure of page, so we do it without
	 * copying the page before we write.
	 */
	XT_SET_DISK_4(data, row_id);
	iref->ir_updated = TRUE;
}

inline static xtBool idx_is_item_deleted(register XTIdxBranchDPtr branch, register XTIdxItemPtr item)
{
	xtWord1	*data;

	data = &branch->tb_data[item->i_item_offset + item->i_item_size - XT_RECORD_REF_SIZE + XT_RECORD_ID_SIZE];
	return XT_GET_DISK_4(data) == (xtRowID) -1;
}

inline static void idx_set_item_deleted(XTIndReferencePtr iref, register XTIdxItemPtr item)
{
	idx_set_item_row_id(iref, item, (xtRowID) -1);
	
	/* This should be safe because there is only one thread,
	 * the sweeper, that does this!
	 *
	 * Threads that decrement this value have an xlock on
	 * the page, or the index.
	 */
	iref->ir_block->cp_del_count++;
}

/*
 * {LAZY-DEL-INDEX-ITEMS}
 * Do a lazy delete of an item by just setting the Row ID
 * to the delete indicator: row ID -1.
 */
static void idx_lazy_delete_branch_item(XTOpenTablePtr ot, XTIndexPtr ind, XTIndReferencePtr iref, register XTIdxItemPtr item)
{
	idx_set_item_deleted(iref, item);
	xt_ind_release(ot, ind, iref->ir_xlock ? XT_UNLOCK_W_UPDATE : XT_UNLOCK_R_UPDATE, iref);
}

/*
 * This function compacts the leaf, but preserves the
 * position of the item.
 */
static xtBool idx_compact_leaf(XTOpenTablePtr ot, XTIndexPtr ind, XTIndReferencePtr iref, register XTIdxItemPtr item)
{
	register XTIdxBranchDPtr branch = iref->ir_branch;
	int		item_idx, count, i, idx;
	u_int	size;
	xtWord1	*s_data;
	xtWord1	*d_data;
	xtWord1	*data;
	xtRowID	row_id;

	if (iref->ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(iref)) {
			xt_ind_release(ot, ind, iref->ir_xlock ? XT_UNLOCK_WRITE : XT_UNLOCK_READ, iref);
			return FAILED;
		}
	}

	ASSERT_NS(!item->i_node_ref_size);
	ASSERT_NS(ind->mi_fix_key);
	size = item->i_item_size;
	count = item->i_total_size / size;
	item_idx = item->i_item_offset / size;
	s_data = d_data = branch->tb_data;
	idx = 0;
	for (i=0; i<count; i++) {
		data = s_data + item->i_item_size - XT_RECORD_REF_SIZE + XT_RECORD_ID_SIZE;
		row_id = XT_GET_DISK_4(data);
		if (row_id == (xtRowID) -1) {
			if (idx < item_idx)
				item_idx--;
		}
		else {
			if (d_data != s_data)
				memcpy(d_data, s_data, size);
			d_data += size;
			idx++;
		}
		s_data += size;
	}
	iref->ir_block->cp_del_count = 0;
	item->i_total_size = d_data - branch->tb_data;
	ASSERT_NS(idx * size == item->i_total_size);
	item->i_item_offset = item_idx * size;
	XT_SET_DISK_2(branch->tb_size_2, XT_MAKE_BRANCH_SIZE(item->i_total_size, 0));
	iref->ir_updated = TRUE;
	return OK;
}

static xtBool idx_lazy_remove_leaf_item_right(XTOpenTablePtr ot, XTIndexPtr ind, XTIndReferencePtr iref, register XTIdxItemPtr item)
{
	register XTIdxBranchDPtr branch = iref->ir_branch;
	int		item_idx, count, i;
	u_int	size;
	xtWord1	*s_data;
	xtWord1	*d_data;
	xtWord1	*data;
	xtRowID	row_id;

	ASSERT_NS(!item->i_node_ref_size);

	if (iref->ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(iref)) {
			xt_ind_release(ot, ind, XT_UNLOCK_WRITE, iref);
			return FAILED;
		}
	}

	ASSERT_NS(ind->mi_fix_key);
	size = item->i_item_size;
	count = item->i_total_size / size;
	item_idx = item->i_item_offset / size;
	s_data = d_data = branch->tb_data;
	for (i=0; i<count; i++) {
		if (i == item_idx)
			item->i_item_offset = d_data - branch->tb_data;
		else {
			data = s_data + item->i_item_size - XT_RECORD_REF_SIZE + XT_RECORD_ID_SIZE;
			row_id = XT_GET_DISK_4(data);
			if (row_id != (xtRowID) -1) {
				if (d_data != s_data)
					memcpy(d_data, s_data, size);
				d_data += size;
			}
		}
		s_data += size;
	}
	iref->ir_block->cp_del_count = 0;
	item->i_total_size = d_data - branch->tb_data;
	XT_SET_DISK_2(branch->tb_size_2, XT_MAKE_BRANCH_SIZE(item->i_total_size, 0));
	iref->ir_updated = TRUE;
	xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, iref);
	return OK;
}

/*
 * Remove an item and save to disk.
 */
static xtBool idx_remove_branch_item_right(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID, XTIndReferencePtr iref, register XTIdxItemPtr item)
{
	register XTIdxBranchDPtr branch = iref->ir_branch;
	u_int size = item->i_item_size + item->i_node_ref_size;

	/* {HANDLE-COUNT-USAGE}
	 * This access is safe because we have the right to update
	 * the page, so no other thread can modify the page.
	 *
	 * This means:
	 * We either have an Xlock on the index, or we have
	 * an Xlock on the cache block.
	 */
	if (iref->ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(iref)) {
			xt_ind_release(ot, ind, item->i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE, iref);
			return FAILED;
		}
	}
	if (ind->mi_lazy_delete) {
		if (idx_is_item_deleted(branch, item))
			iref->ir_block->cp_del_count--;
	}
	/* Remove the node reference to the left of the item: */
	memmove(&branch->tb_data[item->i_item_offset],
		&branch->tb_data[item->i_item_offset + size],
		item->i_total_size - item->i_item_offset - size);
	item->i_total_size -= size;
	XT_SET_DISK_2(branch->tb_size_2, XT_MAKE_BRANCH_SIZE(item->i_total_size, item->i_node_ref_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(branch->tb_size_2));
	iref->ir_updated = TRUE;
	xt_ind_release(ot, ind, item->i_node_ref_size ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_W_UPDATE, iref);
	return OK;
}

static xtBool idx_remove_branch_item_left(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID, XTIndReferencePtr iref, register XTIdxItemPtr item, xtBool *lazy_delete_cleanup_required)
{
	register XTIdxBranchDPtr branch = iref->ir_branch;
	u_int size = item->i_item_size + item->i_node_ref_size;

	ASSERT_NS(item->i_node_ref_size);
	if (iref->ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(iref)) {
			xt_ind_release(ot, ind, item->i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE, iref);
			return FAILED;
		}
	}
	if (ind->mi_lazy_delete) {
		if (idx_is_item_deleted(branch, item))
			iref->ir_block->cp_del_count--;
		if (lazy_delete_cleanup_required)
			*lazy_delete_cleanup_required = idx_lazy_delete_on_node(ind, iref->ir_block, item);
	}
	/* Remove the node reference to the left of the item: */
	memmove(&branch->tb_data[item->i_item_offset - item->i_node_ref_size],
		&branch->tb_data[item->i_item_offset + item->i_item_size],
		item->i_total_size - item->i_item_offset - item->i_item_size);
	item->i_total_size -= size;
	XT_SET_DISK_2(branch->tb_size_2, XT_MAKE_BRANCH_SIZE(item->i_total_size, item->i_node_ref_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(address), (int) XT_GET_DISK_2(branch->tb_size_2));
	iref->ir_updated = TRUE;
	xt_ind_release(ot, ind, item->i_node_ref_size ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_W_UPDATE, iref);
	return OK;
}

static void idx_insert_leaf_item(XTIndexPtr XT_UNUSED(ind), XTIdxBranchDPtr leaf, XTIdxKeyValuePtr value, XTIdxResultPtr result)
{
	xtWord1 *item;

	/* This will ensure we do not overwrite the end of the buffer: */
	ASSERT_NS(value->sv_length <= XT_INDEX_MAX_KEY_SIZE);
	memmove(&leaf->tb_data[result->sr_item.i_item_offset + value->sv_length + XT_RECORD_REF_SIZE],
		&leaf->tb_data[result->sr_item.i_item_offset],
		result->sr_item.i_total_size - result->sr_item.i_item_offset);
	item = &leaf->tb_data[result->sr_item.i_item_offset];
	memcpy(item, value->sv_key, value->sv_length);
	xt_set_val_record_ref(item + value->sv_length, value);
	result->sr_item.i_total_size += value->sv_length + XT_RECORD_REF_SIZE;
	XT_SET_DISK_2(leaf->tb_size_2, XT_MAKE_LEAF_SIZE(result->sr_item.i_total_size));
}

static void idx_insert_node_item(XTTableHPtr XT_UNUSED(tab), XTIndexPtr XT_UNUSED(ind), XTIdxBranchDPtr leaf, XTIdxKeyValuePtr value, XTIdxResultPtr result, xtIndexNodeID branch)
{
	xtWord1 *item;

	/* This will ensure we do not overwrite the end of the buffer: */
	ASSERT_NS(value->sv_length <= XT_INDEX_MAX_KEY_SIZE);
	memmove(&leaf->tb_data[result->sr_item.i_item_offset + value->sv_length + XT_RECORD_REF_SIZE + result->sr_item.i_node_ref_size],
		&leaf->tb_data[result->sr_item.i_item_offset],
		result->sr_item.i_total_size - result->sr_item.i_item_offset);
	item = &leaf->tb_data[result->sr_item.i_item_offset];
	memcpy(item, value->sv_key, value->sv_length);
	xt_set_val_record_ref(item + value->sv_length, value);
	XT_SET_NODE_REF(tab, item + value->sv_length + XT_RECORD_REF_SIZE, branch);
	result->sr_item.i_total_size += value->sv_length + XT_RECORD_REF_SIZE + result->sr_item.i_node_ref_size;
	XT_SET_DISK_2(leaf->tb_size_2, XT_MAKE_NODE_SIZE(result->sr_item.i_total_size));
}

static void idx_get_middle_branch_item(XTIndexPtr ind, XTIdxBranchDPtr branch, XTIdxKeyValuePtr value, XTIdxResultPtr result)
{
	xtWord1	*bitem;

	if (ind->mi_fix_key) {
		u_int full_item_size = result->sr_item.i_item_size + result->sr_item.i_node_ref_size;

		result->sr_item.i_item_offset = ((result->sr_item.i_total_size - result->sr_item.i_node_ref_size)
			/ full_item_size / 2 * full_item_size) + result->sr_item.i_node_ref_size;

		bitem = &branch->tb_data[result->sr_item.i_item_offset];
		value->sv_flags = XT_SEARCH_WHOLE_KEY;
		value->sv_length = result->sr_item.i_item_size - XT_RECORD_REF_SIZE;
		xt_get_record_ref(bitem + value->sv_length, &value->sv_rec_id, &value->sv_row_id);
		memcpy(value->sv_key, bitem, value->sv_length);
	}
	else {
		u_int	node_ref_size;
		u_int	ilen;
		xtWord1	*bend;

		node_ref_size = result->sr_item.i_node_ref_size;
		bitem = branch->tb_data + node_ref_size;;
		bend = &branch->tb_data[(result->sr_item.i_total_size - node_ref_size) / 2 + node_ref_size];
		ilen = 0;
		if (bitem < bend) {
			for (;;) {
				ilen = myxt_get_key_length(ind, bitem);
				if (bitem + ilen + XT_RECORD_REF_SIZE + node_ref_size >= bend)
					break;
				bitem += ilen + XT_RECORD_REF_SIZE + node_ref_size;
			}
		}

		result->sr_item.i_item_offset = bitem - branch->tb_data;
		result->sr_item.i_item_size = ilen + XT_RECORD_REF_SIZE;

		value->sv_flags = XT_SEARCH_WHOLE_KEY;
		value->sv_length = ilen;
		xt_get_record_ref(bitem + ilen, &value->sv_rec_id, &value->sv_row_id);
		memcpy(value->sv_key, bitem, value->sv_length);
	}
}

static size_t idx_write_branch_item(XTIndexPtr XT_UNUSED(ind), xtWord1 *item, XTIdxKeyValuePtr value)
{
	memcpy(item, value->sv_key, value->sv_length);
	xt_set_val_record_ref(item + value->sv_length, value);
	return value->sv_length + XT_RECORD_REF_SIZE;
}

static xtBool idx_replace_node_key(XTOpenTablePtr ot, XTIndexPtr ind, IdxStackItemPtr item, IdxBranchStackPtr stack, u_int item_size, xtWord1 *item_buf)
{
	XTIndReferenceRec	iref;
	xtIndexNodeID		new_branch;
	XTIdxResultRec		result;
	xtIndexNodeID		current = item->i_branch;
	u_int				new_size;
	XTIdxBranchDPtr		new_branch_ptr;
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE];

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	if (!xt_ind_fetch(ot, ind, current, XT_LOCK_WRITE, &iref))
		return FAILED;
	if (iref.ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(&iref))
			goto failed_1;
	}
	if (ind->mi_lazy_delete) {
		ASSERT_NS(item_size == item->i_pos.i_item_size);
		if (idx_is_item_deleted(iref.ir_branch, &item->i_pos))
			iref.ir_block->cp_del_count--;
	}
	memmove(&iref.ir_branch->tb_data[item->i_pos.i_item_offset + item_size],
		&iref.ir_branch->tb_data[item->i_pos.i_item_offset + item->i_pos.i_item_size],
		item->i_pos.i_total_size - item->i_pos.i_item_offset - item->i_pos.i_item_size);
	memcpy(&iref.ir_branch->tb_data[item->i_pos.i_item_offset],
		item_buf, item_size);
	if (ind->mi_lazy_delete) {
		if (idx_is_item_deleted(iref.ir_branch, &item->i_pos))
			iref.ir_block->cp_del_count++;
	}
	item->i_pos.i_total_size = item->i_pos.i_total_size + item_size - item->i_pos.i_item_size;
	XT_SET_DISK_2(iref.ir_branch->tb_size_2, XT_MAKE_NODE_SIZE(item->i_pos.i_total_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(iref.ir_branch->tb_size_2));
	iref.ir_updated = TRUE;

#ifdef DEBUG
	if (ind->mi_lazy_delete)
		ASSERT_NS(item->i_pos.i_total_size <= XT_INDEX_PAGE_DATA_SIZE);
#endif
	if (item->i_pos.i_total_size <= XT_INDEX_PAGE_DATA_SIZE)
		return xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);

	/* The node has overflowed!! */
	result.sr_item = item->i_pos;

	/* Adjust the stack (we want the parents of the delete node): */
	for (;;) {
		if (idx_pop(stack) == item)
			break;
	}		

	/* We assume that value can be overwritten (which is the case) */
	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	key_value.sv_key = key_buf;
	idx_get_middle_branch_item(ind, iref.ir_branch, &key_value, &result);

	if (!idx_new_branch(ot, ind, &new_branch))
		goto failed_1;

	/* Split the node: */
	new_size = result.sr_item.i_total_size - result.sr_item.i_item_offset - result.sr_item.i_item_size;
	// TODO: Are 2 buffers now required?
	new_branch_ptr = (XTIdxBranchDPtr) &ot->ot_ind_wbuf.tb_data[XT_INDEX_PAGE_DATA_SIZE];
	memmove(new_branch_ptr->tb_data, &iref.ir_branch->tb_data[result.sr_item.i_item_offset + result.sr_item.i_item_size], new_size);

	XT_SET_DISK_2(new_branch_ptr->tb_size_2, XT_MAKE_NODE_SIZE(new_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(new_branch), (int) XT_GET_DISK_2(new_branch_ptr->tb_size_2));
	if (!xt_ind_write(ot, ind, new_branch, offsetof(XTIdxBranchDRec, tb_data) + new_size, (xtWord1 *) new_branch_ptr))
		goto failed_2;

	/* Change the size of the old branch: */
	XT_SET_DISK_2(iref.ir_branch->tb_size_2, XT_MAKE_NODE_SIZE(result.sr_item.i_item_offset));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(iref.ir_branch->tb_size_2));
	iref.ir_updated = TRUE;

	xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);

	/* Insert the new branch into the parent node, using the new middle key value: */
	if (!idx_insert_node(ot, ind, stack, &key_value, new_branch)) {
		/* 
		 * TODO: Mark the index as corrupt.
		 * This should not fail because everything has been
		 * preallocated.
		 * However, if it does fail the index
		 * will be corrupt.
		 * I could modify and release the branch above,
		 * after this point.
		 * But that would mean holding the lock longer,
		 * and also may not help because idx_insert_node()
		 * is recursive.
		 */
		idx_free_branch(ot, ind, new_branch);
		return FAILED;
	}

	return OK;

	failed_2:
	idx_free_branch(ot, ind, new_branch);

	failed_1:
	xt_ind_release(ot, ind, XT_UNLOCK_WRITE, &iref);

	return FAILED;
}

/*ot_ind_wbuf
 * -----------------------------------------------------------------------
 * Standard b-tree insert
 */

/*
 * Insert the given branch into the node on the top of the stack. If the stack
 * is empty we need to add a new root.
 */
static xtBool idx_insert_node(XTOpenTablePtr ot, XTIndexPtr ind, IdxBranchStackPtr stack, XTIdxKeyValuePtr key_value, xtIndexNodeID branch)
{
	IdxStackItemPtr		stack_item;
	xtIndexNodeID		new_branch;
	size_t				size;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;
	u_int				new_size;
	XTIdxBranchDPtr		new_branch_ptr;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	/* Insert a new branch (key, data)... */
	if (!(stack_item = idx_pop(stack))) {
		xtWord1 *ditem;

		/* New root */
		if (!idx_new_branch(ot, ind, &new_branch))
			goto failed;

		ditem = ot->ot_ind_wbuf.tb_data;
		XT_SET_NODE_REF(ot->ot_table, ditem, ind->mi_root);
		ditem += XT_NODE_REF_SIZE;
		ditem += idx_write_branch_item(ind, ditem, key_value);
		XT_SET_NODE_REF(ot->ot_table, ditem, branch);
		ditem += XT_NODE_REF_SIZE;
		size = ditem - ot->ot_ind_wbuf.tb_data;
		XT_SET_DISK_2(ot->ot_ind_wbuf.tb_size_2, XT_MAKE_NODE_SIZE(size));
		IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(new_branch), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
		if (!xt_ind_write(ot, ind, new_branch, offsetof(XTIdxBranchDRec, tb_data) + size, (xtWord1 *) &ot->ot_ind_wbuf))
			goto failed_2;
		ind->mi_root = new_branch;
		goto done_ok;
	}

	current = stack_item->i_branch;
	/* This read does not count (towards ot_ind_reads), because we are only
	 * counting each loaded page once. We assume that the page is in
	 * cache, and will remain in cache when we read again below for the
	 * purpose of update.
	 */
	if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
		goto failed;
	ASSERT_NS(XT_IS_NODE(XT_GET_DISK_2(iref.ir_branch->tb_size_2)));
	ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, key_value, &result);

	if (result.sr_item.i_total_size + key_value->sv_length + XT_RECORD_REF_SIZE + result.sr_item.i_node_ref_size <= XT_INDEX_PAGE_DATA_SIZE) {
		if (iref.ir_block->cb_handle_count) {
			if (!xt_ind_copy_on_write(&iref))
				goto failed_1;
		}
		idx_insert_node_item(ot->ot_table, ind, iref.ir_branch, key_value, &result, branch);
		IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
		iref.ir_updated = TRUE;
		ASSERT_NS(result.sr_item.i_total_size <= XT_INDEX_PAGE_DATA_SIZE);
		xt_ind_release(ot, ind, XT_UNLOCK_R_UPDATE, &iref);
		goto done_ok;
	}

	memcpy(&ot->ot_ind_wbuf, iref.ir_branch, offsetof(XTIdxBranchDRec, tb_data) + result.sr_item.i_total_size);
	idx_insert_node_item(ot->ot_table, ind, &ot->ot_ind_wbuf, key_value, &result, branch);
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
	ASSERT_NS(result.sr_item.i_total_size > XT_INDEX_PAGE_DATA_SIZE);

	/* We assume that value can be overwritten (which is the case) */
	idx_get_middle_branch_item(ind, &ot->ot_ind_wbuf, key_value, &result);

	if (!idx_new_branch(ot, ind, &new_branch))
		goto failed_1;

	/* Split the node: */
	new_size = result.sr_item.i_total_size - result.sr_item.i_item_offset - result.sr_item.i_item_size;
	new_branch_ptr = (XTIdxBranchDPtr) &ot->ot_ind_wbuf.tb_data[XT_INDEX_PAGE_DATA_SIZE];
	memmove(new_branch_ptr->tb_data, &ot->ot_ind_wbuf.tb_data[result.sr_item.i_item_offset + result.sr_item.i_item_size], new_size);

	XT_SET_DISK_2(new_branch_ptr->tb_size_2, XT_MAKE_NODE_SIZE(new_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(new_branch), (int) XT_GET_DISK_2(new_branch_ptr->tb_size_2));
	if (!xt_ind_write(ot, ind, new_branch, offsetof(XTIdxBranchDRec, tb_data) + new_size, (xtWord1 *) new_branch_ptr))
		goto failed_2;

	/* Change the size of the old branch: */
	XT_SET_DISK_2(ot->ot_ind_wbuf.tb_size_2, XT_MAKE_NODE_SIZE(result.sr_item.i_item_offset));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
	if (iref.ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(&iref))
			goto failed_2;
	}
	memcpy(iref.ir_branch, &ot->ot_ind_wbuf, offsetof(XTIdxBranchDRec, tb_data) + result.sr_item.i_item_offset);
	iref.ir_updated = TRUE;
	xt_ind_release(ot, ind, XT_UNLOCK_R_UPDATE, &iref);

	/* Insert the new branch into the parent node, using the new middle key value: */
	if (!idx_insert_node(ot, ind, stack, key_value, new_branch)) {
		// Index may be inconsistant now...
		idx_free_branch(ot, ind, new_branch);
		goto failed;
	}

	done_ok:
	return OK;

	failed_2:
	idx_free_branch(ot, ind, new_branch);

	failed_1:
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);

	failed:
	return FAILED;
}

static xtBool idx_out_of_memory_failure(XTOpenTablePtr ot)
{
#ifdef XT_TRACK_INDEX_UPDATES
	/* If the index has been changed when we run out of memory, we
	 * will corrupt the index!
	 */
	ASSERT_NS(ot->ot_ind_changed == 0);
#endif
	if (ot->ot_thread->t_exception.e_xt_err == XT_ERR_NO_INDEX_CACHE) {
		/* Flush index and retry! */
		xt_clear_exception(ot->ot_thread);
		if (!xt_flush_indices(ot, NULL, FALSE))
			return FAILED;
		return TRUE;
	}
	return FALSE;
}

/*
 * Check all the duplicate variation in an index.
 * If one of them is visible, then we have a duplicate key
 * error.
 *
 * GOTCHA: This routine must use the write index buffer!
 */
static xtBool idx_check_duplicates(XTOpenTablePtr ot, XTIndexPtr ind, XTIdxKeyValuePtr key_value)
{
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;
	xtBool				on_key = FALSE;
	xtXactID			xn_id;
	int					save_flags;				
	XTXactWaitRec		xw;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	retry:
	idx_newstack(&stack);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
		return OK;

	save_flags = key_value->sv_flags;
	key_value->sv_flags = 0;

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref)) {
			key_value->sv_flags = save_flags;
			return FAILED;
		}
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, key_value, &result);
		if (result.sr_found)
			/* If we have found the key in a node: */
			on_key = TRUE;
		if (!result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		if (!idx_push(&stack, current, &result.sr_item)) {
			key_value->sv_flags = save_flags;
			return FAILED;
		}
		current = result.sr_branch;
	}

	key_value->sv_flags = save_flags;

	if (!on_key) {
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		return OK;
	}

	for (;;) {
		if (result.sr_item.i_item_offset == result.sr_item.i_total_size) {
			IdxStackItemPtr node;

			/* We are at the end of a leaf node.
			 * Go up the stack to find the start position of the next key.
			 * If we find none, then we are the end of the index.
			 */
			xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
			while ((node = idx_pop(&stack))) {
				if (node->i_pos.i_item_offset < node->i_pos.i_total_size) {
					current = node->i_branch;
					if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
						return FAILED;
					xt_get_res_record_ref(&iref.ir_branch->tb_data[node->i_pos.i_item_offset + node->i_pos.i_item_size - XT_RECORD_REF_SIZE], &result);
					result.sr_item = node->i_pos;
					goto check_value;
				}
			}
			break;
		}

		check_value:
		/* Quit the loop if the key is no longer matched! */
		if (myxt_compare_key(ind, 0, key_value->sv_length, key_value->sv_key, &iref.ir_branch->tb_data[result.sr_item.i_item_offset]) != 0) {
			xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
			break;
		}

		if (ind->mi_lazy_delete) {
			if (result.sr_row_id == (xtRowID) -1)
				goto next_item;
		}

		switch (xt_tab_maybe_committed(ot, result.sr_rec_id, &xn_id, NULL, NULL)) {
			case XT_MAYBE:
				/* Record is not committed, wait for the transaction. */
				xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
				XT_INDEX_UNLOCK(ind, ot);				
				xw.xw_xn_id = xn_id;
				if (!xt_xn_wait_for_xact(ot->ot_thread, &xw, NULL)) {
					XT_INDEX_WRITE_LOCK(ind, ot);
					return FAILED;
				}
				XT_INDEX_WRITE_LOCK(ind, ot);
				goto retry;			
			case XT_ERR:
				/* Error while reading... */
				goto failed;
			case TRUE:
				/* Record is committed or belongs to me, duplicate key: */
				XT_DEBUG_TRACE(("DUPLICATE KEY tx=%d rec=%d\n", (int) ot->ot_thread->st_xact_data->xd_start_xn_id, (int) result.sr_rec_id));
				xt_register_xterr(XT_REG_CONTEXT, XT_ERR_DUPLICATE_KEY);
				goto failed;
			case FALSE:
				/* Record is deleted or rolled-back: */
				break;
		}

		next_item:
		idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);

		if (result.sr_item.i_node_ref_size) {
			/* Go down to the bottom: */
			while (XT_NODE_ID(current)) {
				xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
				if (!idx_push(&stack, current, &result.sr_item))
					return FAILED;
				current = result.sr_branch;
				if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
					return FAILED;
				idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
				if (!result.sr_item.i_node_ref_size)
					break;
			}
		}
	}

	return OK;
	
	failed:
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	return FAILED;
}

inline static void idx_still_on_key(XTIndexPtr ind, register XTIdxSearchKeyPtr search_key, register XTIdxBranchDPtr branch, register XTIdxItemPtr item)
{
	if (search_key && search_key->sk_on_key) {
		search_key->sk_on_key = myxt_compare_key(ind, search_key->sk_key_value.sv_flags, search_key->sk_key_value.sv_length,
			search_key->sk_key_value.sv_key, &branch->tb_data[item->i_item_offset]) == 0;
	}
}

/*
 * Insert a value into the given index. Return FALSE if an error occurs.
 */
xtPublic xtBool xt_idx_insert(XTOpenTablePtr ot, XTIndexPtr ind, xtRowID row_id, xtRecordID rec_id, xtWord1 *rec_buf, xtWord1 *bef_buf, xtBool allow_dups)
{
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE];
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	xtIndexNodeID		new_branch;
	XTIdxBranchDPtr		new_branch_ptr;
	size_t				size;
	XTIdxResultRec		result;
	size_t				new_size;
	xtBool				check_for_dups = ind->mi_flags & (HA_UNIQUE_CHECK | HA_NOSAME) && !allow_dups;
	xtBool				lock_structure = FALSE;
	xtBool				updated = FALSE;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
#ifdef CHECK_AND_PRINT
	//idx_check_index(ot, ind, TRUE);
#endif

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	key_value.sv_rec_id = rec_id;
	key_value.sv_row_id = row_id;		/* Should always be zero on insert (will be update by sweeper later). 
										 * Non-zero only during recovery, assuming that sweeper will process such records right after recovery.
										 */
	key_value.sv_key = key_buf;
	key_value.sv_length = myxt_create_key_from_row(ind, key_buf, rec_buf, &check_for_dups);

	if (bef_buf && check_for_dups) {
		/* If we have a before image, and we are required to check for duplicates.
		 * then compare the before image key with the after image key.
		 */
		xtWord1	bef_key_buf[XT_INDEX_MAX_KEY_SIZE];
		u_int	len;
		xtBool	has_no_null = TRUE;

		len = myxt_create_key_from_row(ind, bef_key_buf, bef_buf, &has_no_null);
		if (has_no_null) {
			/* If the before key has no null values, then compare with the after key value.
			 * We only have to check for duplicates if the key has changed!
			 */
			check_for_dups = myxt_compare_key(ind, 0, len, bef_key_buf, key_buf) != 0;
		}
	}

	/* The index appears to have no root: */
	if (!XT_NODE_ID(ind->mi_root))
		lock_structure = TRUE;

	lock_and_retry:
	idx_newstack(&stack);

	/* A write lock is only required if we are going to change the
	 * strcuture of the index!
	 */
	if (lock_structure)
		XT_INDEX_WRITE_LOCK(ind, ot);
	else
		XT_INDEX_READ_LOCK(ind, ot);

	retry:
	/* Create a root node if required: */
	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root))) {
		/* Index is empty, create a new one: */
		ASSERT_NS(lock_structure);
		if (!xt_ind_reserve(ot, 1, NULL))
			goto failed;
		if (!idx_new_branch(ot, ind, &new_branch))
			goto failed;
		size = idx_write_branch_item(ind, ot->ot_ind_wbuf.tb_data, &key_value);
		XT_SET_DISK_2(ot->ot_ind_wbuf.tb_size_2, XT_MAKE_LEAF_SIZE(size));
		IDX_TRACE("%d-> %x\n", (int) new_branch, (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
		if (!xt_ind_write(ot, ind, new_branch, offsetof(XTIdxBranchDRec, tb_data) + size, (xtWord1 *) &ot->ot_ind_wbuf))
			goto failed_2;
		ind->mi_root = new_branch;
		goto done_ok;
	}

	/* Search down the tree for the insertion point. */
	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_XLOCK_LEAF, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &key_value, &result);
		if (result.sr_duplicate) {
			if (check_for_dups) {
				/* Duplicates are not allowed, at least one has been
				 * found...
				 */

				/* Leaf nodes (i_node_ref_size == 0) are write locked,
				 * non-leaf nodes are read locked.
				 */
				xt_ind_release(ot, ind, result.sr_item.i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE, &iref);

				if (!idx_check_duplicates(ot, ind, &key_value))
					goto failed;
				/* We have checked all the "duplicate" variations. None of them are
				 * relevant. So this will cause a correct insert.
				 */
				check_for_dups = FALSE;
				idx_newstack(&stack);
				goto retry;
			}
		}
		if (result.sr_found) {
			/* Node found, can happen during recovery of indexes! 
			 * We have found an exact match of both key and record.
			 */
			XTPageUnlockType	utype;
			xtBool				overwrite = FALSE;

			/* {LAZY-DEL-INDEX-ITEMS}
			 * If the item has been lazy deleted, then just overwrite!
			 */ 
			if (result.sr_row_id == (xtRowID) -1) {
				xtWord2 del_count;
	
				/* This is safe because we have an xlock on the leaf. */
				if ((del_count = iref.ir_block->cp_del_count))
					iref.ir_block->cp_del_count = del_count-1;
				overwrite = TRUE;
			}

			if (!result.sr_row_id && row_id) {
				/* {INDEX-RECOV_ROWID} Set the row-id
				 * during recovery, even if the index entry
				 * is not committed.
				 * It will be removed later by the sweeper.
				 */
				overwrite = TRUE;
			}

			if (overwrite) {
				idx_set_item_row_id(&iref, &result.sr_item, row_id);
				utype = result.sr_item.i_node_ref_size ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_W_UPDATE;
			}
			else
				utype = result.sr_item.i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE;
			xt_ind_release(ot, ind, utype, &iref);
			goto done_ok;
		}
		/* Stop when we get to a leaf: */
		if (!result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, result.sr_item.i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE, &iref);
		if (!idx_push(&stack, current, NULL))
			goto failed;
		current = result.sr_branch;
	}
	ASSERT_NS(XT_NODE_ID(current));
	
	/* Must be a leaf!: */
	ASSERT_NS(!result.sr_item.i_node_ref_size);

	updated = FALSE;
	if (ind->mi_lazy_delete && iref.ir_block->cp_del_count) {
		/* There are a number of possibilities:
		 * - We could just replace a lazy deleted slot.
		 * - We could compact and insert.
		 * - We could just insert
		 */

		if (result.sr_item.i_item_offset > 0) {
			/* Check if it can go into the previous node: */
			XTIdxResultRec	t_res;

			t_res.sr_item = result.sr_item;
			xt_prev_branch_item_fix(ot->ot_table, ind, iref.ir_branch, &t_res);
			if (t_res.sr_row_id != (xtRowID) -1)
				goto try_current;

			/* Yup, it can, but first check to see if it would be 
			 * better to put it in the current node.
			 * This is the case if the previous node key is not the
			 * same as the key we are adding...
			 */
			if (result.sr_item.i_item_offset < result.sr_item.i_total_size &&
				result.sr_row_id == (xtRowID) -1) {
				if (!idx_cmp_item_key_fix(&iref, &t_res.sr_item, &key_value))
					goto try_current;
			}

			idx_set_item_key_fix(&iref, &t_res.sr_item, &key_value);
			iref.ir_block->cp_del_count--;
			xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);
			goto done_ok;
		}

		try_current:
		if (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
			if (result.sr_row_id == (xtRowID) -1) {
				idx_set_item_key_fix(&iref, &result.sr_item, &key_value);
				iref.ir_block->cp_del_count--;
				xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);
				goto done_ok;
			}
		}

		/* Check if we must compact... 
		 * It makes no sense to split as long as there are lazy deleted items
		 * in the page. So, delete them if a split would otherwise be required!
		 */
		ASSERT_NS(key_value.sv_length + XT_RECORD_REF_SIZE == result.sr_item.i_item_size);
		if (result.sr_item.i_total_size + key_value.sv_length + XT_RECORD_REF_SIZE > XT_INDEX_PAGE_DATA_SIZE) {
			if (!idx_compact_leaf(ot, ind, &iref, &result.sr_item))
				goto failed;
			updated = TRUE;
		}
		
		/* Fall through to the insert code... */
		/* NOTE: if there were no lazy deleted items in the leaf, then
		 * idx_compact_leaf is a NOP. This is the only case in which it may not
		 * fall through and do the insert below.
		 *
		 * Normally, if the cp_del_count is correct then the insert
		 * will work below, and the assertion here will not fail.
		 *
		 * In this case, the xt_ind_release() will correctly indicate an update.
		 */
		ASSERT_NS(result.sr_item.i_total_size + key_value.sv_length + XT_RECORD_REF_SIZE <= XT_INDEX_PAGE_DATA_SIZE);
	}

	if (result.sr_item.i_total_size + key_value.sv_length + XT_RECORD_REF_SIZE <= XT_INDEX_PAGE_DATA_SIZE) {
		if (iref.ir_block->cb_handle_count) {
			if (!xt_ind_copy_on_write(&iref))
				goto failed_1;
		}

		idx_insert_leaf_item(ind, iref.ir_branch, &key_value, &result);
		IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
		ASSERT_NS(result.sr_item.i_total_size <= XT_INDEX_PAGE_DATA_SIZE);
		iref.ir_updated = TRUE;
		xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);
		goto done_ok;
	}

	/* Key does not fit. Must split the node.
	 * Make sure we have a structural lock:
	 */
	if (!lock_structure) {
		xt_ind_release(ot, ind, updated ? XT_UNLOCK_W_UPDATE : XT_UNLOCK_WRITE, &iref);
		XT_INDEX_UNLOCK(ind, ot);
		lock_structure = TRUE;
		goto lock_and_retry;
	}

	memcpy(&ot->ot_ind_wbuf, iref.ir_branch, offsetof(XTIdxBranchDRec, tb_data) + result.sr_item.i_total_size);
	idx_insert_leaf_item(ind, &ot->ot_ind_wbuf, &key_value, &result);
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));
	ASSERT_NS(result.sr_item.i_total_size > XT_INDEX_PAGE_DATA_SIZE);

	/* This is the number of potential writes. In other words, the total number
	 * of blocks that may be accessed.
	 *
	 * Note that this assume if a block is read and written soon after that the block
	 * will not be freed in-between (a safe assumption?)
	 */
	if (!xt_ind_reserve(ot, stack.s_top * 2 + 3, iref.ir_branch))
		goto failed_1;

	/* Key does not fit, must split... */
	idx_get_middle_branch_item(ind, &ot->ot_ind_wbuf, &key_value, &result);

	if (!idx_new_branch(ot, ind, &new_branch))
		goto failed_1;

	/* Copy and write the rest of the data to the new node: */
	new_size = result.sr_item.i_total_size - result.sr_item.i_item_offset - result.sr_item.i_item_size;
	new_branch_ptr = (XTIdxBranchDPtr) &ot->ot_ind_wbuf.tb_data[XT_INDEX_PAGE_DATA_SIZE];
	memmove(new_branch_ptr->tb_data, &ot->ot_ind_wbuf.tb_data[result.sr_item.i_item_offset + result.sr_item.i_item_size], new_size);

	XT_SET_DISK_2(new_branch_ptr->tb_size_2, XT_MAKE_LEAF_SIZE(new_size));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(new_branch), (int) XT_GET_DISK_2(new_branch_ptr->tb_size_2));
	if (!xt_ind_write(ot, ind, new_branch, offsetof(XTIdxBranchDRec, tb_data) + new_size, (xtWord1 *) new_branch_ptr))
		goto failed_2;

	/* Modify the first node: */
	XT_SET_DISK_2(ot->ot_ind_wbuf.tb_size_2, XT_MAKE_LEAF_SIZE(result.sr_item.i_item_offset));
	IDX_TRACE("%d-> %x\n", (int) XT_NODE_ID(current), (int) XT_GET_DISK_2(ot->ot_ind_wbuf.tb_size_2));

	if (iref.ir_block->cb_handle_count) {
		if (!xt_ind_copy_on_write(&iref))
			goto failed_2;
	}
	memcpy(iref.ir_branch, &ot->ot_ind_wbuf, offsetof(XTIdxBranchDRec, tb_data) + result.sr_item.i_item_offset);
	iref.ir_updated = TRUE;
	xt_ind_release(ot, ind, XT_UNLOCK_W_UPDATE, &iref);

	/* Insert the new branch into the parent node, using the new middle key value: */
	if (!idx_insert_node(ot, ind, &stack, &key_value, new_branch)) {
		// Index may be inconsistant now...
		idx_free_branch(ot, ind, new_branch);
		goto failed;
	}

#ifdef XT_TRACK_INDEX_UPDATES
	ASSERT_NS(ot->ot_ind_reserved >= ot->ot_ind_reads);
#endif

	done_ok:
	XT_INDEX_UNLOCK(ind, ot);

#ifdef DEBUG
	//printf("INSERT OK\n");
	//idx_check_index(ot, ind, TRUE);
#endif
	xt_ind_unreserve(ot);
	return OK;

	failed_2:
	idx_free_branch(ot, ind, new_branch);

	failed_1:
	xt_ind_release(ot, ind, updated ? XT_UNLOCK_W_UPDATE : XT_UNLOCK_WRITE, &iref);

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;

#ifdef DEBUG
	//printf("INSERT FAILED\n");
	//idx_check_index(ot, ind, TRUE);
#endif
	xt_ind_unreserve(ot);
	return FAILED;
}


/* Remove the given item in the node.
 * This is done by going down the tree to find a replacement
 * for the deleted item!
 */
static xtBool idx_remove_item_in_node(XTOpenTablePtr ot, XTIndexPtr ind, IdxBranchStackPtr stack, XTIndReferencePtr iref, XTIdxKeyValuePtr key_value)
{
	IdxStackItemPtr		delete_node;
	XTIdxResultRec		result;
	xtIndexNodeID		current;
	xtBool				lazy_delete_cleanup_required = FALSE;
	IdxStackItemPtr		current_top;

	delete_node = idx_top(stack);
	current = delete_node->i_branch;
	result.sr_item = delete_node->i_pos;

	/* Follow the branch after this item: */
	idx_next_branch_item(ot->ot_table, ind, iref->ir_branch, &result);
	xt_ind_release(ot, ind, iref->ir_updated ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_READ, iref);

	/* Go down the left-hand side until we reach a leaf: */
	while (XT_NODE_ID(current)) {
		current = result.sr_branch;
		if (!xt_ind_fetch(ot, ind, current, XT_XLOCK_LEAF, iref))
			return FAILED;
		idx_first_branch_item(ot->ot_table, ind, iref->ir_branch, &result);
		if (!result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, iref);
		if (!idx_push(stack, current, &result.sr_item))
			return FAILED;
	}

	ASSERT_NS(XT_NODE_ID(current));
	ASSERT_NS(!result.sr_item.i_node_ref_size);

	if (!xt_ind_reserve(ot, stack->s_top + 2, iref->ir_branch)) {
		xt_ind_release(ot, ind, XT_UNLOCK_WRITE, iref);
		return FAILED;
	}
	
	/* This code removes lazy deleted items from the leaf,
	 * before we promote an item to a leaf.
	 * This is not essential, but prevents lazy deleted
	 * items from being propogated up the tree.
	 */
	if (ind->mi_lazy_delete) {
		if (iref->ir_block->cp_del_count) {
			if (!idx_compact_leaf(ot, ind, iref, &result.sr_item))
				return FAILED;
		}
	}

	/* Crawl back up the stack trace, looking for a key
	 * that can be used to replace the deleted key.
	 *
	 * Any empty nodes on the way up can be removed!
	 */
	if (result.sr_item.i_total_size > 0) {
		/* There is a key in the leaf, extract it, and put it in the node: */
		memcpy(key_value->sv_key, &iref->ir_branch->tb_data[result.sr_item.i_item_offset], result.sr_item.i_item_size);
		/* This call also frees the iref.ir_branch page! */
		if (!idx_remove_branch_item_right(ot, ind, current, iref, &result.sr_item))
			return FAILED;
		if (!idx_replace_node_key(ot, ind, delete_node, stack, result.sr_item.i_item_size, key_value->sv_key))
			return FAILED;
		goto done_ok;
	}

	xt_ind_release(ot, ind, iref->ir_updated ? XT_UNLOCK_W_UPDATE : XT_UNLOCK_WRITE, iref);

	for (;;) {
		/* The current node/leaf is empty, remove it: */
		idx_free_branch(ot, ind, current);

		current_top = idx_pop(stack);
		current = current_top->i_branch;
		if (!xt_ind_fetch(ot, ind, current, XT_XLOCK_LEAF, iref))
			return FAILED;
		
		if (current_top == delete_node) {
			/* All children have been removed. Delete the key and done: */
			if (!idx_remove_branch_item_right(ot, ind, current, iref, &current_top->i_pos))
				return FAILED;
			goto done_ok;
		}

		if (current_top->i_pos.i_total_size > current_top->i_pos.i_node_ref_size) {
			/* Save the key: */
			memcpy(key_value->sv_key, &iref->ir_branch->tb_data[current_top->i_pos.i_item_offset], current_top->i_pos.i_item_size);
			/* This function also frees the cache page: */
			if (!idx_remove_branch_item_left(ot, ind, current, iref, &current_top->i_pos, &lazy_delete_cleanup_required))
				return FAILED;
			if (!idx_replace_node_key(ot, ind, delete_node, stack, current_top->i_pos.i_item_size, key_value->sv_key))
				return FAILED;
			/* */
			if (lazy_delete_cleanup_required) {
				if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, iref))
					return FAILED;
				if (!idx_remove_lazy_deleted_item_in_node(ot, ind, current, iref, key_value))
					return FAILED;
			}
			goto done_ok;
		}
		xt_ind_release(ot, ind, current_top->i_pos.i_node_ref_size ? XT_UNLOCK_READ : XT_UNLOCK_WRITE, iref);
	}

	done_ok:
#ifdef XT_TRACK_INDEX_UPDATES
	ASSERT_NS(ot->ot_ind_reserved >= ot->ot_ind_reads);
#endif
	return OK;
}

/*
 * This function assumes we have a lock on the structure of the index.
 */
static xtBool idx_remove_lazy_deleted_item_in_node(XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID current, XTIndReferencePtr iref, XTIdxKeyValuePtr key_value)
{
	IdxBranchStackRec	stack;
	XTIdxResultRec		result;

	/* Now remove all lazy deleted items in this node.... */
	idx_first_branch_item(ot->ot_table, ind, (XTIdxBranchDPtr) iref->ir_block->cb_data, &result);

	for (;;) {
		while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
			if (result.sr_row_id == (xtRowID) -1)
				goto remove_item;
			idx_next_branch_item(ot->ot_table, ind, (XTIdxBranchDPtr) iref->ir_block->cb_data, &result);
		}
		break;

		remove_item:

		idx_newstack(&stack);
		if (!idx_push(&stack, current, &result.sr_item)) {
			xt_ind_release(ot, ind, iref->ir_updated ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_READ, iref);
			return FAILED;
		}

		if (!idx_remove_item_in_node(ot, ind, &stack, iref, key_value))
			return FAILED;

		/* Go back up to the node we are trying to
		 * free of things.
		 */
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, iref))
			return FAILED;
		/* Load the data again: */
		idx_reload_item_fix(ind, iref->ir_branch, &result);
	}

	xt_ind_release(ot, ind, iref->ir_updated ? XT_UNLOCK_R_UPDATE : XT_UNLOCK_READ, iref);
	return OK;
}

static xtBool idx_delete(XTOpenTablePtr ot, XTIndexPtr ind, XTIdxKeyValuePtr key_value)
{
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;
	xtBool				lock_structure = FALSE;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	/* The index appears to have no root: */
	if (!XT_NODE_ID(ind->mi_root))
		lock_structure = TRUE;

	lock_and_retry:
	idx_newstack(&stack);

	if (lock_structure)
		XT_INDEX_WRITE_LOCK(ind, ot);
	else
		XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
		goto done_ok;

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_XLOCK_DEL_LEAF, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, key_value, &result);
		if (!result.sr_item.i_node_ref_size) {
			/* A leaf... */
			if (result.sr_found) {
				if (ind->mi_lazy_delete) {
					/* If the we have a W lock, then fetch decided that we
					 * need to compact the page.
					 * The decision is made by xt_idx_lazy_delete_on_leaf() 
					 */
					if (!iref.ir_xlock)
						idx_lazy_delete_branch_item(ot, ind, &iref, &result.sr_item);
					else {
						if (!iref.ir_block->cp_del_count) {
							if (!idx_remove_branch_item_right(ot, ind, current, &iref, &result.sr_item))
								goto failed;
						}
						else {
							if (!idx_lazy_remove_leaf_item_right(ot, ind, &iref, &result.sr_item))
								goto failed;
						}
					}
				}
				else {
					if (!idx_remove_branch_item_right(ot, ind, current, &iref, &result.sr_item))
						goto failed;
				}
			}
			else
				xt_ind_release(ot, ind, iref.ir_xlock ? XT_UNLOCK_WRITE : XT_UNLOCK_READ, &iref);
			goto done_ok;
		}
		if (!idx_push(&stack, current, &result.sr_item)) {
			xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
			goto failed;
		}
		if (result.sr_found)
			/* If we have found the key in a node: */
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		current = result.sr_branch;
	}

	/* Must be a non-leaf!: */
	ASSERT_NS(result.sr_item.i_node_ref_size);

	if (ind->mi_lazy_delete) {
		if (!idx_lazy_delete_on_node(ind, iref.ir_block, &result.sr_item)) {
			/* We need to remove some items from this node: */

			if (!lock_structure) {
				xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
				XT_INDEX_UNLOCK(ind, ot);
				lock_structure = TRUE;
				goto lock_and_retry;
			}

			idx_set_item_deleted(&iref, &result.sr_item);
			if (!idx_remove_lazy_deleted_item_in_node(ot, ind, current, &iref, key_value))
				goto failed;
			goto done_ok;
		}

		if (!ot->ot_table->tab_dic.dic_no_lazy_delete) {
			/* {LAZY-DEL-INDEX-ITEMS}
			 * We just set item to deleted, this is a significant time
			 * saver.
			 * But this item can only be cleaned up when all
			 * items on the node below are deleted.
			 */
			idx_lazy_delete_branch_item(ot, ind, &iref, &result.sr_item);
			goto done_ok;
		}
	}

	/* We will have to remove the key from a non-leaf node,
	 * which means we are changing the structure of the index.
	 * Make sure we have a structural lock:
	 */
	if (!lock_structure) {
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		XT_INDEX_UNLOCK(ind, ot);
		lock_structure = TRUE;
		goto lock_and_retry;
	}

	/* This is the item we will have to replace: */
	if (!idx_remove_item_in_node(ot, ind, &stack, &iref, key_value))
		goto failed;

	done_ok:
	XT_INDEX_UNLOCK(ind, ot);

#ifdef DEBUG
	//printf("DELETE OK\n");
	//idx_check_index(ot, ind, TRUE);
#endif
	xt_ind_unreserve(ot);
	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	xt_ind_unreserve(ot);
	return FAILED;
}

xtPublic xtBool xt_idx_delete(XTOpenTablePtr ot, XTIndexPtr ind, xtRecordID rec_id, xtWord1 *rec_buf)
{
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE + XT_MAX_RECORD_REF_SIZE];

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif

	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	key_value.sv_rec_id = rec_id;
	key_value.sv_row_id = 0;
	key_value.sv_key = key_buf;
	key_value.sv_length = myxt_create_key_from_row(ind, key_buf, rec_buf, NULL);

	if (!idx_delete(ot, ind, &key_value)) {
		if (idx_out_of_memory_failure(ot))
			goto retry_after_oom;
		return FAILED;
	}
	return OK;
}

xtPublic xtBool xt_idx_update_row_id(XTOpenTablePtr ot, XTIndexPtr ind, xtRecordID rec_id, xtRowID row_id, xtWord1 *rec_buf)
{
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE + XT_MAX_RECORD_REF_SIZE];

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
#ifdef CHECK_AND_PRINT
	idx_check_index(ot, ind, TRUE);
#endif
	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	key_value.sv_rec_id = rec_id;
	key_value.sv_row_id = 0;
	key_value.sv_key = key_buf;
	key_value.sv_length = myxt_create_key_from_row(ind, key_buf, rec_buf, NULL);

	/* NOTE: Only a read lock is required for this!!
	 *
	 * 09.05.2008 - This has changed because the dirty list now
	 * hangs on the index. And the dirty list may be updated
	 * by any change of the index.
	 * However, the advantage is that I should be able to read
	 * lock in the first phase of the flush.
	 *
	 * 18.02.2009 - This has changed again.
	 * I am now using a read lock, because this update does not
	 * require a structural change. In fact, it does not even
	 * need a WRITE LOCK on the page affected, because there
	 * is only ONE thread that can do this (the sweeper).
	 *
	 * This has the advantage that the sweeper (which uses this
	 * function, causes less conflicts.
	 *
	 * However, it does mean that the dirty list must be otherwise
	 * protected (which it now is be a spin lock - mi_dirty_lock).
	 *
	 * It also has the dissadvantage that I am going to have to
	 * take an xlock in the first phase of the flush.
	 */
	XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
		goto done_ok;

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &key_value, &result);
		if (result.sr_found || !result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		current = result.sr_branch;
	}

	if (result.sr_found) {
		/* TODO: Check that concurrent reads can handle this!
		 * assuming the write is not atomic.
		 */
		idx_set_item_row_id(&iref, &result.sr_item, row_id);
		xt_ind_release(ot, ind, XT_UNLOCK_R_UPDATE, &iref);
	}
	else
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);

	done_ok:
	XT_INDEX_UNLOCK(ind, ot);

#ifdef DEBUG
	//idx_check_index(ot, ind, TRUE);
	//idx_check_on_key(ot);
#endif
	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;
	return FAILED;
}

xtPublic void xt_idx_prep_key(XTIndexPtr ind, register XTIdxSearchKeyPtr search_key, int flags, xtWord1 *in_key_buf, size_t in_key_length)
{
	search_key->sk_key_value.sv_flags = flags;
	search_key->sk_key_value.sv_rec_id = 0;
	search_key->sk_key_value.sv_row_id = 0;
	search_key->sk_key_value.sv_key = search_key->sk_key_buf;
	search_key->sk_key_value.sv_length = myxt_create_key_from_key(ind, search_key->sk_key_buf, in_key_buf, in_key_length);
	search_key->sk_on_key = FALSE;
}

xtPublic xtBool xt_idx_research(XTOpenTablePtr ot, XTIndexPtr ind)
{
	XTIdxSearchKeyRec search_key;

	xt_ind_lock_handle(ot->ot_ind_rhandle);
	search_key.sk_key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	xt_get_record_ref(&ot->ot_ind_rhandle->ih_branch->tb_data[ot->ot_ind_state.i_item_offset + ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE],
		&search_key.sk_key_value.sv_rec_id, &search_key.sk_key_value.sv_row_id);
	search_key.sk_key_value.sv_key = search_key.sk_key_buf;
	search_key.sk_key_value.sv_length = ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE;
	search_key.sk_on_key = FALSE;
	memcpy(search_key.sk_key_buf, &ot->ot_ind_rhandle->ih_branch->tb_data[ot->ot_ind_state.i_item_offset], search_key.sk_key_value.sv_length);
	xt_ind_unlock_handle(ot->ot_ind_rhandle);
	return xt_idx_search(ot, ind, &search_key);
}

/*
 * Search for a given key and position the current pointer on the first
 * key in the list of duplicates. If the key is not found the current
 * pointer is placed at the first position after the key.
 */
xtPublic xtBool xt_idx_search(XTOpenTablePtr ot, XTIndexPtr ind, register XTIdxSearchKeyPtr search_key)
{
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, ot->ot_thread);
		ot->ot_ind_rhandle = NULL;
	}
#ifdef DEBUG
	//idx_check_index(ot, ind, TRUE);
#endif

	/* Calling from recovery, this is not the case.
	 * But the index read does not require a transaction!
	 * Only insert requires this to check for duplicates.
	if (!ot->ot_thread->st_xact_data) {
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_TRANSACTION);
		return FAILED;
	}
	*/

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	idx_newstack(&stack);

	ot->ot_curr_rec_id = 0;
	ot->ot_curr_row_id = 0;

	XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
		goto done_ok;

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &search_key->sk_key_value, &result);
		if (result.sr_found)
			/* If we have found the key in a node: */
			search_key->sk_on_key = TRUE;
		if (!result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		if (!idx_push(&stack, current, &result.sr_item))
			goto failed;
		current = result.sr_branch;
	}

	if (ind->mi_lazy_delete) {
		ignore_lazy_deleted_items:
		while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
			if (result.sr_row_id != (xtRowID) -1) {
				idx_still_on_key(ind, search_key, iref.ir_branch, &result.sr_item);
				break;
			}
			idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
		}
	}

	if (result.sr_item.i_item_offset == result.sr_item.i_total_size) {
		IdxStackItemPtr node;

		/* We are at the end of a leaf node.
		 * Go up the stack to find the start position of the next key.
		 * If we find none, then we are the end of the index.
		 */
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		while ((node = idx_pop(&stack))) {
			if (node->i_pos.i_item_offset < node->i_pos.i_total_size) {
				if (!xt_ind_fetch(ot, ind, node->i_branch, XT_LOCK_READ, &iref))
					goto failed;
				xt_get_res_record_ref(&iref.ir_branch->tb_data[node->i_pos.i_item_offset + node->i_pos.i_item_size - XT_RECORD_REF_SIZE], &result);

				if (ind->mi_lazy_delete) {
					result.sr_item = node->i_pos;
					if (result.sr_row_id == (xtRowID) -1) {
						/* If this node pointer is lazy deleted, then
						 * go down the next branch...
						 */
						idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);

						/* Go down to the bottom: */
						current = node->i_branch;
						while (XT_NODE_ID(current)) {
							xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
							if (!idx_push(&stack, current, &result.sr_item))
								goto failed;
							current = result.sr_branch;
							if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
								goto failed;
							idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
							if (!result.sr_item.i_node_ref_size)
								break;
						}

						goto ignore_lazy_deleted_items;
					}
					idx_still_on_key(ind, search_key, iref.ir_branch, &result.sr_item);
				}

				ot->ot_curr_rec_id = result.sr_rec_id;
				ot->ot_curr_row_id = result.sr_row_id;
				ot->ot_ind_state = node->i_pos;

				/* Convert the pointer to a handle which can be used in later operations: */
				ASSERT_NS(!ot->ot_ind_rhandle);
				if (!(ot->ot_ind_rhandle = xt_ind_get_handle(ot, ind, &iref)))
					goto failed;
				/* Keep the node for next operations: */
				/*
				branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref.ir_branch->tb_size_2));
				memcpy(&ot->ot_ind_rbuf, iref.ir_branch, branch_size);
				xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
				*/
				break;
			}
		}
	}
	else {
		ot->ot_curr_rec_id = result.sr_rec_id;
		ot->ot_curr_row_id = result.sr_row_id;
		ot->ot_ind_state = result.sr_item;

		/* Convert the pointer to a handle which can be used in later operations: */
		ASSERT_NS(!ot->ot_ind_rhandle);
		if (!(ot->ot_ind_rhandle = xt_ind_get_handle(ot, ind, &iref)))
			goto failed;
		/* Keep the node for next operations: */
		/*
		branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref.ir_branch->tb_size_2));
		memcpy(&ot->ot_ind_rbuf, iref.ir_branch, branch_size);
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		*/
	}

	done_ok:
	XT_INDEX_UNLOCK(ind, ot);

#ifdef DEBUG
	//idx_check_index(ot, ind, TRUE);
	//idx_check_on_key(ot);
#endif
	ASSERT_NS(iref.ir_xlock == 2);
	ASSERT_NS(iref.ir_updated == 2);
	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;
	ASSERT_NS(iref.ir_xlock == 2);
	ASSERT_NS(iref.ir_updated == 2);
	return FAILED;
}

xtPublic xtBool xt_idx_search_prev(XTOpenTablePtr ot, XTIndexPtr ind, register XTIdxSearchKeyPtr search_key)
{
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	XTIdxResultRec		result;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, ot->ot_thread);
		ot->ot_ind_rhandle = NULL;
	}
#ifdef DEBUG
	//idx_check_index(ot, ind, TRUE);
#endif

	/* see the comment above in xt_idx_search */
	/*
	if (!ot->ot_thread->st_xact_data) {
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_TRANSACTION);
		return FAILED;
	}
	*/

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	idx_newstack(&stack);

	ot->ot_curr_rec_id = 0;
	ot->ot_curr_row_id = 0;

	XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
		goto done_ok;

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &search_key->sk_key_value, &result);
		if (result.sr_found)
			/* If we have found the key in a node: */
			search_key->sk_on_key = TRUE;
		if (!result.sr_item.i_node_ref_size)
			break;
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		if (!idx_push(&stack, current, &result.sr_item))
			goto failed;
		current = result.sr_branch;
	}

	if (result.sr_item.i_item_offset == 0) {
		IdxStackItemPtr node;

		search_up_stack:
		/* We are at the start of a leaf node.
		 * Go up the stack to find the start position of the next key.
		 * If we find none, then we are the end of the index.
		 */
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		while ((node = idx_pop(&stack))) {
			if (node->i_pos.i_item_offset > node->i_pos.i_node_ref_size) {
				if (!xt_ind_fetch(ot, ind, node->i_branch, XT_LOCK_READ, &iref))
					goto failed;
				result.sr_item = node->i_pos;
				ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);

				if (ind->mi_lazy_delete) {
					if (result.sr_row_id == (xtRowID) -1) {
						/* Go down to the bottom, in order to scan the leaf backwards: */
						current = node->i_branch;
						while (XT_NODE_ID(current)) {
							xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
							if (!idx_push(&stack, current, &result.sr_item))
								goto failed;
							current = result.sr_branch;
							if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
								goto failed;
							ind->mi_last_item(ot->ot_table, ind, iref.ir_branch, &result);
							if (!result.sr_item.i_node_ref_size)
								break;
						}

						/* If the leaf empty we have to go up the stack again... */
						if (result.sr_item.i_total_size == 0)
							goto search_up_stack;

						goto scan_back_in_leaf;
					}
				}

				goto record_found;
			}
		}
		goto done_ok;
	}

	/* We must just step once to the left in this leaf node... */
	ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);

	if (ind->mi_lazy_delete) {
		scan_back_in_leaf:
		while (result.sr_row_id == (xtRowID) -1) {
			if (result.sr_item.i_item_offset == 0)
				goto search_up_stack;
			ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);
		}
		idx_still_on_key(ind, search_key, iref.ir_branch, &result.sr_item);
	}

	record_found:
	ot->ot_curr_rec_id = result.sr_rec_id;
	ot->ot_curr_row_id = result.sr_row_id;
	ot->ot_ind_state = result.sr_item;

	/* Convert to handle for later operations: */
	ASSERT_NS(!ot->ot_ind_rhandle);
	if (!(ot->ot_ind_rhandle = xt_ind_get_handle(ot, ind, &iref)))
		goto failed;
	/* Keep a copy of the node for previous operations... */
	/*
	u_int branch_size;

	branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref.ir_branch->tb_size_2));
	memcpy(&ot->ot_ind_rbuf, iref.ir_branch, branch_size);
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	*/

	done_ok:
	XT_INDEX_UNLOCK(ind, ot);

#ifdef DEBUG
	//idx_check_index(ot, ind, TRUE);
	//idx_check_on_key(ot);
#endif
	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;
	return FAILED;
}

/*
 * Copy the current index value to the record.
 */
xtPublic xtBool xt_idx_read(XTOpenTablePtr ot, XTIndexPtr ind, xtWord1 *rec_buf)
{
	xtWord1	*bitem;

#ifdef DEBUG
	//idx_check_on_key(ot);
#endif
	xt_ind_lock_handle(ot->ot_ind_rhandle);
	bitem = ot->ot_ind_rhandle->ih_branch->tb_data + ot->ot_ind_state.i_item_offset;
	myxt_create_row_from_key(ot, ind, bitem, ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE, rec_buf);
	xt_ind_unlock_handle(ot->ot_ind_rhandle);
	return OK;
}

xtPublic xtBool xt_idx_next(register XTOpenTablePtr ot, register XTIndexPtr ind, register XTIdxSearchKeyPtr search_key)
{
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE];
	XTIdxResultRec		result;
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	ASSERT_NS(ot->ot_ind_rhandle);
	xt_ind_lock_handle(ot->ot_ind_rhandle);
	result.sr_item = ot->ot_ind_state;
	if (!result.sr_item.i_node_ref_size && 
		result.sr_item.i_item_offset < result.sr_item.i_total_size && 
		ot->ot_ind_rhandle->ih_cache_reference) {
		XTIdxItemRec prev_item;

		key_value.sv_key = &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset];
		key_value.sv_length = result.sr_item.i_item_size - XT_RECORD_REF_SIZE;

		prev_item = result.sr_item;
		idx_next_branch_item(ot->ot_table, ind, ot->ot_ind_rhandle->ih_branch, &result);

		if (ind->mi_lazy_delete) {
			while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
				if (result.sr_row_id != (xtRowID) -1)
					break;
				prev_item = result.sr_item;
				idx_next_branch_item(ot->ot_table, ind, ot->ot_ind_rhandle->ih_branch, &result);
			}
		}

		if (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
			/* Still on key? */
			idx_still_on_key(ind, search_key, ot->ot_ind_rhandle->ih_branch, &result.sr_item);
			xt_ind_unlock_handle(ot->ot_ind_rhandle);
			goto checked_on_key;
		}

		result.sr_item = prev_item;
	}

	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	xt_get_record_ref(&ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset + result.sr_item.i_item_size - XT_RECORD_REF_SIZE], &key_value.sv_rec_id, &key_value.sv_row_id);
	key_value.sv_key = key_buf;
	key_value.sv_length = result.sr_item.i_item_size - XT_RECORD_REF_SIZE;
	memcpy(key_buf, &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset], key_value.sv_length);
	xt_ind_release_handle(ot->ot_ind_rhandle, TRUE, ot->ot_thread);
	ot->ot_ind_rhandle = NULL;

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	idx_newstack(&stack);

	XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root))) {
		XT_INDEX_UNLOCK(ind, ot);
		return OK;
	}

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &key_value, &result);
		if (result.sr_item.i_node_ref_size) {
			if (result.sr_found) {
				/* If we have found the key in a node: */
				idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);

				/* Go down to the bottom: */
				while (XT_NODE_ID(current)) {
					xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
					if (!idx_push(&stack, current, &result.sr_item))
						goto failed;
					current = result.sr_branch;
					if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
						goto failed;
					idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
					if (!result.sr_item.i_node_ref_size)
						break;
				}

				/* Is the leaf not empty, then we are done... */
				break;
			}
		}
		else {
			/* We have reached the leaf. */
			if (result.sr_found)
				/* If we have found the key in a leaf: */
				idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
			/* If we did not find the key (although we should have). Our
			 * position is automatically the next one.
			 */
			break;
		}
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		if (!idx_push(&stack, current, &result.sr_item))
			goto failed;
		current = result.sr_branch;
	}

	if (ind->mi_lazy_delete) {
		ignore_lazy_deleted_items:
		while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
			if (result.sr_row_id != (xtRowID) -1)
				break;
			idx_next_branch_item(NULL, ind, iref.ir_branch, &result);
		}
	}

	/* Check the current position in a leaf: */
	if (result.sr_item.i_item_offset == result.sr_item.i_total_size) {
		/* At the end: */
		IdxStackItemPtr node;

		/* We are at the end of a leaf node.
		 * Go up the stack to find the start poition of the next key.
		 * If we find none, then we are the end of the index.
		 */
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		while ((node = idx_pop(&stack))) {
			if (node->i_pos.i_item_offset < node->i_pos.i_total_size) {
				if (!xt_ind_fetch(ot, ind, node->i_branch, XT_LOCK_READ, &iref))
					goto failed;
				result.sr_item = node->i_pos;
				xt_get_res_record_ref(&iref.ir_branch->tb_data[result.sr_item.i_item_offset + result.sr_item.i_item_size - XT_RECORD_REF_SIZE], &result);

				if (ind->mi_lazy_delete) {
					if (result.sr_row_id == (xtRowID) -1) {
						/* If this node pointer is lazy deleted, then
						 * go down the next branch...
						 */
						idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);

						/* Go down to the bottom: */
						current = node->i_branch;
						while (XT_NODE_ID(current)) {
							xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
							if (!idx_push(&stack, current, &result.sr_item))
								goto failed;
							current = result.sr_branch;
							if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
								goto failed;
							idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
							if (!result.sr_item.i_node_ref_size)
								break;
						}

						/* And scan the leaf... */
						goto ignore_lazy_deleted_items;
					}
				}

				goto unlock_check_on_key;
			}
		}

		/* No more keys: */
		if (search_key)
			search_key->sk_on_key = FALSE;
		ot->ot_curr_rec_id = 0;
		ot->ot_curr_row_id = 0;
		XT_INDEX_UNLOCK(ind, ot);
		return OK;
	}

	unlock_check_on_key:

	ASSERT_NS(!ot->ot_ind_rhandle);
	if (!(ot->ot_ind_rhandle = xt_ind_get_handle(ot, ind, &iref)))
		goto failed;
	/*
	u_int branch_size;

	branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref.ir_branch->tb_size_2));
	memcpy(&ot->ot_ind_rbuf, iref.ir_branch, branch_size);
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	*/

	XT_INDEX_UNLOCK(ind, ot);

	/* Still on key? */
	if (search_key && search_key->sk_on_key) {
		/* GOTCHA: As a short-cut I was using a length compare
		 * and a memcmp() here to check whether we as still on
		 * the original search key.
		 * This does not work because it does not take into account
		 * trialing spaces (which are ignored in comparison).
		 * So lengths can be different, but values still equal.
		 * 
		 * NOTE: We have to use the original search flags for
		 * this compare.
		 */
		xt_ind_lock_handle(ot->ot_ind_rhandle);
		search_key->sk_on_key = myxt_compare_key(ind, search_key->sk_key_value.sv_flags, search_key->sk_key_value.sv_length,
			search_key->sk_key_value.sv_key, &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset]) == 0;
		xt_ind_unlock_handle(ot->ot_ind_rhandle);
	}

	checked_on_key:
	ot->ot_curr_rec_id = result.sr_rec_id;
	ot->ot_curr_row_id = result.sr_row_id;
	ot->ot_ind_state = result.sr_item;

	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;
	return FAILED;
}

xtPublic xtBool xt_idx_prev(register XTOpenTablePtr ot, register XTIndexPtr ind, register XTIdxSearchKeyPtr search_key)
{
	XTIdxKeyValueRec	key_value;
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE];
	XTIdxResultRec		result;
	IdxBranchStackRec	stack;
	xtIndexNodeID		current;
	XTIndReferenceRec	iref;
	IdxStackItemPtr		node;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	ASSERT_NS(ot->ot_ind_rhandle);
	xt_ind_lock_handle(ot->ot_ind_rhandle);
	result.sr_item = ot->ot_ind_state;
	if (!result.sr_item.i_node_ref_size && result.sr_item.i_item_offset > 0) {
		key_value.sv_key = &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset];
		key_value.sv_length = result.sr_item.i_item_size - XT_RECORD_REF_SIZE;

		ind->mi_prev_item(ot->ot_table, ind, ot->ot_ind_rhandle->ih_branch, &result);

		if (ind->mi_lazy_delete) {
			while (result.sr_row_id == (xtRowID) -1) {
				if (result.sr_item.i_item_offset == 0)
					goto research;
				ind->mi_prev_item(ot->ot_table, ind, ot->ot_ind_rhandle->ih_branch, &result);
			}
		}

		idx_still_on_key(ind, search_key, ot->ot_ind_rhandle->ih_branch, &result.sr_item);

		xt_ind_unlock_handle(ot->ot_ind_rhandle);
		goto checked_on_key;
	}

	research:
	key_value.sv_flags = XT_SEARCH_WHOLE_KEY;
	key_value.sv_rec_id = ot->ot_curr_rec_id;
	key_value.sv_row_id = 0;
	key_value.sv_key = key_buf;
	key_value.sv_length = result.sr_item.i_item_size - XT_RECORD_REF_SIZE;
	memcpy(key_buf, &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset], key_value.sv_length);
	xt_ind_release_handle(ot->ot_ind_rhandle, TRUE, ot->ot_thread);
	ot->ot_ind_rhandle = NULL;

	retry_after_oom:
#ifdef XT_TRACK_INDEX_UPDATES
	ot->ot_ind_changed = 0;
#endif
	idx_newstack(&stack);

	XT_INDEX_READ_LOCK(ind, ot);

	if (!(XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root))) {
		XT_INDEX_UNLOCK(ind, ot);
		return OK;
	}

	while (XT_NODE_ID(current)) {
		if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
			goto failed;
		ind->mi_scan_branch(ot->ot_table, ind, iref.ir_branch, &key_value, &result);
		if (result.sr_item.i_node_ref_size) {
			if (result.sr_found) {
				/* If we have found the key in a node: */

				search_down_stack:
				/* Go down to the bottom: */
				while (XT_NODE_ID(current)) {
					xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
					if (!idx_push(&stack, current, &result.sr_item))
						goto failed;
					current = result.sr_branch;
					if (!xt_ind_fetch(ot, ind, current, XT_LOCK_READ, &iref))
						goto failed;
					ind->mi_last_item(ot->ot_table, ind, iref.ir_branch, &result);
					if (!result.sr_item.i_node_ref_size)
						break;
				}

				/* If the leaf empty we have to go up the stack again... */
				if (result.sr_item.i_total_size == 0)
					break;

				if (ind->mi_lazy_delete) {
					while (result.sr_row_id == (xtRowID) -1) {
						if (result.sr_item.i_item_offset == 0)
							goto search_up_stack;
						ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);
					}
				}

				goto unlock_check_on_key;
			}
		}
		else {
			/* We have reached the leaf.
			 * Whether we found the key or not, we have
			 * to move one to the left.
			 */
			if (result.sr_item.i_item_offset == 0)
				break;
			ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);

			if (ind->mi_lazy_delete) {
				while (result.sr_row_id == (xtRowID) -1) {
					if (result.sr_item.i_item_offset == 0)
						goto search_up_stack;
					ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);
				}
			}

			goto unlock_check_on_key;
		}
		xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
		if (!idx_push(&stack, current, &result.sr_item))
			goto failed;
		current = result.sr_branch;
	}

	search_up_stack:
	/* We are at the start of a leaf node.
	 * Go up the stack to find the start poition of the next key.
	 * If we find none, then we are the end of the index.
	 */
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	while ((node = idx_pop(&stack))) {
		if (node->i_pos.i_item_offset > node->i_pos.i_node_ref_size) {
			if (!xt_ind_fetch(ot, ind, node->i_branch, XT_LOCK_READ, &iref))
				goto failed;
			result.sr_item = node->i_pos;
			ind->mi_prev_item(ot->ot_table, ind, iref.ir_branch, &result);

			if (ind->mi_lazy_delete) {
				if (result.sr_row_id == (xtRowID) -1) {
					current = node->i_branch;
					goto search_down_stack;
				}
			}

			goto unlock_check_on_key;
		}
	}

	/* No more keys: */
	if (search_key)
		search_key->sk_on_key = FALSE;
	ot->ot_curr_rec_id = 0;
	ot->ot_curr_row_id = 0;

	XT_INDEX_UNLOCK(ind, ot);
	return OK;

	unlock_check_on_key:
	ASSERT_NS(!ot->ot_ind_rhandle);
	if (!(ot->ot_ind_rhandle = xt_ind_get_handle(ot, ind, &iref)))
		goto failed;
	/*
	u_int branch_size;

	branch_size = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(iref.ir_branch->tb_size_2));
	memcpy(&ot->ot_ind_rbuf, iref.ir_branch, branch_size);
	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	*/

	XT_INDEX_UNLOCK(ind, ot);

	/* Still on key? */
	if (search_key && search_key->sk_on_key) {
		xt_ind_lock_handle(ot->ot_ind_rhandle);
		search_key->sk_on_key = myxt_compare_key(ind, search_key->sk_key_value.sv_flags, search_key->sk_key_value.sv_length,
			search_key->sk_key_value.sv_key, &ot->ot_ind_rhandle->ih_branch->tb_data[result.sr_item.i_item_offset]) == 0;
		xt_ind_unlock_handle(ot->ot_ind_rhandle);
	}

	checked_on_key:
	ot->ot_curr_rec_id = result.sr_rec_id;
	ot->ot_curr_row_id = result.sr_row_id;
	ot->ot_ind_state = result.sr_item;
	return OK;

	failed:
	XT_INDEX_UNLOCK(ind, ot);
	if (idx_out_of_memory_failure(ot))
		goto retry_after_oom;
	return FAILED;
}

/* Return TRUE if the record matches the current index search! */
xtPublic xtBool xt_idx_match_search(register XTOpenTablePtr XT_UNUSED(ot), register XTIndexPtr ind, register XTIdxSearchKeyPtr search_key, xtWord1 *buf, int mode)
{
	int		r;
	xtWord1	key_buf[XT_INDEX_MAX_KEY_SIZE];

	myxt_create_key_from_row(ind, key_buf, (xtWord1 *) buf, NULL);
	r = myxt_compare_key(ind, search_key->sk_key_value.sv_flags, search_key->sk_key_value.sv_length, search_key->sk_key_value.sv_key, key_buf);
	switch (mode) {
		case XT_S_MODE_MATCH:
			return r == 0;
		case XT_S_MODE_NEXT:
			return r <= 0;
		case XT_S_MODE_PREV:
			return r >= 0;
	}
	return FALSE;
}

static void idx_set_index_selectivity(XTThreadPtr self, XTOpenTablePtr ot, XTIndexPtr ind)
{
	static const xtRecordID MAX_RECORDS = 100;

	XTIdxSearchKeyRec	search_key;
	XTIndexSegPtr		key_seg;
	u_int				select_count[2] = {0, 0};
	xtWord1				key_buf[XT_INDEX_MAX_KEY_SIZE];
	u_int				key_len;
	xtWord1				*next_key_buf;
	u_int				next_key_len;
	u_int				curr_len;
	u_int				diff;
	u_int				j, i;
	/* these 2 vars are used to check the overlapping if we have < 200 records */
	xtRecordID			last_rec = 0;		/* last record accounted in this iteration */
	xtRecordID			last_iter_rec = 0;	/* last record accounted in the previous iteration */

	xtBool	(* xt_idx_iterator[2])(
		register struct XTOpenTable *ot, register struct XTIndex *ind, register XTIdxSearchKeyPtr search_key) = {

		xt_idx_next,
		xt_idx_prev
	};

	xtBool	(* xt_idx_begin[2])(
		struct XTOpenTable *ot, struct XTIndex *ind, register XTIdxSearchKeyPtr search_key) = {
	
		xt_idx_search,
		xt_idx_search_prev
	};

	ind->mi_select_total = 0;
	key_seg = ind->mi_seg;
	for (i=0; i < ind->mi_seg_count; key_seg++, i++) {
		key_seg->is_selectivity = 1;
		key_seg->is_recs_in_range = 1;
	}

	for (j=0; j < 2; j++) {
		xt_idx_prep_key(ind, &search_key, j == 0 ? XT_SEARCH_FIRST_FLAG : XT_SEARCH_AFTER_LAST_FLAG, NULL, 0);
		if (!(xt_idx_begin[j])(ot, ind, &search_key))
			goto failed;

		/* Initialize the buffer with the first index valid index entry: */
		while (!select_count[j] && ot->ot_curr_rec_id != last_iter_rec) {
			if (ot->ot_curr_row_id) {
				select_count[j]++;
				last_rec = ot->ot_curr_rec_id;

				key_len = ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE;
				xt_ind_unlock_handle(ot->ot_ind_rhandle);
				memcpy(key_buf, ot->ot_ind_rhandle->ih_branch->tb_data + ot->ot_ind_state.i_item_offset, key_len);
				xt_ind_unlock_handle(ot->ot_ind_rhandle);
			}
			if (!(xt_idx_iterator[j])(ot, ind, &search_key))
				goto failed_1;
		}

		while (select_count[j] < MAX_RECORDS && ot->ot_curr_rec_id != last_iter_rec) {
			/* Check if the index entry is committed: */
			if (ot->ot_curr_row_id) {
				xt_ind_lock_handle(ot->ot_ind_rhandle);
				select_count[j]++;
				last_rec = ot->ot_curr_rec_id;

				next_key_len = ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE;
				next_key_buf = ot->ot_ind_rhandle->ih_branch->tb_data + ot->ot_ind_state.i_item_offset;
			
				curr_len = 0;
				diff = FALSE;
				key_seg = ind->mi_seg;
				for (i=0; i < ind->mi_seg_count; key_seg++, i++) {
					curr_len += myxt_key_seg_length(key_seg, curr_len, key_buf);
					if (!diff && myxt_compare_key(ind, 0, curr_len, key_buf, next_key_buf) != 0)
						diff = i+1;
					if (diff)
						key_seg->is_selectivity++;
				}

				/* Store the key for the next comparison: */
				key_len = next_key_len;
				memcpy(key_buf, next_key_buf, key_len);
				xt_ind_unlock_handle(ot->ot_ind_rhandle);
			}

			if (!(xt_idx_iterator[j])(ot, ind, &search_key))
				goto failed_1;
		}

		last_iter_rec = last_rec;

		if (ot->ot_ind_rhandle) {
			xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, self);
			ot->ot_ind_rhandle = NULL;
		}
	}

	u_int select_total;

	select_total = select_count[0] + select_count[1];
	if (select_total) {
		u_int recs;

		ind->mi_select_total = select_total;
		key_seg = ind->mi_seg;
		for (i=0; i < ind->mi_seg_count; key_seg++, i++) {
			recs = (u_int) ((double) select_total / (double) key_seg->is_selectivity + (double) 0.5);
			key_seg->is_recs_in_range = recs ? recs : 1;
		}
	}
	return;

	failed_1:
	xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, self);
	ot->ot_ind_rhandle = NULL;

	failed:
	ot->ot_table->tab_dic.dic_disable_index = XT_INDEX_CORRUPTED;
	xt_log_and_clear_exception_ns();
	return;
}

xtPublic void xt_ind_set_index_selectivity(XTThreadPtr self, XTOpenTablePtr ot)
{
	XTTableHPtr		tab = ot->ot_table;
	XTIndexPtr		*ind;
	u_int			i;

	if (!tab->tab_dic.dic_disable_index) {
		for (i=0, ind=tab->tab_dic.dic_keys; i<tab->tab_dic.dic_key_count; i++, ind++)
			idx_set_index_selectivity(self, ot, *ind);
	}
}

/*
 * -----------------------------------------------------------------------
 * Print a b-tree
 */

#ifdef TEST_CODE
static void idx_check_on_key(XTOpenTablePtr ot)
{
	u_int		offs = ot->ot_ind_state.i_item_offset + ot->ot_ind_state.i_item_size - XT_RECORD_REF_SIZE;
	xtRecordID	rec_id;
	xtRowID		row_id;
	
	if (ot->ot_curr_rec_id && ot->ot_ind_state.i_item_offset < ot->ot_ind_state.i_total_size) {
		xt_get_record_ref(&ot->ot_ind_rbuf.tb_data[offs], &rec_id, &row_id);
		
		ASSERT_NS(rec_id == ot->ot_curr_rec_id);
	}
}
#endif

static void idx_check_space(int depth)
{
	for (int i=0; i<depth; i++)
		printf(". ");
}

static u_int idx_check_node(XTOpenTablePtr ot, XTIndexPtr ind, int depth, xtIndexNodeID node)
{
	XTIdxResultRec		result;
	u_int				block_count = 1;
	XTIndReferenceRec	iref;

#ifdef DEBUG
	iref.ir_xlock = 2;
	iref.ir_updated = 2;
#endif
	ASSERT_NS(XT_NODE_ID(node) <= XT_NODE_ID(ot->ot_table->tab_ind_eof));
	if (!xt_ind_fetch(ot, ind, node, XT_LOCK_READ, &iref))
		return 0;

	idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
	ASSERT_NS(result.sr_item.i_total_size + offsetof(XTIdxBranchDRec, tb_data) <= XT_INDEX_PAGE_SIZE);
	if (result.sr_item.i_node_ref_size) {
		idx_check_space(depth);
		printf("%04d -->\n", (int) XT_NODE_ID(result.sr_branch));
#ifdef TRACK_ACTIVITY
		track_block_exists(result.sr_branch);
#endif
		block_count += idx_check_node(ot, ind, depth+1, result.sr_branch);
	}

	while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
#ifdef CHECK_PRINTS_RECORD_REFERENCES
		idx_check_space(depth);
		if (result.sr_item.i_item_size == 12) {
			/* Assume this is a NOT-NULL INT!: */
			xtWord4 val = XT_GET_DISK_4(&iref.ir_branch->tb_data[result.sr_item.i_item_offset]);
			printf("(%6d) ", (int) val);
		}
		printf("rec=%d row=%d ", (int) result.sr_rec_id, (int) result.sr_row_id);
		printf("\n");
#endif
		idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
		if (result.sr_item.i_node_ref_size) {
			idx_check_space(depth);
			printf("%04d -->\n", (int) XT_NODE_ID(result.sr_branch));
#ifdef TRACK_ACTIVITY
			track_block_exists(result.sr_branch);
#endif
			block_count += idx_check_node(ot, ind, depth+1, result.sr_branch);
		}
	}

	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
	return block_count;
}

static u_int idx_check_index(XTOpenTablePtr ot, XTIndexPtr ind, xtBool with_lock)
{
	xtIndexNodeID			current;
	u_int					block_count = 0;
	u_int					i;

	if (with_lock)
		XT_INDEX_WRITE_LOCK(ind, ot);

	printf("INDEX (%d) %04d ---------------------------------------\n", (int) ind->mi_index_no, (int) XT_NODE_ID(ind->mi_root));
	if ((XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root))) {
#ifdef TRACK_ACTIVITY
		track_block_exists(ind->mi_root);
#endif
		block_count = idx_check_node(ot, ind, 0, current);
	}

	if (ind->mi_free_list && ind->mi_free_list->fl_free_count) {
		printf("INDEX (%d) FREE ---------------------------------------", (int) ind->mi_index_no);
		ASSERT_NS(ind->mi_free_list->fl_start == 0);
		for (i=0; i<ind->mi_free_list->fl_free_count; i++) {
			if ((i % 40) == 0)
				printf("\n");
			block_count++;
#ifdef TRACK_ACTIVITY
			track_block_exists(ind->mi_free_list->fl_page_id[i]);
#endif
			printf("%2d ", (int) XT_NODE_ID(ind->mi_free_list->fl_page_id[i]));
		}
		if ((i % 40) != 0)
			printf("\n");
	}

	if (with_lock)
		XT_INDEX_UNLOCK(ind, ot);
	return block_count;

}

xtPublic void xt_check_indices(XTOpenTablePtr ot)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTIndexPtr				*ind;
	xtIndexNodeID			current;
	XTIndFreeBlockRec		free_block;
	u_int					ind_count, block_count = 0;
	u_int					free_count = 0;
	u_int					i, j;

	xt_lock_mutex_ns(&tab->tab_ind_flush_lock);
	printf("CHECK INDICES %s ==============================\n", tab->tab_name->ps_path);
#ifdef TRACK_ACTIVITY
	track_reset_missing();
#endif

	ind = tab->tab_dic.dic_keys;
	for (u_int k=0; k<tab->tab_dic.dic_key_count; k++, ind++) {
		ind_count = idx_check_index(ot, *ind, TRUE);
		block_count += ind_count;
	}

	xt_lock_mutex_ns(&tab->tab_ind_lock);
	printf("\nFREE: ---------------------------------------\n");
	if (tab->tab_ind_free_list) {
		XTIndFreeListPtr	ptr;

		ptr = tab->tab_ind_free_list;
		while (ptr) {
			printf("Memory List:");
			i = 0;
			for (j=ptr->fl_start; j<ptr->fl_free_count; j++, i++) {
				if ((i % 40) == 0)
					printf("\n");
				free_count++;
#ifdef TRACK_ACTIVITY
				track_block_exists(ptr->fl_page_id[j]);
#endif
				printf("%2d ", (int) XT_NODE_ID(ptr->fl_page_id[j]));
			}
			if ((i % 40) != 0)
				printf("\n");
			ptr = ptr->fl_next_list;
		}
	}

	current = tab->tab_ind_free;
	if (XT_NODE_ID(current)) {
		u_int k = 0;
		printf("Disk List:");
		while (XT_NODE_ID(current)) {
			if ((k % 40) == 0)
				printf("\n");
			free_count++;
#ifdef TRACK_ACTIVITY
			track_block_exists(current);
#endif
			printf("%d ", (int) XT_NODE_ID(current));
			if (!xt_ind_read_bytes(ot, *ind, current, sizeof(XTIndFreeBlockRec), (xtWord1 *) &free_block)) {
				xt_log_and_clear_exception_ns();
				break;
			}
			XT_NODE_ID(current) = (xtIndexNodeID) XT_GET_DISK_8(free_block.if_next_block_8);
			k++;
		}
		if ((k % 40) != 0)
			printf("\n");
	}
	printf("\n-----------------------------\n");
	printf("used blocks %d + free blocks %d = %d\n", block_count, free_count, block_count + free_count);
	printf("EOF = %"PRIu64", total blocks = %d\n", (xtWord8) xt_ind_node_to_offset(tab, tab->tab_ind_eof), (int) (XT_NODE_ID(tab->tab_ind_eof) - 1));
	printf("-----------------------------\n");
	xt_unlock_mutex_ns(&tab->tab_ind_lock);
#ifdef TRACK_ACTIVITY
	track_dump_missing(tab->tab_ind_eof);
	printf("===================================================\n");
	track_dump_all((u_int) (XT_NODE_ID(tab->tab_ind_eof) - 1));
#endif
	printf("===================================================\n");
	xt_unlock_mutex_ns(&tab->tab_ind_flush_lock);
}

/*
 * -----------------------------------------------------------------------
 * Load index
 */

static void idx_load_node(XTThreadPtr self, XTOpenTablePtr ot, XTIndexPtr ind, xtIndexNodeID node)
{
	XTIdxResultRec		result;
	XTIndReferenceRec	iref;

	ASSERT_NS(XT_NODE_ID(node) <= XT_NODE_ID(ot->ot_table->tab_ind_eof));
	if (!xt_ind_fetch(ot, ind, node, XT_LOCK_READ, &iref))
		xt_throw(self);

	idx_first_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
	if (result.sr_item.i_node_ref_size)
		idx_load_node(self, ot, ind, result.sr_branch);
	while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
		idx_next_branch_item(ot->ot_table, ind, iref.ir_branch, &result);
		if (result.sr_item.i_node_ref_size)
			idx_load_node(self, ot, ind, result.sr_branch);
	}

	xt_ind_release(ot, ind, XT_UNLOCK_READ, &iref);
}

xtPublic void xt_load_indices(XTThreadPtr self, XTOpenTablePtr ot)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTIndexPtr				*ind_ptr;
	XTIndexPtr				ind;
	xtIndexNodeID			current;

	xt_lock_mutex(self, &tab->tab_ind_flush_lock);
	pushr_(xt_unlock_mutex, &tab->tab_ind_flush_lock);

	ind_ptr = tab->tab_dic.dic_keys;
	for (u_int k=0; k<tab->tab_dic.dic_key_count; k++, ind_ptr++) {
		ind = *ind_ptr;
		XT_INDEX_WRITE_LOCK(ind, ot);
		if ((XT_NODE_ID(current) = XT_NODE_ID(ind->mi_root)))
			idx_load_node(self, ot, ind, current);
		XT_INDEX_UNLOCK(ind, ot);
	}

	freer_(); // xt_unlock_mutex(&tab->tab_ind_flush_lock)
}

/*
 * -----------------------------------------------------------------------
 * Count the number of deleted entries in a node:
 */

/*
 * {LAZY-DEL-INDEX-ITEMS}
 *
 * Use this function to count the number of deleted items 
 * in a node when it is loaded.
 *
 * The count helps us decide of the node should be "packed".
 */
xtPublic void xt_ind_count_deleted_items(XTTableHPtr tab, XTIndexPtr ind, XTIndBlockPtr block)
{
	XTIdxResultRec		result;
	int					del_count = 0;
	xtWord2				branch_size;

	branch_size = XT_GET_DISK_2(((XTIdxBranchDPtr) block->cb_data)->tb_size_2);

	/* This is possible when reading free pages. */
	if (XT_GET_INDEX_BLOCK_LEN(branch_size) < 2 || XT_GET_INDEX_BLOCK_LEN(branch_size) > XT_INDEX_PAGE_SIZE)
		return;

	idx_first_branch_item(tab, ind, (XTIdxBranchDPtr) block->cb_data, &result);
	while (result.sr_item.i_item_offset < result.sr_item.i_total_size) {
		if (result.sr_row_id == (xtRowID) -1)
			del_count++;
		idx_next_branch_item(tab, ind, (XTIdxBranchDPtr) block->cb_data, &result);
	}
	block->cp_del_count = del_count;
}

/*
 * -----------------------------------------------------------------------
 * Index consistant flush
 */

static xtBool idx_flush_dirty_list(XTIndexLogPtr il, XTOpenTablePtr ot, u_int *flush_count, XTIndBlockPtr *flush_list)
{
	for (u_int i=0; i<*flush_count; i++)
		il->il_write_block(ot, flush_list[i]);
	*flush_count = 0;
	return OK;
}

static xtBool ind_add_to_dirty_list(XTIndexLogPtr il, XTOpenTablePtr ot, u_int *flush_count, XTIndBlockPtr *flush_list, XTIndBlockPtr block)
{
	register u_int		count;
	register u_int		i;
	register u_int		guess;

	if (*flush_count == IND_FLUSH_BUFFER_SIZE) {
		if (!idx_flush_dirty_list(il, ot, flush_count, flush_list))
			return FAILED;
	}

	count = *flush_count;
	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		if (XT_NODE_ID(block->cb_address) == XT_NODE_ID(flush_list[guess]->cb_address)) {
			// Should not happen...
			ASSERT_NS(FALSE);
			return OK;
		}
		if (XT_NODE_ID(block->cb_address) < XT_NODE_ID(flush_list[guess]->cb_address))
			count = guess;
		else
			i = guess + 1;
	}

	/* Insert at position i */
	memmove(flush_list + i + 1, flush_list + i, (*flush_count - i) * sizeof(XTIndBlockPtr));
	flush_list[i] = block;
	*flush_count = *flush_count + 1;
	return OK;
}

xtPublic xtBool xt_flush_indices(XTOpenTablePtr ot, off_t *bytes_flushed, xtBool have_table_lock)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTIndexLogPtr			il;
	XTIndexPtr				*indp;
	XTIndexPtr				ind;
	u_int					i, j;
	xtBool					wrote_something = FALSE;
	u_int					flush_count = 0;
	XTIndBlockPtr			flush_list[IND_FLUSH_BUFFER_SIZE];
	XTIndBlockPtr			block, fblock;
	xtWord1					*data;
	xtIndexNodeID			ind_free;
	xtBool					something_to_free = FALSE;
	xtIndexNodeID			last_address, next_address;
	xtWord2					curr_flush_seq;
	XTIndFreeListPtr		list_ptr;
	u_int					dirty_blocks;
	XTCheckPointTablePtr	cp_tab;
	XTCheckPointStatePtr	cp = NULL;

	if (!xt_begin_checkpoint(tab->tab_db, have_table_lock, ot->ot_thread))
		return FAILED;

#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	xt_lock_mutex_ns(&tab->tab_ind_flush_lock);

	if (!tab->tab_db->db_indlogs.ilp_get_log(&il, ot->ot_thread))
		goto failed_3;

	il->il_reset(tab->tab_id);
	if (!il->il_write_byte(ot, XT_DT_FREE_LIST))
		goto failed_2;
	if (!il->il_write_word4(ot, tab->tab_id))
		goto failed_2;
	if (!il->il_write_word4(ot, 0))
		goto failed_2;

	/* Lock all: */
	dirty_blocks = 0;
	indp = tab->tab_dic.dic_keys;
	for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
		ind = *indp;
		XT_INDEX_WRITE_LOCK(ind, ot);
		if (ind->mi_free_list && ind->mi_free_list->fl_free_count)
			something_to_free = TRUE;
		dirty_blocks += ind->mi_dirty_blocks;
	}
	// 128 dirty blocks == 2MB
#ifdef TRACE_FLUSH
	printf("FLUSH index   %d %s\n", (int) dirty_blocks * XT_INDEX_PAGE_SIZE, tab->tab_name->ps_path);
	fflush(stdout);
#endif
	if (bytes_flushed)
		*bytes_flushed += (dirty_blocks * XT_INDEX_PAGE_SIZE);

	curr_flush_seq = tab->tab_ind_flush_seq;
	tab->tab_ind_flush_seq++;

	/* Write the dirty pages: */
	indp = tab->tab_dic.dic_keys;
	data = tab->tab_index_head->tp_data;
	for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
		ind = *indp;
		xt_spinlock_lock(&ind->mi_dirty_lock);
		if ((block = ind->mi_dirty_list)) {
			wrote_something = TRUE;
			while (block) {
				ASSERT_NS(block->cb_state == IDX_CAC_BLOCK_DIRTY);
				ASSERT_NS(block->cp_flush_seq == curr_flush_seq);
				if (!ind_add_to_dirty_list(il, ot, &flush_count, flush_list, block))
					goto failed;
				block = block->cb_dirty_next;
			}
		}
		xt_spinlock_unlock(&ind->mi_dirty_lock);
		XT_SET_NODE_REF(tab, data, ind->mi_root);
		data += XT_NODE_REF_SIZE;
	}

	/* Flush the dirty blocks: */
	if (!idx_flush_dirty_list(il, ot, &flush_count, flush_list))
		goto failed;

	xt_lock_mutex_ns(&tab->tab_ind_lock);

	/* Write the free list: */
	if (something_to_free) {
		union {
			xtWord1				buffer[XT_BLOCK_SIZE_FOR_DIRECT_IO];
			XTIndFreeBlockRec	free_block;
		} x;
		memset(x.buffer, 0, sizeof(XTIndFreeBlockRec));

		/* The old start of the free list: */
		XT_NODE_ID(ind_free) = 0;
		while ((list_ptr = tab->tab_ind_free_list)) {
			if (list_ptr->fl_start < list_ptr->fl_free_count) {
				ind_free = list_ptr->fl_page_id[list_ptr->fl_start];
				break;
			}
			tab->tab_ind_free_list = list_ptr->fl_next_list;
			xt_free_ns(list_ptr);
		}
		if (!XT_NODE_ID(ind_free))
			ind_free = tab->tab_ind_free;

		if (!il->il_write_byte(ot, XT_DT_FREE_LIST))
			goto failed;
		indp = tab->tab_dic.dic_keys;
		XT_NODE_ID(last_address) = 0;
		for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
			ind = *indp;
			//ASSERT_NS(XT_INDEX_HAVE_XLOCK(ind, ot));
			if (ind->mi_free_list && ind->mi_free_list->fl_free_count) {
				for (j=0; j<ind->mi_free_list->fl_free_count; j++) {
					next_address = ind->mi_free_list->fl_page_id[j];
					if (!il->il_write_word4(ot, XT_NODE_ID(ind->mi_free_list->fl_page_id[j])))
						goto failed;
					if (XT_NODE_ID(last_address)) {
						XT_SET_DISK_8(x.free_block.if_next_block_8, XT_NODE_ID(next_address));
						if (!xt_ind_write_cache(ot, last_address, 8, x.buffer))
							goto failed;
					}
					last_address = next_address;
				}
			}
		}
		if (!il->il_write_word4(ot, XT_NODE_ID(ind_free)))
			goto failed;
		if (XT_NODE_ID(last_address)) {
			XT_SET_DISK_8(x.free_block.if_next_block_8, XT_NODE_ID(tab->tab_ind_free));
			if (!xt_ind_write_cache(ot, last_address, 8, x.buffer))
				goto failed;
		}
		if (!il->il_write_word4(ot, 0xFFFFFFFF))
			goto failed;
	}

	/*
	 * Add the free list caches to the global free list cache.
	 * Added backwards to match the write order.
	 */
	indp = tab->tab_dic.dic_keys + tab->tab_dic.dic_key_count-1;
	for (i=0; i<tab->tab_dic.dic_key_count; i++, indp--) {
		ind = *indp;
		//ASSERT_NS(XT_INDEX_HAVE_XLOCK(ind, ot));
		if (ind->mi_free_list) {
			wrote_something = TRUE;
			ind->mi_free_list->fl_next_list = tab->tab_ind_free_list;
			tab->tab_ind_free_list = ind->mi_free_list;
		}
		ind->mi_free_list = NULL;
	}

	/*
	 * The new start of the free list is the first
	 * item on the table free list:
	 */
	XT_NODE_ID(ind_free) = 0;
	while ((list_ptr = tab->tab_ind_free_list)) {
		if (list_ptr->fl_start < list_ptr->fl_free_count) {
			ind_free = list_ptr->fl_page_id[list_ptr->fl_start];
			break;
		}
		tab->tab_ind_free_list = list_ptr->fl_next_list;
		xt_free_ns(list_ptr);
	}
	if (!XT_NODE_ID(ind_free))
		ind_free = tab->tab_ind_free;
	xt_unlock_mutex_ns(&tab->tab_ind_lock);

	XT_SET_DISK_6(tab->tab_index_head->tp_ind_eof_6, XT_NODE_ID(tab->tab_ind_eof));
	XT_SET_DISK_6(tab->tab_index_head->tp_ind_free_6, XT_NODE_ID(ind_free));

	if (!il->il_write_header(ot, XT_INDEX_HEAD_SIZE, (xtWord1 *) tab->tab_index_head))
		goto failed;

	indp = tab->tab_dic.dic_keys;
	for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
		ind = *indp;
		XT_INDEX_UNLOCK(ind, ot);
	}

	if (wrote_something) {
		/* Flush the log before we flush the index.
		 *
		 * The reason is, we must make sure that changes that
		 * will be in the index are already in the transaction
		 * log.
		 *
		 * Only then are we able to undo those changes on
		 * recovery.
		 *
		 * Simple example:
		 * CREATE TABLE t1 (s1 INT PRIMARY KEY);
		 * INSERT INTO t1 VALUES (1);
		 *
		 * BEGIN;
		 * INSERT INTO t1 VALUES (2);
		 *
		 * --- INDEX IS FLUSHED HERE ---
		 *
		 * --- SERVER CRASH HERE ---
		 *
		 *
		 * The INSERT VALUES (2) has been written
		 * to the log, but not flushed.
		 * But the index has been updated.
		 * If the index is flushed it will contain
		 * the entry for record with s1=2.
		 * 
		 * This entry must be removed on recovery.
		 *
		 * To prevent this situation I flush the log
		 * here.
		 */
		if (!(tab->tab_dic.dic_tab_flags & XT_TAB_FLAGS_TEMP_TAB)) {
			if (!xt_xlog_flush_log(ot->ot_thread))
				goto failed_2;
			if (!il->il_flush(ot))
				goto failed_2;
		}

		if (!il->il_apply_log(ot))
			goto failed_2;

		indp = tab->tab_dic.dic_keys;
		for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
			ind = *indp;
			XT_INDEX_WRITE_LOCK(ind, ot);
		}

		/* Free up flushed pages: */
		indp = tab->tab_dic.dic_keys;
		for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
			ind = *indp;
			xt_spinlock_lock(&ind->mi_dirty_lock);
			if ((block = ind->mi_dirty_list)) {
				while (block) {
					fblock = block;
					block = block->cb_dirty_next;
					ASSERT_NS(fblock->cb_state == IDX_CAC_BLOCK_DIRTY);
					if (fblock->cp_flush_seq == curr_flush_seq) {
						/* Take the block off the dirty list: */
						if (fblock->cb_dirty_next)
							fblock->cb_dirty_next->cb_dirty_prev = fblock->cb_dirty_prev;
						if (fblock->cb_dirty_prev)
							fblock->cb_dirty_prev->cb_dirty_next = fblock->cb_dirty_next;
						if (ind->mi_dirty_list == fblock)
							ind->mi_dirty_list = fblock->cb_dirty_next;
						ind->mi_dirty_blocks--;
						fblock->cb_state = IDX_CAC_BLOCK_CLEAN;
					}
				}
			}
			xt_spinlock_unlock(&ind->mi_dirty_lock);
		}

		indp = tab->tab_dic.dic_keys;
		for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
			ind = *indp;
			XT_INDEX_UNLOCK(ind, ot);
		}
	}

	il->il_release();

	/* Mark this table as index flushed: */
	cp = &tab->tab_db->db_cp_state;
	xt_lock_mutex_ns(&cp->cp_state_lock);
	if (cp->cp_running) {
		cp_tab = (XTCheckPointTablePtr) xt_sl_find(NULL, cp->cp_table_ids, &tab->tab_id);
		if (cp_tab && (cp_tab->cpt_flushed & XT_CPT_ALL_FLUSHED) != XT_CPT_ALL_FLUSHED) {
			cp_tab->cpt_flushed |= XT_CPT_INDEX_FLUSHED;
			if ((cp_tab->cpt_flushed & XT_CPT_ALL_FLUSHED) == XT_CPT_ALL_FLUSHED) {
				ASSERT_NS(cp->cp_flush_count < xt_sl_get_size(cp->cp_table_ids));
				cp->cp_flush_count++;
			}
		}
	}
	xt_unlock_mutex_ns(&cp->cp_state_lock);

	xt_unlock_mutex_ns(&tab->tab_ind_flush_lock);
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache((XTIndex *) 1);
#endif
#ifdef TRACE_FLUSH
	printf("FLUSH --end-- %s\n", tab->tab_name->ps_path);
	fflush(stdout);
#endif
	if (!xt_end_checkpoint(tab->tab_db, ot->ot_thread, NULL))
		return FAILED;
	return OK;

	failed:
	indp = tab->tab_dic.dic_keys;
	for (i=0; i<tab->tab_dic.dic_key_count; i++, indp++) {
		ind = *indp;
		XT_INDEX_UNLOCK(ind, ot);
	}

	failed_2:
	il->il_release();

	failed_3:
	xt_unlock_mutex_ns(&tab->tab_ind_flush_lock);
#ifdef DEBUG_CHECK_IND_CACHE
	xt_ind_check_cache(NULL);
#endif
	return FAILED;
}

void XTIndexLogPool::ilp_init(struct XTThread *self, struct XTDatabase *db, size_t log_buffer_size)
{
	char			path[PATH_MAX];
	XTOpenDirPtr	od;
	xtLogID			log_id;
	char			*file;
	XTIndexLogPtr	il = NULL;
	XTOpenTablePtr	ot = NULL;

	ilp_db = db;
	ilp_log_buffer_size = log_buffer_size;
	xt_init_mutex_with_autoname(self, &ilp_lock);

	xt_strcpy(PATH_MAX, path, db->db_main_path);
	xt_add_system_dir(PATH_MAX, path);
	if (xt_fs_exists(path)) {
		pushsr_(od, xt_dir_close, xt_dir_open(self, path, NULL));
		while (xt_dir_next(self, od)) {
			file = xt_dir_name(self, od);
			if (xt_starts_with(file, "ilog")) {
				if ((log_id = (xtLogID) xt_file_name_to_id(file))) {
					if (!ilp_open_log(&il, log_id, FALSE, self))
						goto failed;
					if (il->il_tab_id && il->il_log_eof) {
						if (!il->il_open_table(&ot))
							goto failed;
						if (ot) {
							if (!il->il_apply_log(ot))
								goto failed;
							ot->ot_thread = self;
							il->il_close_table(ot);
						}
					}
					il->il_close(TRUE);
				}
			}
		}
		freer_(); // xt_dir_close(od)
	}
	return;

	failed:
	if (ot && il)
		il->il_close_table(ot);
	if (il)
		il->il_close(FALSE);
	xt_throw(self);
}

void XTIndexLogPool::ilp_close(struct XTThread *XT_UNUSED(self), xtBool lock)
{
	XTIndexLogPtr	il;

	if (lock)
		xt_lock_mutex_ns(&ilp_lock);
	while ((il = ilp_log_pool)) {
		ilp_log_pool = il->il_next_in_pool;
		il_pool_count--;
		il->il_close(TRUE);
	}
	if (lock)
		xt_unlock_mutex_ns(&ilp_lock);
}

void XTIndexLogPool::ilp_exit(struct XTThread *self)
{
	ilp_close(self, FALSE);
	ASSERT_NS(il_pool_count == 0);
	xt_free_mutex(&ilp_lock);
}

void XTIndexLogPool::ilp_name(size_t size, char *path, xtLogID log_id)
{
	char name[50];

	sprintf(name, "ilog-%lu.xt", (u_long) log_id);
	xt_strcpy(size, path, ilp_db->db_main_path);
	xt_add_system_dir(size, path);
	xt_add_dir_char(size, path);
	xt_strcat(size, path, name);
}

xtBool XTIndexLogPool::ilp_open_log(XTIndexLogPtr *ret_il, xtLogID log_id, xtBool excl, XTThreadPtr thread)
{
	char				log_path[PATH_MAX];
	XTIndexLogPtr		il;
	XTIndLogHeadDRec	log_head;
	size_t				read_size;

	ilp_name(PATH_MAX, log_path, log_id);
	if (!(il = (XTIndexLogPtr) xt_calloc_ns(sizeof(XTIndexLogRec))))
		return FAILED;
	il->il_log_id = log_id;
	il->il_pool = this;

	/* Writes will be rounded up to the nearest direct write block size (see [+]),
	 * so make sure we have space in the buffer for that:
	 */
	if (!(il->il_buffer = (xtWord1 *) xt_malloc_ns(ilp_log_buffer_size + XT_BLOCK_SIZE_FOR_DIRECT_IO)))
		goto failed;
	il->il_buffer_size = ilp_log_buffer_size;

	if (!(il->il_of = xt_open_file_ns(log_path, (excl ? XT_FS_EXCLUSIVE : 0) | XT_FS_CREATE | XT_FS_MAKE_PATH)))
		goto failed;

	if (!xt_pread_file(il->il_of, 0, sizeof(XTIndLogHeadDRec), 0, &log_head, &read_size, &thread->st_statistics.st_ilog, thread))
		goto failed;

	if (read_size == sizeof(XTIndLogHeadDRec)) {
		il->il_tab_id = XT_GET_DISK_4(log_head.ilh_tab_id_4);
		il->il_log_eof = XT_GET_DISK_4(log_head.ilh_log_eof_4);
	}
	else {
		il->il_tab_id = 0;
		il->il_log_eof = 0;
	}

	*ret_il = il;
	return OK;

	failed:
	il->il_close(FALSE);
	return FAILED;
}

xtBool XTIndexLogPool::ilp_get_log(XTIndexLogPtr *ret_il, XTThreadPtr thread)
{
	XTIndexLogPtr	il;
	xtLogID			log_id = 0;

	xt_lock_mutex_ns(&ilp_lock);
	if ((il = ilp_log_pool)) {
		ilp_log_pool = il->il_next_in_pool;
		il_pool_count--;
	}
	else {
		ilp_next_log_id++;
		log_id = ilp_next_log_id;
	}
	xt_unlock_mutex_ns(&ilp_lock);
	if (!il) {
		if (!ilp_open_log(&il, log_id, TRUE, thread))
			return FAILED;
	}
	*ret_il= il;
	return OK;
}

void XTIndexLogPool::ilp_release_log(XTIndexLogPtr il)
{
	xt_lock_mutex_ns(&ilp_lock);
	if (il_pool_count == 5)
		il->il_close(TRUE);
	else {
		il_pool_count++;
		il->il_next_in_pool = ilp_log_pool;
		ilp_log_pool = il;
	}
	xt_unlock_mutex_ns(&ilp_lock);
}

void XTIndexLog::il_reset(xtTableID tab_id)
{
	il_tab_id = tab_id;
	il_log_eof = 0;
	il_buffer_len = 0;
	il_buffer_offset = 0;
}

void XTIndexLog::il_close(xtBool delete_it)
{
	xtLogID	log_id = il_log_id;

	if (il_of) {
		xt_close_file_ns(il_of);
		il_of = NULL;
	}
	
	if (delete_it && log_id) {
		char	log_path[PATH_MAX];

		il_pool->ilp_name(PATH_MAX, log_path, log_id);
		xt_fs_delete(NULL, log_path);
	}

	if (il_buffer) {
		xt_free_ns(il_buffer);
		il_buffer = NULL;
	}

	xt_free_ns(this);
}


void XTIndexLog::il_release()
{
	il_pool->ilp_db->db_indlogs.ilp_release_log(this);
}

xtBool XTIndexLog::il_require_space(size_t bytes, XTThreadPtr thread)
{
	if (il_buffer_len + bytes > il_buffer_size) {
		if (!xt_pwrite_file(il_of, il_buffer_offset, il_buffer_len, il_buffer, &thread->st_statistics.st_ilog, thread))
			return FAILED;
		il_buffer_offset += il_buffer_len;
		il_buffer_len = 0;
	}

	return OK;
}

xtBool XTIndexLog::il_write_byte(struct XTOpenTable *ot, xtWord1 byte)
{
	if (!il_require_space(1, ot->ot_thread))
		return FAILED;
	*(il_buffer + il_buffer_len) = byte;
	il_buffer_len++;
	return OK;
}

xtBool XTIndexLog::il_write_word4(struct XTOpenTable *ot, xtWord4 value)
{
	xtWord1 *buffer;

	if (!il_require_space(4, ot->ot_thread))
		return FAILED;
	buffer = il_buffer + il_buffer_len;
	XT_SET_DISK_4(buffer, value);
	il_buffer_len += 4;
	return OK;
}

xtBool XTIndexLog::il_write_block(struct XTOpenTable *ot, XTIndBlockPtr block)
{
	XTIndPageDataDPtr	page_data;
	xtIndexNodeID		node_id;
	XTIdxBranchDPtr		node;
	u_int				block_len;

	node_id = block->cb_address;
	node = (XTIdxBranchDPtr) block->cb_data;
	block_len = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(node->tb_size_2));

	if (!il_require_space(offsetof(XTIndPageDataDRec, ild_data) + block_len, ot->ot_thread))
		return FAILED;

	ASSERT_NS(offsetof(XTIndPageDataDRec, ild_data) + XT_INDEX_PAGE_SIZE <= il_buffer_size);

	page_data = (XTIndPageDataDPtr) (il_buffer + il_buffer_len);
	TRACK_BLOCK_TO_FLUSH(node_id);
	page_data->ild_data_type = XT_DT_INDEX_PAGE;
	XT_SET_DISK_4(page_data->ild_page_id_4, XT_NODE_ID(node_id));
	memcpy(page_data->ild_data, block->cb_data, block_len);

	il_buffer_len += offsetof(XTIndPageDataDRec, ild_data) + block_len;

	return OK;
}

xtBool XTIndexLog::il_write_header(struct XTOpenTable *ot, size_t head_size, xtWord1 *head_buf)
{
	XTIndHeadDataDPtr	head_data;

	if (!il_require_space(offsetof(XTIndHeadDataDRec, ilh_data) + head_size, ot->ot_thread))
		return FAILED;

	head_data = (XTIndHeadDataDPtr) (il_buffer + il_buffer_len);
	head_data->ilh_data_type = XT_DT_HEADER;
	XT_SET_DISK_2(head_data->ilh_head_size_2, head_size);
	memcpy(head_data->ilh_data, head_buf, head_size);

	il_buffer_len += offsetof(XTIndHeadDataDRec, ilh_data) + head_size;

	return OK;
}

xtBool XTIndexLog::il_flush(struct XTOpenTable *ot)
{
	XTIndLogHeadDRec	log_head;
	xtTableID			tab_id = ot->ot_table->tab_id;

	if (il_buffer_len) {
		if (!xt_pwrite_file(il_of, il_buffer_offset, il_buffer_len, il_buffer, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
			return FAILED;
		il_buffer_offset += il_buffer_len;
		il_buffer_len = 0;
	}

	if (il_log_eof != il_buffer_offset) {
		log_head.ilh_data_type = XT_DT_LOG_HEAD;
		XT_SET_DISK_4(log_head.ilh_tab_id_4, tab_id);
		XT_SET_DISK_4(log_head.ilh_log_eof_4, il_buffer_offset);

		if (!xt_flush_file(il_of, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
			return FAILED;

		if (!xt_pwrite_file(il_of, 0, sizeof(XTIndLogHeadDRec), (xtWord1 *) &log_head, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
			return FAILED;

		if (!xt_flush_file(il_of, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
			return FAILED;

		il_tab_id = tab_id;
		il_log_eof = il_buffer_offset;
	}
	return OK;
}

xtBool XTIndexLog::il_apply_log(struct XTOpenTable *ot)
{
	XT_NODE_TEMP;
	register XTTableHPtr	tab = ot->ot_table;
	off_t					offset;
	size_t					pos;
	xtWord1					*buffer;
	off_t					address;
	xtIndexNodeID			node_id;
	size_t					req_size = 0;
	XTIndLogHeadDRec		log_head;

	offset = 0;
	while (offset < il_log_eof) {
		if (offset < il_buffer_offset ||
			offset >= il_buffer_offset + (off_t) il_buffer_len) {
			il_buffer_len = il_buffer_size;
			if (il_log_eof - offset < (off_t) il_buffer_len)
				il_buffer_len = (size_t) (il_log_eof - offset);

			/* Corrupt log?! */
			if (il_buffer_len < req_size) {
				xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_INDEX_LOG_CORRUPT, xt_file_path(il_of));
				xt_log_and_clear_exception_ns();
				return OK;
			}
			if (!xt_pread_file(il_of, offset, il_buffer_len, il_buffer_len, il_buffer, NULL, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
				return FAILED;
			il_buffer_offset = offset;
		}
		pos = (size_t) (offset - il_buffer_offset);
		ASSERT_NS(pos < il_buffer_len);
		buffer = il_buffer + pos;
		switch (*buffer) {
			case XT_DT_LOG_HEAD:
				req_size = sizeof(XTIndLogHeadDRec);
				if (il_buffer_len - pos < req_size) {
					il_buffer_len = 0;
					continue;
				}
				offset += req_size;
				req_size = 0;
				break;
			case XT_DT_INDEX_PAGE:
				XTIndPageDataDPtr	page_data;
				XTIdxBranchDPtr		node;
				u_int				block_len;
				size_t				size;

				req_size = offsetof(XTIndPageDataDRec, ild_data) + 2;
				if (il_buffer_len - pos < req_size) {
					il_buffer_len = 0;
					continue;
				}
				page_data = (XTIndPageDataDPtr) buffer;
				node_id = XT_RET_NODE_ID(XT_GET_DISK_4(page_data->ild_page_id_4));
				node = (XTIdxBranchDPtr) page_data->ild_data;
				block_len = XT_GET_INDEX_BLOCK_LEN(XT_GET_DISK_2(node->tb_size_2));
				if (block_len < 2 || block_len > XT_INDEX_PAGE_SIZE) {
					xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_CORRUPTED, tab->tab_name);
					return FAILED;
				}

				req_size = offsetof(XTIndPageDataDRec, ild_data) + block_len;
				if (il_buffer_len - pos < req_size) {
					il_buffer_len = 0;
					continue;
				}

				TRACK_BLOCK_FLUSH_N(node_id);
				address = xt_ind_node_to_offset(tab, node_id);
				/* [+] Round up the block size. Space has been provided. */
				size = (((block_len - 1) / XT_BLOCK_SIZE_FOR_DIRECT_IO) + 1) * XT_BLOCK_SIZE_FOR_DIRECT_IO;
				IDX_TRACE("%d- W%x\n", (int) XT_NODE_ID(node_id), (int) XT_GET_DISK_2(page_data->ild_data));
				ASSERT_NS(size > 0 && size <= XT_INDEX_PAGE_SIZE);
				if (!xt_pwrite_file(ot->ot_ind_file, address, size, page_data->ild_data, &ot->ot_thread->st_statistics.st_ind, ot->ot_thread))
					return FAILED;

				offset += req_size;
				req_size = 0;
				break;
			case XT_DT_FREE_LIST:
				xtWord4	block, nblock;
				union {
					xtWord1				buffer[XT_BLOCK_SIZE_FOR_DIRECT_IO];
					XTIndFreeBlockRec	free_block;
				} x;
				off_t	aoff;

				memset(x.buffer, 0, sizeof(XTIndFreeBlockRec));

				pos++;
				offset++;
				
				for (;;) {
					req_size = 8;
					if (il_buffer_len - pos < req_size) {
						il_buffer_len = il_buffer_size;
						if (il_log_eof - offset < (off_t) il_buffer_len)
							il_buffer_len = (size_t) (il_log_eof - offset);
						/* Corrupt log?! */
						if (il_buffer_len < req_size) {
							xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_INDEX_LOG_CORRUPT, xt_file_path(il_of));
							xt_log_and_clear_exception_ns();
							return OK;
						}
						if (!xt_pread_file(il_of, offset, il_buffer_len, il_buffer_len, il_buffer, NULL, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
							return FAILED;
						pos = 0;
					}
					block = XT_GET_DISK_4(il_buffer + pos);
					nblock = XT_GET_DISK_4(il_buffer + pos + 4);
					if (nblock == 0xFFFFFFFF)
						break;
					aoff = xt_ind_node_to_offset(tab, XT_RET_NODE_ID(block));
					XT_SET_DISK_8(x.free_block.if_next_block_8, nblock);
					IDX_TRACE("%d- *%x\n", (int) block, (int) XT_GET_DISK_2(x.buffer));
					if (!xt_pwrite_file(ot->ot_ind_file, aoff, XT_BLOCK_SIZE_FOR_DIRECT_IO, x.buffer, &ot->ot_thread->st_statistics.st_ind, ot->ot_thread))
						return FAILED;
					pos += 4;
					offset += 4;
				}

				offset += 8;
				req_size = 0;
				break;
			case XT_DT_HEADER:
				XTIndHeadDataDPtr	head_data;
				size_t				len;

				req_size = offsetof(XTIndHeadDataDRec, ilh_data);
				if (il_buffer_len - pos < req_size) {
					il_buffer_len = 0;
					continue;
				}
				head_data = (XTIndHeadDataDPtr) buffer;
				len = XT_GET_DISK_2(head_data->ilh_head_size_2);

				req_size = offsetof(XTIndHeadDataDRec, ilh_data) + len;
				if (il_buffer_len - pos < req_size) {
					il_buffer_len = 0;
					continue;
				}

				if (!xt_pwrite_file(ot->ot_ind_file, 0, len, head_data->ilh_data, &ot->ot_thread->st_statistics.st_ind, ot->ot_thread))
					return FAILED;

				offset += req_size;
				req_size = 0;
				break;
			default:
				xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_INDEX_LOG_CORRUPT, xt_file_path(il_of));
				xt_log_and_clear_exception_ns();
				return OK;
		}
	}

	if (!xt_flush_file(ot->ot_ind_file, &ot->ot_thread->st_statistics.st_ind, ot->ot_thread))
		return FAILED;

	log_head.ilh_data_type = XT_DT_LOG_HEAD;
	XT_SET_DISK_4(log_head.ilh_tab_id_4, il_tab_id);
	XT_SET_DISK_4(log_head.ilh_log_eof_4, 0);

	if (!xt_pwrite_file(il_of, 0, sizeof(XTIndLogHeadDRec), (xtWord1 *) &log_head, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
		return FAILED;

	if (!(tab->tab_dic.dic_tab_flags & XT_TAB_FLAGS_TEMP_TAB)) {
		if (!xt_flush_file(il_of, &ot->ot_thread->st_statistics.st_ilog, ot->ot_thread))
			return FAILED;
	}
	return OK;
}

xtBool XTIndexLog::il_open_table(struct XTOpenTable **ot)
{
	return xt_db_open_pool_table_ns(ot, il_pool->ilp_db, il_tab_id);
}

void XTIndexLog::il_close_table(struct XTOpenTable *ot)
{
	xt_db_return_table_to_pool_ns(ot);
}


