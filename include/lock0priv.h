/******************************************************
Definition of the lock module internal structures.

(c) 2007 Innobase Oy

Created 12/07/2007 Vasil Dimov
*******************************************************/

#ifndef lock0priv_h
#define lock0priv_h

#ifndef LOCK_MODULE_IMPLEMENTATION
/* If you need to access members of the structures defined in this
file, please write appropriate functions that retrieve them and put
those functions in lock/ */
#error Do not include lock0priv.h outside of the lock/ module
#endif

#include "univ.i"
#include "dict0types.h"
#include "hash0hash.h"
#include "trx0types.h"
#include "ut0lst.h"

/* A table lock */
typedef struct lock_table_struct	lock_table_t;
struct lock_table_struct {
	dict_table_t*	table;		/* database table in dictionary
					cache */
	UT_LIST_NODE_T(lock_t)
			locks;		/* list of locks on the same
					table */
};

/* Record lock for a page */
typedef struct lock_rec_struct		lock_rec_t;
struct lock_rec_struct {
	ulint	space;			/* space id */
	ulint	page_no;		/* page number */
	ulint	n_bits;			/* number of bits in the lock
					bitmap; NOTE: the lock bitmap is
					placed immediately after the
					lock struct */
};

/* Lock struct */
struct lock_struct {
	trx_t*		trx;		/* transaction owning the
					lock */
	UT_LIST_NODE_T(lock_t)
			trx_locks;	/* list of the locks of the
					transaction */
	ulint		type_mode;	/* lock type, mode, LOCK_GAP or
					LOCK_REC_NOT_GAP,
					LOCK_INSERT_INTENTION,
					wait flag, ORed */
	hash_node_t	hash;		/* hash chain node for a record
					lock */
	dict_index_t*	index;		/* index for a record lock */
	union {
		lock_table_t	tab_lock;/* table lock */
		lock_rec_t	rec_lock;/* record lock */
	} un_member;
};

#endif /* lock0priv_h */
