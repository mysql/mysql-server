/************************************************************************
The hash table with external chains

(c) 1994-1997 Innobase Oy

Created 8/22/1994 Heikki Tuuri
*************************************************************************/

#include "ha0ha.h"
#ifdef UNIV_NONINL
#include "ha0ha.ic"
#endif

#include "buf0buf.h"

/*****************************************************************
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n. */

hash_table_t*
ha_create(
/*======*/
				/* out, own: created table */
	ibool	in_btr_search,	/* in: TRUE if the hash table is used in
				the btr_search module */
	ulint	n,		/* in: number of array cells */
	ulint	n_mutexes,	/* in: number of mutexes to protect the
				hash table: must be a power of 2, or 0 */
	ulint	mutex_level)	/* in: level of the mutexes in the latching
				order: this is used in the debug version */
{
	hash_table_t*	table;
	ulint		i;

	table = hash_create(n);

	if (n_mutexes == 0) {
		if (in_btr_search) {
			table->heap = mem_heap_create_in_btr_search(4096);
		} else {
			table->heap = mem_heap_create_in_buffer(4096);
		}

		return(table);
	}
	
	hash_create_mutexes(table, n_mutexes, mutex_level);

	table->heaps = mem_alloc(n_mutexes * sizeof(void*));

	for (i = 0; i < n_mutexes; i++) {
		if (in_btr_search) {
			table->heaps[i] = mem_heap_create_in_btr_search(4096);
		} else {
			table->heaps[i] = mem_heap_create_in_buffer(4096);
		}
	}
	
	return(table);
}

/*****************************************************************
Checks that a hash table node is in the chain. */

ibool
ha_node_in_chain(
/*=============*/
				/* out: TRUE if in chain */
	hash_cell_t*	cell,	/* in: hash table cell */
	ha_node_t*	node)	/* in: external chain node */
{
	ha_node_t*	node2;
	
	node2 = cell->node;

	while (node2 != NULL) {

		if (node2 == node) {

			return(TRUE);
		}

		node2 = node2->next;
	}

	return(FALSE);
}

/*****************************************************************
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted. */

ibool
ha_insert_for_fold(
/*===============*/
				/* out: TRUE if succeed, FALSE if no more
				memory could be allocated */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
	void*		data)	/* in: data, must not be NULL */
{
	hash_cell_t*	cell;
	ha_node_t*	node;
	ha_node_t*	prev_node;
	ulint		hash;

	ut_ad(table && data);
	ut_ad(!table->mutexes || mutex_own(hash_get_mutex(table, fold)));
	
	hash = hash_calc_hash(fold, table);

	cell = hash_get_nth_cell(table, hash);

	prev_node = cell->node;

	while (prev_node != NULL) {
		if (prev_node->fold == fold) {

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
	
	ha_node_set_data(node, data);
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

/***************************************************************
Deletes a hash node. */

void
ha_delete_hash_node(
/*================*/
	hash_table_t*	table,		/* in: hash table */
	ha_node_t*	del_node)	/* in: node to be deleted */
{
	HASH_DELETE_AND_COMPACT(ha_node_t, next, table, del_node);
}

/*****************************************************************
Deletes an entry from a hash table. */

void
ha_delete(
/*======*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data */
	void*		data)	/* in: data, must not be NULL and must exist
				in the hash table */
{
	ha_node_t*	node;

	ut_ad(!table->mutexes || mutex_own(hash_get_mutex(table, fold)));

	node = ha_search_with_data(table, fold, data);

	ut_ad(node);

	ha_delete_hash_node(table, node);
}	

/*********************************************************************
Removes from the chain determined by fold all nodes whose data pointer
points to the page given. */

void
ha_remove_all_nodes_to_page(
/*========================*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: fold value */
	page_t*		page)	/* in: buffer page */
{
	ha_node_t*	node;

	ut_ad(!table->mutexes || mutex_own(hash_get_mutex(table, fold)));

	node = ha_chain_get_first(table, fold);

	while (node) {
		if (buf_frame_align(ha_node_get_data(node)) == page) {

			/* Remove the hash node */

			ha_delete_hash_node(table, node);

			/* Start again from the first node in the chain
			because the deletion may compact the heap of
			nodes and move other nodes! */

			node = ha_chain_get_first(table, fold);
		} else {
			node = ha_chain_get_next(table, node);
		}
	}
}

/*****************************************************************
Validates a hash table. */

ibool
ha_validate(
/*========*/
				/* out: TRUE if ok */
	hash_table_t*	table)	/* in: hash table */
{
	hash_cell_t*	cell;
	ha_node_t*	node;
	ulint		i;

	for (i = 0; i < hash_get_n_cells(table); i++) {

		cell = hash_get_nth_cell(table, i);

		node = cell->node;

		while (node) {
			ut_a(hash_calc_hash(node->fold, table) == i);

			node = node->next;
		}
	}

	return(TRUE);
}	

/*****************************************************************
Prints info of a hash table. */

void
ha_print_info(
/*==========*/
	hash_table_t*	table)	/* in: hash table */
{
	hash_cell_t*	cell;
	ha_node_t*	node;
	ulint		nodes	= 0;
	ulint		cells	= 0;
	ulint		len	= 0;
	ulint		max_len	= 0;
	ulint		i;
	
	for (i = 0; i < hash_get_n_cells(table); i++) {

		cell = hash_get_nth_cell(table, i);

		if (cell->node) {

			cells++;

			len = 0;

			node = cell->node;

			for (;;) {
				len++;
				nodes++;

				if (ha_chain_get_next(table, node) == NULL) {

					break;
				}

				node = node->next;
			}

			if (len > max_len) {
				max_len = len;
			}
		}
	}

	printf("Hash table size %lu, used cells %lu, nodes %lu\n",
				hash_get_n_cells(table), cells, nodes);
	printf("max chain length %lu\n", max_len);

	ut_a(ha_validate(table));
}	
