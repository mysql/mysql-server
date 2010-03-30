/**********************************************************************
File-based list utilities

(c) 1995 Innobase Oy

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#include "fut0lst.h"

#ifdef UNIV_NONINL
#include "fut0lst.ic"
#endif

#include "buf0buf.h"


/************************************************************************
Adds a node to an empty list. */
static
void
flst_add_to_empty(
/*==============*/
	flst_base_node_t*	base,	/* in: pointer to base node of
					empty list */
	flst_node_t*		node,	/* in: node to add */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	fil_addr_t	node_addr;
	ulint		len;

	ut_ad(mtr && base && node);
	ut_ad(base != node);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node),
				MTR_MEMO_PAGE_X_FIX));
	len = flst_get_len(base, mtr);
	ut_a(len == 0);

	buf_ptr_get_fsp_addr(node, &space, &node_addr);

	/* Update first and last fields of base node */
	flst_write_addr(base + FLST_FIRST, node_addr, mtr);
	flst_write_addr(base + FLST_LAST, node_addr, mtr);

	/* Set prev and next fields of node to add */
	flst_write_addr(node + FLST_PREV, fil_addr_null, mtr);
	flst_write_addr(node + FLST_NEXT, fil_addr_null, mtr);

	/* Update len of base node */
	mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/************************************************************************
Adds a node as the last node in a list. */

void
flst_add_last(
/*==========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node,	/* in: node to add */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	fil_addr_t	node_addr;
	ulint		len;
	fil_addr_t	last_addr;
	flst_node_t*	last_node;

	ut_ad(mtr && base && node);
	ut_ad(base != node);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node),
				MTR_MEMO_PAGE_X_FIX));
	len = flst_get_len(base, mtr);
	last_addr = flst_get_last(base, mtr);

	buf_ptr_get_fsp_addr(node, &space, &node_addr);

	/* If the list is not empty, call flst_insert_after */
	if (len != 0) {
		if (last_addr.page == node_addr.page) {
			last_node = buf_frame_align(node) + last_addr.boffset;
		} else {
			last_node = fut_get_ptr(space, last_addr, RW_X_LATCH,
						mtr);
		}

		flst_insert_after(base, last_node, node, mtr);
	} else {
		/* else call flst_add_to_empty */
		flst_add_to_empty(base, node, mtr);
	}
}

/************************************************************************
Adds a node as the first node in a list. */

void
flst_add_first(
/*===========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node,	/* in: node to add */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	fil_addr_t	node_addr;
	ulint		len;
	fil_addr_t	first_addr;
	flst_node_t*	first_node;

	ut_ad(mtr && base && node);
	ut_ad(base != node);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node),
				MTR_MEMO_PAGE_X_FIX));
	len = flst_get_len(base, mtr);
	first_addr = flst_get_first(base, mtr);

	buf_ptr_get_fsp_addr(node, &space, &node_addr);

	/* If the list is not empty, call flst_insert_before */
	if (len != 0) {
		if (first_addr.page == node_addr.page) {
			first_node = buf_frame_align(node)
				+ first_addr.boffset;
		} else {
			first_node = fut_get_ptr(space, first_addr,
						 RW_X_LATCH, mtr);
		}

		flst_insert_before(base, node, first_node, mtr);
	} else {
		/* else call flst_add_to_empty */
		flst_add_to_empty(base, node, mtr);
	}
}

/************************************************************************
Inserts a node after another in a list. */

void
flst_insert_after(
/*==============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node1,	/* in: node to insert after */
	flst_node_t*		node2,	/* in: node to add */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	fil_addr_t	node1_addr;
	fil_addr_t	node2_addr;
	flst_node_t*	node3;
	fil_addr_t	node3_addr;
	ulint		len;

	ut_ad(mtr && node1 && node2 && base);
	ut_ad(base != node1);
	ut_ad(base != node2);
	ut_ad(node2 != node1);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node1),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node2),
				MTR_MEMO_PAGE_X_FIX));

	buf_ptr_get_fsp_addr(node1, &space, &node1_addr);
	buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

	node3_addr = flst_get_next_addr(node1, mtr);

	/* Set prev and next fields of node2 */
	flst_write_addr(node2 + FLST_PREV, node1_addr, mtr);
	flst_write_addr(node2 + FLST_NEXT, node3_addr, mtr);

	if (!fil_addr_is_null(node3_addr)) {
		/* Update prev field of node3 */
		node3 = fut_get_ptr(space, node3_addr, RW_X_LATCH, mtr);
		flst_write_addr(node3 + FLST_PREV, node2_addr, mtr);
	} else {
		/* node1 was last in list: update last field in base */
		flst_write_addr(base + FLST_LAST, node2_addr, mtr);
	}

	/* Set next field of node1 */
	flst_write_addr(node1 + FLST_NEXT, node2_addr, mtr);

	/* Update len of base node */
	len = flst_get_len(base, mtr);
	mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/************************************************************************
Inserts a node before another in a list. */

void
flst_insert_before(
/*===============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: node to insert */
	flst_node_t*		node3,	/* in: node to insert before */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	flst_node_t*	node1;
	fil_addr_t	node1_addr;
	fil_addr_t	node2_addr;
	fil_addr_t	node3_addr;
	ulint		len;

	ut_ad(mtr && node2 && node3 && base);
	ut_ad(base != node2);
	ut_ad(base != node3);
	ut_ad(node2 != node3);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node2),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node3),
				MTR_MEMO_PAGE_X_FIX));

	buf_ptr_get_fsp_addr(node2, &space, &node2_addr);
	buf_ptr_get_fsp_addr(node3, &space, &node3_addr);

	node1_addr = flst_get_prev_addr(node3, mtr);

	/* Set prev and next fields of node2 */
	flst_write_addr(node2 + FLST_PREV, node1_addr, mtr);
	flst_write_addr(node2 + FLST_NEXT, node3_addr, mtr);

	if (!fil_addr_is_null(node1_addr)) {
		/* Update next field of node1 */
		node1 = fut_get_ptr(space, node1_addr, RW_X_LATCH, mtr);
		flst_write_addr(node1 + FLST_NEXT, node2_addr, mtr);
	} else {
		/* node3 was first in list: update first field in base */
		flst_write_addr(base + FLST_FIRST, node2_addr, mtr);
	}

	/* Set prev field of node3 */
	flst_write_addr(node3 + FLST_PREV, node2_addr, mtr);

	/* Update len of base node */
	len = flst_get_len(base, mtr);
	mlog_write_ulint(base + FLST_LEN, len + 1, MLOG_4BYTES, mtr);
}

/************************************************************************
Removes a node. */

void
flst_remove(
/*========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: node to remove */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	flst_node_t*	node1;
	fil_addr_t	node1_addr;
	fil_addr_t	node2_addr;
	flst_node_t*	node3;
	fil_addr_t	node3_addr;
	ulint		len;

	ut_ad(mtr && node2 && base);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node2),
				MTR_MEMO_PAGE_X_FIX));

	buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

	node1_addr = flst_get_prev_addr(node2, mtr);
	node3_addr = flst_get_next_addr(node2, mtr);

	if (!fil_addr_is_null(node1_addr)) {

		/* Update next field of node1 */

		if (node1_addr.page == node2_addr.page) {

			node1 = buf_frame_align(node2) + node1_addr.boffset;
		} else {
			node1 = fut_get_ptr(space, node1_addr, RW_X_LATCH,
					    mtr);
		}

		ut_ad(node1 != node2);

		flst_write_addr(node1 + FLST_NEXT, node3_addr, mtr);
	} else {
		/* node2 was first in list: update first field in base */
		flst_write_addr(base + FLST_FIRST, node3_addr, mtr);
	}

	if (!fil_addr_is_null(node3_addr)) {
		/* Update prev field of node3 */

		if (node3_addr.page == node2_addr.page) {

			node3 = buf_frame_align(node2) + node3_addr.boffset;
		} else {
			node3 = fut_get_ptr(space, node3_addr, RW_X_LATCH,
					    mtr);
		}

		ut_ad(node2 != node3);

		flst_write_addr(node3 + FLST_PREV, node1_addr, mtr);
	} else {
		/* node2 was last in list: update last field in base */
		flst_write_addr(base + FLST_LAST, node1_addr, mtr);
	}

	/* Update len of base node */
	len = flst_get_len(base, mtr);
	ut_ad(len > 0);

	mlog_write_ulint(base + FLST_LEN, len - 1, MLOG_4BYTES, mtr);
}

/************************************************************************
Cuts off the tail of the list, including the node given. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */

void
flst_cut_end(
/*=========*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: first node to remove */
	ulint			n_nodes,/* in: number of nodes to remove,
					must be >= 1 */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	ulint		space;
	flst_node_t*	node1;
	fil_addr_t	node1_addr;
	fil_addr_t	node2_addr;
	ulint		len;

	ut_ad(mtr && node2 && base);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node2),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(n_nodes > 0);

	buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

	node1_addr = flst_get_prev_addr(node2, mtr);

	if (!fil_addr_is_null(node1_addr)) {

		/* Update next field of node1 */

		if (node1_addr.page == node2_addr.page) {

			node1 = buf_frame_align(node2) + node1_addr.boffset;
		} else {
			node1 = fut_get_ptr(space, node1_addr, RW_X_LATCH,
					    mtr);
		}

		flst_write_addr(node1 + FLST_NEXT, fil_addr_null, mtr);
	} else {
		/* node2 was first in list: update the field in base */
		flst_write_addr(base + FLST_FIRST, fil_addr_null, mtr);
	}

	flst_write_addr(base + FLST_LAST, node1_addr, mtr);

	/* Update len of base node */
	len = flst_get_len(base, mtr);
	ut_ad(len >= n_nodes);

	mlog_write_ulint(base + FLST_LEN, len - n_nodes, MLOG_4BYTES, mtr);
}

/************************************************************************
Cuts off the tail of the list, not including the given node. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */

void
flst_truncate_end(
/*==============*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	flst_node_t*		node2,	/* in: first node not to remove */
	ulint			n_nodes,/* in: number of nodes to remove */
	mtr_t*			mtr)	/* in: mini-transaction handle */
{
	fil_addr_t	node2_addr;
	ulint		len;
	ulint		space;

	ut_ad(mtr && node2 && base);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(node2),
				MTR_MEMO_PAGE_X_FIX));
	if (n_nodes == 0) {

		ut_ad(fil_addr_is_null(flst_get_next_addr(node2, mtr)));

		return;
	}

	buf_ptr_get_fsp_addr(node2, &space, &node2_addr);

	/* Update next field of node2 */
	flst_write_addr(node2 + FLST_NEXT, fil_addr_null, mtr);

	flst_write_addr(base + FLST_LAST, node2_addr, mtr);

	/* Update len of base node */
	len = flst_get_len(base, mtr);
	ut_ad(len >= n_nodes);

	mlog_write_ulint(base + FLST_LEN, len - n_nodes, MLOG_4BYTES, mtr);
}

/************************************************************************
Validates a file-based list. */

ibool
flst_validate(
/*==========*/
					/* out: TRUE if ok */
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	mtr_t*			mtr1)	/* in: mtr */
{
	ulint		space;
	flst_node_t*	node;
	fil_addr_t	node_addr;
	fil_addr_t	base_addr;
	ulint		len;
	ulint		i;
	mtr_t		mtr2;

	ut_ad(base);
	ut_ad(mtr_memo_contains(mtr1, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));

	/* We use two mini-transaction handles: the first is used to
	lock the base node, and prevent other threads from modifying the
	list. The second is used to traverse the list. We cannot run the
	second mtr without committing it at times, because if the list
	is long, then the x-locked pages could fill the buffer resulting
	in a deadlock. */

	/* Find out the space id */
	buf_ptr_get_fsp_addr(base, &space, &base_addr);

	len = flst_get_len(base, mtr1);
	node_addr = flst_get_first(base, mtr1);

	for (i = 0; i < len; i++) {
		mtr_start(&mtr2);

		node = fut_get_ptr(space, node_addr, RW_X_LATCH, &mtr2);
		node_addr = flst_get_next_addr(node, &mtr2);

		mtr_commit(&mtr2); /* Commit mtr2 each round to prevent buffer
				   becoming full */
	}

	ut_a(fil_addr_is_null(node_addr));

	node_addr = flst_get_last(base, mtr1);

	for (i = 0; i < len; i++) {
		mtr_start(&mtr2);

		node = fut_get_ptr(space, node_addr, RW_X_LATCH, &mtr2);
		node_addr = flst_get_prev_addr(node, &mtr2);

		mtr_commit(&mtr2); /* Commit mtr2 each round to prevent buffer
				   becoming full */
	}

	ut_a(fil_addr_is_null(node_addr));

	return(TRUE);
}

/************************************************************************
Prints info of a file-based list. */

void
flst_print(
/*=======*/
	flst_base_node_t*	base,	/* in: pointer to base node of list */
	mtr_t*			mtr)	/* in: mtr */
{
	buf_frame_t*	frame;
	ulint		len;

	ut_ad(base && mtr);
	ut_ad(mtr_memo_contains(mtr, buf_block_align(base),
				MTR_MEMO_PAGE_X_FIX));
	frame = buf_frame_align(base);

	len = flst_get_len(base, mtr);

	fprintf(stderr,
		"FILE-BASED LIST:\n"
		"Base node in space %lu page %lu byte offset %lu; len %lu\n",
		(ulong) buf_frame_get_space_id(frame),
		(ulong) buf_frame_get_page_no(frame),
		(ulong) (base - frame), (ulong) len);
}
