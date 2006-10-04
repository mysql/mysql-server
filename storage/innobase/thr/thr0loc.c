/******************************************************
The thread local storage

(c) 1995 Innobase Oy

Created 10/5/1995 Heikki Tuuri
*******************************************************/

#include "thr0loc.h"
#ifdef UNIV_NONINL
#include "thr0loc.ic"
#endif

#include "sync0sync.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "srv0srv.h"

/*
	IMPLEMENTATION OF THREAD LOCAL STORAGE
	======================================

The threads sometimes need private data which depends on the thread id.
This is implemented as a hash table, where the hash value is calculated
from the thread id, to prepare for a large number of threads. The hash table
is protected by a mutex. If you need modify the program and put new data to
the thread local storage, just add it to struct thr_local_struct in the
header file. */

/* Mutex protecting the local storage hash table */
mutex_t	thr_local_mutex;

/* The hash table. The module is not yet initialized when it is NULL. */
hash_table_t*	thr_local_hash	= NULL;

/* The private data for each thread should be put to
the structure below and the accessor functions written
for the field. */
typedef struct thr_local_struct thr_local_t;

struct thr_local_struct{
	os_thread_id_t	id;	/* id of the thread which owns this struct */
	os_thread_t	handle;	/* operating system handle to the thread */
	ulint		slot_no;/* the index of the slot in the thread table
				for this thread */
	ibool		in_ibuf;/* TRUE if the the thread is doing an ibuf
				operation */
	hash_node_t	hash;	/* hash chain node */
	ulint		magic_n;
};

#define THR_LOCAL_MAGIC_N	1231234

/***********************************************************************
Returns the local storage struct for a thread. */
static
thr_local_t*
thr_local_get(
/*==========*/
				/* out: local storage */
	os_thread_id_t	id)	/* in: thread id of the thread */
{
	thr_local_t*	local;

try_again:
	ut_ad(thr_local_hash);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&thr_local_mutex));
#endif /* UNIV_SYNC_DEBUG */

	/* Look for the local struct in the hash table */

	local = NULL;

	HASH_SEARCH(hash, thr_local_hash, os_thread_pf(id),
		    local, os_thread_eq(local->id, id));
	if (local == NULL) {
		mutex_exit(&thr_local_mutex);

		thr_local_create();

		mutex_enter(&thr_local_mutex);

		goto try_again;
	}

	ut_ad(local->magic_n == THR_LOCAL_MAGIC_N);

	return(local);
}

/***********************************************************************
Gets the slot number in the thread table of a thread. */

ulint
thr_local_get_slot_no(
/*==================*/
				/* out: slot number */
	os_thread_id_t	id)	/* in: thread id of the thread */
{
	ulint		slot_no;
	thr_local_t*	local;

	mutex_enter(&thr_local_mutex);

	local = thr_local_get(id);

	slot_no = local->slot_no;

	mutex_exit(&thr_local_mutex);

	return(slot_no);
}

/***********************************************************************
Sets the slot number in the thread table of a thread. */

void
thr_local_set_slot_no(
/*==================*/
	os_thread_id_t	id,	/* in: thread id of the thread */
	ulint		slot_no)/* in: slot number */
{
	thr_local_t*	local;

	mutex_enter(&thr_local_mutex);

	local = thr_local_get(id);

	local->slot_no = slot_no;

	mutex_exit(&thr_local_mutex);
}

/***********************************************************************
Returns pointer to the 'in_ibuf' field within the current thread local
storage. */

ibool*
thr_local_get_in_ibuf_field(void)
/*=============================*/
			/* out: pointer to the in_ibuf field */
{
	thr_local_t*	local;

	mutex_enter(&thr_local_mutex);

	local = thr_local_get(os_thread_get_curr_id());

	mutex_exit(&thr_local_mutex);

	return(&(local->in_ibuf));
}

/***********************************************************************
Creates a local storage struct for the calling new thread. */

void
thr_local_create(void)
/*==================*/
{
	thr_local_t*	local;

	if (thr_local_hash == NULL) {
		thr_local_init();
	}

	local = mem_alloc(sizeof(thr_local_t));

	local->id = os_thread_get_curr_id();
	local->handle = os_thread_get_curr();
	local->magic_n = THR_LOCAL_MAGIC_N;

	local->in_ibuf = FALSE;

	mutex_enter(&thr_local_mutex);

	HASH_INSERT(thr_local_t, hash, thr_local_hash,
		    os_thread_pf(os_thread_get_curr_id()),
		    local);

	mutex_exit(&thr_local_mutex);
}

/***********************************************************************
Frees the local storage struct for the specified thread. */

void
thr_local_free(
/*===========*/
	os_thread_id_t	id)	/* in: thread id */
{
	thr_local_t*	local;

	mutex_enter(&thr_local_mutex);

	/* Look for the local struct in the hash table */

	HASH_SEARCH(hash, thr_local_hash, os_thread_pf(id),
		    local, os_thread_eq(local->id, id));
	if (local == NULL) {
		mutex_exit(&thr_local_mutex);

		return;
	}

	HASH_DELETE(thr_local_t, hash, thr_local_hash,
		    os_thread_pf(id), local);

	mutex_exit(&thr_local_mutex);

	ut_a(local->magic_n == THR_LOCAL_MAGIC_N);

	mem_free(local);
}

/********************************************************************
Initializes the thread local storage module. */

void
thr_local_init(void)
/*================*/
{

	ut_a(thr_local_hash == NULL);

	thr_local_hash = hash_create(OS_THREAD_MAX_N + 100);

	mutex_create(&thr_local_mutex, SYNC_THR_LOCAL);
}
