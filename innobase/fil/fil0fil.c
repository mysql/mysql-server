/******************************************************
The low-level file system

(c) 1995 Innobase Oy

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#include "fil0fil.h"

#include "mem0mem.h"
#include "sync0sync.h"
#include "hash0hash.h"
#include "os0file.h"
#include "os0sync.h"
#include "mach0data.h"
#include "ibuf0ibuf.h"
#include "buf0buf.h"
#include "log0log.h"
#include "log0recv.h"
#include "fsp0fsp.h"
#include "srv0srv.h"

/*
		IMPLEMENTATION OF THE LOW-LEVEL FILE SYSTEM
		===========================================

The file system is responsible for providing fast read/write access to
tablespaces and logs of the database. File creation and deletion is done
in other modules which know more of the logic of the operation, however.

A tablespace consists of a chain of files. The size of the files does not
have to be divisible by the database block size, because we may just leave
the last incomplete block unused. When a new file is appended to the
tablespace, the maximum size of the file is also specified. At the moment,
we think that it is best to extend the file to its maximum size already at
the creation of the file, because then we can avoid dynamically extending
the file when more space is needed for the tablespace.

A block's position in the tablespace is specified with a 32-bit unsigned
integer. The files in the chain are thought to be catenated, and the block
corresponding to an address n is the nth block in the catenated file (where
the first block is named the 0th block, and the incomplete block fragments
at the end of files are not taken into account). A tablespace can be extended
by appending a new file at the end of the chain.

Our tablespace concept is similar to the one of Oracle.

To acquire more speed in disk transfers, a technique called disk striping is
sometimes used. This means that logical block addresses are divided in a
round-robin fashion across several disks. Windows NT supports disk striping,
so there we do not need to support it in the database. Disk striping is
implemented in hardware in RAID disks. We conclude that it is not necessary
to implement it in the database. Oracle 7 does not support disk striping,
either.

Another trick used at some database sites is replacing tablespace files by
raw disks, that is, the whole physical disk drive, or a partition of it, is
opened as a single file, and it is accessed through byte offsets calculated
from the start of the disk or the partition. This is recommended in some
books on database tuning to achieve more speed in i/o. Using raw disk
certainly prevents the OS from fragmenting disk space, but it is not clear
if it really adds speed. We measured on the Pentium 100 MHz + NT + NTFS file
system + EIDE Conner disk only a negligible difference in speed when reading
from a file, versus reading from a raw disk. 

To have fast access to a tablespace or a log file, we put the data structures
to a hash table. Each tablespace and log file is given an unique 32-bit
identifier.

Some operating systems do not support many open files at the same time,
though NT seems to tolerate at least 900 open files. Therefore, we put the
open files in an LRU-list. If we need to open another file, we may close the
file at the end of the LRU-list. When an i/o-operation is pending on a file,
the file cannot be closed. We take the file nodes with pending i/o-operations
out of the LRU-list and keep a count of pending operations. When an operation
completes, we decrement the count and return the file node to the LRU-list if
the count drops to zero. */

ulint	fil_n_pending_log_flushes		= 0;
ulint	fil_n_pending_tablespace_flushes	= 0;

/* Null file address */
fil_addr_t	fil_addr_null = {FIL_NULL, 0};

/* File system file node data structure */
typedef	struct fil_node_struct	fil_node_t;
struct fil_node_struct {
	char*		name;	/* the file name or path */
	ibool		open;	/* TRUE if file open */
	os_file_t	handle;	/* OS handle to the file, if file open */
	ulint		size;	/* size of the file in database blocks
				(where the possible last incomplete block
				is ignored) */
	ulint		n_pending;
				/* count of pending i/o-ops on this file */
	ibool		is_modified; /* this is set to TRUE when we write
				to the file and FALSE when we call fil_flush
				for this file space */
	UT_LIST_NODE_T(fil_node_t) chain;
				/* link field for the file chain */
	UT_LIST_NODE_T(fil_node_t) LRU;
				/* link field for the LRU list */
	ulint		magic_n;
};

#define	FIL_NODE_MAGIC_N	89389

/* File system tablespace or log data structure: let us call them by a common
name space */
struct fil_space_struct {
	char*		name;	/* space name */
	ulint		id;	/* space id */
	ulint		purpose;/* FIL_TABLESPACE, FIL_LOG, or FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/* base node for the file chain */
	ulint		size;	/* space size in pages */
	ulint		n_reserved_extents;
				/* number of reserved free extents for
				ongoing operations like B-tree page split */
	hash_node_t	hash; 	/* hash chain node */
	rw_lock_t	latch;	/* latch protecting the file space storage
				allocation */
	UT_LIST_NODE_T(fil_space_t) space_list;
				/* list of all spaces */
	ibuf_data_t*	ibuf_data;
				/* insert buffer data */
	ulint		magic_n;
};

#define	FIL_SPACE_MAGIC_N	89472

/* The file system data structure */

typedef	struct fil_system_struct	fil_system_t;
struct fil_system_struct {
	mutex_t		mutex;		/* The mutex protecting the system */
	hash_table_t*	spaces;		/* The hash table of spaces in the
					system */	
	UT_LIST_BASE_NODE_T(fil_node_t) LRU;
					/* base node for the LRU list of the
					most recently used open files */
	ulint		n_open_pending;	/* current number of open files with
					pending i/o-ops on them */
	ulint		max_n_open;	/* maximum allowed open files */
	os_event_t	can_open;	/* this event is set to the signaled
					state when the system is capable of
					opening a new file, i.e.,
					n_open_pending < max_n_open */
	UT_LIST_BASE_NODE_T(fil_space_t) space_list;
					/* list of all file spaces */
};

/* The file system. This variable is NULL before the module is initialized. */
fil_system_t*	fil_system	= NULL;

/* The file system hash table size */
#define	FIL_SYSTEM_HASH_SIZE	500


/***********************************************************************
Reserves a right to open a single file. The right must be released with
fil_release_right_to_open. */

void
fil_reserve_right_to_open(void)
/*===========================*/
{
loop:
	mutex_enter(&(fil_system->mutex));
	
	if (fil_system->n_open_pending == fil_system->max_n_open) {

		/* It is not sure we can open the file if it is closed: wait */

		os_event_reset(fil_system->can_open);

		mutex_exit(&(fil_system->mutex));

		os_event_wait(fil_system->can_open);

		goto loop;
	}

	fil_system->max_n_open--;

	mutex_exit(&(fil_system->mutex));
}

/***********************************************************************
Releases a right to open a single file. */

void
fil_release_right_to_open(void)
/*===========================*/
{
	mutex_enter(&(fil_system->mutex));
	
	if (fil_system->n_open_pending == fil_system->max_n_open) {

		os_event_set(fil_system->can_open);
	}

	fil_system->max_n_open++;

	mutex_exit(&(fil_system->mutex));
}

/***********************************************************************
Returns the latch of a file space. */

rw_lock_t*
fil_space_get_latch(
/*================*/
			/* out: latch protecting storage allocation */
	ulint	id)	/* in: space id */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	mutex_exit(&(system->mutex));

	return(&(space->latch));
}

/***********************************************************************
Returns the type of a file space. */

ulint
fil_space_get_type(
/*===============*/
			/* out: FIL_TABLESPACE or FIL_LOG */
	ulint	id)	/* in: space id */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	mutex_exit(&(system->mutex));

	return(space->purpose);
}

/***********************************************************************
Returns the ibuf data of a file space. */

ibuf_data_t*
fil_space_get_ibuf_data(
/*====================*/
			/* out: ibuf data for this space */
	ulint	id)	/* in: space id */
{
	fil_space_t*	space;
	fil_system_t*	system	= fil_system;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	mutex_exit(&(system->mutex));

	return(space->ibuf_data);
}

/***********************************************************************
Appends a new file to the chain of files of a space. File must be closed. */

void
fil_node_create(
/*============*/
	char*	name,	/* in: file name (file must be closed) */
	ulint	size,	/* in: file size in database blocks, rounded downwards
			to an integer */
	ulint	id)	/* in: space id where to append */
{
	fil_node_t*	node;
	fil_space_t*	space;
	char*		name2;
	fil_system_t*	system		= fil_system;

	ut_a(system);
	ut_a(name);
	ut_a(size > 0);

	mutex_enter(&(system->mutex));

	node = mem_alloc(sizeof(fil_node_t));

	name2 = mem_alloc(ut_strlen(name) + 1);

	ut_strcpy(name2, name);

	node->name = name2;
	node->open = FALSE;
	node->size = size;
	node->magic_n = FIL_NODE_MAGIC_N;
	node->n_pending = 0;

	node->is_modified = FALSE;
	
	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	space->size += size;

	UT_LIST_ADD_LAST(chain, space->chain, node);
				
	mutex_exit(&(system->mutex));
}

/**************************************************************************
Closes a file. */
static
void
fil_node_close(
/*===========*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system)	/* in: file system */
{
	ibool	ret;

	ut_ad(node && system);
	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->open);
	ut_a(node->n_pending == 0);

	ret = os_file_close(node->handle);
	ut_a(ret);

	node->open = FALSE;

	/* The node is in the LRU list, remove it */
	UT_LIST_REMOVE(LRU, system->LRU, node);
}

/***********************************************************************
Frees a file node object from a file system. */
static
void
fil_node_free(
/*==========*/
	fil_node_t*	node,	/* in, own: file node */
	fil_system_t*	system,	/* in: file system */
	fil_space_t*	space)	/* in: space where the file node is chained */
{
	ut_ad(node && system && space);
	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->magic_n == FIL_NODE_MAGIC_N);

	if (node->open) {
		fil_node_close(node, system);
	}

	space->size -= node->size;
	
	UT_LIST_REMOVE(chain, space->chain, node);

	mem_free(node->name);
	mem_free(node);
}

/********************************************************************
Drops files from the start of a file space, so that its size is cut by
the amount given. */

void
fil_space_truncate_start(
/*=====================*/
	ulint	id,		/* in: space id */
	ulint	trunc_len)	/* in: truncate by this much; it is an error
				if this does not equal to the combined size of
				some initial files in the space */
{
	fil_node_t*	node;
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space);
	
	while (trunc_len > 0) {

		node = UT_LIST_GET_FIRST(space->chain);

		ut_a(node->size * UNIV_PAGE_SIZE >= trunc_len);

		trunc_len -= node->size * UNIV_PAGE_SIZE;

		fil_node_free(node, system, space);
	}	
				
	mutex_exit(&(system->mutex));
}				

/********************************************************************
Creates a file system object. */
static
fil_system_t*
fil_system_create(
/*==============*/
				/* out, own: file system object */
	ulint	hash_size,	/* in: hash table size */
	ulint	max_n_open)	/* in: maximum number of open files */
{
	fil_system_t*	system;

	ut_a(hash_size > 0);
	ut_a(max_n_open > 0);

	system = mem_alloc(sizeof(fil_system_t));

	mutex_create(&(system->mutex));

	mutex_set_level(&(system->mutex), SYNC_ANY_LATCH);

	system->spaces = hash_create(hash_size);

	UT_LIST_INIT(system->LRU);

	system->n_open_pending = 0;
	system->max_n_open = max_n_open;
	system->can_open = os_event_create(NULL);

	UT_LIST_INIT(system->space_list);

	return(system);
}

/********************************************************************
Initializes the file system of this module. */

void
fil_init(
/*=====*/
	ulint	max_n_open)	/* in: max number of open files */
{
	ut_a(fil_system == NULL);

	fil_system = fil_system_create(FIL_SYSTEM_HASH_SIZE, max_n_open);
}

/********************************************************************
Writes the flushed lsn to the header of each file space. */

void
fil_ibuf_init_at_db_start(void)
/*===========================*/
{
	fil_space_t*	space;

	space = UT_LIST_GET_FIRST(fil_system->space_list);
	
	while (space) {
		if (space->purpose == FIL_TABLESPACE) {
			space->ibuf_data = ibuf_data_init_for_space(space->id);
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}
}

/********************************************************************
Writes the flushed lsn and the latest archived log number to the page
header of the first page of a data file. */
static
ulint
fil_write_lsn_and_arch_no_to_file(
/*==============================*/
	ulint	space_id,	/* in: space number */
	ulint	sum_of_sizes,	/* in: combined size of previous files in space,
				in database pages */
	dulint	lsn,		/* in: lsn to write */
	ulint	arch_log_no)	/* in: archived log number to write */
{
	byte*	buf1;
	byte*	buf;

	buf1 = mem_alloc(2 * UNIV_PAGE_SIZE);
	buf = ut_align(buf1, UNIV_PAGE_SIZE);

	fil_read(TRUE, space_id, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	mach_write_to_8(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);
	mach_write_to_4(buf + FIL_PAGE_ARCH_LOG_NO, arch_log_no);

	fil_write(TRUE, space_id, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	return(DB_SUCCESS);	
}

/********************************************************************
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file. */

ulint
fil_write_flushed_lsn_to_data_files(
/*================================*/
				/* out: DB_SUCCESS or error number */
	dulint	lsn,		/* in: lsn to write */
	ulint	arch_log_no)	/* in: latest archived log file number */
{
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		sum_of_sizes;
	ulint		err;

	mutex_enter(&(fil_system->mutex));
	
	space = UT_LIST_GET_FIRST(fil_system->space_list);
	
	while (space) {
		if (space->purpose == FIL_TABLESPACE) {
			sum_of_sizes = 0;

			node = UT_LIST_GET_FIRST(space->chain);

			while (node) {
				mutex_exit(&(fil_system->mutex));

				err = fil_write_lsn_and_arch_no_to_file(
							space->id,
							sum_of_sizes,
							lsn, arch_log_no);
				if (err != DB_SUCCESS) {

					return(err);
				}

				mutex_enter(&(fil_system->mutex));

				sum_of_sizes += node->size;

				node = UT_LIST_GET_NEXT(chain, node);
			}
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(fil_system->mutex));

	return(DB_SUCCESS);
}

/***********************************************************************
Reads the flushed lsn and arch no fields from a data file at database
startup. */

void
fil_read_flushed_lsn_and_arch_log_no(
/*=================================*/
	os_file_t data_file,		/* in: open data file */
	ibool	one_read_already,	/* in: TRUE if min and max parameters
					below already contain sensible data */
	dulint*	min_flushed_lsn,	/* in/out: */
	ulint*	min_arch_log_no,	/* in/out: */
	dulint*	max_flushed_lsn,	/* in/out: */
	ulint*	max_arch_log_no)	/* in/out: */
{
	byte*	buf;
	dulint	flushed_lsn;
	ulint	arch_log_no;

	buf = ut_malloc(UNIV_PAGE_SIZE);

	os_file_read(data_file, buf, 0, 0, UNIV_PAGE_SIZE);

	flushed_lsn = mach_read_from_8(buf + FIL_PAGE_FILE_FLUSH_LSN);
	arch_log_no = mach_read_from_4(buf + FIL_PAGE_ARCH_LOG_NO);

	ut_free(buf);

	if (!one_read_already) {
		*min_flushed_lsn = flushed_lsn;
		*max_flushed_lsn = flushed_lsn;
		*min_arch_log_no = arch_log_no;
		*max_arch_log_no = arch_log_no;

		return;
	}

	if (ut_dulint_cmp(*min_flushed_lsn, flushed_lsn) > 0) {
		*min_flushed_lsn = flushed_lsn;
	}
	if (ut_dulint_cmp(*max_flushed_lsn, flushed_lsn) < 0) {
		*max_flushed_lsn = flushed_lsn;
	}
	if (*min_arch_log_no > arch_log_no) {
		*min_arch_log_no = arch_log_no;
	}
	if (*max_arch_log_no < arch_log_no) {
		*max_arch_log_no = arch_log_no;
	}
}

/***********************************************************************
Creates a space object and puts it to the file system. */

void
fil_space_create(
/*=============*/
	char*	name,	/* in: space name */
	ulint	id,	/* in: space id */
	ulint	purpose)/* in: FIL_TABLESPACE, or FIL_LOG if log */
{
	fil_space_t*	space;	
	char*		name2;
	fil_system_t*	system = fil_system;
	
	ut_a(system);
	ut_a(name);

#ifndef UNIV_BASIC_LOG_DEBUG
	/* Spaces with an odd id number are reserved to replicate spaces
	used in log debugging */
	
	ut_a((purpose == FIL_LOG) || (id % 2 == 0));
#endif
	mutex_enter(&(system->mutex));

	space = mem_alloc(sizeof(fil_space_t));

	name2 = mem_alloc(ut_strlen(name) + 1);

	ut_strcpy(name2, name);

	space->name = name2;
	space->id = id;
	space->purpose = purpose;
	space->size = 0;

	space->n_reserved_extents = 0;
	
	UT_LIST_INIT(space->chain);
	space->magic_n = FIL_SPACE_MAGIC_N;

	space->ibuf_data = NULL;
	
	rw_lock_create(&(space->latch));
	rw_lock_set_level(&(space->latch), SYNC_FSP);
	
	HASH_INSERT(fil_space_t, hash, system->spaces, id, space);

	UT_LIST_ADD_LAST(space_list, system->space_list, space);
				
	mutex_exit(&(system->mutex));
}

/***********************************************************************
Frees a space object from a file system. Closes the files in the chain
but does not delete them. */

void
fil_space_free(
/*===========*/
	ulint	id)	/* in: space id */
{
	fil_space_t*	space;
	fil_node_t*	fil_node;
	fil_system_t*	system 		= fil_system;
	
	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	HASH_DELETE(fil_space_t, hash, system->spaces, id, space);

	UT_LIST_REMOVE(space_list, system->space_list, space);

	ut_a(space->magic_n == FIL_SPACE_MAGIC_N);

	fil_node = UT_LIST_GET_FIRST(space->chain);

	ut_d(UT_LIST_VALIDATE(chain, fil_node_t, space->chain));

	while (fil_node != NULL) {
		fil_node_free(fil_node, system, space);

		fil_node = UT_LIST_GET_FIRST(space->chain);
	}	
	
	ut_d(UT_LIST_VALIDATE(chain, fil_node_t, space->chain));
	ut_ad(0 == UT_LIST_GET_LEN(space->chain));

	mutex_exit(&(system->mutex));

	mem_free(space->name);
	mem_free(space);
}

/***********************************************************************
Returns the size of the space in pages. */

ulint
fil_space_get_size(
/*===============*/
			/* out: space size */
	ulint	id)	/* in: space id */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;
	ulint		size;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	size = space->size;
	
	mutex_exit(&(system->mutex));

	return(size);
}

/***********************************************************************
Checks if the pair space, page_no refers to an existing page in a
tablespace file space. */

ibool
fil_check_adress_in_tablespace(
/*===========================*/
			/* out: TRUE if the address is meaningful */
	ulint	id,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;
	ulint		size;
	ibool		ret;
	
	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		ret = FALSE;
	} else {
		size = space->size;

		if (page_no > size) {
			ret = FALSE;
		} else if (space->purpose != FIL_TABLESPACE) {
			ret = FALSE;
		} else {
			ret = TRUE;
		}
	}
	
	mutex_exit(&(system->mutex));

	return(ret);
}

/***********************************************************************
Tries to reserve free extents in a file space. */

ibool
fil_space_reserve_free_extents(
/*===========================*/
				/* out: TRUE if succeed */
	ulint	id,		/* in: space id */
	ulint	n_free_now,	/* in: number of free extents now */
	ulint	n_to_reserve)	/* in: how many one wants to reserve */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;
	ibool		success;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space->n_reserved_extents + n_to_reserve > n_free_now) {
		success = FALSE;
	} else {
		space->n_reserved_extents += n_to_reserve;
		success = TRUE;
	}
	
	mutex_exit(&(system->mutex));

	return(success);
}

/***********************************************************************
Releases free extents in a file space. */

void
fil_space_release_free_extents(
/*===========================*/
	ulint	id,		/* in: space id */
	ulint	n_reserved)	/* in: how many one reserved */
{
	fil_space_t*	space;
	fil_system_t*	system		= fil_system;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space->n_reserved_extents >= n_reserved);
	
	space->n_reserved_extents -= n_reserved;
	
	mutex_exit(&(system->mutex));
}

/************************************************************************
Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. */
static
void
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: file system */
	fil_space_t*	space)	/* in: space */
{
	ibool		ret;
	fil_node_t*	last_node;

	ut_ad(node && system && space);
	ut_ad(mutex_own(&(system->mutex)));
	
	if (node->open == FALSE) {
		/* File is closed */
		ut_a(node->n_pending == 0);

		/* If too many files are open, close one */

		if (system->n_open_pending + UT_LIST_GET_LEN(system->LRU)
						== system->max_n_open) {

		    	ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

			last_node = UT_LIST_GET_LAST(system->LRU);

			if (last_node == NULL) {
				fprintf(stderr,
	"InnoDB: Error: cannot close any file to open another for i/o\n"
	"InnoDB: Pending i/o's on %lu files exist\n",
					system->n_open_pending);

				ut_a(0);
			}

			fil_node_close(last_node, system);
		}

		if (space->purpose == FIL_LOG) {	
			node->handle = os_file_create(node->name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_LOG_FILE, &ret);
		} else {
			node->handle = os_file_create(node->name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_DATA_FILE, &ret);
		}
		
		ut_a(ret);
		
		node->open = TRUE;

		system->n_open_pending++;
		node->n_pending = 1;

		/* File was closed: the node was not in the LRU list */

		return;
	}

	/* File is open */
	if (node->n_pending == 0) {
		/* The node is in the LRU list, remove it */

		UT_LIST_REMOVE(LRU, system->LRU, node);

		system->n_open_pending++;
		node->n_pending = 1;
	} else {
		/* There is already a pending i/o-op on the file: the node is
		not in the LRU list */

		node->n_pending++;
	}
}

/************************************************************************
Updates the data structures when an i/o operation finishes. Updates the
pending i/os field in the node and the system appropriately. Puts the node
in the LRU list if there are no other pending i/os. */
static
void
fil_node_complete_io(
/*=================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: file system */
	ulint		type)	/* in: OS_FILE_WRITE or ..._READ */
{
	ut_ad(node);
	ut_ad(system);
	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->n_pending > 0);
	
	node->n_pending--;

	if (type != OS_FILE_READ) {
		node->is_modified = TRUE;
	}
	
	if (node->n_pending == 0) {
		/* The node must be put back to the LRU list */
		UT_LIST_ADD_FIRST(LRU, system->LRU, node);

		ut_a(system->n_open_pending > 0);

		system->n_open_pending--;

		if (system->n_open_pending == system->max_n_open - 1) {

			os_event_set(system->can_open);
		}
	}
}
		
/************************************************************************
Reads or writes data. This operation is asynchronous (aio). */

void
fil_io(
/*===*/
	ulint	type,		/* in: OS_FILE_READ or OS_FILE_WRITE,
				ORed to OS_FILE_LOG, if a log i/o
				and ORed to OS_AIO_SIMULATED_WAKE_LATER
				if simulated aio and we want to post a
				batch of i/os; NOTE that a simulated batch
				may introduce hidden chances of deadlocks,
				because i/os are not actually handled until
				all have been posted: use with great
				caution! */
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in
				aio this must be divisible by the OS block
				size */
	ulint	len,		/* in: how many bytes to read; this must
				not cross a file boundary; in aio this must
				be a block size multiple */
	void*	buf,		/* in/out: buffer where to store read data
				or from where to write; in aio this must be
				appropriately aligned */
	void*	message)	/* in: message for aio handler if non-sync
				aio used, else ignored */
{
	ulint		mode;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		offset_high;
	ulint		offset_low;
	fil_system_t*	system;
	os_event_t	event;
	ibool		ret;
	ulint		is_log;
	ulint		wake_later;
	ulint		count;
	
	is_log = type & OS_FILE_LOG;
	type = type & ~OS_FILE_LOG;

	wake_later = type & OS_AIO_SIMULATED_WAKE_LATER;
	type = type & ~OS_AIO_SIMULATED_WAKE_LATER;
	
	ut_ad(byte_offset < UNIV_PAGE_SIZE);
	ut_ad(buf);
	ut_ad(len > 0);
	ut_ad((1 << UNIV_PAGE_SIZE_SHIFT) == UNIV_PAGE_SIZE);
	ut_ad(fil_validate());
#ifndef UNIV_LOG_DEBUG
	/* ibuf bitmap pages must be read in the sync aio mode: */
	ut_ad(recv_no_ibuf_operations || (type == OS_FILE_WRITE)
		|| !ibuf_bitmap_page(block_offset) || sync || is_log);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!ibuf_inside() || is_log || (type == OS_FILE_WRITE)
					|| ibuf_page(space_id, block_offset));
#endif
#endif
	if (sync) {
		mode = OS_AIO_SYNC;
	} else if (type == OS_FILE_READ && !is_log
				&& ibuf_page(space_id, block_offset)) {
		mode = OS_AIO_IBUF;
	} else if (is_log) {
		mode = OS_AIO_LOG;
	} else {
		mode = OS_AIO_NORMAL;
	}

	system = fil_system;

	count = 0;
loop:
	count++;
	
	/* NOTE that there is a possibility of a hang here:
	if the read i/o-handler thread needs to complete
	a read by reading from the insert buffer, it may need to
	post another read. But if the maximum number of files
	are already open, it cannot proceed from here! */
	
	mutex_enter(&(system->mutex));
	
	if (count < 500 && !is_log && !ibuf_inside()
	    && system->n_open_pending >= (3 * system->max_n_open) / 4) {

	    	/* We are not doing an ibuf operation: leave a
	    	safety margin of openable files for possible ibuf
	    	merges needed in page read completion */

		mutex_exit(&(system->mutex));

		/* Wake the i/o-handler threads to make sure pending
		i/o's are handled and eventually we can open the file */
		
		os_aio_simulated_wake_handler_threads();

		os_thread_sleep(100000);

		if (count > 50) {
			fprintf(stderr,
		"InnoDB: Warning: waiting for file closes to proceed\n"
		"InnoDB: round %lu\n", count);
		}

		goto loop;
	}

	if (system->n_open_pending == system->max_n_open) {

		/* It is not sure we can open the file if it is closed: wait */

		event = system->can_open;
		os_event_reset(event);

		mutex_exit(&(system->mutex));

		/* Wake the i/o-handler threads to make sure pending
		i/o's are handled and eventually we can open the file */
		
		os_aio_simulated_wake_handler_threads();

		fprintf(stderr,
		"InnoDB: Warning: max allowed number of files is open\n");

		os_event_wait(event);

		goto loop;
	}	 

	HASH_SEARCH(hash, system->spaces, space_id, space,
						space->id == space_id);
	ut_a(space);

	ut_ad((mode != OS_AIO_IBUF) || (space->purpose == FIL_TABLESPACE));

	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {
		ut_a(node);

		if (node->size > block_offset) {
			/* Found! */
			break;
		} else {
			block_offset -= node->size;
			node = UT_LIST_GET_NEXT(chain, node);
		}
	}		
	
	/* Open file if closed */
	fil_node_prepare_for_io(node, system, space);

	/* Now we have made the changes in the data structures of system */
	mutex_exit(&(system->mutex));

	/* Calculate the low 32 bits and the high 32 bits of the file offset */

	offset_high = (block_offset >> (32 - UNIV_PAGE_SIZE_SHIFT));
	offset_low  = ((block_offset << UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFF)
			+ byte_offset;

	ut_a(node->size - block_offset >=
 		(byte_offset + len + (UNIV_PAGE_SIZE - 1)) / UNIV_PAGE_SIZE);

	/* Do aio */

	ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a((len % OS_FILE_LOG_BLOCK_SIZE) == 0);

	/* Queue the aio request */
	ret = os_aio(type, mode | wake_later, node->name, node->handle, buf,
				offset_low, offset_high, len, node, message);
	ut_a(ret);

	if (mode == OS_AIO_SYNC) {
		/* The i/o operation is already completed when we return from
		os_aio: */
		
		mutex_enter(&(system->mutex));

		fil_node_complete_io(node, system, type);

		mutex_exit(&(system->mutex));

		ut_ad(fil_validate());
	}
}

/************************************************************************
Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

void
fil_read(
/*=====*/
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/* in: how many bytes to read; this must not
				cross a file boundary; in aio this must be a
				block size multiple */
	void*	buf,		/* in/out: buffer where to store data read;
				in aio this must be appropriately aligned */
	void*	message)	/* in: message for aio handler if non-sync
				aio used, else ignored */
{
	fil_io(OS_FILE_READ, sync, space_id, block_offset, byte_offset, len,
								buf, message);
}

/************************************************************************
Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

void
fil_write(
/*======*/
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/* in: how many bytes to write; this must
				not cross a file boundary; in aio this must
				be a block size multiple */
	void*	buf,		/* in: buffer from which to write; in aio
				this must be appropriately aligned */
	void*	message)	/* in: message for aio handler if non-sync
				aio used, else ignored */
{
	fil_io(OS_FILE_WRITE, sync, space_id, block_offset, byte_offset, len,
								buf, message);
}

/**************************************************************************
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.c for more info). The thread specifies which
segment it wants to wait for. */

void
fil_aio_wait(
/*=========*/
	ulint	segment)	/* in: the number of the segment in the aio
				array to wait for */ 
{
	ibool		ret;		
	fil_node_t*	fil_node;
	fil_system_t*	system		= fil_system;
	void*		message;
	ulint		type;
	
	ut_ad(fil_validate());

	if (os_aio_use_native_aio) {
		srv_io_thread_op_info[segment] = "native aio handle";
#ifdef WIN_ASYNC_IO
		ret = os_aio_windows_handle(segment, 0, &fil_node, &message,
								&type);
#elif defined(POSIX_ASYNC_IO)
		ret = os_aio_posix_handle(segment, &fil_node, &message);
#else
		ret = 0; /* Eliminate compiler warning */
		ut_a(0);
#endif
	} else {
		srv_io_thread_op_info[segment] = "simulated aio handle";

		ret = os_aio_simulated_handle(segment, (void**) &fil_node,
	                                               &message, &type);
	}
	
	ut_a(ret);

	srv_io_thread_op_info[segment] = "complete io for fil node";

	mutex_enter(&(system->mutex));

	fil_node_complete_io(fil_node, fil_system, type);

	mutex_exit(&(system->mutex));

	ut_ad(fil_validate());

	/* Do the i/o handling */

	if (buf_pool_is_block(message)) {
		srv_io_thread_op_info[segment] = "complete io for buf page";
		buf_page_io_complete(message);
	} else {
		srv_io_thread_op_info[segment] = "complete io for log";
		log_io_complete(message);
	}
}

/**************************************************************************
Flushes to disk possible writes cached by the OS. */

void
fil_flush(
/*======*/
	ulint	space_id)	/* in: file space id (this can be a group of
				log files or a tablespace of the database) */
{
	fil_system_t*	system	= fil_system;
	fil_space_t*	space;
	fil_node_t*	node;
	os_file_t	file;

	mutex_enter(&(system->mutex));
	
	HASH_SEARCH(hash, system->spaces, space_id, space,
						space->id == space_id);
	ut_a(space);

	node = UT_LIST_GET_FIRST(space->chain);

	while (node) {
		if (node->open && node->is_modified) {
			file = node->handle;

			node->is_modified = FALSE;
			
			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes++;
			} else {
				fil_n_pending_log_flushes++;
			}

			mutex_exit(&(system->mutex));

			/* Note that it is not certain, when we have
			released the mutex above, that the file of the
			handle is still open: we assume that the OS
			will not crash or trap even if we pass a handle
			to a closed file below in os_file_flush! */

			/* printf("Flushing to file %s\n", node->name); */
			
			os_file_flush(file);
			
			mutex_enter(&(system->mutex));

			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes--;
			} else {
				fil_n_pending_log_flushes--;
			}
		}

		node = UT_LIST_GET_NEXT(chain, node);
	}		

	mutex_exit(&(system->mutex));
}

/**************************************************************************
Flushes to disk writes in file spaces of the given type possibly cached by
the OS. */

void
fil_flush_file_spaces(
/*==================*/
	ulint	purpose)	/* in: FIL_TABLESPACE, FIL_LOG */
{
	fil_system_t*	system	= fil_system;
	fil_space_t*	space;

	mutex_enter(&(system->mutex));

	space = UT_LIST_GET_FIRST(system->space_list);

	while (space) {
		if (space->purpose == purpose) {
			mutex_exit(&(system->mutex));

			fil_flush(space->id);

			mutex_enter(&(system->mutex));
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}
	
	mutex_exit(&(system->mutex));
}

/**********************************************************************
Checks the consistency of the file system. */

ibool
fil_validate(void)
/*==============*/
			/* out: TRUE if ok */
{	
	fil_space_t*	space;
	fil_node_t*	fil_node;
	ulint		pending_count	= 0;
	fil_system_t*	system;
	ulint		i;

	system = fil_system;
	
	mutex_enter(&(system->mutex));

	/* Look for spaces in the hash table */

	for (i = 0; i < hash_get_n_cells(system->spaces); i++) {

		space = HASH_GET_FIRST(system->spaces, i);
	
		while (space != NULL) {

			UT_LIST_VALIDATE(chain, fil_node_t, space->chain); 

			fil_node = UT_LIST_GET_FIRST(space->chain);

			while (fil_node != NULL) {

				if (fil_node->n_pending > 0) {

					pending_count++;
					ut_a(fil_node->open);
				}

				fil_node = UT_LIST_GET_NEXT(chain, fil_node);
			}

			space = HASH_GET_NEXT(hash, space);
		}
	}

	ut_a(pending_count == system->n_open_pending);

	UT_LIST_VALIDATE(LRU, fil_node_t, system->LRU);

	fil_node = UT_LIST_GET_FIRST(system->LRU);

	while (fil_node != NULL) {

		ut_a(fil_node->n_pending == 0);
		ut_a(fil_node->open);

		fil_node = UT_LIST_GET_NEXT(LRU, fil_node);
	}
	
	mutex_exit(&(system->mutex));

	return(TRUE);
}

/************************************************************************
Returns TRUE if file address is undefined. */
ibool
fil_addr_is_null(
/*=============*/
				/* out: TRUE if undefined */
	fil_addr_t	addr)	/* in: address */
{
	if (addr.page == FIL_NULL) {

		return(TRUE);
	}

	return(FALSE);
}

/************************************************************************
Accessor functions for a file page */

ulint
fil_page_get_prev(byte*	page)
{
	return(mach_read_from_4(page + FIL_PAGE_PREV));
}

ulint
fil_page_get_next(byte*	page)
{
	return(mach_read_from_4(page + FIL_PAGE_NEXT));
}

/*************************************************************************
Sets the file page type. */

void
fil_page_set_type(
/*==============*/
	byte* 	page,	/* in: file page */
	ulint	type)	/* in: type */
{
	ut_ad(page);
	ut_ad((type == FIL_PAGE_INDEX) || (type == FIL_PAGE_UNDO_LOG));

	mach_write_to_2(page + FIL_PAGE_TYPE, type);
}	

/*************************************************************************
Gets the file page type. */

ulint
fil_page_get_type(
/*==============*/
			/* out: type; NOTE that if the type has not been
			written to page, the return value not defined */
	byte* 	page)	/* in: file page */
{
	ut_ad(page);

	return(mach_read_from_2(page + FIL_PAGE_TYPE));
}	
