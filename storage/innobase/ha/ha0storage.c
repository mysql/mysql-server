/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

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

/**************************************************//**
@file ha/ha0storage.c
Hash storage.
Provides a data structure that stores chunks of data in
its own storage, avoiding duplicates.

Created September 22, 2007 Vasil Dimov
*******************************************************/

#include "univ.i"
#include "ha0storage.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "ut0rnd.h"

#ifdef UNIV_NONINL
#include "ha0storage.ic"
#endif

/*******************************************************************//**
Retrieves a data from a storage. If it is present, a pointer to the
stored copy of data is returned, otherwise NULL is returned. */
static
const void*
ha_storage_get(
/*===========*/
	ha_storage_t*	storage,	/*!< in: hash storage */
	const void*	data,		/*!< in: data to check for */
	ulint		data_len)	/*!< in: data length */
{
	ha_storage_node_t*	node;
	ulint			fold;

	/* avoid repetitive calls to ut_fold_binary() in the HASH_SEARCH
	macro */
	fold = ut_fold_binary(data, data_len);

#define IS_FOUND	\
	node->data_len == data_len && memcmp(node->data, data, data_len) == 0

	HASH_SEARCH(
		next,			/* node->"next" */
		storage->hash,		/* the hash table */
		fold,			/* key */
		ha_storage_node_t*,	/* type of node->next */
		node,			/* auxiliary variable */
		,			/* assertion */
		IS_FOUND);		/* search criteria */

	if (node == NULL) {

		return(NULL);
	}
	/* else */

	return(node->data);
}

/*******************************************************************//**
Copies data into the storage and returns a pointer to the copy. If the
same data chunk is already present, then pointer to it is returned.
Data chunks are considered to be equal if len1 == len2 and
memcmp(data1, data2, len1) == 0. If "data" is not present (and thus
data_len bytes need to be allocated) and the size of storage is going to
become more than "memlim" then "data" is not added and NULL is returned.
To disable this behavior "memlim" can be set to 0, which stands for
"no limit". */
UNIV_INTERN
const void*
ha_storage_put_memlim(
/*==================*/
	ha_storage_t*	storage,	/*!< in/out: hash storage */
	const void*	data,		/*!< in: data to store */
	ulint		data_len,	/*!< in: data length */
	ulint		memlim)		/*!< in: memory limit to obey */
{
	void*			raw;
	ha_storage_node_t*	node;
	const void*		data_copy;
	ulint			fold;

	/* check if data chunk is already present */
	data_copy = ha_storage_get(storage, data, data_len);
	if (data_copy != NULL) {

		return(data_copy);
	}

	/* not present */

	/* check if we are allowed to allocate data_len bytes */
	if (memlim > 0
	    && ha_storage_get_size(storage) + data_len > memlim) {

		return(NULL);
	}

	/* we put the auxiliary node struct and the data itself in one
	continuous block */
	raw = mem_heap_alloc(storage->heap,
			     sizeof(ha_storage_node_t) + data_len);

	node = (ha_storage_node_t*) raw;
	data_copy = (byte*) raw + sizeof(*node);

	memcpy((byte*) raw + sizeof(*node), data, data_len);

	node->data_len = data_len;
	node->data = data_copy;

	/* avoid repetitive calls to ut_fold_binary() in the HASH_INSERT
	macro */
	fold = ut_fold_binary(data, data_len);

	HASH_INSERT(
		ha_storage_node_t,	/* type used in the hash chain */
		next,			/* node->"next" */
		storage->hash,		/* the hash table */
		fold,			/* key */
		node);			/* add this data to the hash */

	/* the output should not be changed because it will spoil the
	hash table */
	return(data_copy);
}

#ifdef UNIV_COMPILE_TEST_FUNCS

void
test_ha_storage()
{
	ha_storage_t*	storage;
	char		buf[1024];
	int		i;
	const void*	stored[256];
	const void*	p;

	storage = ha_storage_create(0, 0);

	for (i = 0; i < 256; i++) {

		memset(buf, i, sizeof(buf));
		stored[i] = ha_storage_put(storage, buf, sizeof(buf));
	}

	//ha_storage_empty(&storage);

	for (i = 255; i >= 0; i--) {

		memset(buf, i, sizeof(buf));
		p = ha_storage_put(storage, buf, sizeof(buf));

		if (p != stored[i]) {

			fprintf(stderr, "ha_storage_put() returned %p "
				"instead of %p, i=%d\n", p, stored[i], i);
			return;
		}
	}

	fprintf(stderr, "all ok\n");

	ha_storage_free(storage);
}

#endif /* UNIV_COMPILE_TEST_FUNCS */
