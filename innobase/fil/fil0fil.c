/******************************************************
The tablespace memory cache

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
#include "buf0flu.h"
#include "buf0lru.h"
#include "log0log.h"
#include "log0recv.h"
#include "fsp0fsp.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "mtr0mtr.h"
#include "mtr0log.h"

	 
/*
		IMPLEMENTATION OF THE TABLESPACE MEMORY CACHE
		=============================================

The tablespace cache is responsible for providing fast read/write access to
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

/* When mysqld is run, the default directory "." is the mysqld datadir,
but in the MySQL Embedded Server Library and ibbackup it is not the default
directory, and we must set the base file path explicitly */
const char*	fil_path_to_mysql_datadir	= ".";

/* The number of fsyncs done to the log */
ulint	fil_n_log_flushes                       = 0;

ulint	fil_n_pending_log_flushes		= 0;
ulint	fil_n_pending_tablespace_flushes	= 0;

/* Null file address */
fil_addr_t	fil_addr_null = {FIL_NULL, 0};

/* File node of a tablespace or the log data space */
typedef	struct fil_node_struct	fil_node_t;
struct fil_node_struct {
	fil_space_t*	space;	/* backpointer to the space where this node
				belongs */
	char*		name;	/* path to the file */
	ibool		open;	/* TRUE if file open */
	os_file_t	handle;	/* OS handle to the file, if file open */
	ibool		is_raw_disk;/* TRUE if the 'file' is actually a raw
				device or a raw disk partition */
	ulint		size;	/* size of the file in database pages, 0 if
				not known yet; the possible last incomplete
				megabyte is ignored if space == 0 */
	ulint		n_pending;
				/* count of pending i/o's on this file;
				closing of the file is not allowed if
				this is > 0 */
	ulint		n_pending_flushes;
				/* count of pending flushes on this file;
				closing of the file is not allowed if
				this is > 0 */	
	ib_longlong	modification_counter;/* when we write to the file we
				increment this by one */
	ib_longlong	flush_counter;/* up to what modification_counter value
				we have flushed the modifications to disk */
	UT_LIST_NODE_T(fil_node_t) chain;
				/* link field for the file chain */
	UT_LIST_NODE_T(fil_node_t) LRU;
				/* link field for the LRU list */
	ulint		magic_n;
};

#define	FIL_NODE_MAGIC_N	89389

/* Tablespace or log data space: let us call them by a common name space */
struct fil_space_struct {
	char*		name;	/* space name = the path to the first file in
				it */
	ulint		id;	/* space id */
	ib_longlong	tablespace_version;
				/* in DISCARD/IMPORT this timestamp is used to
				check if we should ignore an insert buffer
				merge request for a page because it actually
				was for the previous incarnation of the
				space */
	ibool		mark;	/* this is set to TRUE at database startup if
				the space corresponds to a table in the InnoDB
				data dictionary; so we can print a warning of
				orphaned tablespaces */
	ibool		stop_ios;/* TRUE if we want to rename the .ibd file of
				tablespace and want to stop temporarily
				posting of new i/o requests on the file */
	ibool		stop_ibuf_merges;
				/* we set this TRUE when we start deleting a
				single-table tablespace */
	ibool		is_being_deleted;
				/* this is set to TRUE when we start
				deleting a single-table tablespace and its
				file; when this flag is set no further i/o
				or flush requests can be placed on this space,
				though there may be such requests still being
				processed on this space */
	ulint		purpose;/* FIL_TABLESPACE, FIL_LOG, or FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/* base node for the file chain */
	ulint		size;	/* space size in pages; 0 if a single-table
				tablespace whose size we do not know yet */
	ulint		n_reserved_extents;
				/* number of reserved free extents for
				ongoing operations like B-tree page split */
	ulint		n_pending_flushes; /* this is > 0 when flushing
				the tablespace to disk; dropping of the
				tablespace is forbidden if this is > 0 */
	ulint		n_pending_ibuf_merges;/* this is > 0 when merging
				insert buffer entries to a page so that we
				may need to access the ibuf bitmap page in the
				tablespade: dropping of the tablespace is
				forbidden if this is > 0 */
	hash_node_t	hash; 	/* hash chain node */
	hash_node_t	name_hash;/* hash chain the name_hash table */
	rw_lock_t	latch;	/* latch protecting the file space storage
				allocation */
	UT_LIST_NODE_T(fil_space_t) space_list;
				/* list of all spaces */
	ibuf_data_t*	ibuf_data;
				/* insert buffer data */
	ulint		magic_n;
};

#define	FIL_SPACE_MAGIC_N	89472

/* The tablespace memory cache; also the totality of logs = the log data space,
is stored here; below we talk about tablespaces, but also the ib_logfiles
form a 'space' and it is handled here */

typedef	struct fil_system_struct	fil_system_t;
struct fil_system_struct {
	mutex_t		mutex;		/* The mutex protecting the cache */
	hash_table_t*	spaces;		/* The hash table of spaces in the
					system; they are hashed on the space
					id */
	hash_table_t*	name_hash;	/* hash table based on the space
					name */
	UT_LIST_BASE_NODE_T(fil_node_t) LRU;
					/* base node for the LRU list of the
					most recently used open files with no
					pending i/o's; if we start an i/o on
					the file, we first remove it from this
					list, and return it to the start of
					the list when the i/o ends;
					log files and the system tablespace are
					not put to this list: they are opened
					after the startup, and kept open until
					shutdown */
	ulint		n_open;		/* number of files currently open */
	ulint		max_n_open;	/* n_open is not allowed to exceed
					this */
	ib_longlong	modification_counter;/* when we write to a file we
					increment this by one */
	ulint		max_assigned_id;/* maximum space id in the existing
					tables, or assigned during the time
					mysqld has been up; at an InnoDB
					startup we scan the data dictionary
					and set here the maximum of the
					space id's of the tables there */
	ib_longlong	tablespace_version;
					/* a counter which is incremented for
					every space object memory creation;
					every space mem object gets a
					'timestamp' from this; in DISCARD/
					IMPORT this is used to check if we
					should ignore an insert buffer merge
					request */
	UT_LIST_BASE_NODE_T(fil_space_t) space_list;
					/* list of all file spaces */
};

/* The tablespace memory cache. This variable is NULL before the module is
initialized. */
fil_system_t*	fil_system	= NULL;

/* The tablespace memory cache hash table size */
#define	FIL_SYSTEM_HASH_SIZE	50 /* TODO: make bigger! */


/************************************************************************
NOTE: you must call fil_mutex_enter_and_prepare_for_io() first!

Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. The caller must hold the fil_sys
mutex. */
static
void
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	fil_space_t*	space);	/* in: space */
/************************************************************************
Updates the data structures when an i/o operation finishes. Updates the
pending i/o's field in the node appropriately. */
static
void
fil_node_complete_io(
/*=================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	ulint		type);	/* in: OS_FILE_WRITE or OS_FILE_READ; marks
				the node as modified if
				type == OS_FILE_WRITE */
/***********************************************************************
Checks if a single-table tablespace for a given table name exists in the
tablespace memory cache. */
static
ulint
fil_get_space_id_for_table(
/*=======================*/
				/* out: space id, ULINT_UNDEFINED if not
				found */
	const char*	name);	/* in: table name in the standard
				'databasename/tablename' format */


/***********************************************************************
Returns the version number of a tablespace, -1 if not found. */

ib_longlong
fil_space_get_version(
/*==================*/
			/* out: version number, -1 if the tablespace does not
			exist in the memory cache */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	ib_longlong	version		= -1;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space) {
		version = space->tablespace_version;
	}

	mutex_exit(&(system->mutex));

	return(version);
}

/***********************************************************************
Returns the latch of a file space. */

rw_lock_t*
fil_space_get_latch(
/*================*/
			/* out: latch protecting storage allocation */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space);

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
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space);

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
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	ut_a(id == 0);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	mutex_exit(&(system->mutex));

	ut_a(space);

	return(space->ibuf_data);
}

/***********************************************************************
Appends a new file to the chain of files of a space. File must be closed. */

void
fil_node_create(
/*============*/
	const char*	name,	/* in: file name (file must be closed) */
	ulint		size,	/* in: file size in database blocks, rounded
				downwards to an integer */
	ulint		id,	/* in: space id where to append */
	ibool		is_raw)	/* in: TRUE if a raw device or
				a raw disk partition */
{
	fil_system_t*	system	= fil_system;
	fil_node_t*	node;
	fil_space_t*	space;

	ut_a(system);
	ut_a(name);

	mutex_enter(&(system->mutex));

	node = mem_alloc(sizeof(fil_node_t));

	node->name = mem_strdup(name);
	node->open = FALSE;

	ut_a(!is_raw || srv_start_raw_disk_in_use);

	node->is_raw_disk = is_raw;
	node->size = size;
	node->magic_n = FIL_NODE_MAGIC_N;
	node->n_pending = 0;
	node->n_pending_flushes = 0;

	node->modification_counter = 0;
	node->flush_counter = 0;
	
	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (!space) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Error: Could not find tablespace %lu for\n"
"InnoDB: file ", (ulong) id);
		ut_print_filename(stderr, name);
		fputs(" in the tablespace memory cache.\n", stderr);
		mem_free(node->name);

		mem_free(node);

		mutex_exit(&(system->mutex));

		return;
	}

	space->size += size;

	node->space = space;

	UT_LIST_ADD_LAST(chain, space->chain, node);
				
	mutex_exit(&(system->mutex));
}

/************************************************************************
Opens a the file of a node of a tablespace. The caller must own the fil_system
mutex. */
static
void
fil_node_open_file(
/*===============*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	fil_space_t*	space)	/* in: space */
{
	ib_longlong	size_bytes;
	ulint		size_low;
	ulint		size_high;
	ibool		ret;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(node->n_pending == 0);
	ut_a(node->open == FALSE);

	/* printf("Opening file %s\n", node->name); */

	if (space->purpose == FIL_LOG) {	
		node->handle = os_file_create(node->name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_LOG_FILE, &ret);
	} else if (node->is_raw_disk) {
		node->handle = os_file_create(node->name,
				        OS_FILE_OPEN_RAW,
					OS_FILE_AIO, OS_DATA_FILE, &ret);
	} else {
		node->handle = os_file_create(node->name, OS_FILE_OPEN,
					OS_FILE_AIO, OS_DATA_FILE, &ret);
	}
		
	ut_a(ret);
		
	node->open = TRUE;

	system->n_open++;

	if (node->size == 0) {
		os_file_get_size(node->handle, &size_low, &size_high);

		size_bytes = (((ib_longlong)size_high) << 32)
				     		+ (ib_longlong)size_low;
#ifdef UNIV_HOTBACKUP
		node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);

#else
		/* It must be a single-table tablespace and we do not know the
		size of the file yet */

		ut_a(space->id != 0);

		if (size_bytes >= FSP_EXTENT_SIZE * UNIV_PAGE_SIZE) {
			node->size = (ulint) ((size_bytes / (1024 * 1024))
					   * ((1024 * 1024) / UNIV_PAGE_SIZE));
		} else {
			node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);
		}
#endif
		space->size += node->size;
	}

	if (space->purpose == FIL_TABLESPACE && space->id != 0) {
		/* Put the node to the LRU list */
		UT_LIST_ADD_FIRST(LRU, system->LRU, node);
	}
}

/**************************************************************************
Closes a file. */
static
void
fil_node_close_file(
/*================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system)	/* in: tablespace memory cache */
{
	ibool	ret;

	ut_ad(node && system);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(node->open);
	ut_a(node->n_pending == 0);
	ut_a(node->n_pending_flushes == 0);
	ut_a(node->modification_counter == node->flush_counter);

	ret = os_file_close(node->handle);
	ut_a(ret);

	/* printf("Closing file %s\n", node->name); */

	node->open = FALSE;
	ut_a(system->n_open > 0);
	system->n_open--;

	if (node->space->purpose == FIL_TABLESPACE && node->space->id != 0) {
		ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

		/* The node is in the LRU list, remove it */
		UT_LIST_REMOVE(LRU, system->LRU, node);
	}
}

/************************************************************************
Tries to close a file in the LRU list. The caller must hold the fil_sys
mutex. */
static
ibool
fil_try_to_close_file_in_LRU(
/*=========================*/
				/* out: TRUE if success, FALSE if should retry
				later; since i/o's generally complete in < 
				100 ms, and as InnoDB writes at most 128 pages
				from the buffer pool in a batch, and then
				immediately flushes the files, there is a good
				chance that the next time we find a suitable
				node from the LRU list */
	ibool	print_info)	/* in: if TRUE, prints information why it
				cannot close a file */
{
	fil_system_t*	system		= fil_system;
	fil_node_t*	node;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	node = UT_LIST_GET_LAST(system->LRU);

	if (print_info) {
		fprintf(stderr,
"InnoDB: fil_sys open file LRU len %lu\n", (ulong) UT_LIST_GET_LEN(system->LRU));
	}

	while (node != NULL) {
		if (node->modification_counter == node->flush_counter
		    && node->n_pending_flushes == 0) {

			fil_node_close_file(node, system);
			
			return(TRUE);
		}
		
		if (print_info && node->n_pending_flushes > 0) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr, ", because n_pending_flushes %lu\n",
				       (ulong) node->n_pending_flushes);
		}

		if (print_info
		    && node->modification_counter != node->flush_counter) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr,
				", because mod_count %lld != fl_count %lld\n",
				node->modification_counter,
				node->flush_counter);
		}

		node = UT_LIST_GET_PREV(LRU, node);
	}

	return(FALSE);
}

/***********************************************************************
Reserves the fil_system mutex and tries to make sure we can open at least one
file while holding it. This should be called before calling
fil_node_prepare_for_io(), because that function may need to open a file. */
static
void
fil_mutex_enter_and_prepare_for_io(
/*===============================*/
	ulint	space_id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	ibool		success;
	ibool		print_info	= FALSE;
	ulint		count		= 0;
	ulint		count2		= 0;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
retry:
	mutex_enter(&(system->mutex));

	if (space_id == 0 || space_id >= SRV_LOG_SPACE_FIRST_ID) {
		/* We keep log files and system tablespace files always open;
		this is important in preventing deadlocks in this module, as
		a page read completion often performs another read from the
		insert buffer. The insert buffer is in tablespace 0, and we
		cannot end up waiting in this function. */

		return;
	}

	if (system->n_open < system->max_n_open) {

		return;
	}

	HASH_SEARCH(hash, system->spaces, space_id, space,
							space->id == space_id);
	if (space != NULL && space->stop_ios) {
		/* We are going to do a rename file and want to stop new i/o's
		for a while */

		if (count2 > 20000) {
			fputs("InnoDB: Warning: tablespace ", stderr);
			ut_print_filename(stderr, space->name);
			fprintf(stderr,
				" has i/o ops stopped for a long time %lu\n",
				(ulong) count2);
		}

		mutex_exit(&(system->mutex));

		os_thread_sleep(20000);

		count2++;

		goto retry;
	}

	/* If the file is already open, no need to do anything; if the space
	does not exist, we handle the situation in the function which called
	this function */

	if (!space || UT_LIST_GET_FIRST(space->chain)->open) {

		return;
	}

	if (count > 1) {
		print_info = TRUE;
	}

	/* Too many files are open, try to close some */
close_more:
	success = fil_try_to_close_file_in_LRU(print_info);

	if (success && system->n_open >= system->max_n_open) {

		goto close_more;
	}

	if (system->n_open < system->max_n_open) {
		/* Ok */

		return;
	}

	if (count >= 2) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Warning: too many (%lu) files stay open while the maximum\n"
"InnoDB: allowed value would be %lu.\n"
"InnoDB: You may need to raise the value of innodb_max_files_open in\n"
"InnoDB: my.cnf.\n", (ulong) system->n_open, (ulong) system->max_n_open);

		return;
	}

	mutex_exit(&(system->mutex));

#ifndef UNIV_HOTBACKUP
	/* Wake the i/o-handler threads to make sure pending i/o's are
	performed */
	os_aio_simulated_wake_handler_threads();

	os_thread_sleep(20000);
#endif
	/* Flush tablespaces so that we can close modified files in the LRU
	list */

	fil_flush_file_spaces(FIL_TABLESPACE);		

	count++;

	goto retry;
}

/***********************************************************************
Frees a file node object from a tablespace memory cache. */
static
void
fil_node_free(
/*==========*/
	fil_node_t*	node,	/* in, own: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	fil_space_t*	space)	/* in: space where the file node is chained */
{
	ut_ad(node && system && space);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(node->magic_n == FIL_NODE_MAGIC_N);
	ut_a(node->n_pending == 0);

	if (node->open) {
		/* We fool the assertion in fil_node_close_file() to think
		there are no unflushed modifications in the file */

		node->modification_counter = node->flush_counter;

		fil_node_close_file(node, system);
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
	fil_system_t*	system		= fil_system;
	fil_node_t*	node;
	fil_space_t*	space;

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

/***********************************************************************
Creates a space memory object and puts it to the tablespace memory cache. If
there is an error, prints an error message to the .err log. */

ibool
fil_space_create(
/*=============*/
				/* out: TRUE if success */
	const char*	name,	/* in: space name */
	ulint		id,	/* in: space id */
	ulint		purpose)/* in: FIL_TABLESPACE, or FIL_LOG if log */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;	
	ulint		namesake_id;
try_again:
	/*printf(
	"InnoDB: Adding tablespace %lu of name %s, purpose %lu\n", id, name,
	  purpose);*/

	ut_a(system);
	ut_a(name);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(name_hash, system->name_hash, ut_fold_string(name), space,
					0 == strcmp(name, space->name));
	if (space != NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Warning: trying to init to the tablespace memory cache\n"
"InnoDB: a tablespace %lu of name ", (ulong) id);
		ut_print_filename(stderr, name);
		fprintf(stderr, ",\n"
"InnoDB: but a tablespace %lu of the same name\n"
"InnoDB: already exists in the tablespace memory cache!\n",
			(ulong) space->id);

		if (id == 0 || purpose != FIL_TABLESPACE) {

			mutex_exit(&(system->mutex));

			return(FALSE);
		}

		fprintf(stderr,
"InnoDB: We assume that InnoDB did a crash recovery, and you had\n"
"InnoDB: an .ibd file for which the table did not exist in the\n"
"InnoDB: InnoDB internal data dictionary in the ibdata files.\n"
"InnoDB: We assume that you later removed the .ibd and .frm files,\n"
"InnoDB: and are now trying to recreate the table. We now remove the\n"
"InnoDB: conflicting tablespace object from the memory cache and try\n"
"InnoDB: the init again.\n");

		namesake_id = space->id;

		mutex_exit(&(system->mutex));

		fil_space_free(namesake_id);

		goto try_again;
	}

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space != NULL) {
		fprintf(stderr,
"InnoDB: Error: trying to add tablespace %lu of name ", (ulong) id);
		ut_print_filename(stderr, name);
		fprintf(stderr, "\n"
"InnoDB: to the tablespace memory cache, but tablespace\n"
"InnoDB: %lu of name ", (ulong) space->id);
		ut_print_filename(stderr, space->name);
		fputs(" already exists in the tablespace\n"
"InnoDB: memory cache!\n", stderr);

		mutex_exit(&(system->mutex));

		return(FALSE);
	}

	space = mem_alloc(sizeof(fil_space_t));

	space->name = mem_strdup(name);
	space->id = id;

	system->tablespace_version++;
	space->tablespace_version = system->tablespace_version;
	space->mark = FALSE;

	if (purpose == FIL_TABLESPACE && id > system->max_assigned_id) {
		system->max_assigned_id = id;
	}

	space->stop_ios = FALSE;
	space->stop_ibuf_merges = FALSE;
	space->is_being_deleted = FALSE;
	space->purpose = purpose;
	space->size = 0;

	space->n_reserved_extents = 0;
	
	space->n_pending_flushes = 0;
	space->n_pending_ibuf_merges = 0;

	UT_LIST_INIT(space->chain);
	space->magic_n = FIL_SPACE_MAGIC_N;

	space->ibuf_data = NULL;
	
	rw_lock_create(&(space->latch));
	rw_lock_set_level(&(space->latch), SYNC_FSP);
	
	HASH_INSERT(fil_space_t, hash, system->spaces, id, space);

	HASH_INSERT(fil_space_t, name_hash, system->name_hash,
						ut_fold_string(name), space);
	UT_LIST_ADD_LAST(space_list, system->space_list, space);
				
	mutex_exit(&(system->mutex));

	return(TRUE);
}

/***********************************************************************
Assigns a new space id for a new single-table tablespace. This works simply by
incrementing the global counter. If 4 billion id's is not enough, we may need
to recycle id's. */
static
ulint
fil_assign_new_space_id(void)
/*=========================*/
			/* out: new tablespace id; ULINT_UNDEFINED if could
			not assign an id */
{
	fil_system_t*	system = fil_system;
	ulint		id;

	mutex_enter(&(system->mutex));

	system->max_assigned_id++;

	id = system->max_assigned_id;

	if (id > (SRV_LOG_SPACE_FIRST_ID / 2) && (id % 1000000UL == 0)) {
	        ut_print_timestamp(stderr);
	        fprintf(stderr,
"InnoDB: Warning: you are running out of new single-table tablespace id's.\n"
"InnoDB: Current counter is %lu and it must not exceed %lu!\n"
"InnoDB: To reset the counter to zero you have to dump all your tables and\n"
"InnoDB: recreate the whole InnoDB installation.\n", (ulong) id,
					(ulong) SRV_LOG_SPACE_FIRST_ID);
	}

	if (id >= SRV_LOG_SPACE_FIRST_ID) {
	        ut_print_timestamp(stderr);
	        fprintf(stderr,
"InnoDB: You have run out of single-table tablespace id's!\n"
"InnoDB: Current counter is %lu.\n"
"InnoDB: To reset the counter to zero you have to dump all your tables and\n"
"InnoDB: recreate the whole InnoDB installation.\n", (ulong) id);
		system->max_assigned_id--;

		id = ULINT_UNDEFINED;
	}

	mutex_exit(&(system->mutex));

	return(id);
}

/***********************************************************************
Frees a space object from the tablespace memory cache. Closes the files in
the chain but does not delete them. There must not be any pending i/o's or
flushes on the files. */

ibool
fil_space_free(
/*===========*/
			/* out: TRUE if success */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system = fil_system;
	fil_space_t*	space;
	fil_space_t*	namespace;
	fil_node_t*	fil_node;

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (!space) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Error: trying to remove tablespace %lu from the cache but\n"
"InnoDB: it is not there.\n", (ulong) id);

		mutex_exit(&(system->mutex));
		
		return(FALSE);
	}

	HASH_DELETE(fil_space_t, hash, system->spaces, id, space);

	HASH_SEARCH(name_hash, system->name_hash, ut_fold_string(space->name),
		    namespace, 0 == strcmp(space->name, namespace->name));
	ut_a(namespace);
	ut_a(space == namespace);

	HASH_DELETE(fil_space_t, name_hash, system->name_hash,
					   ut_fold_string(space->name), space);

	UT_LIST_REMOVE(space_list, system->space_list, space);

	ut_a(space->magic_n == FIL_SPACE_MAGIC_N);
	ut_a(0 == space->n_pending_flushes);

	fil_node = UT_LIST_GET_FIRST(space->chain);

	while (fil_node != NULL) {
		fil_node_free(fil_node, system, space);

		fil_node = UT_LIST_GET_FIRST(space->chain);
	}	
	
	ut_a(0 == UT_LIST_GET_LEN(space->chain));

	mutex_exit(&(system->mutex));

	rw_lock_free(&(space->latch));

	mem_free(space->name);
	mem_free(space);

	return(TRUE);
}

#ifdef UNIV_HOTBACKUP
/***********************************************************************
Returns the tablespace object for a given id, or NULL if not found from the
tablespace memory cache. */
static
fil_space_t*
fil_get_space_for_id_low(
/*=====================*/
			/* out: tablespace object or NULL; NOTE that you must
			own &(fil_system->mutex) to call this function! */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	return(space);
}
#endif

/***********************************************************************
Returns the size of the space in pages. The tablespace must be cached in the
memory cache. */

ulint
fil_space_get_size(
/*===============*/
			/* out: space size, 0 if space not found */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system 		= fil_system;
	fil_node_t*	node;
	fil_space_t*	space;
	ulint		size;

	ut_ad(system);

	fil_mutex_enter_and_prepare_for_io(id);

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		mutex_exit(&(system->mutex));

		return(0);
	}

	if (space->size == 0 && space->purpose == FIL_TABLESPACE) {
		ut_a(id != 0);

		ut_a(1 == UT_LIST_GET_LEN(space->chain));

		node = UT_LIST_GET_FIRST(space->chain);

		/* It must be a single-table tablespace and we have not opened
		the file yet; the following calls will open it and update the
		size fields */

		fil_node_prepare_for_io(node, system, space);
		fil_node_complete_io(node, system, OS_FILE_READ);
	}

	size = space->size;
	
	mutex_exit(&(system->mutex));

	return(size);
}

/***********************************************************************
Checks if the pair space, page_no refers to an existing page in a tablespace
file space. The tablespace must be cached in the memory cache. */

ibool
fil_check_adress_in_tablespace(
/*===========================*/
			/* out: TRUE if the address is meaningful */
	ulint	id,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	if (fil_space_get_size(id) > page_no) {

		return(TRUE);
	}

	return(FALSE);
}		

/********************************************************************
Creates a the tablespace memory cache. */
static
fil_system_t*
fil_system_create(
/*==============*/
				/* out, own: tablespace memory cache */
	ulint	hash_size,	/* in: hash table size */
	ulint	max_n_open)	/* in: maximum number of open files; must be
				> 10 */
{
	fil_system_t*	system;

	ut_a(hash_size > 0);
	ut_a(max_n_open > 0);

	system = mem_alloc(sizeof(fil_system_t));

	mutex_create(&(system->mutex));

	mutex_set_level(&(system->mutex), SYNC_ANY_LATCH);

	system->spaces = hash_create(hash_size);
	system->name_hash = hash_create(hash_size);

	UT_LIST_INIT(system->LRU);

	system->n_open = 0;
	system->max_n_open = max_n_open;

	system->modification_counter = 0;
	system->max_assigned_id = 0;

	system->tablespace_version = 0;

	UT_LIST_INIT(system->space_list);

	return(system);
}

/********************************************************************
Initializes the tablespace memory cache. */

void
fil_init(
/*=====*/
	ulint	max_n_open)	/* in: max number of open files */
{
	ut_a(fil_system == NULL);

	/*printf("Initializing the tablespace cache with max %lu open files\n",
							       max_n_open); */
	fil_system = fil_system_create(FIL_SYSTEM_HASH_SIZE, max_n_open);
}

/***********************************************************************
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */

void
fil_open_log_and_system_tablespace_files(void)
/*==========================================*/
{
	fil_system_t*	system = fil_system;
	fil_space_t*	space;
	fil_node_t*	node;

	mutex_enter(&(system->mutex));

	space = UT_LIST_GET_FIRST(system->space_list);

	while (space != NULL) {
		if (space->purpose != FIL_TABLESPACE || space->id == 0) {
			node = UT_LIST_GET_FIRST(space->chain);

			while (node != NULL) {
				if (!node->open) {
					fil_node_open_file(node, system,
									space);
				}
				if (system->max_n_open < 10 + system->n_open) {
					fprintf(stderr,
"InnoDB: Warning: you must raise the value of innodb_max_open_files in\n"
"InnoDB: my.cnf! Remember that InnoDB keeps all log files and all system\n"
"InnoDB: tablespace files open for the whole time mysqld is running, and\n"
"InnoDB: needs to open also some .ibd files if the file-per-table storage\n"
"InnoDB: model is used. Current open files %lu, max allowed open files %lu.\n",
				     (ulong) system->n_open,
				     (ulong) system->max_n_open);
				}
				node = UT_LIST_GET_NEXT(chain, node);
			}
		}
		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(system->mutex));
}

/***********************************************************************
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */

void
fil_close_all_files(void)
/*=====================*/
{
	fil_system_t*	system = fil_system;
	fil_space_t*	space;
	fil_node_t*	node;

	mutex_enter(&(system->mutex));

	space = UT_LIST_GET_FIRST(system->space_list);

	while (space != NULL) {
		node = UT_LIST_GET_FIRST(space->chain);

		while (node != NULL) {
			if (node->open) {
				fil_node_close_file(node, system);
			}
			node = UT_LIST_GET_NEXT(chain, node);
		}
		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(system->mutex));
}

/***********************************************************************
Sets the max tablespace id counter if the given number is bigger than the
previous value. */

void
fil_set_max_space_id_if_bigger(
/*===========================*/
	ulint	max_id)	/* in: maximum known id */
{
	fil_system_t*	system = fil_system;

	if (max_id >= SRV_LOG_SPACE_FIRST_ID) {
		fprintf(stderr,
"InnoDB: Fatal error: max tablespace id is too high, %lu\n", (ulong) max_id);
		ut_a(0);
	}

	mutex_enter(&(system->mutex));

	if (system->max_assigned_id < max_id) {

		system->max_assigned_id = max_id;
	}

	mutex_exit(&(system->mutex));
}

/********************************************************************
Initializes the ibuf data structure for space 0 == the system tablespace.
This can be called after the file space headers have been created and the
dictionary system has been initialized. */

void
fil_ibuf_init_at_db_start(void)
/*===========================*/
{
	fil_space_t*	space;

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	ut_a(space);
        ut_a(space->purpose == FIL_TABLESPACE);	

	space->ibuf_data = ibuf_data_init_for_space(space->id);
}

/********************************************************************
Writes the flushed lsn and the latest archived log number to the page header
of the first page of a data file. */
static
ulint
fil_write_lsn_and_arch_no_to_file(
/*==============================*/
	ulint	space_id,	/* in: space number */
	ulint	sum_of_sizes,	/* in: combined size of previous files in
				space, in database pages */
	dulint	lsn,		/* in: lsn to write */
	ulint	arch_log_no	/* in: archived log number to write */
	__attribute__((unused)))
{
	byte*	buf1;
	byte*	buf;

	buf1 = mem_alloc(2 * UNIV_PAGE_SIZE);
	buf = ut_align(buf1, UNIV_PAGE_SIZE);

	fil_read(TRUE, space_id, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	mach_write_to_8(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);

	fil_write(TRUE, space_id, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	return(DB_SUCCESS);	
}

/********************************************************************
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file in the system tablespace. */

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
		/* We only write the lsn to all existing data files which have
		been open during the lifetime of the mysqld process; they are
		represented by the space objects in the tablespace memory
		cache. Note that all data files in the system tablespace 0 are
		always open. */

		if (space->purpose == FIL_TABLESPACE) {
			sum_of_sizes = 0;

			node = UT_LIST_GET_FIRST(space->chain);
			while (node) {
				mutex_exit(&(fil_system->mutex));

				err = fil_write_lsn_and_arch_no_to_file(
						space->id, sum_of_sizes,
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
#ifdef UNIV_LOG_ARCHIVE
	ulint*	min_arch_log_no,	/* in/out: */
	ulint*	max_arch_log_no,	/* in/out: */
#endif /* UNIV_LOG_ARCHIVE */
	dulint*	min_flushed_lsn,	/* in/out: */
	dulint*	max_flushed_lsn)	/* in/out: */
{
	byte*	buf;
	byte*	buf2;
	dulint	flushed_lsn;

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for a possible read from a raw device */
	buf = ut_align(buf2, UNIV_PAGE_SIZE);
	
	os_file_read(data_file, buf, 0, 0, UNIV_PAGE_SIZE);

	flushed_lsn = mach_read_from_8(buf + FIL_PAGE_FILE_FLUSH_LSN);

	ut_free(buf2);

	if (!one_read_already) {
		*min_flushed_lsn = flushed_lsn;
		*max_flushed_lsn = flushed_lsn;
#ifdef UNIV_LOG_ARCHIVE
		*min_arch_log_no = arch_log_no;
		*max_arch_log_no = arch_log_no;
#endif /* UNIV_LOG_ARCHIVE */
		return;
	}

	if (ut_dulint_cmp(*min_flushed_lsn, flushed_lsn) > 0) {
		*min_flushed_lsn = flushed_lsn;
	}
	if (ut_dulint_cmp(*max_flushed_lsn, flushed_lsn) < 0) {
		*max_flushed_lsn = flushed_lsn;
	}
#ifdef UNIV_LOG_ARCHIVE
	if (*min_arch_log_no > arch_log_no) {
		*min_arch_log_no = arch_log_no;
	}
	if (*max_arch_log_no < arch_log_no) {
		*max_arch_log_no = arch_log_no;
	}
#endif /* UNIV_LOG_ARCHIVE */
}

/*================ SINGLE-TABLE TABLESPACES ==========================*/

/***********************************************************************
Increments the count of pending insert buffer page merges, if space is not
being deleted. */

ibool
fil_inc_pending_ibuf_merges(
/*========================*/
			/* out: TRUE if being deleted, and ibuf merges should
			be skipped */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	
	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		fprintf(stderr,
"InnoDB: Error: trying to do ibuf merge to a dropped tablespace %lu\n",
			(ulong) id);
	}

	if (space == NULL || space->stop_ibuf_merges) {
		mutex_exit(&(system->mutex));

		return(TRUE);
	}

	space->n_pending_ibuf_merges++;

	mutex_exit(&(system->mutex));

	return(FALSE);
}

/***********************************************************************
Decrements the count of pending insert buffer page merges. */

void
fil_decr_pending_ibuf_merges(
/*========================*/
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	
	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		fprintf(stderr,
"InnoDB: Error: decrementing ibuf merge of a dropped tablespace %lu\n",
			(ulong) id);
	}

	if (space != NULL) {
		space->n_pending_ibuf_merges--;
	}

	mutex_exit(&(system->mutex));
}

/************************************************************
Creates the database directory for a table if it does not exist yet. */
static
void
fil_create_directory_for_tablename(
/*===============================*/
	const char*	name)	/* in: name in the standard
				'databasename/tablename' format */
{
	const char*	namend;
	char*		path;
	ulint		len;

	len = strlen(fil_path_to_mysql_datadir);
	namend = strchr(name, '/');
	ut_a(namend);
	path = mem_alloc(len + (namend - name) + 2);

	memcpy(path, fil_path_to_mysql_datadir, len);
	path[len] = '/';
	memcpy(path + len + 1, name, namend - name);
	path[len + (namend - name) + 1] = 0;

	srv_normalize_path_for_win(path);

	ut_a(os_file_create_directory(path, FALSE));
	mem_free(path);
}

#ifndef UNIV_HOTBACKUP
/************************************************************
Writes a log record about an .ibd file create/rename/delete. */
static
void
fil_op_write_log(
/*=============*/
	ulint		type,		/* in: MLOG_FILE_CREATE,
					MLOG_FILE_DELETE, or
					MLOG_FILE_RENAME */
	ulint		space_id,	/* in: space id */
	const char*	name,		/* in: table name in the familiar
					'databasename/tablename' format, or
					the file path in the case of
					MLOG_FILE_DELETE */ 
	const char*	new_name,	/* in: if type is MLOG_FILE_RENAME,
					the new table name in the
					'databasename/tablename' format */
	mtr_t*		mtr)		/* in: mini-transaction handle */
{
	byte*	log_ptr;

	log_ptr = mlog_open(mtr, 30);
	
	log_ptr = mlog_write_initial_log_record_for_file_op(type, space_id, 0,
								log_ptr, mtr);
	/* Let us store the strings as null-terminated for easier readability
	and handling */

	mach_write_to_2(log_ptr, ut_strlen(name) + 1);
	log_ptr += 2;
	
	mlog_close(mtr, log_ptr);

	mlog_catenate_string(mtr, (byte*) name, ut_strlen(name) + 1);

	if (type == MLOG_FILE_RENAME) {
		log_ptr = mlog_open(mtr, 30);
		mach_write_to_2(log_ptr, ut_strlen(new_name) + 1);
		log_ptr += 2;
	
		mlog_close(mtr, log_ptr);

		mlog_catenate_string(mtr, (byte*) new_name,
						ut_strlen(new_name) + 1);
	}
}
#endif

/***********************************************************************
Parses the body of a log record written about an .ibd file operation. That is,
the log record part after the standard (type, space id, page no) header of the
log record.

If desired, also replays the delete or rename operation if the .ibd file
exists and the space id in it matches. Replays the create operation if a file
at that path does not exist yet. If the database directory for the file to be
created does not exist, then we create the directory, too.

Note that ibbackup --apply-log sets fil_path_to_mysql_datadir to point to the
datadir that we should use in replaying the file operations. */

byte*
fil_op_log_parse_or_replay(
/*=======================*/
                        	/* out: end of log record, or NULL if the
				record was not completely contained between
				ptr and end_ptr */
        byte*   ptr,    	/* in: buffer containing the log record body,
				or an initial segment of it, if the record does
				not fir completely between ptr and end_ptr */
        byte*   end_ptr,	/* in: buffer end */
	ulint	type,		/* in: the type of this log record */
	ibool	do_replay,	/* in: TRUE if we want to replay the
				operation, and not just parse the log record */
	ulint	space_id)	/* in: if do_replay is TRUE, the space id of
				the tablespace in question; otherwise
				ignored */
{
	ulint		name_len;
	ulint		new_name_len;
	const char*	name;
	const char*	new_name	= NULL;

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	name_len = mach_read_from_2(ptr);

	ptr += 2;

	if (end_ptr < ptr + name_len) {
		
		return(NULL);
	}

	name = (const char*) ptr;

	ptr += name_len;

	if (type == MLOG_FILE_RENAME) {
		if (end_ptr < ptr + 2) {

			return(NULL);
		}

		new_name_len = mach_read_from_2(ptr);
		
		ptr += 2;

		if (end_ptr < ptr + new_name_len) {
		
			return(NULL);
		}

		new_name = (const char*) ptr;

		ptr += new_name_len;
	}

	/* We managed to parse a full log record body */
/*
	printf("Parsed log rec of type %lu space %lu\n"
		"name %s\n", type, space_id, name);

	if (type == MLOG_FILE_RENAME) {
		printf("new name %s\n", new_name);
	}
*/
	if (do_replay == FALSE) {

		return(ptr);
	}

	/* Let us try to perform the file operation, if sensible. Note that
	ibbackup has at this stage already read in all space id info to the
	fil0fil.c data structures.
	
	NOTE that our algorithm is not guaranteed to work correctly if there
	were renames of tables during the backup. See ibbackup code for more
	on the problem. */

	if (type == MLOG_FILE_DELETE) {
		if (fil_tablespace_exists_in_mem(space_id)) {
			ut_a(fil_delete_tablespace(space_id));
		}
	} else if (type == MLOG_FILE_RENAME) {
		/* We do the rename based on space id, not old file name;
		this should guarantee that after the log replay each .ibd file
		has the correct name for the latest log sequence number; the
		proof is left as an exercise :) */

		if (fil_tablespace_exists_in_mem(space_id)) {
			/* Create the database directory for the new name, if
			it does not exist yet */
			fil_create_directory_for_tablename(new_name);
	
			/* Rename the table if there is not yet a tablespace
			with the same name */

			if (fil_get_space_id_for_table(new_name)
			    == ULINT_UNDEFINED) {
				/* We do not care of the old name, that is
				why we pass NULL as the first argument */
				ut_a(fil_rename_tablespace(NULL, space_id,
								new_name));
			}
		}
	} else {
		ut_a(type == MLOG_FILE_CREATE);

		if (fil_tablespace_exists_in_mem(space_id)) {
			/* Do nothing */
		} else if (fil_get_space_id_for_table(name) !=
							ULINT_UNDEFINED) {
			/* Do nothing */
		} else {
			/* Create the database directory for name, if it does
			not exist yet */
			fil_create_directory_for_tablename(name);

			ut_a(space_id != 0);

			ut_a(DB_SUCCESS == 
				fil_create_new_single_table_tablespace(
						&space_id, name, FALSE,
						FIL_IBD_FILE_INITIAL_SIZE));
		}
	}

	return(ptr);
}

/***********************************************************************
Deletes a single-table tablespace. The tablespace must be cached in the
memory cache. */

ibool
fil_delete_tablespace(
/*==================*/
			/* out: TRUE if success */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	ibool		success;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	char*		path;

	ut_a(id != 0);
stop_ibuf_merges:
	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space != NULL) {
		space->stop_ibuf_merges = TRUE;

		if (space->n_pending_ibuf_merges == 0) {
			mutex_exit(&(system->mutex));

			count = 0;

			goto try_again;
		} else {
			if (count > 5000) {
			   ut_print_timestamp(stderr);
			   fputs(
"  InnoDB: Warning: trying to delete tablespace ", stderr);
			   ut_print_filename(stderr, space->name);
			   fprintf(stderr, ",\n"
"InnoDB: but there are %lu pending ibuf merges on it.\n"
"InnoDB: Loop %lu.\n", (ulong) space->n_pending_ibuf_merges,
				   (ulong) count);
			}

			mutex_exit(&(system->mutex));

			os_thread_sleep(20000);
			count++;

			goto stop_ibuf_merges;
		}
	}

	mutex_exit(&(system->mutex));
	count = 0;

try_again:
	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Error: cannot delete tablespace %lu\n"
"InnoDB: because it is not found in the tablespace memory cache.\n",
			(ulong) id);

		mutex_exit(&(system->mutex));
	
		return(FALSE);
	}	

	ut_a(space);
	ut_a(space->n_pending_ibuf_merges == 0);

	space->is_being_deleted = TRUE;

	ut_a(UT_LIST_GET_LEN(space->chain) == 1);
	node = UT_LIST_GET_FIRST(space->chain);

	if (space->n_pending_flushes > 0 || node->n_pending > 0) {
		if (count > 1000) {
			ut_print_timestamp(stderr);
			fputs(
"  InnoDB: Warning: trying to delete tablespace ", stderr);
			ut_print_filename(stderr, space->name);
			fprintf(stderr, ",\n"
"InnoDB: but there are %lu flushes and %lu pending i/o's on it\n"
"InnoDB: Loop %lu.\n", (ulong) space->n_pending_flushes,
				(ulong) node->n_pending,
				(ulong) count);
		}
		mutex_exit(&(system->mutex));
		os_thread_sleep(20000);

		count++;

		goto try_again;
	}

	path = mem_strdup(space->name);

	mutex_exit(&(system->mutex));
#ifndef UNIV_HOTBACKUP
	/* Invalidate in the buffer pool all pages belonging to the
	tablespace. Since we have set space->is_being_deleted = TRUE, readahead
	or ibuf merge can no longer read more pages of this tablespace to the
	buffer pool. Thus we can clean the tablespace out of the buffer pool
	completely and permanently. The flag is_being_deleted also prevents
	fil_flush() from being applied to this tablespace. */

	buf_LRU_invalidate_tablespace(id);
#endif
	/* printf("Deleting tablespace %s id %lu\n", space->name, id); */

	success = fil_space_free(id);

	if (success) {
		success = os_file_delete(path);

		if (!success) {
			success = os_file_delete_if_exists(path);
		}
	}

	if (success) {
#ifndef UNIV_HOTBACKUP
		/* Write a log record about the deletion of the .ibd
		file, so that ibbackup can replay it in the
		--apply-log phase. We use a dummy mtr and the familiar
		log write mechanism. */
		mtr_t		mtr;

		/* When replaying the operation in ibbackup, do not try
		to write any log record */
		mtr_start(&mtr);

		fil_op_write_log(MLOG_FILE_DELETE, id, path, NULL, &mtr);
		mtr_commit(&mtr);
#endif
		mem_free(path);

		return(TRUE);
	}

	mem_free(path);

	return(FALSE);
}

/***********************************************************************
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but
1) we do not drop the table from the data dictionary;
2) we remove all insert buffer entries for the tablespace immediately; in DROP
TABLE they are only removed gradually in the background;
3) when the user does IMPORT TABLESPACE, the tablespace will have the same id
as it originally had. */

ibool
fil_discard_tablespace(
/*===================*/
			/* out: TRUE if success */
	ulint	id)	/* in: space id */
{
	ibool	success;

	success = fil_delete_tablespace(id);

	if (!success) {
		fprintf(stderr,
"InnoDB: Warning: cannot delete tablespace %lu in DISCARD TABLESPACE.\n"
"InnoDB: But let us remove the insert buffer entries for this tablespace.\n",
			(ulong) id); 
	}

	/* Remove all insert buffer entries for the tablespace */

	ibuf_delete_for_discarded_space(id);

	return(TRUE);
}

/***********************************************************************
Renames the memory cache structures of a single-table tablespace. */
static
ibool
fil_rename_tablespace_in_mem(
/*=========================*/
				/* out: TRUE if success */
	fil_space_t*	space,	/* in: tablespace memory object */
	fil_node_t*	node,	/* in: file node of that tablespace */
	const char*	path)	/* in: new name */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space2;
	const char*	old_name	= space->name;
	
	HASH_SEARCH(name_hash, system->name_hash, ut_fold_string(old_name),
			       space2, 0 == strcmp(old_name, space2->name));
	if (space != space2) {
		fputs("InnoDB: Error: cannot find ", stderr);
		ut_print_filename(stderr, old_name);
		fputs(" in tablespace memory cache\n", stderr);

		return(FALSE);
	}

	HASH_SEARCH(name_hash, system->name_hash, ut_fold_string(path),
			       space2, 0 == strcmp(path, space2->name));
	if (space2 != NULL) {
		fputs("InnoDB: Error: ", stderr);
		ut_print_filename(stderr, path);
		fputs(" is already in tablespace memory cache\n", stderr);
		
		return(FALSE);
	}

	HASH_DELETE(fil_space_t, name_hash, system->name_hash,
					   ut_fold_string(space->name), space);
	mem_free(space->name);
	mem_free(node->name);

	space->name = mem_strdup(path);
	node->name = mem_strdup(path);

	HASH_INSERT(fil_space_t, name_hash, system->name_hash,
						ut_fold_string(path), space);
	return(TRUE);
}

/***********************************************************************
Allocates a file name for a single-table tablespace. The string must be freed
by caller with mem_free(). */
static
char*
fil_make_ibd_name(
/*==============*/
					/* out, own: file name */
	const char*	name,		/* in: table name or a dir path of a
					TEMPORARY table */
	ibool		is_temp)	/* in: TRUE if it is a dir path */
{
	ulint	namelen		= strlen(name);
	ulint	dirlen		= strlen(fil_path_to_mysql_datadir);
	char*	filename	= mem_alloc(namelen + dirlen + sizeof "/.ibd");

	if (is_temp) {
		memcpy(filename, name, namelen);
		memcpy(filename + namelen, ".ibd", sizeof ".ibd");
	} else {
		memcpy(filename, fil_path_to_mysql_datadir, dirlen);
		filename[dirlen] = '/';

		memcpy(filename + dirlen + 1, name, namelen);
		memcpy(filename + dirlen + namelen + 1, ".ibd", sizeof ".ibd");
	}

	srv_normalize_path_for_win(filename);

	return(filename);
}

/***********************************************************************
Renames a single-table tablespace. The tablespace must be cached in the
tablespace memory cache. */

ibool
fil_rename_tablespace(
/*==================*/
					/* out: TRUE if success */
	const char*	old_name,	/* in: old table name in the standard
					databasename/tablename format of
					InnoDB, or NULL if we do the rename
					based on the space id only */
	ulint		id,		/* in: space id */
	const char*	new_name)	/* in: new table name in the standard
					databasename/tablename format
					of InnoDB */
{
	fil_system_t*	system		= fil_system;
	ibool		success;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	char*		path;
	ibool		old_name_was_specified 		= TRUE;
	char*		old_path;

	ut_a(id != 0);
	
	if (old_name == NULL) {
		old_name = "(name not specified)";
		old_name_was_specified = FALSE;
	}
retry:
	count++;

	if (count > 1000) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Warning: problems renaming ", stderr);
		ut_print_filename(stderr, old_name);
		fputs(" to ", stderr);
		ut_print_filename(stderr, new_name);
		fprintf(stderr, ", %lu iterations\n", (ulong) count);
	}

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		fprintf(stderr,
"InnoDB: Error: cannot find space id %lu from the tablespace memory cache\n"
"InnoDB: though the table ", (ulong) id);
		ut_print_filename(stderr, old_name);
		fputs(" in a rename operation should have that id\n", stderr);
		mutex_exit(&(system->mutex));

		return(FALSE);
	}

	if (count > 25000) {
		space->stop_ios = FALSE;
		mutex_exit(&(system->mutex));

		return(FALSE);
	}

	/* We temporarily close the .ibd file because we do not trust that
	operating systems can rename an open file. For the closing we have to
	wait until there are no pending i/o's or flushes on the file. */

	space->stop_ios = TRUE;

	ut_a(UT_LIST_GET_LEN(space->chain) == 1);
	node = UT_LIST_GET_FIRST(space->chain);

	if (node->n_pending > 0 || node->n_pending_flushes > 0) {
		/* There are pending i/o's or flushes, sleep for a while and
		retry */

		mutex_exit(&(system->mutex));

		os_thread_sleep(20000);

		goto retry;

	} else if (node->modification_counter > node->flush_counter) {
		/* Flush the space */

		mutex_exit(&(system->mutex));

		os_thread_sleep(20000);

		fil_flush(id);

		goto retry;

	} else if (node->open) {
		/* Close the file */

		fil_node_close_file(node, system);
	}

	/* Check that the old name in the space is right */

	if (old_name_was_specified) {
		old_path = fil_make_ibd_name(old_name, FALSE);

		ut_a(strcmp(space->name, old_path) == 0);
		ut_a(strcmp(node->name, old_path) == 0);
	} else {
		old_path = mem_strdup(space->name);
	}

	/* Rename the tablespace and the node in the memory cache */
	path = fil_make_ibd_name(new_name, FALSE);
	success = fil_rename_tablespace_in_mem(space, node, path);

	if (success) {
		success = os_file_rename(old_path, path);

		if (!success) {
			/* We have to revert the changes we made
			to the tablespace memory cache */

			ut_a(fil_rename_tablespace_in_mem(space, node,
								old_path));
		}
	}

	mem_free(path);
	mem_free(old_path);

	space->stop_ios = FALSE;

	mutex_exit(&(system->mutex));

#ifndef UNIV_HOTBACKUP	
	if (success) {
		mtr_t		mtr;

		mtr_start(&mtr);

		fil_op_write_log(MLOG_FILE_RENAME, id, old_name, new_name,
								&mtr);
		mtr_commit(&mtr);
	}
#endif
	return(success);
}

/***********************************************************************
Creates a new single-table tablespace to a database directory of MySQL.
Database directories are under the 'datadir' of MySQL. The datadir is the
directory of a running mysqld program. We can refer to it by simply the
path '.'. Tables created with CREATE TEMPORARY TABLE we place in the temp
dir of the mysqld server. */

ulint
fil_create_new_single_table_tablespace(
/*===================================*/
					/* out: DB_SUCCESS or error code */
	ulint*		space_id,	/* in/out: space id; if this is != 0,
					then this is an input parameter,
					otherwise output */
	const char*	tablename,	/* in: the table name in the usual
					databasename/tablename format
					of InnoDB, or a dir path to a temp
					table */
	ibool		is_temp,	/* in: TRUE if a table created with
					CREATE TEMPORARY TABLE */
	ulint		size)		/* in: the initial size of the
					tablespace file in pages,
					must be >= FIL_IBD_FILE_INITIAL_SIZE */
{
	os_file_t       file;
	ibool		ret;
	ulint		err;
	byte*		buf2;
	byte*		page;
	ibool		success;
	char*		path;

	ut_a(size >= FIL_IBD_FILE_INITIAL_SIZE);

	path = fil_make_ibd_name(tablename, is_temp);
	
	file = os_file_create(path, OS_FILE_CREATE, OS_FILE_NORMAL,
						    OS_DATA_FILE, &ret);
	if (ret == FALSE) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error creating file ", stderr);
		ut_print_filename(stderr, path);
		fputs(".\n", stderr);

		/* The following call will print an error message */
		 
		err = os_file_get_last_error(TRUE);
		
		if (err == OS_FILE_ALREADY_EXISTS) {
		        fputs(
"InnoDB: The file already exists though the corresponding table did not\n"
"InnoDB: exist in the InnoDB data dictionary. Have you moved InnoDB\n"
"InnoDB: .ibd files around without using the SQL commands\n"
"InnoDB: DISCARD TABLESPACE and IMPORT TABLESPACE, or did\n"
"InnoDB: mysqld crash in the middle of CREATE TABLE? You can\n"
"InnoDB: resolve the problem by removing the file ", stderr);
			ut_print_filename(stderr, path);
			fputs("\n"
"InnoDB: under the 'datadir' of MySQL.\n", stderr);

			mem_free(path);
			return(DB_TABLESPACE_ALREADY_EXISTS);
		}

		if (err == OS_FILE_DISK_FULL) {

			mem_free(path);
			return(DB_OUT_OF_FILE_SPACE);
		}

		mem_free(path);
		return(DB_ERROR);
	}

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	ret = os_file_set_size(path, file, size * UNIV_PAGE_SIZE, 0);
	
	if (!ret) {
		ut_free(buf2);
		os_file_close(file);
		os_file_delete(path);

		mem_free(path);
		return(DB_OUT_OF_FILE_SPACE);
	}

	if (*space_id == 0) {
		*space_id = fil_assign_new_space_id();
	}

	/* printf("Creating tablespace %s id %lu\n", path, *space_id); */

	if (*space_id == ULINT_UNDEFINED) {
		ut_free(buf2);
	error_exit:
		os_file_close(file);
	error_exit2:
		os_file_delete(path);

		mem_free(path);
		return(DB_ERROR);
	}

	/* We have to write the space id to the file immediately and flush the
	file to disk. This is because in crash recovery we must be aware what
	tablespaces exist and what are their space id's, so that we can apply
	the log records to the right file. It may take quite a while until
	buffer pool flush algorithms write anything to the file and flush it to
	disk. If we would not write here anything, the file would be filled
	with zeros from the call of os_file_set_size(), until a buffer pool
	flush would write to it. */

	memset(page, '\0', UNIV_PAGE_SIZE);

	fsp_header_write_space_id(page, *space_id);		

	buf_flush_init_for_writing(page, ut_dulint_zero, *space_id, 0);

	ret = os_file_write(path, file, page, 0, 0, UNIV_PAGE_SIZE);

	ut_free(buf2);

	if (!ret) {
		fputs(
"InnoDB: Error: could not write the first page to tablespace ", stderr);
		ut_print_filename(stderr, path);
		putc('\n', stderr);
		goto error_exit;
	}

	ret = os_file_flush(file);

	if (!ret) {
		fputs(
"InnoDB: Error: file flush of tablespace ", stderr);
		ut_print_filename(stderr, path);
		fputs(" failed\n", stderr);
		goto error_exit;
	}

	os_file_close(file);

	if (*space_id == ULINT_UNDEFINED) {
		goto error_exit2;
	}

	success = fil_space_create(path, *space_id, FIL_TABLESPACE);
	
	if (!success) {
		goto error_exit2;
	}	

	fil_node_create(path, size, *space_id, FALSE);

#ifndef UNIV_HOTBACKUP	
	{
	mtr_t		mtr;

	mtr_start(&mtr);

	fil_op_write_log(MLOG_FILE_CREATE, *space_id, tablename, NULL, &mtr);

	mtr_commit(&mtr);
	}
#endif
	mem_free(path);
	return(DB_SUCCESS);
}

/************************************************************************
It is possible, though very improbable, that the lsn's in the tablespace to be
imported have risen above the current system lsn, if a lengthy purge, ibuf
merge, or rollback was performed on a backup taken with ibbackup. If that is
the case, reset page lsn's in the file. We assume that mysqld was shut down
after it performed these cleanup operations on the .ibd file, so that it at
the shutdown stamped the latest lsn to the FIL_PAGE_FILE_FLUSH_LSN in the
first page of the .ibd file, and we can determine whether we need to reset the
lsn's just by looking at that flush lsn. */

ibool
fil_reset_too_high_lsns(
/*====================*/
					/* out: TRUE if success */
	const char*	name,		/* in: table name in the
					databasename/tablename format */
	dulint		current_lsn)	/* in: reset lsn's if the lsn stamped
					to FIL_PAGE_FILE_FLUSH_LSN in the
					first page is too high */
{
	os_file_t	file;
	char*		filepath;
	byte*		page;
	byte*		buf2;
	dulint		flush_lsn;
	ulint		space_id;
	ib_longlong	file_size;
	ib_longlong	offset;
	ulint		page_no;
	ibool		success;

	filepath = fil_make_ibd_name(name, FALSE);

	file = os_file_create_simple_no_error_handling(filepath, OS_FILE_OPEN,
						OS_FILE_READ_WRITE, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ut_print_timestamp(stderr);

	        fputs(
"  InnoDB: Error: trying to open a table, but could not\n"
"InnoDB: open the tablespace file ", stderr);
		ut_print_filename(stderr, filepath);
		fputs("!\n", stderr);
		mem_free(filepath);

		return(FALSE);
	}

	/* Read the first page of the tablespace */

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}

	/* We have to read the file flush lsn from the header of the file */

	flush_lsn = mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN);

	if (ut_dulint_cmp(current_lsn, flush_lsn) >= 0) {
		/* Ok */
		success = TRUE;

		goto func_exit;
	}

	space_id = fsp_header_get_space_id(page);
	
	ut_print_timestamp(stderr);
	fprintf(stderr,
" InnoDB: Flush lsn in the tablespace file %lu to be imported\n"
"InnoDB: is %lu %lu, which exceeds current system lsn %lu %lu.\n"
"InnoDB: We reset the lsn's in the file ",
			    (ulong) space_id,
			    (ulong) ut_dulint_get_high(flush_lsn),
			    (ulong) ut_dulint_get_low(flush_lsn),
			    (ulong) ut_dulint_get_high(current_lsn),
			    (ulong) ut_dulint_get_low(current_lsn));
	ut_print_filename(stderr, filepath);
	fputs(".\n", stderr);

	/* Loop through all the pages in the tablespace and reset the lsn and
	the page checksum if necessary */

	file_size = os_file_get_size_as_iblonglong(file);

	for (offset = 0; offset < file_size; offset += UNIV_PAGE_SIZE) {
		success = os_file_read(file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), UNIV_PAGE_SIZE);
		if (!success) {

			goto func_exit;
		}
		if (ut_dulint_cmp(mach_read_from_8(page + FIL_PAGE_LSN),
				  current_lsn) > 0) {
			/* We have to reset the lsn */
			space_id = mach_read_from_4(page
					+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
			page_no = mach_read_from_4(page + FIL_PAGE_OFFSET);
			
			buf_flush_init_for_writing(page, current_lsn, space_id,
								      page_no);
			success = os_file_write(filepath, file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), UNIV_PAGE_SIZE);
			if (!success) {

				goto func_exit;
			}
		}
	}

	success = os_file_flush(file);
	if (!success) {

		goto func_exit;
	}

	/* We now update the flush_lsn stamp at the start of the file */
	success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}

	mach_write_to_8(page + FIL_PAGE_FILE_FLUSH_LSN, current_lsn);

	success = os_file_write(filepath, file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}
	success = os_file_flush(file);
func_exit:
	os_file_close(file);
	ut_free(buf2);
	mem_free(filepath);

	return(success);
}

/************************************************************************
Tries to open a single-table tablespace and checks the space id is right in
it. If does not succeed, prints an error message to the .err log. This
function is used to open the tablespace when we load a table definition
to the dictionary cache. NOTE that we assume this operation is used under the
protection of the dictionary mutex, so that two users cannot race here. This
operation does not leave the file associated with the tablespace open, but
closes it after we have looked at the space id in it. */

ibool
fil_open_single_table_tablespace(
/*=============================*/
				/* out: TRUE if success */
	ulint		id,	/* in: space id */
	const char*	name)	/* in: table name in the
				databasename/tablename format */
{
	os_file_t	file;
	char*		filepath;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ibool		ret		= TRUE;

	filepath = fil_make_ibd_name(name, FALSE);

	file = os_file_create_simple_no_error_handling(filepath, OS_FILE_OPEN,
						OS_FILE_READ_ONLY, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ut_print_timestamp(stderr);

	        fputs(
"  InnoDB: Error: trying to open a table, but could not\n"
"InnoDB: open the tablespace file ", stderr);
		ut_print_filename(stderr, filepath);
		fputs("!\n"
"InnoDB: Have you moved InnoDB .ibd files around without using the\n"
"InnoDB: commands DISCARD TABLESPACE and IMPORT TABLESPACE?\n"
"InnoDB: It is also possible that this is a table created with\n"
"InnoDB: CREATE TEMPORARY TABLE, and MySQL removed the .ibd file for this.\n"
"InnoDB: Please refer to\n"
"InnoDB:"
" http://dev.mysql.com/doc/mysql/en/InnoDB_troubleshooting_datadict.html\n"
"InnoDB: how to resolve the issue.\n", stderr);

		mem_free(filepath);

		return(FALSE);
	}

	/* Read the first page of the tablespace */

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);

	/* We have to read the tablespace id from the file */

	space_id = fsp_header_get_space_id(page);

	if (space_id != id) {
		ut_print_timestamp(stderr);

	        fputs(
"  InnoDB: Error: tablespace id in file ", stderr);
		ut_print_filename(stderr, filepath);
		fprintf(stderr, " is %lu, but in the InnoDB\n"
"InnoDB: data dictionary it is %lu.\n"
"InnoDB: Have you moved InnoDB .ibd files around without using the\n"
"InnoDB: commands DISCARD TABLESPACE and IMPORT TABLESPACE?\n"
"InnoDB: Please refer to\n"
"InnoDB:"
" http://dev.mysql.com/doc/mysql/en/InnoDB_troubleshooting_datadict.html\n"
"InnoDB: how to resolve the issue.\n", (ulong) space_id, (ulong) id);

		ret = FALSE;

		goto func_exit;
	}

	success = fil_space_create(filepath, space_id, FIL_TABLESPACE);

	if (!success) {
		goto func_exit;
	}

	/* We do not measure the size of the file, that is why we pass the 0
	below */

	fil_node_create(filepath, 0, space_id, FALSE);
func_exit:
	os_file_close(file);
	ut_free(buf2);
	mem_free(filepath);

	return(ret);
}

#ifdef UNIV_HOTBACKUP
/***********************************************************************
Allocates a file name for an old version of a single-table tablespace.
The string must be freed by caller with mem_free()! */
static
char*
fil_make_ibbackup_old_name(
/*=======================*/
					/* out, own: file name */
	const char*	name)		/* in: original file name */
{
	static const char suffix[] = "_ibbackup_old_vers_";
	ulint	len	= strlen(name);
	char*	path	= mem_alloc(len + (15 + sizeof suffix));

	memcpy(path, name, len);
	memcpy(path + len, suffix, (sizeof suffix) - 1);
	ut_sprintf_timestamp_without_extra_chars(path + len + sizeof suffix);
	return(path);
}
#endif /* UNIV_HOTBACKUP */

/************************************************************************
Opens an .ibd file and adds the associated single-table tablespace to the
InnoDB fil0fil.c data structures. */
static
void
fil_load_single_table_tablespace(
/*=============================*/
	const char*	dbname,		/* in: database name */
	const char*	filename)	/* in: file name (not a path),
					including the .ibd extension */
{
	os_file_t	file;
	char*		filepath;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		size_low;
	ulint		size_high;
	ib_longlong	size;
#ifdef UNIV_HOTBACKUP
	fil_space_t*	space;
#endif
	filepath = mem_alloc(strlen(dbname) + strlen(filename) 
			+ strlen(fil_path_to_mysql_datadir) + 3);

	sprintf(filepath, "%s/%s/%s", fil_path_to_mysql_datadir, dbname,
								filename);
	srv_normalize_path_for_win(filepath);

	file = os_file_create_simple_no_error_handling(filepath, OS_FILE_OPEN,
						OS_FILE_READ_ONLY, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

	        fprintf(stderr,
"InnoDB: Error: could not open single-table tablespace file\n"
"InnoDB: %s!\n"
"InnoDB: We do not continue crash recovery, because the table will become\n"
"InnoDB: corrupt if we cannot apply the log records in the InnoDB log to it.\n"
"InnoDB: To fix the problem and start mysqld:\n"
"InnoDB: 1) If there is a permission problem in the file and mysqld cannot\n"
"InnoDB: open the file, you should modify the permissions.\n"
"InnoDB: 2) If the table is not needed, or you can restore it from a backup,\n"
"InnoDB: then you can remove the .ibd file, and InnoDB will do a normal\n"
"InnoDB: crash recovery and ignore that table.\n"
"InnoDB: 3) If the file system or the disk is broken, and you cannot remove\n"
"InnoDB: the .ibd file, you can set innodb_force_recovery > 0 in my.cnf\n"
"InnoDB: and force InnoDB to continue crash recovery here.\n", filepath);

		mem_free(filepath);

		if (srv_force_recovery > 0) {
			fprintf(stderr,
"InnoDB: innodb_force_recovery was set to %lu. Continuing crash recovery\n"
"InnoDB: even though we cannot access the .ibd file of this table.\n",
							srv_force_recovery);
			return;
		}

		exit(1);
	}

	success = os_file_get_size(file, &size_low, &size_high);

	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

	        fprintf(stderr,
"InnoDB: Error: could not measure the size of single-table tablespace file\n"
"InnoDB: %s!\n"
"InnoDB: We do not continue crash recovery, because the table will become\n"
"InnoDB: corrupt if we cannot apply the log records in the InnoDB log to it.\n"
"InnoDB: To fix the problem and start mysqld:\n"
"InnoDB: 1) If there is a permission problem in the file and mysqld cannot\n"
"InnoDB: access the file, you should modify the permissions.\n"
"InnoDB: 2) If the table is not needed, or you can restore it from a backup,\n"
"InnoDB: then you can remove the .ibd file, and InnoDB will do a normal\n"
"InnoDB: crash recovery and ignore that table.\n"
"InnoDB: 3) If the file system or the disk is broken, and you cannot remove\n"
"InnoDB: the .ibd file, you can set innodb_force_recovery > 0 in my.cnf\n"
"InnoDB: and force InnoDB to continue crash recovery here.\n", filepath);

		os_file_close(file);
		mem_free(filepath);

		if (srv_force_recovery > 0) {
			fprintf(stderr,
"InnoDB: innodb_force_recovery was set to %lu. Continuing crash recovery\n"
"InnoDB: even though we cannot access the .ibd file of this table.\n",
							srv_force_recovery);
			return;
		}

		exit(1);
	}

	/* TODO: What to do in other cases where we cannot access an .ibd
	file during a crash recovery? */

	/* Every .ibd file is created >= 4 pages in size. Smaller files
	cannot be ok. */

	size = (((ib_longlong)size_high) << 32) + (ib_longlong)size_low;
#ifndef UNIV_HOTBACKUP
	if (size < FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE) {
	        fprintf(stderr,
"InnoDB: Error: the size of single-table tablespace file %s\n"
"InnoDB: is only %lu %lu, should be at least %lu!", filepath,
			(ulong) size_high,
			(ulong) size_low, (ulong) (4 * UNIV_PAGE_SIZE));
		os_file_close(file);
		mem_free(filepath);

		return;
	}
#endif
	/* Read the first page of the tablespace if the size big enough */

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	if (size >= FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE) {
		success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);

		/* We have to read the tablespace id from the file */

		space_id = fsp_header_get_space_id(page);
	} else {
		space_id = ULINT_UNDEFINED;
	}

#ifndef UNIV_HOTBACKUP
	if (space_id == ULINT_UNDEFINED || space_id == 0) {
	        fprintf(stderr,
"InnoDB: Error: tablespace id %lu in file %s is not sensible\n",
			(ulong) space_id,
			filepath);
		goto func_exit;
	}
#else
	if (space_id == ULINT_UNDEFINED || space_id == 0) {
		char*	new_path;

		fprintf(stderr,
"InnoDB: Renaming tablespace %s of id %lu,\n"
"InnoDB: to %s_ibbackup_old_vers_<timestamp>\n"
"InnoDB: because its size %lld is too small (< 4 pages 16 kB each),\n"
"InnoDB: or the space id in the file header is not sensible.\n"
"InnoDB: This can happen in an ibbackup run, and is not dangerous.\n",
				filepath, space_id, filepath, size);
		os_file_close(file);

		new_path = fil_make_ibbackup_old_name(filepath);
		ut_a(os_file_rename(filepath, new_path));

		ut_free(buf2);
		mem_free(filepath);
		mem_free(new_path);

		return;
	}

	/* A backup may contain the same space several times, if the space got
	renamed at a sensitive time. Since it is enough to have one version of
	the space, we rename the file if a space with the same space id
	already exists in the tablespace memory cache. We rather rename the
	file than delete it, because if there is a bug, we do not want to
	destroy valuable data. */

	mutex_enter(&(fil_system->mutex));

	space = fil_get_space_for_id_low(space_id);

	if (space) {
		char*	new_path;

		fprintf(stderr,
"InnoDB: Renaming tablespace %s of id %lu,\n"
"InnoDB: to %s_ibbackup_old_vers_<timestamp>\n"
"InnoDB: because space %s with the same id\n"
"InnoDB: was scanned earlier. This can happen if you have renamed tables\n"
"InnoDB: during an ibbackup run.\n", filepath, space_id, filepath,
								space->name);
		os_file_close(file);

		new_path = fil_make_ibbackup_old_name(filepath);

		mutex_exit(&(fil_system->mutex));

		ut_a(os_file_rename(filepath, new_path));

		ut_free(buf2);
		mem_free(filepath);
		mem_free(new_path);

		return;
	}
	mutex_exit(&(fil_system->mutex));
#endif
	success = fil_space_create(filepath, space_id, FIL_TABLESPACE);

	if (!success) {

		goto func_exit;
	}

	/* We do not measure the size of the file, that is why we pass the 0
	below */

	fil_node_create(filepath, 0, space_id, FALSE);
func_exit:
	os_file_close(file);
	ut_free(buf2);
	mem_free(filepath);
}

/************************************************************************
At the server startup, if we need crash recovery, scans the database
directories under the MySQL datadir, looking for .ibd files. Those files are
single-table tablespaces. We need to know the space id in each of them so that
we know into which file we should look to check the contents of a page stored
in the doublewrite buffer, also to know where to apply log records where the
space id is != 0. */

ulint
fil_load_single_table_tablespaces(void)
/*===================================*/
			/* out: DB_SUCCESS or error number */
{
	int		ret;
	char*		dbpath		= NULL;
	ulint		dbpath_len	= 100;
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;

	/* The datadir of MySQL is always the default directory of mysqld */

	dir = os_file_opendir(fil_path_to_mysql_datadir, TRUE);

	if (dir == NULL) {

		return(DB_ERROR);
	}

	dbpath = mem_alloc(dbpath_len);

	/* Scan all directories under the datadir. They are the database
	directories of MySQL. */

	ret = os_file_readdir_next_file(fil_path_to_mysql_datadir, dir,
								&dbinfo);
	while (ret == 0) {
		ulint len;
		/* printf("Looking at %s in datadir\n", dbinfo.name); */

		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

		        goto next_datadir_item;
		}

		/* We found a symlink or a directory; try opening it to see
		if a symlink is a directory */

		len = strlen(fil_path_to_mysql_datadir)
				+ strlen (dbinfo.name) + 2;
		if (len > dbpath_len) {
			dbpath_len = len;

			if (dbpath) {
				mem_free(dbpath);
			}

			dbpath = mem_alloc(dbpath_len);
		}
		sprintf(dbpath, "%s/%s", fil_path_to_mysql_datadir,
								dbinfo.name);
		srv_normalize_path_for_win(dbpath);

		dbdir = os_file_opendir(dbpath, FALSE);

		if (dbdir != NULL) {
			/* printf("Opened dir %s\n", dbinfo.name); */

			/* We found a database directory; loop through it,
			looking for possible .ibd files in it */

			ret = os_file_readdir_next_file(dbpath, dbdir,
								&fileinfo);
			while (ret == 0) {
				/* printf(
"     Looking at file %s\n", fileinfo.name); */

			        if (fileinfo.type == OS_FILE_TYPE_DIR
				    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {
				        goto next_file_item;
				}

				/* We found a symlink or a file */
				if (strlen(fileinfo.name) > 4
				    && 0 == strcmp(fileinfo.name + 
						strlen(fileinfo.name) - 4,
						".ibd")) {
				        /* The name ends in .ibd; try opening
					the file */
					fil_load_single_table_tablespace(
						dbinfo.name, fileinfo.name);
				}
next_file_item:
				ret = os_file_readdir_next_file(dbpath, dbdir,
								&fileinfo);
			}

			if (0 != os_file_closedir(dbdir)) {
				 fputs(
"InnoDB: Warning: could not close database directory ", stderr);
				 ut_print_filename(stderr, dbpath);
				 putc('\n', stderr);
			}
		}
		
next_datadir_item:
		ret = os_file_readdir_next_file(fil_path_to_mysql_datadir,
								dir, &dbinfo);
	}

	mem_free(dbpath);

	/* At the end of directory we should get 1 as the return value, -1
	if there was an error */
	if (ret != 1) {
		fprintf(stderr,
"InnoDB: Error: os_file_readdir_next_file returned %d in MySQL datadir\n",
							       ret);
		os_file_closedir(dir);

		return(DB_ERROR);
	}

	if (0 != os_file_closedir(dir)) {
		fprintf(stderr,
"InnoDB: Error: could not close MySQL datadir\n");

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/************************************************************************
If we need crash recovery, and we have called
fil_load_single_table_tablespaces() and dict_load_single_table_tablespaces(),
we can call this function to print an error message of orphaned .ibd files
for which there is not a data dictionary entry with a matching table name
and space id. */

void
fil_print_orphaned_tablespaces(void)
/*================================*/
{
	fil_system_t*	system 		= fil_system;
	fil_space_t*	space;

	mutex_enter(&(system->mutex));

	space = UT_LIST_GET_FIRST(system->space_list);

	while (space) {
	        if (space->purpose == FIL_TABLESPACE && space->id != 0
							  && !space->mark) {
			fputs("InnoDB: Warning: tablespace ", stderr);
			ut_print_filename(stderr, space->name);
			fprintf(stderr, " of id %lu has no matching table in\n"
"InnoDB: the InnoDB data dictionary.\n", (ulong) space->id);
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(system->mutex));	
}

/***********************************************************************
Returns TRUE if a single-table tablespace does not exist in the memory cache,
or is being deleted there. */

ibool
fil_tablespace_deleted_or_being_deleted_in_mem(
/*===========================================*/
				/* out: TRUE if does not exist or is being\
				deleted */
	ulint		id,	/* in: space id */
	ib_longlong	version)/* in: tablespace_version should be this; if
				you pass -1 as the value of this, then this
				parameter is ignored */
{
	fil_system_t*	system	= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL || space->is_being_deleted) {
		mutex_exit(&(system->mutex));

		return(TRUE);
	}

	if (version != ((ib_longlong)-1)
				&& space->tablespace_version != version) {
		mutex_exit(&(system->mutex));

		return(TRUE);
	}

	mutex_exit(&(system->mutex));

	return(FALSE);
}

/***********************************************************************
Returns TRUE if a single-table tablespace exists in the memory cache. */

ibool
fil_tablespace_exists_in_mem(
/*=========================*/
			/* out: TRUE if exists */
	ulint	id)	/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	if (space == NULL) {
		mutex_exit(&(system->mutex));

		return(FALSE);
	}

	mutex_exit(&(system->mutex));

	return(TRUE);
}

/***********************************************************************
Returns TRUE if a matching tablespace exists in the InnoDB tablespace memory
cache. Note that if we have not done a crash recovery at the database startup,
there may be many tablespaces which are not yet in the memory cache. */

ibool
fil_space_for_table_exists_in_mem(
/*==============================*/
					/* out: TRUE if a matching tablespace
					exists in the memory cache */
	ulint		id,		/* in: space id */
	const char*	name,		/* in: table name in the standard
					'databasename/tablename' format or
					the dir path to a temp table */
	ibool		is_temp,	/* in: TRUE if created with CREATE
					TEMPORARY TABLE */
	ibool		mark_space,	/* in: in crash recovery, at database
					startup we mark all spaces which have
					an associated table in the InnoDB
					data dictionary, so that
					we can print a warning about orphaned
					tablespaces */
	ibool		print_error_if_does_not_exist)
					/* in: print detailed error
					information to the .err log if a
					matching tablespace is not found from
					memory */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	namespace;
	fil_space_t*	space;
	char*		path;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	path = fil_make_ibd_name(name, is_temp);

	/* Look if there is a space with the same id */

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	/* Look if there is a space with the same name; the name is the
	directory path from the datadir to the file */

	HASH_SEARCH(name_hash, system->name_hash,
					ut_fold_string(path), namespace,
					0 == strcmp(namespace->name, path));
	if (space && space == namespace) {
		/* Found */
		
		if (mark_space) {
			space->mark = TRUE;
		}

		mem_free(path);
		mutex_exit(&(system->mutex));

		return(TRUE);
	}

	if (!print_error_if_does_not_exist) {
		
		mem_free(path);
		mutex_exit(&(system->mutex));
		
		return(FALSE);
	}

	if (space == NULL) {
		if (namespace == NULL) {
		        ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
"InnoDB: in InnoDB data dictionary has tablespace id %lu,\n"
"InnoDB: but tablespace with that id or name does not exist. Have\n"
"InnoDB: you deleted or moved .ibd files?\n"
"InnoDB: This may also be a table created with CREATE TEMPORARY TABLE\n"
"InnoDB: whose .ibd and .frm files MySQL automatically removed, but the\n"
"InnoDB: table still exists in the InnoDB internal data dictionary.\n",
				(ulong) id);
		} else {
		        ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
"InnoDB: in InnoDB data dictionary has tablespace id %lu,\n"
"InnoDB: but tablespace with that id does not exist. There is\n"
"InnoDB: a tablespace of name %s and id %lu, though. Have\n"
"InnoDB: you deleted or moved .ibd files?\n",
				(ulong) id, namespace->name,
				(ulong) namespace->id);
		}
	error_exit:
		fputs(
"InnoDB: Please refer to\n"
"InnoDB:"
" http://dev.mysql.com/doc/mysql/en/InnoDB_troubleshooting_datadict.html\n"
"InnoDB: how to resolve the issue.\n", stderr);

		mem_free(path);
		mutex_exit(&(system->mutex));

		return(FALSE);
	}

	if (0 != strcmp(space->name, path)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: table ", stderr);
		ut_print_filename(stderr, name);
		fprintf(stderr, "\n"
"InnoDB: in InnoDB data dictionary has tablespace id %lu,\n"
"InnoDB: but tablespace with that id has name %s.\n"
"InnoDB: Have you deleted or moved .ibd files?\n", (ulong) id, space->name);

		if (namespace != NULL) {
			fputs(
"InnoDB: There is a tablespace with the right name\n"
"InnoDB: ", stderr);
			ut_print_filename(stderr, namespace->name);
			fprintf(stderr, ", but its id is %lu.\n",
				(ulong) namespace->id);
		}

		goto error_exit;
	}

	mem_free(path);
	mutex_exit(&(system->mutex));

	return(FALSE);
}

/***********************************************************************
Checks if a single-table tablespace for a given table name exists in the
tablespace memory cache. */
static
ulint
fil_get_space_id_for_table(
/*=======================*/
				/* out: space id, ULINT_UNDEFINED if not
				found */
	const char*	name)	/* in: table name in the standard
				'databasename/tablename' format */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	namespace;
	ulint		id		= ULINT_UNDEFINED;
	char*		path;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	path = fil_make_ibd_name(name, FALSE);

	/* Look if there is a space with the same name; the name is the
	directory path to the file */

	HASH_SEARCH(name_hash, system->name_hash,
					ut_fold_string(path), namespace,
					0 == strcmp(namespace->name, path));
	if (namespace) {
		id = namespace->id;
	}	

	mem_free(path);

	mutex_exit(&(system->mutex));

	return(id);
}

/**************************************************************************
Tries to extend a data file so that it would accommodate the number of pages
given. The tablespace must be cached in the memory cache. If the space is big
enough already, does nothing. */

ibool
fil_extend_space_to_desired_size(
/*=============================*/
				/* out: TRUE if success */
	ulint*	actual_size,	/* out: size of the space after extension;
				if we ran out of disk space this may be lower
				than the desired size */
	ulint	space_id,	/* in: space id, must be != 0 */
	ulint	size_after_extend)/* in: desired size in pages after the
				extension; if the current space size is bigger
				than this already, the function does nothing */
{
	fil_system_t*	system		= fil_system;
	fil_node_t*	node;
	fil_space_t*	space;
	byte*		buf2;
	byte*		buf;
	ulint		start_page_no;
	ulint		file_start_page_no;
	ulint		n_pages;
	ulint		offset_high;
	ulint		offset_low;
	ibool		success		= TRUE;

	fil_mutex_enter_and_prepare_for_io(space_id);

	HASH_SEARCH(hash, system->spaces, space_id, space,
						space->id == space_id);
	ut_a(space);

	if (space->size >= size_after_extend) {
		/* Space already big enough */

		*actual_size = space->size;

		mutex_exit(&(system->mutex));	

		return(TRUE);
	}
	
	node = UT_LIST_GET_LAST(space->chain);

	fil_node_prepare_for_io(node, system, space);

	/* Extend 1 MB at a time */

	buf2 = mem_alloc(1024 * 1024 + UNIV_PAGE_SIZE);
	buf = ut_align(buf2, UNIV_PAGE_SIZE);

	memset(buf, '\0', 1024 * 1024);

	start_page_no = space->size;
	file_start_page_no = space->size - node->size;

	while (start_page_no < size_after_extend) {	
		n_pages = size_after_extend - start_page_no;

		if (n_pages > (1024 * 1024) / UNIV_PAGE_SIZE) {
			n_pages = (1024 * 1024) / UNIV_PAGE_SIZE;
		}

		offset_high = (start_page_no - file_start_page_no)
				/ (4096 * ((1024 * 1024) / UNIV_PAGE_SIZE));
		offset_low  = ((start_page_no - file_start_page_no)
				% (4096 * ((1024 * 1024) / UNIV_PAGE_SIZE)))
			      * UNIV_PAGE_SIZE;
#ifdef UNIV_HOTBACKUP
		success = os_file_write(node->name, node->handle, buf,
					offset_low, offset_high,
					UNIV_PAGE_SIZE * n_pages);
#else
		success = os_aio(OS_FILE_WRITE, OS_AIO_SYNC,
			node->name, node->handle, buf,
			offset_low, offset_high,
			UNIV_PAGE_SIZE * n_pages,
			NULL, NULL);
#endif
		if (success) {
			node->size += n_pages;
			space->size += n_pages;

			os_has_said_disk_full = FALSE;
		} else {
			/* Let us measure the size of the file to determine
			how much we were able to extend it */
			
			n_pages = ((ulint)
				(os_file_get_size_as_iblonglong(node->handle)
				/ UNIV_PAGE_SIZE)) - node->size;

			node->size += n_pages;
			space->size += n_pages;

			break;
		}

		start_page_no += n_pages;
	}

	mem_free(buf2);

	fil_node_complete_io(node, system, OS_FILE_WRITE);

	*actual_size = space->size;
	/*
        printf("Extended %s to %lu, actual size %lu pages\n", space->name,
                                        size_after_extend, *actual_size); */
	mutex_exit(&(system->mutex));	

	fil_flush(space_id);

	return(success);
}

#ifdef UNIV_HOTBACKUP
/************************************************************************
Extends all tablespaces to the size stored in the space header. During the
ibbackup --apply-log phase we extended the spaces on-demand so that log records
could be applied, but that may have left spaces still too small compared to
the size stored in the space header. */

void
fil_extend_tablespaces_to_stored_len(void)
/*======================================*/
{
	fil_system_t*	system 		= fil_system;
	fil_space_t*	space;
	byte*		buf;
	ulint		actual_size;
	ulint		size_in_header;
	ulint		error;
	ibool		success;

	buf = mem_alloc(UNIV_PAGE_SIZE);

	mutex_enter(&(system->mutex));

	space = UT_LIST_GET_FIRST(system->space_list);

	while (space) {
	        ut_a(space->purpose == FIL_TABLESPACE);

		mutex_exit(&(system->mutex)); /* no need to protect with a
					      mutex, because this is a single-
					      threaded operation */
		error = fil_read(TRUE, space->id, 0, 0, UNIV_PAGE_SIZE, buf,
									NULL);
		ut_a(error == DB_SUCCESS);

		size_in_header = fsp_get_size_low(buf);

		success = fil_extend_space_to_desired_size(&actual_size,
						space->id, size_in_header);
		if (!success) {
			fprintf(stderr,
"InnoDB: Error: could not extend the tablespace of %s\n"
"InnoDB: to the size stored in header, %lu pages;\n"
"InnoDB: size after extension %lu pages\n"
"InnoDB: Check that you have free disk space and retry!\n", space->name,
					size_in_header, actual_size);
			exit(1);				
		}

		mutex_enter(&(system->mutex));

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(system->mutex));

	mem_free(buf);
}
#endif

/*========== RESERVE FREE EXTENTS (for a B-tree split, for example) ===*/

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
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	ibool		success;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space);

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
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);

	ut_a(space);
	ut_a(space->n_reserved_extents >= n_reserved);
	
	space->n_reserved_extents -= n_reserved;
	
	mutex_exit(&(system->mutex));
}

/***********************************************************************
Gets the number of reserved extents. If the database is silent, this number
should be zero. */

ulint
fil_space_get_n_reserved_extents(
/*=============================*/
	ulint	id)		/* in: space id */
{
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	ulint		n;

	ut_ad(system);

	mutex_enter(&(system->mutex));

	HASH_SEARCH(hash, system->spaces, id, space, space->id == id);
	
	ut_a(space);

	n = space->n_reserved_extents;
	
	mutex_exit(&(system->mutex));

	return(n);
}

/*============================ FILE I/O ================================*/

/************************************************************************
NOTE: you must call fil_mutex_enter_and_prepare_for_io() first!

Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. The caller must hold the fil_sys
mutex. */
static
void
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	fil_space_t*	space)	/* in: space */
{
	ut_ad(node && system && space);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	
	if (system->n_open > system->max_n_open + 5) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Warning: open files %lu exceeds the limit %lu\n",
			(ulong) system->n_open,
			(ulong) system->max_n_open);
	}

	if (node->open == FALSE) {
		/* File is closed: open it */
		ut_a(node->n_pending == 0);

		fil_node_open_file(node, system, space);
	}

	if (node->n_pending == 0 && space->purpose == FIL_TABLESPACE
						      && space->id != 0) {
		/* The node is in the LRU list, remove it */

		ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

		UT_LIST_REMOVE(LRU, system->LRU, node);
	}

	node->n_pending++;
}

/************************************************************************
Updates the data structures when an i/o operation finishes. Updates the
pending i/o's field in the node appropriately. */
static
void
fil_node_complete_io(
/*=================*/
	fil_node_t*	node,	/* in: file node */
	fil_system_t*	system,	/* in: tablespace memory cache */
	ulint		type)	/* in: OS_FILE_WRITE or OS_FILE_READ; marks
				the node as modified if
				type == OS_FILE_WRITE */
{
	ut_ad(node);
	ut_ad(system);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(system->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(node->n_pending > 0);
	
	node->n_pending--;

	if (type == OS_FILE_WRITE) {
		system->modification_counter++;
		node->modification_counter = system->modification_counter;
	}
	
	if (node->n_pending == 0 && node->space->purpose == FIL_TABLESPACE
					&& node->space->id != 0) {
		/* The node must be put back to the LRU list */
		UT_LIST_ADD_FIRST(LRU, system->LRU, node);
	}
}

/************************************************************************
Reads or writes data. This operation is asynchronous (aio). */

ulint
fil_io(
/*===*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
	ulint	len,		/* in: how many bytes to read or write; this
				must not cross a file boundary; in aio this
				must be a block size multiple */
	void*	buf,		/* in/out: buffer where to store read data
				or from where to write; in aio this must be
				appropriately aligned */
	void*	message)	/* in: message for aio handler if non-sync
				aio used, else ignored */
{
	fil_system_t*	system		= fil_system;
	ulint		mode;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		offset_high;
	ulint		offset_low;
	ibool		ret;
	ulint		is_log;
	ulint		wake_later;
	
	is_log = type & OS_FILE_LOG;
	type = type & ~OS_FILE_LOG;

	wake_later = type & OS_AIO_SIMULATED_WAKE_LATER;
	type = type & ~OS_AIO_SIMULATED_WAKE_LATER;
	
	ut_ad(byte_offset < UNIV_PAGE_SIZE);
	ut_ad(buf);
	ut_ad(len > 0);
	ut_a((1 << UNIV_PAGE_SIZE_SHIFT) == UNIV_PAGE_SIZE);
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

        if (type == OS_FILE_READ) {
                srv_data_read+= len;
        } else if (type == OS_FILE_WRITE) {
                srv_data_written+= len;
        }

	/* Reserve the fil_system mutex and make sure that we can open at
	least one file while holding it, if the file is not already open */

	fil_mutex_enter_and_prepare_for_io(space_id);
	
	HASH_SEARCH(hash, system->spaces, space_id, space,
							space->id == space_id);
	if (!space) {
		mutex_exit(&(system->mutex));

		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: Error: trying to do i/o to a tablespace which does not exist.\n"
"InnoDB: i/o type %lu, space id %lu, page no. %lu, i/o length %lu bytes\n",
			(ulong) type, (ulong) space_id, (ulong) block_offset,
			(ulong) len);

		return(DB_TABLESPACE_DELETED);
	}

	ut_ad((mode != OS_AIO_IBUF) || (space->purpose == FIL_TABLESPACE));

	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {
		if (space->id != 0 && node->size == 0) {
			/* We do not know the size of a single-table tablespace
			before we open the file */

			break;
		}

		if (node == NULL) {
			fprintf(stderr,
	"InnoDB: Error: trying to access page number %lu in space %lu,\n"
	"InnoDB: space name %s,\n"
	"InnoDB: which is outside the tablespace bounds.\n"
	"InnoDB: Byte offset %lu, len %lu, i/o type %lu\n", 
 			(ulong) block_offset, (ulong) space_id,
			space->name, (ulong) byte_offset, (ulong) len,
			(ulong) type);
 			
			ut_error;
		}

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

	/* Check that at least the start offset is within the bounds of a
	single-table tablespace */
	if (space->purpose == FIL_TABLESPACE && space->id != 0
	    && node->size <= block_offset) {

	        fprintf(stderr,
	"InnoDB: Error: trying to access page number %lu in space %lu,\n"
	"InnoDB: space name %s,\n"
	"InnoDB: which is outside the tablespace bounds.\n"
	"InnoDB: Byte offset %lu, len %lu, i/o type %lu\n", 
 			(ulong) block_offset, (ulong) space_id,
			space->name, (ulong) byte_offset, (ulong) len,
			(ulong) type);
 		ut_a(0);
	}

	/* Now we have made the changes in the data structures of system */
	mutex_exit(&(system->mutex));

	/* Calculate the low 32 bits and the high 32 bits of the file offset */

	offset_high = (block_offset >> (32 - UNIV_PAGE_SIZE_SHIFT));
	offset_low  = ((block_offset << UNIV_PAGE_SIZE_SHIFT) & 0xFFFFFFFFUL)
			+ byte_offset;

	ut_a(node->size - block_offset >=
 		(byte_offset + len + (UNIV_PAGE_SIZE - 1)) / UNIV_PAGE_SIZE);

	/* Do aio */

	ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a((len % OS_FILE_LOG_BLOCK_SIZE) == 0);

#ifdef UNIV_HOTBACKUP
	/* In ibbackup do normal i/o, not aio */
	if (type == OS_FILE_READ) {
		ret = os_file_read(node->handle, buf, offset_low, offset_high,
									len);
	} else {
		ret = os_file_write(node->name, node->handle, buf,
					offset_low, offset_high, len);
	}
#else
	/* Queue the aio request */
	ret = os_aio(type, mode | wake_later, node->name, node->handle, buf,
				offset_low, offset_high, len, node, message);
#endif
	ut_a(ret);

	if (mode == OS_AIO_SYNC) {
		/* The i/o operation is already completed when we return from
		os_aio: */
		
		mutex_enter(&(system->mutex));

		fil_node_complete_io(node, system, type);

		mutex_exit(&(system->mutex));

		ut_ad(fil_validate());
	}

	return(DB_SUCCESS);
}

/************************************************************************
Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

ulint
fil_read(
/*=====*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
	return(fil_io(OS_FILE_READ, sync, space_id, block_offset,
					  byte_offset, len, buf, message));
}

/************************************************************************
Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

ulint
fil_write(
/*======*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
	return(fil_io(OS_FILE_WRITE, sync, space_id, block_offset,
					   byte_offset, len, buf, message));
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
	fil_system_t*	system		= fil_system;
	ibool		ret;		
	fil_node_t*	fil_node;
	void*		message;
	ulint		type;
	
	ut_ad(fil_validate());

	if (os_aio_use_native_aio) {
		srv_set_io_thread_op_info(segment, "native aio handle");
#ifdef WIN_ASYNC_IO
		ret = os_aio_windows_handle(segment, 0, (void**) &fil_node,
					    &message, &type);
#elif defined(POSIX_ASYNC_IO)
		ret = os_aio_posix_handle(segment, &fil_node, &message);
#else
		ret = 0; /* Eliminate compiler warning */
		ut_error;
#endif
	} else {
		srv_set_io_thread_op_info(segment, "simulated aio handle");

		ret = os_aio_simulated_handle(segment, (void**) &fil_node,
	                                               &message, &type);
	}
	
	ut_a(ret);

	srv_set_io_thread_op_info(segment, "complete io for fil node");

	mutex_enter(&(system->mutex));

	fil_node_complete_io(fil_node, fil_system, type);

	mutex_exit(&(system->mutex));

	ut_ad(fil_validate());

	/* Do the i/o handling */
	/* IMPORTANT: since i/o handling for reads will read also the insert
	buffer in tablespace 0, you have to be very careful not to introduce
	deadlocks in the i/o system. We keep tablespace 0 data files always
	open, and use a special i/o thread to serve insert buffer requests. */

	if (buf_pool_is_block(message)) {
		srv_set_io_thread_op_info(segment, "complete io for buf page");
		buf_page_io_complete(message);
	} else {
		srv_set_io_thread_op_info(segment, "complete io for log");
		log_io_complete(message);
	}
}

/**************************************************************************
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */

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
	ib_longlong	old_mod_counter;

	mutex_enter(&(system->mutex));
	
	HASH_SEARCH(hash, system->spaces, space_id, space,
							space->id == space_id);
	if (!space || space->is_being_deleted) {
		mutex_exit(&(system->mutex));

		return;
	}

	space->n_pending_flushes++;	/* prevent dropping of the space while
					we are flushing */
	node = UT_LIST_GET_FIRST(space->chain);

	while (node) {
		if (node->modification_counter > node->flush_counter) {
			ut_a(node->open);

			/* We want to flush the changes at least up to
			old_mod_counter */
			old_mod_counter = node->modification_counter;

			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes++;
			} else {
				fil_n_pending_log_flushes++;
				fil_n_log_flushes++;
			}
#ifdef __WIN__
			if (node->is_raw_disk) {

				goto skip_flush;
			}
#endif
retry:			
			if (node->n_pending_flushes > 0) {
				/* We want to avoid calling os_file_flush() on
				the file twice at the same time, because we do
				not know what bugs OS's may contain in file
				i/o; sleep for a while */

				mutex_exit(&(system->mutex));

				os_thread_sleep(20000);

				mutex_enter(&(system->mutex));

				if (node->flush_counter >= old_mod_counter) {

					goto skip_flush;
				}

				goto retry;
			}

			ut_a(node->open);
			file = node->handle;
			node->n_pending_flushes++;

			mutex_exit(&(system->mutex));

			/* fprintf(stderr, "Flushing to file %s\n",
				node->name); */

			os_file_flush(file);		

			mutex_enter(&(system->mutex));

			node->n_pending_flushes--;
skip_flush:
			if (node->flush_counter < old_mod_counter) {
				node->flush_counter = old_mod_counter;
			}

			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes--;
			} else {
				fil_n_pending_log_flushes--;
			}
		}

		node = UT_LIST_GET_NEXT(chain, node);
	}		

	space->n_pending_flushes--;

	mutex_exit(&(system->mutex));
}

/**************************************************************************
Flushes to disk the writes in file spaces of the given type possibly cached by
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
			space->n_pending_flushes++; /* prevent dropping of the
						    space while we are
						    flushing */
			mutex_exit(&(system->mutex));

			fil_flush(space->id);

			mutex_enter(&(system->mutex));

			space->n_pending_flushes--;
		}
		space = UT_LIST_GET_NEXT(space_list, space);
	}
	
	mutex_exit(&(system->mutex));
}

/**********************************************************************
Checks the consistency of the tablespace cache. */

ibool
fil_validate(void)
/*==============*/
			/* out: TRUE if ok */
{	
	fil_system_t*	system		= fil_system;
	fil_space_t*	space;
	fil_node_t*	fil_node;
	ulint		n_open		= 0;
	ulint		i;
	
	mutex_enter(&(system->mutex));

	/* Look for spaces in the hash table */

	for (i = 0; i < hash_get_n_cells(system->spaces); i++) {

		space = HASH_GET_FIRST(system->spaces, i);
	
		while (space != NULL) {
			UT_LIST_VALIDATE(chain, fil_node_t, space->chain); 

			fil_node = UT_LIST_GET_FIRST(space->chain);

			while (fil_node != NULL) {
				if (fil_node->n_pending > 0) {
					ut_a(fil_node->open);
				}

				if (fil_node->open) {
					n_open++;
				}
				fil_node = UT_LIST_GET_NEXT(chain, fil_node);
			}
			space = HASH_GET_NEXT(hash, space);
		}
	}

	ut_a(system->n_open == n_open);

	UT_LIST_VALIDATE(LRU, fil_node_t, system->LRU);

	fil_node = UT_LIST_GET_FIRST(system->LRU);

	while (fil_node != NULL) {
		ut_a(fil_node->n_pending == 0);
		ut_a(fil_node->open);
		ut_a(fil_node->space->purpose == FIL_TABLESPACE);
		ut_a(fil_node->space->id != 0);

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
