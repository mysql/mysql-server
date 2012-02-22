/*****************************************************************************

Copyright (c) 1994, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/********************************************************************//**
@file ha/ha0ha.c
The hash table with external chains

Created 8/22/1994 Heikki Tuuri
*************************************************************************/

#include "ha0ha.h"
#ifdef UNIV_NONINL
#include "ha0ha.ic"
#endif

#ifdef UNIV_DEBUG
# include "buf0buf.h"
#endif /* UNIV_DEBUG */
#include "btr0sea.h"
#include "page0page.h"

/*************************************************************//**
Creates a hash table with at least n array cells.  The actual number
of cells is chosen to be a prime number slightly bigger than n.
@return	own: created table */
UNIV_INTERN
hash_table_t*
ha_create_func(
/*===========*/
	ulint	n,		/*!< in: number of array cells */
#ifdef UNIV_SYNC_DEBUG
	ulint	mutex_level,	/*!< in: level of the mutexes in the latching
				order: this is used in the debug version */
#endif /* UNIV_SYNC_DEBUG */
	ulint	n_mutexes)	/*!< in: number of mutexes to protect the
				hash table: must be a power of 2, or 0 */
{
	hash_table_t*	table;
#ifndef UNIV_HOTBACKUP
	ulint		i;
#endif /* !UNIV_HOTBACKUP */

	ut_ad(ut_is_2pow(n_mutexes));
	table = hash_create(n);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
	table->adaptive = TRUE;
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	/* Creating MEM_HEAP_BTR_SEARCH type heaps can potentially fail,
	but in practise it never should in this case, hence the asserts. */

	if (n_mutexes == 0) {
		table->heap = mem_heap_create_in_btr_search(
			ut_min(4096, MEM_MAX_ALLOC_IN_BUF));
		ut_a(table->heap);

		return(table);
	}

#ifndef UNIV_HOTBACKUP
	hash_create_mutexes(table, n_mutexes, mutex_level);

	table->heaps = mem_alloc(n_mutexes * sizeof(void*));

	for (i = 0; i < n_mutexes; i++) {
		table->heaps[i] = mem_heap_create_in_btr_search(4096);
		ut_a(table->heaps[i]);
	}
#endif /* !UNIV_HOTBACKUP */

	return(table);
}

/*************************************************************//**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted. If btr_search_enabled is set to FALSE, we will only allow
updating existing nodes, but no new node is allowed to be added.
@return	TRUE if succeed, FALSE if no more memory could be allocated */
UNIV_INTERN
ibool
ha_insert_for_fold_func(
/*====================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold,	/*!< in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	block,	/*!< in: buffer block containing the data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	const rec_t*	data)	/*!< in: data, must not be NULL */
{
	hash_cell_t*	cell;
	ha_node_t*	node;
	ha_node_t*	prev_node;
	ulint		hash;

	ut_ad(data);
	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	ut_a(block->frame == page_align(data));
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ASSERT_HASH_MUTEX_OWN(table, fold);
	ut_ad(btr_search_enabled);

	hash = hash_calc_hash(fold, table);

	cell = hash_get_nth_cell(table, hash);

	prev_node = cell->node;

	while (prev_node != NULL) {
		if (prev_node->fold == fold) {
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
			if (table->adaptive) {
				buf_block_t* prev_block = prev_node->block;
				ut_a(prev_block->frame
				     == page_align(prev_node->data));
				ut_a(prev_block->n_pointers > 0);
				prev_block->n_pointers--;
				block->n_pointers++;
			}
# endif /* !UNIV_HOTBACKUP */

			prev_node->block = block;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
			prev_node->data = data;

			return(TRUE);
		}

		prev_node = prev_node->next;
	}

	/* We have to allocate a new chain node */

	node = mem_heap_alloc(hash_get_heap(table, fold), sizeof(ha_node_t));

	if (node == NULL) {
		/* It was a btr search type memory heap and at the moment
		no more memory could be allocated: return */

		ut_ad(hash_get_heap(table, fold)->type & MEM_HEAP_BTR_SEARCH);

		return(FALSE);
	}

	ha_node_set_data(node, block, data);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
	if (table->adaptive) {
		block->n_pointers++;
	}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

	node->fold = fold;

	node->next = NULL;

	prev_node = cell->node;

	if (prev_node == NULL) {

		cell->node = node;

		return(TRUE);
	}

	while (prev_node->next != NULL) {

		prev_node = prev_node->next;
	}

	prev_node->next = node;

	return(TRUE);
}

/***********************************************************//**
Deletes a hash node. */
UNIV_INTERN
void
ha_delete_hash_node(
/*================*/
	hash_table_t*	table,		/*!< in: hash table */
	ha_node_t*	del_node)	/*!< in: node to be deleted */
{
	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(btr_search_enabled);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
	if (table->adaptive) {
		ut_a(del_node->block->frame = page_align(del_node->data));
		ut_a(del_node->block->n_pointers > 0);
		del_node->block->n_pointers--;
	}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

	HASH_DELETE_AND_COMPACT(ha_node_t, next, table, del_node);
}

/*********************************************************//**
Looks for an element when we know the pointer to the data, and updates
the pointer to data, if found. */
UNIV_INTERN
void
ha_search_and_update_if_found_func(
/*===============================*/
	hash_table_t*	table,	/*!< in/out: hash table */
	ulint		fold,	/*!< in: folded value of the searched data */
	const rec_t*	data,	/*!< in: pointer to the data */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	new_block,/*!< in: block containing new_data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	const rec_t*	new_data)/*!< in: new pointer to the data */
{
	ha_node_t*	node;

	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
	ASSERT_HASH_MUTEX_OWN(table, fold);
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	ut_a(new_block->frame == page_align(new_data));
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (!btr_search_enabled) {
		return;
	}

	node = ha_search_with_data(table, fold, data);

	if (node) {
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
		if (table->adaptive) {
			ut_a(node->block->n_pointers > 0);
			node->block->n_pointers--;
			new_block->n_pointers++;
		}
# endif /* !UNIV_HOTBACKUP */

		node->block = new_block;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
		node->data = new_data;
	}
}

#ifndef UNIV_HOTBACKUP
/*****************************************************************//**
Removes from the chain determined by fold all nodes whose data pointer
points to the page given. */
UNIV_INTERN
void
ha_remove_all_nodes_to_page(
/*========================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold,	/*!< in: fold value */
	const page_t*	page)	/*!< in: buffer page */
{
	ha_node_t*	node;

	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
	ASSERT_HASH_MUTEX_OWN(table, fold);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(btr_search_enabled);

	node = ha_chain_get_first(table, fold);

	while (node) {
		if (page_align(ha_node_get_data(node)) == page) {

			/* Remove the hash node */

			ha_delete_hash_node(table, node);

			/* Start again from the first node in the chain
			because the deletion may compact the heap of
			nodes and move other nodes! */

			node = ha_chain_get_first(table, fold);
		} else {
			node = ha_chain_get_next(node);
		}
	}
#ifdef UNIV_DEBUG
	/* Check that all nodes really got deleted */

	node = ha_chain_get_first(table, fold);

	while (node) {
		ut_a(page_align(ha_node_get_data(node)) != page);

		node = ha_chain_get_next(node);
	}
#endif
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/*************************************************************//**
Validates a given range of the cells in hash table.
@return	TRUE if ok */
UNIV_INTERN
ibool
ha_validate(
/*========*/
	hash_table_t*	table,		/*!< in: hash table */
	ulint		start_index,	/*!< in: start index */
	ulint		end_index)	/*!< in: end index */
{
	hash_cell_t*	cell;
	ha_node_t*	node;
	ibool		ok	= TRUE;
	ulint		i;

	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
	ut_a(start_index <= end_index);
	ut_a(start_index < hash_get_n_cells(table));
	ut_a(end_index < hash_get_n_cells(table));

	for (i = start_index; i <= end_index; i++) {

		cell = hash_get_nth_cell(table, i);

		node = cell->node;

		while (node) {
			if (hash_calc_hash(node->fold, table) != i) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"InnoDB: Error: hash table node"
					" fold value %lu does not\n"
					"InnoDB: match the cell number %lu.\n",
					(ulong) node->fold, (ulong) i);

				ok = FALSE;
			}

			node = node->next;
		}
	}

	return(ok);
}
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */

/*************************************************************//**
Prints info of a hash table. */
UNIV_INTERN
void
ha_print_info(
/*==========*/
	FILE*		file,	/*!< in: file where to print */
	hash_table_t*	table)	/*!< in: hash table */
{
#ifdef UNIV_DEBUG
/* Some of the code here is disabled for performance reasons in production
builds, see http://bugs.mysql.com/36941 */
#define PRINT_USED_CELLS
#endif /* UNIV_DEBUG */

#ifdef PRINT_USED_CELLS
	hash_cell_t*	cell;
	ulint		cells	= 0;
	ulint		i;
#endif /* PRINT_USED_CELLS */
	ulint		n_bufs;

	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#ifdef PRINT_USED_CELLS
	for (i = 0; i < hash_get_n_cells(table); i++) {

		cell = hash_get_nth_cell(table, i);

		if (cell->node) {

			cells++;
		}
	}
#endif /* PRINT_USED_CELLS */

	fprintf(file, "Hash table size %lu",
		(ulong) hash_get_n_cells(table));

#ifdef PRINT_USED_CELLS
	fprintf(file, ", used cells %lu", (ulong) cells);
#endif /* PRINT_USED_CELLS */

	if (table->heaps == NULL && table->heap != NULL) {

		/* This calculation is intended for the adaptive hash
		index: how many buffer frames we have reserved? */

		n_bufs = UT_LIST_GET_LEN(table->heap->base) - 1;

		if (table->heap->free_block) {
			n_bufs++;
		}

		fprintf(file, ", node heap has %lu buffer(s)\n",
			(ulong) n_bufs);
	}
}
#endif /* !UNIV_HOTBACKUP */
