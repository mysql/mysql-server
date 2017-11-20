/*****************************************************************************

Copyright (c) 1997, 2017, Oracle and/or its affiliates. All Rights Reserved.
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
@file log/log0recv.cc
Recovery

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include <my_aes.h>
#include <sys/types.h>

#include <array>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "log0recv.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "dict0dd.h"
#include "mtr0mtr.h"
#include "fil0fil.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0recv.h"
#include "mem0mem.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "os0thread-create.h"
#include "page0cur.h"
#include "page0zip.h"
#include "trx0rec.h"
#include "trx0undo.h"
#include "ut0new.h"
#ifndef UNIV_HOTBACKUP
# include "buf0rea.h"
# include "row0merge.h"
# include "srv0srv.h"
# include "srv0start.h"
# include "trx0purge.h"
#else /* !UNIV_HOTBACKUP */
/** This is set to false if the backup was originally taken with the
mysqlbackup --include regexp option: then we do not want to create tables in
directories which were not included */
bool	recv_replay_file_ops	= true;
#endif /* !UNIV_HOTBACKUP */

const char* const ib_logfile_basename = "ib_logfile";

/** Log records are stored in the hash table in chunks at most of this size;
this must be less than UNIV_PAGE_SIZE as it is stored in the buffer pool */
#define RECV_DATA_BLOCK_SIZE	(MEM_MAX_ALLOC_IN_BUF - sizeof(recv_data_t))

/** Read-ahead area in applying log records to file pages */
static const size_t RECV_READ_AHEAD_AREA = 32;

/** The recovery system */
recv_sys_t*	recv_sys = nullptr;

/** true when applying redo log records during crash recovery; false
otherwise.  Note that this is false while a background thread is
rolling back incomplete transactions. */
volatile bool	recv_recovery_on;

#ifndef UNIV_HOTBACKUP

PSI_memory_key	mem_log_recv_page_hash_key;
PSI_memory_key	mem_log_recv_space_hash_key;

/** true when recv_init_crash_recovery() has been called. */
bool	recv_needed_recovery;

/** true if buf_page_is_corrupted() should check if the log sequence
number (FIL_PAGE_LSN) is in the future.  Initially false, and set by
recv_recovery_from_checkpoint_start(). */
bool	recv_lsn_checks_on;

/** If the following is true, the buffer pool file pages must be invalidated
after recovery and no ibuf operations are allowed; this becomes true if
the log record hash table becomes too full, and log records must be merged
to file pages already before the recovery is finished: in this case no
ibuf operations are allowed, as they could modify the pages read in the
buffer pool before the pages have been recovered to the up-to-date state.

true means that recovery is running and no operations on the log files
are allowed yet: the variable name is misleading. */
bool	recv_no_ibuf_operations;

/** true when the redo log is being backed up */
# define recv_is_making_a_backup		false

/** true when recovering from a backed up redo log file */
# define recv_is_from_backup			false

#else /* !UNIV_HOTBACKUP */

# define recv_needed_recovery			false

/** true When the redo log is being backed up */
bool	recv_is_making_a_backup	= false;

/** true when recovering from a backed up redo log file */
bool	recv_is_from_backup	= false;

# define buf_pool_get_curr_size() (5 * 1024 * 1024)

#endif /* !UNIV_HOTBACKUP */

/** The following counter is used to decide when to print info on
log scan */
static ulint	recv_scan_print_counter;

/** The type of the previous parsed redo log record */
static mlog_id_t	recv_previous_parsed_rec_type;

/** The offset of the previous parsed redo log record */
static ulint	recv_previous_parsed_rec_offset;

/** The 'multi' flag of the previous parsed redo log record */
static ulint	recv_previous_parsed_rec_is_multi;

/** This many frames must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free frames to read in pages when we start applying the
log records to the database.
This is the default value. If the actual size of the buffer pool is
larger than 10 MB we'll set this value to 512. */
ulint	recv_n_pool_free_frames;

/** The maximum lsn we see for a page during the recovery process. If this
is bigger than the lsn we are able to scan up to, that is an indication that
the recovery failed and the database may be corrupt. */
static lsn_t	recv_max_page_lsn;

#ifndef UNIV_HOTBACKUP
# ifdef UNIV_PFS_THREAD
mysql_pfs_key_t	recv_writer_thread_key;
# endif /* UNIV_PFS_THREAD */

/** Flag indicating if recv_writer thread is active. */
static bool	recv_writer_thread_active = false;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Return string name of the redo log record type.
@param[in]	type	record log record enum
@return string name of record log record */
const char*
get_mlog_string(mlog_id_t type);
#endif /* UNIV_DEBUG */

/* prototypes */

#ifndef UNIV_HOTBACKUP

/** Reads a specified log segment to a buffer.
@param[in,out]	buf		buffer where to read
@param[in,out]	group		log group
@param[in]	start_lsn	read area start
@param[in]	end_lsn		read area end */
static
void
recv_read_log_seg(
	byte*		buf,
	log_group_t*	group,
	lsn_t		start_lsn,
	lsn_t		end_lsn);

/** Initialize crash recovery environment. Can be called iff
recv_needed_recovery == false. */
static
void
recv_init_crash_recovery();

#endif /* !UNIV_HOTBACKUP */

/** Calculates the new value for lsn when more data is added to the log.
@param[in]	lsn		Old LSN
@param[in]	len		This many bytes of data is addedd, log block
				headers not included
@return LSN after data addition */
lsn_t
recv_calc_lsn_on_data_add(
	lsn_t		lsn,
	uint64_t	len)
{
	ulint		frag_len;
	uint64_t	lsn_len;

	frag_len = (lsn % OS_FILE_LOG_BLOCK_SIZE) - LOG_BLOCK_HDR_SIZE;

	ut_ad(frag_len < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE
	      - LOG_BLOCK_TRL_SIZE);

	lsn_len = len;

	lsn_len += (lsn_len + frag_len)
		/ (OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE
		   - LOG_BLOCK_TRL_SIZE)
		* (LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE);

	return(lsn + lsn_len);
}

/** Destructor */
MetadataRecover::~MetadataRecover()
{
	PersistentTables::iterator	iter;

	for (iter = m_tables.begin(); iter != m_tables.end(); ++iter) {

		UT_DELETE(iter->second);
	}
}

/** Get the dynamic metadata of a specified table, create a new one
if not exist
@param[in]	id	table id
@return the metadata of the specified table */
PersistentTableMetadata*
MetadataRecover::getMetadata(
	table_id_t	id)
{
	PersistentTableMetadata*	metadata = nullptr;
	PersistentTables::iterator	iter = m_tables.find(id);

	if (iter == m_tables.end()) {
		PersistentTableMetadata* mem =
			static_cast<PersistentTableMetadata*>(
				ut_zalloc_nokey(sizeof *metadata));

		metadata = new (mem) PersistentTableMetadata(id, 0);

		m_tables.insert(std::make_pair(id, metadata));
	} else {
		metadata = iter->second;
		ut_ad(metadata->get_table_id() == id);
	}

	ut_ad(metadata != nullptr);
	return(metadata);
}

/** Parse a dynamic metadata redo log of a table and store
the metadata locally
@param[in]	id	table id
@param[in]	version	table dynamic metadata version
@param[in]	ptr	redo log start
@param[in]	end	end of redo log
@retval ptr to next redo log record, nullptr if this log record
was truncated */
byte*
MetadataRecover::parseMetadataLog(
	table_id_t	id,
	uint64_t	version,
	byte*		ptr,
	byte*		end)
{
	if (ptr + 2 > end) {
		/* At least we should get type byte and another one byte
		for data, if not, it's an incompleted log */
		return(nullptr);
	}

	persistent_type_t	type = static_cast<persistent_type_t>(ptr[0]);

	ut_ad(dict_persist->persisters != nullptr);

	Persister*		persister = dict_persist->persisters->get(
		type);
	PersistentTableMetadata*metadata = getMetadata(id);

	bool			corrupt;
	ulint			consumed = persister->read(
		*metadata, ptr, end - ptr, &corrupt);

	if (corrupt) {
		recv_sys->found_corrupt_log = true;
	} else if (consumed != 0) {
		metadata->set_version(version);
	}

	if (consumed == 0) {
		return(nullptr);
	} else {
		return(ptr + consumed);
	}
}

/** Apply the collected persistent dynamic metadata to in-memory
table objects */
void
MetadataRecover::apply()
{
	PersistentTables::iterator	iter;

	for (iter = m_tables.begin();
	     iter != m_tables.end();
	     ++iter) {

		table_id_t		table_id = iter->first;
		PersistentTableMetadata*metadata = iter->second;
		dict_table_t*		table;

		table = dd_table_open_on_id(table_id, nullptr, nullptr, false, true);

		/* If the table is nullptr, it might be already dropped */
		if (table == nullptr) {
			continue;
		}

		mutex_enter(&dict_sys->mutex);

		/* At this time, the metadata in DDTableBuffer has
		already been applied to table object, we can apply
		the latest status of metadata read from redo logs to
		the table now. We can read the dirty_status directly
		since it's in recovery phase */

		/* The table should be either CLEAN or applied BUFFERED
		metadata from DDTableBuffer just now */
		ut_ad(table->dirty_status == METADATA_CLEAN
		      || table->dirty_status == METADATA_BUFFERED);

		bool buffered = (table->dirty_status == METADATA_BUFFERED);

		mutex_enter(&dict_persist->mutex);

		bool is_dirty = dict_table_apply_dynamic_metadata(
			table, metadata);

		if (is_dirty) {
			/* This table was not marked as METADATA_BUFFERED
			before the redo logs are applied, so it's not in
			the list */
			if (!buffered) {
				ut_ad(!table->in_dirty_dict_tables_list);

				UT_LIST_ADD_LAST(
					dict_persist->dirty_dict_tables,
					table);
			}

			table->dirty_status = METADATA_DIRTY;

			ut_d(table->in_dirty_dict_tables_list = true);
		}

		mutex_exit(&dict_persist->mutex);
		mutex_exit(&dict_sys->mutex);

		dd_table_close(table, NULL, NULL, false);
	}
}

/** Store the collected persistent dynamic metadata to
mysql.innodb_dynamic_metadata */
void
MetadataRecover::store()
{
	ut_ad(dict_sys->dynamic_metadata != nullptr);
	ut_ad(dict_persist->table_buffer != nullptr);

	DDTableBuffer*		table_buffer = dict_persist->table_buffer;

	if (empty()) {
		return;
	}

	mutex_enter(&dict_persist->mutex);

	for (auto meta : m_tables) {
		table_id_t		table_id = meta.first;
		PersistentTableMetadata*metadata = meta.second;
		byte			buffer[REC_MAX_DATA_SIZE];
		ulint			size;

		size = dict_persist->persisters->write(*metadata, buffer);

		dberr_t	error = table_buffer->replace(
			table_id, metadata->get_version(), buffer, size);
		if (error != DB_SUCCESS) {
			ut_ad(0);
		}
	}

	mutex_exit(&dict_persist->mutex);
}

/** Creates the recovery system. */
void
recv_sys_create()
{
	if (recv_sys != nullptr) {

		return;
	}

	recv_sys = static_cast<recv_sys_t*>(
		ut_zalloc_nokey(sizeof(*recv_sys)));

	mutex_create(LATCH_ID_RECV_SYS, &recv_sys->mutex);
	mutex_create(LATCH_ID_RECV_WRITER, &recv_sys->writer_mutex);

	recv_sys->spaces = nullptr;
}

/** Free up recovery data structures. */
static
void
recv_sys_finish()
{
	if (recv_sys->spaces != nullptr) {
		for (auto& space : *recv_sys->spaces) {

			if (space.second.m_heap != nullptr) {
				mem_heap_free(space.second.m_heap);
				space.second.m_heap = nullptr;
			}
		}

		UT_DELETE(recv_sys->spaces);
	}

	ut_a(recv_sys->dblwr.pages.empty());

	if (!recv_sys->dblwr.deferred.empty()) {

		/* Free the pages that were not required for recovery. */
		for (auto& page : recv_sys->dblwr.deferred) {
			page.close();
		}
	}

	recv_sys->dblwr.deferred.clear();

	ut_free(recv_sys->buf);
	ut_free(recv_sys->last_block_buf_start);
	UT_DELETE(recv_sys->metadata_recover);

	recv_sys->buf = nullptr;
	recv_sys->spaces = nullptr;
	recv_sys->metadata_recover = nullptr;
	recv_sys->last_block_buf_start = nullptr;
}

/** Release recovery system mutexes. */
void
recv_sys_close()
{
	if (recv_sys == nullptr) {

		return;
	}

	recv_sys_finish();

	if (recv_sys->flush_start != nullptr) {
		os_event_destroy(recv_sys->flush_start);
	}

	if (recv_sys->flush_end != nullptr) {
		os_event_destroy(recv_sys->flush_end);
	}

#ifndef UNIV_HOTBACKUP
	ut_ad(!recv_writer_thread_active);
	mutex_free(&recv_sys->writer_mutex);
#endif /* !UNIV_HOTBACKUP */

	call_destructor(&recv_sys->dblwr);
	call_destructor(&recv_sys->deleted);
	call_destructor(&recv_sys->missing_ids);

	mutex_free(&recv_sys->mutex);

	ut_free(recv_sys);
	recv_sys = nullptr;
}

#ifndef UNIV_HOTBACKUP
/** Reset the state of the recovery system variables. */
void
recv_sys_var_init()
{
	recv_recovery_on = false;
	recv_needed_recovery = false;
	recv_lsn_checks_on = false;
	recv_no_ibuf_operations = false;
	recv_scan_print_counter	= 0;
	recv_previous_parsed_rec_type = MLOG_SINGLE_REC_FLAG;
	recv_previous_parsed_rec_offset	= 0;
	recv_previous_parsed_rec_is_multi = 0;
	recv_n_pool_free_frames	= 256;
	recv_max_page_lsn = 0;
}

/** Get the number of bytes used by all the heaps
@return number of bytes used */
static
size_t
recv_heap_used()
{
	size_t	size = 0;

	for (auto& space : *recv_sys->spaces) {

		if (space.second.m_heap != nullptr) {
			size += mem_heap_get_size(space.second.m_heap);
		}
	}

	return(size);
}

/** Prints diagnostic info of corrupt log.
@param[in]	ptr	pointer to corrupt log record
@param[in]	type	type of the log record (could be garbage)
@param[in]	space	tablespace ID (could be garbage)
@param[in]	page_no	page number (could be garbage)
@return whether processing should continue */
static
bool
recv_report_corrupt_log(
	const byte*	ptr,
	int		type,
	space_id_t	space,
	page_no_t	page_no)
{
	ib::error()
		<< "############### CORRUPT LOG RECORD FOUND ###############";

	ib::info()
		<< "Log record type " << type << ", page " << space << ":"
		<< page_no << ". Log parsing proceeded successfully up to "
		<< recv_sys->recovered_lsn << ". Previous log record type "
		<< recv_previous_parsed_rec_type << ", is multi "
		<< recv_previous_parsed_rec_is_multi << " Recv offset "
		<< (ptr - recv_sys->buf) << ", prev "
		<< recv_previous_parsed_rec_offset;

	ut_ad(ptr <= recv_sys->buf + recv_sys->len);

	const ulint	limit	= 100;
	const ulint	before
		= std::min(recv_previous_parsed_rec_offset, limit);
	const ulint	after
		= std::min(recv_sys->len - (ptr - recv_sys->buf), limit);

	ib::info()
		<< "Hex dump starting " << before << " bytes before and"
		" ending " << after << " bytes after the corrupted record:";

	ut_print_buf(stderr,
		     recv_sys->buf
		     + recv_previous_parsed_rec_offset - before,
		     ptr - recv_sys->buf + before + after
		     - recv_previous_parsed_rec_offset);
	putc('\n', stderr);

#ifndef UNIV_HOTBACKUP
	if (srv_force_recovery == 0) {

		ib::info()
			<< "Set innodb_force_recovery to ignore this error.";

		return(false);
	}
#endif /* !UNIV_HOTBACKUP */

	ib::warn()
		<< "The log file may have been corrupt and it is possible"
		" that the log scan did not proceed far enough in recovery!"
		" Please run CHECK TABLE on your InnoDB tables to check"
		" that they are ok! If mysqld crashes after this recovery; "
		<< FORCE_RECOVERY_MSG;
	return(true);
}

/** recv_writer thread tasked with flushing dirty pages from the buffer
pools. */
static
void
recv_writer_thread()
{
	ut_ad(!srv_read_only_mode);

	/* The code flow is as follows:
	Step 1: In recv_recovery_from_checkpoint_start().
	Step 2: This recv_writer thread is started.
	Step 3: In recv_recovery_from_checkpoint_finish().
	Step 4: Wait for recv_writer thread to complete. This is based
	        on the flag recv_writer_thread_active.
	Step 5: Assert that recv_writer thread is not active anymore.

	It is possible that the thread that is started in step 2,
	becomes active only after step 4 and hence the assert in
	step 5 fails.  So mark this thread active only if necessary. */
	mutex_enter(&recv_sys->writer_mutex);

	if (recv_recovery_on) {
		recv_writer_thread_active = true;
	} else {
		mutex_exit(&recv_sys->writer_mutex);
		return;
	}
	mutex_exit(&recv_sys->writer_mutex);

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {

		os_thread_sleep(100000);

		mutex_enter(&recv_sys->writer_mutex);

		if (!recv_recovery_on) {
			mutex_exit(&recv_sys->writer_mutex);
			break;
		}

		/* Flush pages from end of LRU if required */
		os_event_reset(recv_sys->flush_end);
		recv_sys->flush_type = BUF_FLUSH_LRU;
		os_event_set(recv_sys->flush_start);
		os_event_wait(recv_sys->flush_end);

		mutex_exit(&recv_sys->writer_mutex);
	}

	recv_writer_thread_active = false;

	my_thread_end();
}
#endif /* !UNIV_HOTBACKUP */

/** Inits the recovery system for a recovery operation.
@param[in]	max_mem		Available memory in bytes */
void
recv_sys_init(ulint max_mem)
{
	if (recv_sys->spaces != nullptr) {

		return;
	}

#ifndef UNIV_HOTBACKUP
	mutex_enter(&recv_sys->mutex);

	if (!srv_read_only_mode) {
		recv_sys->flush_start = os_event_create(0);
		recv_sys->flush_end = os_event_create(0);
	}
#else /* !UNIV_HOTBACKUP */
	recv_is_from_backup = true;
	recv_sys->heap = mem_heap_create(256);
#endif /* !UNIV_HOTBACKUP */

	/* Set appropriate value of recv_n_pool_free_frames. If capacity
	is at least 10M and 25% above 512 pages then bump free frames to
	512. */
	if (buf_pool_get_curr_size() >= (10 * 1024 * 1024)
	    && (buf_pool_get_curr_size() >= ((512 + 128) * UNIV_PAGE_SIZE))) {
		/* Buffer pool of size greater than 10 MB. */
		recv_n_pool_free_frames = 512;
	}

	recv_sys->buf = static_cast<byte*>(
		ut_malloc_nokey(RECV_PARSING_BUF_SIZE));

	recv_sys->len = 0;
	recv_sys->recovered_offset = 0;

	using Spaces = recv_sys_t::Spaces;

	recv_sys->spaces = UT_NEW(Spaces(), mem_log_recv_space_hash_key);

	recv_sys->n_addrs = 0;

	recv_sys->apply_log_recs = false;
	recv_sys->apply_batch_on = false;
	recv_sys->is_cloned_db = false;

	recv_sys->last_block_buf_start = static_cast<byte*>(
		ut_malloc_nokey(2 * OS_FILE_LOG_BLOCK_SIZE));

	recv_sys->last_block = static_cast<byte*>(ut_align(
		recv_sys->last_block_buf_start, OS_FILE_LOG_BLOCK_SIZE));

	recv_sys->found_corrupt_log = false;
	recv_sys->found_corrupt_fs = false;

	recv_max_page_lsn = 0;

	/* Call the constructor for both placement new objects. */
	new (&recv_sys->dblwr) recv_dblwr_t();

	new (&recv_sys->deleted) recv_sys_t::Missing_Ids();

	new (&recv_sys->missing_ids) recv_sys_t::Missing_Ids();

	recv_sys->metadata_recover = UT_NEW_NOKEY(MetadataRecover());

	mutex_exit(&recv_sys->mutex);
}

/** Empties the hash table when it has been fully processed. */
static
void
recv_sys_empty_hash()
{
	ut_ad(mutex_own(&recv_sys->mutex));

	if (recv_sys->n_addrs != 0) {

		ib::fatal()
			<< recv_sys->n_addrs
			<< " pages with log records"
			<< " were left unprocessed!";
	}

	for (auto& space : *recv_sys->spaces) {

		if (space.second.m_heap != nullptr) {
			mem_heap_free(space.second.m_heap);
			space.second.m_heap = nullptr;
		}
	}

	UT_DELETE(recv_sys->spaces);

	using Spaces = recv_sys_t::Spaces;

	recv_sys->spaces = UT_NEW(Spaces(), mem_log_recv_space_hash_key);
}

#ifndef UNIV_HOTBACKUP
/** Frees the recovery system. */
void
recv_sys_free()
{
	mutex_enter(&recv_sys->mutex);

	recv_sys_finish();

	/* wake page cleaner up to progress */
	if (!srv_read_only_mode) {
		ut_ad(!recv_recovery_on);
		ut_ad(!recv_writer_thread_active);
		os_event_reset(buf_flush_event);
		os_event_set(recv_sys->flush_start);
	}

	/* Free encryption data structures. */
	if (recv_sys->keys != nullptr) {

		for (auto& key : *recv_sys->keys) {

			if (key.ptr != nullptr) {
				ut_free(key.ptr);
				key.ptr = nullptr;
			}

			if (key.iv != nullptr) {
				ut_free(key.iv);
				key.iv = nullptr;
			}
		}

		recv_sys->keys->swap(*recv_sys->keys);

		UT_DELETE(recv_sys->keys);
		recv_sys->keys = nullptr;
	}

	mutex_exit(&recv_sys->mutex);
}

/** Copies a log segment from the most up-to-date log group to the other log
groups, so that they all contain the latest log data. Also writes the info
about the latest checkpoint to the groups, and inits the fields in the group
memory structs to up-to-date values. */
static
void
recv_synchronize_groups()
{
	lsn_t	recovered_lsn;

	recovered_lsn = recv_sys->recovered_lsn;

	/* Read the last recovered log block to the recovery system buffer:
	the block is always incomplete */

	lsn_t	start_lsn = ut_uint64_align_down(
		recovered_lsn, OS_FILE_LOG_BLOCK_SIZE);

	lsn_t	end_lsn;

	end_lsn = ut_uint64_align_up(recovered_lsn, OS_FILE_LOG_BLOCK_SIZE);

	ut_a(start_lsn != end_lsn);

	recv_read_log_seg(
		recv_sys->last_block,
		UT_LIST_GET_FIRST(log_sys->log_groups), start_lsn, end_lsn);

	for (log_group_t* group = UT_LIST_GET_FIRST(log_sys->log_groups);
	     group;
	     group = UT_LIST_GET_NEXT(log_groups, group)) {
		/* Update the fields in the group struct to correspond to
		recovered_lsn */

		log_group_set_fields(group, recovered_lsn);
	}

	/* Copy the checkpoint info to the log; remember that we have
	incremented checkpoint_no by one, and the info will not be written
	over the max checkpoint info, thus making the preservation of max
	checkpoint info on disk certain */

	log_write_checkpoint_info(true);
	log_mutex_enter();
}
#endif /* !UNIV_HOTBACKUP */

/** Check the consistency of a log header block.
@param[in]	buf	header block
@return true if ok */
static
bool
recv_check_log_header_checksum(const byte* buf)
{
	return(log_block_get_checksum(buf)
	       == log_block_calc_checksum_crc32(buf));
}

#ifndef UNIV_HOTBACKUP
/** Copy of the LOG_HEADER_CREATOR field. */
static char log_header_creator[LOG_HEADER_CREATOR_END - LOG_HEADER_CREATOR + 1];

/** Determine if a redo log from MySQL 5.7.9 is clean.
@param[in]	lsn	checkpoint LSN
@return error code
@retval	DB_SUCCESS	if the redo log is clean
@retval DB_ERROR	if the redo log is corrupted or dirty */
static
dberr_t
recv_log_recover_5_7(lsn_t lsn)
{
	log_mutex_enter();

	log_group_t*	group = UT_LIST_GET_FIRST(log_sys->log_groups);
	lsn_t		source_offset = log_group_calc_lsn_offset(lsn, group);

	log_mutex_exit();

	page_no_t	page_no;

	page_no = (page_no_t) (source_offset / univ_page_size.physical());

	byte*		buf = log_sys->buf;

	static const char* NO_UPGRADE_RECOVERY_MSG =
		"Upgrade after a crash is not supported."
		" This redo log was created with ";

	static const char* NO_UPGRADE_RTFM_MSG =
		". Please follow the instructions at "
		REFMAN "upgrading.html";

	fil_io(IORequestLogRead, true,
	       page_id_t(group->space_id, page_no),
	       univ_page_size,
	       (ulint) ((source_offset & ~(OS_FILE_LOG_BLOCK_SIZE - 1))
			% univ_page_size.physical()),
	       OS_FILE_LOG_BLOCK_SIZE, buf, nullptr);

	if (log_block_calc_checksum(buf) != log_block_get_checksum(buf)) {

		ib::error()
			<< NO_UPGRADE_RECOVERY_MSG
			<< log_header_creator
			<< ", and it appears corrupted"
			<< NO_UPGRADE_RTFM_MSG;

		return(DB_CORRUPTION);
	}

	/* On a clean shutdown, the redo log will be logically empty
	after the checkpoint lsn. */

	if (log_block_get_data_len(buf)
	    != (source_offset & (OS_FILE_LOG_BLOCK_SIZE - 1))) {

		ib::error()
			<< NO_UPGRADE_RECOVERY_MSG
			<< log_header_creator
			<< NO_UPGRADE_RTFM_MSG;

		return(DB_ERROR);
	}

	/* Mark the redo log for upgrading. */
	srv_log_file_size = 0;

	recv_sys->parse_start_lsn = recv_sys->recovered_lsn
		= recv_sys->scanned_lsn = lsn;

	log_sys->last_checkpoint_lsn = log_sys->next_checkpoint_lsn
		= log_sys->lsn = log_sys->write_lsn
		= log_sys->current_flush_lsn = log_sys->flushed_to_disk_lsn
		= lsn;

	log_sys->next_checkpoint_no = 0;

	return(DB_SUCCESS);
}

/** Find the latest checkpoint in the log header.
@param[out]	max_group	log group, or nullptr
@param[out]	max_field	LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
recv_find_max_checkpoint(
	log_group_t**	max_group,
	ulint*		max_field)
{
	uint64_t	max_no = 0;

	*max_field = 0;
	*max_group = nullptr;

	byte*	buf = log_sys->checkpoint_buf;

	for (auto group = UT_LIST_GET_FIRST(log_sys->log_groups);
	     group != nullptr;
	     /* No op */ ) {

		group->state = LOG_GROUP_CORRUPTED;

		log_group_header_read(group, 0);

		/* Check the header page checksum. There was no
		checksum in the first redo log format (version 0). */
		group->format = mach_read_from_4(buf + LOG_HEADER_FORMAT);

		if (group->format != 0
		    && !recv_check_log_header_checksum(buf)) {

			ib::error() << "Invalid redo log header checksum.";

			return(DB_CORRUPTION);
		}

		memcpy(log_header_creator, buf + LOG_HEADER_CREATOR,
		       sizeof log_header_creator);

		log_header_creator[(sizeof log_header_creator) - 1] = 0;

		switch (group->format) {
		case 0:
			ib::error() << "Unsupported redo log format."
				" The redo log was created"
				" before MySQL 5.7.9.";
			return(DB_ERROR);

		case LOG_HEADER_FORMAT_5_7_9:
			/* The checkpoint page format is identical. */

		case LOG_HEADER_FORMAT_CURRENT:
			break;

		default:
			ib::error() << "Unsupported redo log format."
				" The redo log was created"
				" with " << log_header_creator <<
				". Please follow the instructions at "
				REFMAN "upgrading-downgrading.html";
			return(DB_ERROR);
		}

		for (auto field = LOG_CHECKPOINT_1;
		     field <= LOG_CHECKPOINT_2;
		     field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {

			log_group_header_read(group, field);

			if (!recv_check_log_header_checksum(buf)) {
				DBUG_PRINT("ib_log",
					   ("invalid checkpoint,"
					    " group " ULINTPF " at %d"
					    ", checksum %x",
					    group->id, field,
					    (unsigned) log_block_get_checksum(
						    buf)));
				continue;
			}

			group->state = LOG_GROUP_OK;

			group->lsn = mach_read_from_8(
				buf + LOG_CHECKPOINT_LSN);

			group->lsn_offset = mach_read_from_8(
				buf + LOG_CHECKPOINT_OFFSET);

			uint64_t	checkpoint_no = mach_read_from_8(
				buf + LOG_CHECKPOINT_NO);

			DBUG_PRINT("ib_log",
				   ("checkpoint " UINT64PF " at " LSN_PF
				    " found in group " ULINTPF,
				    checkpoint_no, group->lsn, group->id));

			if (checkpoint_no >= max_no) {
				*max_group = group;
				*max_field = field;
				max_no = checkpoint_no;
			}
		}

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	if (*max_group == nullptr) {

		/* Before 5.7.9, we could get here during database
		initialization if we created an ib_logfile0 file that
		was filled with zeroes, and were killed. After
		5.7.9, we would reject such a file already earlier,
		when checking the file header. */

		ib::error()
			<< "No valid checkpoint found"
			" (corrupted redo log)."
			" You can try --innodb-force-recovery=6"
			" as a last resort.";
		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}
#else /* !UNIV_HOTBACKUP */
/** Reads the checkpoint info needed in hot backup.
@param[in]	hdr		buffer containing the log group header
@param[out]	lsn		checkpoint lsn
@param[out]	offset		checkpoint offset in the log group
@param[out]	cp_no		checkpoint number
@param[out]	first_header_lsn lsn of of the start of the first log file
@return true if success */
bool
recv_read_checkpoint_info_for_backup(
	const byte*	hdr,
	lsn_t*		lsn,
	lsn_t*		offset,
	lsn_t*		cp_no,
	lsn_t*		first_header_lsn)
{
	ulint		max_cp		= 0;
	uint64_t	max_cp_no	= 0;
	const byte*	cp_buf = hdr + LOG_CHECKPOINT_1;

	if (recv_check_log_header_checksum(cp_buf)) {
		max_cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);
		max_cp = LOG_CHECKPOINT_1;
	}

	cp_buf = hdr + LOG_CHECKPOINT_2;

	if (recv_check_log_header_checksum(cp_buf)) {
		if (mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO) > max_cp_no) {
			max_cp = LOG_CHECKPOINT_2;
		}
	}

	if (max_cp == 0) {
		return(false);
	}

	cp_buf = hdr + max_cp;

	*lsn = mach_read_from_8(cp_buf + LOG_CHECKPOINT_LSN);
	*offset = mach_read_from_4(
		cp_buf + LOG_CHECKPOINT_OFFSET_LOW32);
	*offset |= ((lsn_t) mach_read_from_4(
			    cp_buf + LOG_CHECKPOINT_OFFSET_HIGH32)) << 32;

	*cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);

	*first_header_lsn = mach_read_from_8(hdr + LOG_FILE_START_LSN);

	return(true);
}
#endif /* !UNIV_HOTBACKUP */

/** Check the 4-byte checksum to the trailer checksum field of a log
block.
@param[in]	block	pointer to a log block
@return whether the checksum matches */
static
bool
log_block_checksum_is_ok(
	const byte*	block)
{
	return(!innodb_log_checksums
	       || log_block_get_checksum(block)
	       == log_block_calc_checksum(block));
}

#ifdef UNIV_HOTBACKUP
/** Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned.
@param[in]	buf		buffer containing log data
@param[in]	buf_len		data length in that buffer
@param[in,out]	scanned_lsn	LSN of buffer start, we return scanned lsn
@param[in,out]	scanned_checkpoint_no	4 lowest bytes of the highest scanned
				checkpoint number so far
@param[out]	n_bytes_scanned	how much we were able to scan, smaller than
				buf_len if log data ended here */
void
recv_scan_log_seg_for_backup(
	byte*		buf,
	ulint		buf_len,
	lsn_t*		scanned_lsn,
	ulint*		scanned_checkpoint_no,
	ulint*		n_bytes_scanned)
{
	*n_bytes_scanned = 0;

	for (auto log_block = buf;
	     log_block < buf + buf_len;
	     log_block += OS_FILE_LOG_BLOCK_SIZE) {

		ulint	no = log_block_get_hdr_no(log_block);

		if (no != log_block_convert_lsn_to_no(*scanned_lsn)
		    || !log_block_checksum_is_ok(log_block)) {

			/* Garbage or an incompletely written log block */

			log_block += OS_FILE_LOG_BLOCK_SIZE;
			break;
		}

		if (*scanned_checkpoint_no > 0
		    && log_block_get_checkpoint_no(log_block)
		    < *scanned_checkpoint_no
		    && *scanned_checkpoint_no
		    - log_block_get_checkpoint_no(log_block)
		    > 0x80000000UL) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */
			break;
		}

		ulint	data_len = log_block_get_data_len(log_block);

		*scanned_checkpoint_no
			= log_block_get_checkpoint_no(log_block);
		*scanned_lsn += data_len;

		*n_bytes_scanned += data_len;

		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data ends here */

			break;
		}
	}
}
#endif /* UNIV_HOTBACKUP */

/** Parse or process a write encryption info record.
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	space_id	the tablespace ID
@return log record end, nullptr if not a complete record */
static
byte*
fil_write_encryption_parse(
	byte*		ptr,
	const byte*	end,
	space_id_t	space_id)
{
	byte*		iv = nullptr;
	byte*		key = nullptr;
	bool		is_new = false;

	fil_space_t*	space = fil_space_get(space_id);

	if (space == nullptr) {

		if (recv_sys->keys == nullptr) {

			recv_sys->keys = UT_NEW_NOKEY(
				recv_sys_t::Encryption_Keys());
		}

		for (auto& recv_key : *recv_sys->keys) {

			if (recv_key.space_id == space_id) {
				iv = recv_key.iv;
				key = recv_key.ptr;
			}
		}

		if (key == nullptr) {

			key = static_cast<byte*>(
				ut_malloc_nokey(ENCRYPTION_KEY_LEN));

			iv = static_cast<byte*>(
				ut_malloc_nokey(ENCRYPTION_KEY_LEN));

			is_new = true;
		}

	} else {
		iv = space->encryption_iv;
		key = space->encryption_key;
	}

	ulint	offset;

	offset = mach_read_from_2(ptr);
	ptr += 2;

	ulint	len;

	len = mach_read_from_2(ptr);
	ptr += 2;

	if (end < ptr + len) {
		return(nullptr);
	}

	if (offset >= UNIV_PAGE_SIZE
	    || len + offset > UNIV_PAGE_SIZE
	    || (len != ENCRYPTION_INFO_SIZE_V1
		&& len != ENCRYPTION_INFO_SIZE_V2)) {

		recv_sys->found_corrupt_log = true;
		return(nullptr);
	}

	if (!Encryption::decode_encryption_info(key, iv, ptr)) {

		recv_sys->found_corrupt_log = true;

		ib::warn()
			<< "Encryption information"
			<< " in the redo log of space "
			<< space_id << " is invalid";
	}

	ut_ad(len == ENCRYPTION_INFO_SIZE_V1
	      || len == ENCRYPTION_INFO_SIZE_V2);

	ptr += len;

	if (space == nullptr) {

		if (is_new) {

			recv_sys_t::Encryption_Key	new_key;

			new_key.iv = iv;
			new_key.ptr = key;
			new_key.space_id = space_id;

			recv_sys->keys->push_back(new_key);
		}

	} else {
		ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

		space->encryption_type = Encryption::AES;
		space->encryption_klen = ENCRYPTION_KEY_LEN;
	}

	return(ptr);
}

/** Try to parse a single log record body and also applies it if
specified.
@param[in]	type		redo log entry type
@param[in]	ptr		redo log record body
@param[in]	end_ptr		end of buffer
@param[in]	space_id	tablespace identifier
@param[in]	page_no		page number
@param[in,out]	block		buffer block, or nullptr if
				a page log record should not be applied
				or if it is a MLOG_FILE_ operation
@param[in,out]	mtr		mini-transaction, or nullptr if
				a page log record should not be applied
@param[in]	parsed_bytes	Number of bytes parsed so far
@return log record end, nullptr if not a complete record */
static
byte*
recv_parse_or_apply_log_rec_body(
	mlog_id_t	type,
	byte*		ptr,
	byte*		end_ptr,
	space_id_t	space_id,
	page_no_t	page_no,
	buf_block_t*	block,
	mtr_t*		mtr,
	ulint		parsed_bytes)
{
	ut_ad(!block == !mtr);

	switch (type) {
	case MLOG_FILE_OPEN:
		ut_ad(parsed_bytes != ULINT_UNDEFINED);
		// Fall through
	case MLOG_FILE_DELETE:
	case MLOG_FILE_CREATE2:
	case MLOG_FILE_RENAME2:

		ut_ad(block == nullptr);

		return(fil_tablespace_name_recover(
			ptr, end_ptr, page_id_t(space_id, page_no), type,
			parsed_bytes));

	case MLOG_INDEX_LOAD:

		if (end_ptr < ptr + 8) {

			return(nullptr);
		}

		return(ptr + 8);

	case MLOG_WRITE_STRING:

		/* For encrypted tablespace, we need to get the
		encryption key information before the page 0 is recovered.
		Otherwise, redo will not find the key to decrypt
		the data pages. */

		if (page_no == 0
		    && !fsp_is_system_or_temp_tablespace(space_id)) {

			return(fil_write_encryption_parse(
				ptr, end_ptr, space_id));
		}

		break;

	default:
		break;
	}

	page_t*		page;
	page_zip_des_t*	page_zip;
	dict_index_t*	index = nullptr;

#ifdef UNIV_DEBUG
	ulint		page_type;
#endif /* UNIV_DEBUG */

	if (block != nullptr) {

		/* Applying a page log record. */

		page = block->frame;
		page_zip = buf_block_get_page_zip(block);

		ut_d(page_type = fil_page_get_type(page));

	} else {

		/* Parsing a page log record. */
		page = nullptr;
		page_zip = nullptr;

		ut_d(page_type = FIL_PAGE_TYPE_ALLOCATED);
	}

	const byte*	old_ptr = ptr;

	switch (type) {
#ifdef UNIV_LOG_LSN_DEBUG
	case MLOG_LSN:
		/* The LSN is checked in recv_parse_log_rec(). */
		break;
#endif /* UNIV_LOG_LSN_DEBUG */
	case MLOG_4BYTES:

		ut_ad(page == nullptr || end_ptr > ptr + 2);

		/* Most FSP flags can only be changed by CREATE or ALTER with
		ALGORITHM=COPY, so they do not change once the file
		is created. The SDI flag is the only one that can be
		changed by a recoverable transaction. So if there is
		change in FSP flags, update the in-memory space structure
		(fil_space_t) */

		if (page != nullptr
		    && page_no == 0
		    && mach_read_from_2(ptr)
		    == FSP_HEADER_OFFSET + FSP_SPACE_FLAGS) {

			ptr = mlog_parse_nbytes(
				MLOG_4BYTES, ptr, end_ptr, page, page_zip);

			/* When applying log, we have complete records.
			They can be incomplete (ptr=nullptr) only during
			scanning (page==nullptr) */

			ut_ad(ptr != nullptr);

			fil_space_t*	space = fil_space_acquire(space_id);

			ut_ad(space != nullptr);

			fil_space_set_flags(
				space, mach_read_from_4(
					FSP_HEADER_OFFSET
					+ FSP_SPACE_FLAGS
					+ page));

			fil_space_release(space);

			break;
		}

		// fall through

	case MLOG_1BYTE:
	case MLOG_2BYTES:
	case MLOG_8BYTES:
#ifdef UNIV_DEBUG
		if (page
		    && page_type == FIL_PAGE_TYPE_ALLOCATED
		    && end_ptr >= ptr + 2) {

			/* It is OK to set FIL_PAGE_TYPE and certain
			list node fields on an empty page.  Any other
			write is not OK. */

			/* NOTE: There may be bogus assertion failures for
			dict_hdr_create(), trx_rseg_header_create(),
			trx_sys_create_doublewrite_buf(), and
			trx_sysf_create().
			These are only called during database creation. */

			ulint	offs = mach_read_from_2(ptr);

			switch (type) {
			default:
				ut_error;
			case MLOG_2BYTES:
				/* Note that this can fail when the
				redo log been written with something
				older than InnoDB Plugin 1.0.4. */
				ut_ad(offs == FIL_PAGE_TYPE
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + FIL_ADDR_SIZE
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_OFFSET
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + 0 /*FLST_PREV*/
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_BYTE
				      + FIL_ADDR_SIZE /*FLST_NEXT*/);
				break;
			case MLOG_4BYTES:
				/* Note that this can fail when the
				redo log been written with something
				older than InnoDB Plugin 1.0.4. */
				ut_ad(0
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_SPACE
				      || offs == IBUF_TREE_SEG_HEADER
				      + IBUF_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER/* flst_init */
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      || offs == PAGE_BTR_IBUF_FREE_LIST
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + FIL_ADDR_SIZE
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_SEG_LEAF
				      + PAGE_HEADER + FSEG_HDR_SPACE
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_PAGE_NO
				      || offs == PAGE_BTR_SEG_TOP
				      + PAGE_HEADER + FSEG_HDR_SPACE
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + 0 /*FLST_PREV*/
				      || offs == PAGE_BTR_IBUF_FREE_LIST_NODE
				      + PAGE_HEADER + FIL_ADDR_PAGE
				      + FIL_ADDR_SIZE /*FLST_NEXT*/);
				break;
			}
		}
#endif /* UNIV_DEBUG */

		ptr = mlog_parse_nbytes(type, ptr, end_ptr, page, page_zip);

		if (ptr != nullptr
		    && page != nullptr
		    && page_no == 0 && type == MLOG_4BYTES) {

			ulint	offs = mach_read_from_2(old_ptr);

			switch (offs) {
				fil_space_t*	space;
				uint32_t	val;
			default:
				break;

			case FSP_HEADER_OFFSET + FSP_SPACE_FLAGS:
			case FSP_HEADER_OFFSET + FSP_SIZE:
			case FSP_HEADER_OFFSET + FSP_FREE_LIMIT:
			case FSP_HEADER_OFFSET + FSP_FREE + FLST_LEN:

				space = fil_space_get(space_id);

				ut_a(space != nullptr);

				val = mach_read_from_4(page + offs);

				switch (offs) {
				case FSP_HEADER_OFFSET + FSP_SPACE_FLAGS:
					space->flags = val;
					break;

				case FSP_HEADER_OFFSET + FSP_SIZE:
					bool	success;
					space->size_in_header = val;
					success = fil_space_extend(space, val);
					if (!success) {
						ib::error()
						<< "Could not extend tablespace"
						<< ": " << space->id << " space"
						<< " name: " << space->name
						<< " to new size: " << val
						<< " pages during recovery.";
					}
					break;

				case FSP_HEADER_OFFSET + FSP_FREE_LIMIT:
					space->free_limit = val;
					break;

				case FSP_HEADER_OFFSET + FSP_FREE + FLST_LEN:
					space->free_len = val;
					ut_ad(val == flst_get_len(page + offs));
					break;
				}
			}
		}
		break;

	case MLOG_REC_INSERT:
	case MLOG_COMP_REC_INSERT:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_INSERT,
				     &index))) {
			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = page_cur_parse_insert_rec(
				FALSE, ptr, end_ptr, block, index, mtr);
		}

		break;

	case MLOG_REC_CLUST_DELETE_MARK:
	case MLOG_COMP_REC_CLUST_DELETE_MARK:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_CLUST_DELETE_MARK,
				     &index))) {

			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = btr_cur_parse_del_mark_set_clust_rec(
				ptr, end_ptr, page, page_zip, index);
		}

		break;

	case MLOG_COMP_REC_SEC_DELETE_MARK:

		ut_ad(!page || fil_page_type_is_index(page_type));

		/* This log record type is obsolete, but we process it for
		backward compatibility with MySQL 5.0.3 and 5.0.4. */

		ut_a(!page || page_is_comp(page));
		ut_a(!page_zip);

		ptr = mlog_parse_index(ptr, end_ptr, true, &index);

		if (ptr == nullptr) {
			break;
		}

		/* Fall through */

	case MLOG_REC_SEC_DELETE_MARK:

		ut_ad(!page || fil_page_type_is_index(page_type));

		ptr = btr_cur_parse_del_mark_set_sec_rec(
			ptr, end_ptr, page, page_zip);
		break;

	case MLOG_REC_UPDATE_IN_PLACE:
	case MLOG_COMP_REC_UPDATE_IN_PLACE:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_UPDATE_IN_PLACE,
				     &index))) {
			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = btr_cur_parse_update_in_place(
				ptr, end_ptr, page, page_zip, index);
		}

		break;

	case MLOG_LIST_END_DELETE:
	case MLOG_COMP_LIST_END_DELETE:
	case MLOG_LIST_START_DELETE:
	case MLOG_COMP_LIST_START_DELETE:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_LIST_END_DELETE
				     || type == MLOG_COMP_LIST_START_DELETE,
				     &index))) {
			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = page_parse_delete_rec_list(
				type, ptr, end_ptr, block, index, mtr);
		}

		break;

	case MLOG_LIST_END_COPY_CREATED: case MLOG_COMP_LIST_END_COPY_CREATED:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_LIST_END_COPY_CREATED,
				     &index))) {

			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = page_parse_copy_rec_list_to_created_page(
				ptr, end_ptr, block, index, mtr);
		}

		break;

	case MLOG_PAGE_REORGANIZE:
	case MLOG_COMP_PAGE_REORGANIZE:
	case MLOG_ZIP_PAGE_REORGANIZE:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type != MLOG_PAGE_REORGANIZE,
				     &index))) {
			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = btr_parse_page_reorganize(
				ptr, end_ptr, index,
				type == MLOG_ZIP_PAGE_REORGANIZE,
				block, mtr);
		}

		break;

	case MLOG_PAGE_CREATE:
	case MLOG_COMP_PAGE_CREATE:

		/* Allow anything in page_type when creating a page. */
		ut_a(!page_zip);

		page_parse_create(
			block, type == MLOG_COMP_PAGE_CREATE,
			FIL_PAGE_INDEX);

		break;

	case MLOG_PAGE_CREATE_RTREE:
	case MLOG_COMP_PAGE_CREATE_RTREE:

		page_parse_create(
			block, type == MLOG_COMP_PAGE_CREATE_RTREE,
			FIL_PAGE_RTREE);

		break;

	case MLOG_PAGE_CREATE_SDI:
	case MLOG_COMP_PAGE_CREATE_SDI:

		page_parse_create(
			block, type == MLOG_COMP_PAGE_CREATE_SDI,
			FIL_PAGE_SDI);

		break;

	case MLOG_UNDO_INSERT:

		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

		ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);

		break;

	case MLOG_UNDO_ERASE_END:

		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

		ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);

		break;

	case MLOG_UNDO_INIT:

		/* Allow anything in page_type when creating a page. */

		ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);

		break;
	case MLOG_UNDO_HDR_CREATE:
	case MLOG_UNDO_HDR_REUSE:

		ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

		ptr = trx_undo_parse_page_header(
			type, ptr, end_ptr, page, mtr);

		break;

	case MLOG_REC_MIN_MARK:
	case MLOG_COMP_REC_MIN_MARK:

		ut_ad(!page || fil_page_type_is_index(page_type));

		/* On a compressed page, MLOG_COMP_REC_MIN_MARK
		will be followed by MLOG_COMP_REC_DELETE
		or MLOG_ZIP_WRITE_HEADER(FIL_PAGE_PREV, FIL_nullptr)
		in the same mini-transaction. */

		ut_a(type == MLOG_COMP_REC_MIN_MARK || !page_zip);

		ptr = btr_parse_set_min_rec_mark(
			ptr, end_ptr, type == MLOG_COMP_REC_MIN_MARK,
			page, mtr);

		break;

	case MLOG_REC_DELETE:
	case MLOG_COMP_REC_DELETE:

		ut_ad(!page || fil_page_type_is_index(page_type));

		if (nullptr != (ptr = mlog_parse_index(
				     ptr, end_ptr,
				     type == MLOG_COMP_REC_DELETE,
				     &index))) {
			ut_a(!page
			     || (ibool)!!page_is_comp(page)
			     == dict_table_is_comp(index->table));

			ptr = page_cur_parse_delete_rec(
				ptr, end_ptr, block, index, mtr);
		}

		break;

	case MLOG_IBUF_BITMAP_INIT:

		/* Allow anything in page_type when creating a page. */

		ptr = ibuf_parse_bitmap_init(ptr, end_ptr, block, mtr);

		break;

	case MLOG_INIT_FILE_PAGE:
	case MLOG_INIT_FILE_PAGE2:

		/* Allow anything in page_type when creating a page. */

		ptr = fsp_parse_init_file_page(ptr, end_ptr, block);

		break;

	case MLOG_WRITE_STRING:

		ut_ad(!page || page_type != FIL_PAGE_TYPE_ALLOCATED
		      || page_no == 0);

		ptr = mlog_parse_string(ptr, end_ptr, page, page_zip);

		break;

	case MLOG_ZIP_WRITE_NODE_PTR:

		ut_ad(!page || fil_page_type_is_index(page_type));

		ptr = page_zip_parse_write_node_ptr(
			ptr, end_ptr, page, page_zip);

		break;

	case MLOG_ZIP_WRITE_BLOB_PTR:

		ut_ad(!page || fil_page_type_is_index(page_type));

		ptr = page_zip_parse_write_blob_ptr(
			ptr, end_ptr, page, page_zip);

		break;

	case MLOG_ZIP_WRITE_HEADER:

		ut_ad(!page || fil_page_type_is_index(page_type));

		ptr = page_zip_parse_write_header(
			ptr, end_ptr, page, page_zip);

		break;

	case MLOG_ZIP_PAGE_COMPRESS:

		/* Allow anything in page_type when creating a page. */
		ptr = page_zip_parse_compress(ptr, end_ptr, page, page_zip);
		break;

	case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:

		if (nullptr != (ptr = mlog_parse_index(
				ptr, end_ptr, true, &index))) {

			ut_a(!page || ((ibool)!!page_is_comp(page)
				== dict_table_is_comp(index->table)));

			ptr = page_zip_parse_compress_no_data(
				ptr, end_ptr, page, page_zip, index);
		}

		break;

	default:
		ptr = nullptr;
		recv_sys->found_corrupt_log = true;
	}

	if (index != nullptr) {

		dict_table_t*	table = index->table;

		dict_mem_index_free(index);
		dict_mem_table_free(table);
	}

	return(ptr);
}

/** Get the page map for a tablespace. It will create one if one isn't found.
@param[in]	space_id	Tablespace ID for which page map required.
@param[in]	create		false if lookup only
@return the space data or null if not found */
static
recv_sys_t::Space*
recv_get_page_map(space_id_t space_id, bool create)
{
	auto	it = recv_sys->spaces->find(space_id);

	if (it != recv_sys->spaces->end()) {

		return(&it->second);

	} else if (create) {

		mem_heap_t*	heap;

		heap = mem_heap_create_typed(256, MEM_HEAP_FOR_RECV_SYS);

		using Space = recv_sys_t::Space;
		using value_type = recv_sys_t::Spaces::value_type;

		auto	where = recv_sys->spaces->insert(
			it, value_type(space_id, Space(heap)));

		return(&where->second);
	}

	return(nullptr);
}

/** Gets the list of log records for a <space, page>.
@param[in]	space_id	Tablespace ID
@param[in]	page_no		Page number
@return the redo log entries or nullptr if not found */
static
recv_addr_t*
recv_get_rec(
	space_id_t	space_id,
	page_no_t	page_no)
{
	recv_sys_t::Space*	space;

	space = recv_get_page_map(space_id, false);

	if (space != nullptr) {

		auto	it = space->m_pages.find(page_no);

		if (it != space->m_pages.end()) {

			return(it->second);
		}
	}

	return(nullptr);
}

/** Adds a new log record to the hash table of log records.
@param[in]	type		log record type
@param[in]	space_id	Tablespace id
@param[in]	page_no		page number
@param[in]	body		log record body
@param[in]	rec_end		log record end
@param[in]	start_lsn	start lsn of the mtr
@param[in]	end_lsn		end lsn of the mtr */
static
void
recv_add_to_hash_table(
	mlog_id_t	type,
	space_id_t	space_id,
	page_no_t	page_no,
	byte*		body,
	byte*		rec_end,
	lsn_t		start_lsn,
	lsn_t		end_lsn)
{
	ut_ad(type != MLOG_FILE_OPEN);
	ut_ad(type != MLOG_FILE_DELETE);
	ut_ad(type != MLOG_FILE_CREATE2);
	ut_ad(type != MLOG_FILE_RENAME2);
	ut_ad(type != MLOG_DUMMY_RECORD);
	ut_ad(type != MLOG_INDEX_LOAD);

	recv_sys_t::Space*	space;

	space = recv_get_page_map(space_id, true);

	recv_t*	recv;

	recv = static_cast<recv_t*>(
		mem_heap_alloc(space->m_heap, sizeof(*recv)));

	recv->type = type;
	recv->end_lsn = end_lsn;
	recv->len = rec_end - body;
	recv->start_lsn = start_lsn;

	auto	it = space->m_pages.find(page_no);

	recv_addr_t*	recv_addr;

	if (it != space->m_pages.end()) {

		recv_addr = it->second;

	} else {

		recv_addr = static_cast<recv_addr_t*>(
			mem_heap_alloc(space->m_heap, sizeof(*recv_addr)));

		recv_addr->space = space_id;
		recv_addr->page_no = page_no;
		recv_addr->state = RECV_NOT_PROCESSED;

		UT_LIST_INIT(recv_addr->rec_list, &recv_t::rec_list);

		using value_type = recv_sys_t::Pages::value_type;

		space->m_pages.insert(it, value_type(page_no, recv_addr));

		++recv_sys->n_addrs;
	}

	UT_LIST_ADD_LAST(recv_addr->rec_list, recv);

	recv_data_t**	prev_field;

	prev_field = &recv->data;

	/* Store the log record body in chunks of less than UNIV_PAGE_SIZE:
	the heap grows into the buffer pool, and bigger chunks could not
	be allocated */

	while (rec_end > body) {

		ulint	len = rec_end - body;

		if (len > RECV_DATA_BLOCK_SIZE) {
			len = RECV_DATA_BLOCK_SIZE;
		}

		recv_data_t*	recv_data;

		recv_data = static_cast<recv_data_t*>(
			mem_heap_alloc(
				space->m_heap, sizeof(*recv_data) + len));

		*prev_field = recv_data;

		memcpy(recv_data + 1, body, len);

		prev_field = &recv_data->next;

		body += len;
	}

	*prev_field = nullptr;
}

/** Copies the log record body from recv to buf.
@param[in]	buf		Buffer of length at least recv->len
@param[in]	recv		Log record */
static
void
recv_data_copy_to_buf(byte* buf, recv_t* recv)
{
	ulint		len = recv->len;
	recv_data_t*	recv_data = recv->data;

	while (len > 0) {

		ulint	part_len;

		if (len > RECV_DATA_BLOCK_SIZE) {
			part_len = RECV_DATA_BLOCK_SIZE;
		} else {
			part_len = len;
		}

		memcpy(buf, ((byte*) recv_data) + sizeof(*recv_data), part_len);

		buf += part_len;
		len -= part_len;

		recv_data = recv_data->next;
	}
}

/** Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.
@param[in]	just_read_in	true if the IO handler calls this for a freshly
				read page
@param[in,out]	block		Buffer block */
void
recv_recover_page_func(
#ifndef UNIV_HOTBACKUP
	bool		just_read_in,
#endif /* !UNIV_HOTBACKUP */
	buf_block_t*	block)
{
	mutex_enter(&recv_sys->mutex);

	if (recv_sys->apply_log_recs == false) {

		/* Log records should not be applied now */

		mutex_exit(&recv_sys->mutex);

		return;
	}

	recv_addr_t*	recv_addr;

	recv_addr = recv_get_rec(
		block->page.id.space(), block->page.id.page_no());

	if (recv_addr == nullptr
	    || recv_addr->state == RECV_BEING_PROCESSED
	    || recv_addr->state == RECV_PROCESSED) {

		ut_ad(recv_addr == nullptr || recv_needed_recovery);

		mutex_exit(&recv_sys->mutex);

		return;
	}

	ut_ad(recv_needed_recovery);

	DBUG_PRINT("ib_log",
		   ("Applying log to page %u:%u",
		    recv_addr->space, recv_addr->page_no));

#ifdef UNIV_DEBUG
	lsn_t		max_lsn;

	ut_d(max_lsn = UT_LIST_GET_FIRST(log_sys->log_groups)->scanned_lsn);
#endif /* UNIV_DEBUG */

	recv_addr->state = RECV_BEING_PROCESSED;

	mutex_exit(&recv_sys->mutex);

	mtr_t	mtr;

	mtr_start(&mtr);

	mtr_set_log_mode(&mtr, MTR_LOG_NONE);

	page_t*	page = block->frame;

	page_zip_des_t*	page_zip = buf_block_get_page_zip(block);

#ifndef UNIV_HOTBACKUP
	if (just_read_in) {
		/* Move the ownership of the x-latch on the page to
		this OS thread, so that we can acquire a second
		x-latch on it.  This is needed for the operations to
		the page to pass the debug checks. */

		rw_lock_x_lock_move_ownership(&block->lock);
	}

	bool	success = buf_page_get_known_nowait(
		RW_X_LATCH, block, BUF_KEEP_OLD,
		__FILE__, __LINE__, &mtr);
	ut_a(success);

	buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);
#endif /* !UNIV_HOTBACKUP */

	/* Read the newest modification lsn from the page */
	lsn_t	page_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

#ifndef UNIV_HOTBACKUP

	/* It may be that the page has been modified in the buffer
	pool: read the newest modification LSN there */

	lsn_t	page_newest_lsn;

	page_newest_lsn = buf_page_get_newest_modification(&block->page);

	if (page_newest_lsn) {

		page_lsn = page_newest_lsn;
	}
#else /* !UNIV_HOTBACKUP */
	/* In recovery from a backup we do not really use the buffer pool */
	lsn_t	page_newest_lsn = 0;
#endif /* !UNIV_HOTBACKUP */

	lsn_t	end_lsn = 0;
	lsn_t	start_lsn = 0;
	bool	modification_to_page = false;

	for (auto recv = UT_LIST_GET_FIRST(recv_addr->rec_list);
	     recv != nullptr;
	     recv = UT_LIST_GET_NEXT(rec_list, recv)) {

		end_lsn = recv->end_lsn;

		ut_ad(end_lsn <= max_lsn);

		byte*	buf;

		if (recv->len > RECV_DATA_BLOCK_SIZE) {

			/* We have to copy the record body to a separate
			buffer */

			buf = static_cast<byte*>(ut_malloc_nokey(recv->len));

			recv_data_copy_to_buf(buf, recv);
		} else {
			buf = ((byte*)(recv->data)) + sizeof(recv_data_t);
		}

		if (recv->type == MLOG_INIT_FILE_PAGE) {

			page_lsn = page_newest_lsn;

			memset(FIL_PAGE_LSN + page, 0, 8);
			memset(UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM
			       + page, 0, 8);

			if (page_zip) {
				memset(FIL_PAGE_LSN + page_zip->data, 0, 8);
			}
		}

		/* Ignore applying the redo logs for tablespace that is
		truncated. Truncated tablespaces are handled explicitly
		post-recovery, where we will restore the tablespace back
		to a normal state.

		Applying redo at this stage will cause problems because the
		redo will have action recorded on page before tablespace
		was re-inited and that would lead to a problem later. */

		if (recv->start_lsn >= page_lsn
		    && undo::is_active(recv_addr->space)) {

			lsn_t	end_lsn;

			if (!modification_to_page) {

				modification_to_page = true;
				start_lsn = recv->start_lsn;
			}

			DBUG_PRINT("ib_log",
				   ("apply " LSN_PF ":"
				    " %s len " ULINTPF " page %u:%u",
				    recv->start_lsn,
				    get_mlog_string(recv->type), recv->len,
				    recv_addr->space,
				    recv_addr->page_no));

			recv_parse_or_apply_log_rec_body(
				recv->type, buf, buf + recv->len,
				recv_addr->space, recv_addr->page_no,
				block, &mtr, ULINT_UNDEFINED);

			end_lsn = recv->start_lsn + recv->len;

			mach_write_to_8(FIL_PAGE_LSN + page, end_lsn);

			mach_write_to_8(UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM
					+ page, end_lsn);

			if (page_zip) {

				mach_write_to_8(FIL_PAGE_LSN
						+ page_zip->data, end_lsn);
			}
		}

		if (recv->len > RECV_DATA_BLOCK_SIZE) {
			ut_free(buf);
		}
	}

#ifdef UNIV_ZIP_DEBUG
	if (fil_page_index_page_check(page)) {
		page_zip_des_t*	page_zip = buf_block_get_page_zip(block);

		ut_a(!page_zip
		     || page_zip_validate_low(page_zip, page, nullptr, FALSE));
	}
#endif /* UNIV_ZIP_DEBUG */

#ifndef UNIV_HOTBACKUP
	if (modification_to_page) {
		ut_a(block);

		log_flush_order_mutex_enter();
		buf_flush_recv_note_modification(block, start_lsn, end_lsn);
		log_flush_order_mutex_exit();
	}
#endif /* !UNIV_HOTBACKUP */

	/* Make sure that committing mtr does not change the modification
	LSN values of page */

	mtr.discard_modifications();

	mtr_commit(&mtr);

	mutex_enter(&recv_sys->mutex);

	if (recv_max_page_lsn < page_lsn) {
		recv_max_page_lsn = page_lsn;
	}

	recv_addr->state = RECV_PROCESSED;

	ut_a(recv_sys->n_addrs > 0);
	--recv_sys->n_addrs;

	mutex_exit(&recv_sys->mutex);
}

#ifndef UNIV_HOTBACKUP
/** Reads in pages which have hashed log records, from an area around a given
page number.
@param[in]	page_id		Read the pages around this page number
@return number of pages found */
static
ulint
recv_read_in_area(const page_id_t& page_id)
{
	page_no_t	low_limit;

	low_limit = page_id.page_no()
		- (page_id.page_no() % RECV_READ_AHEAD_AREA);

	ulint	n = 0;

	std::array<page_no_t, RECV_READ_AHEAD_AREA>	page_nos;

	for (page_no_t page_no = low_limit;
	     page_no < low_limit + RECV_READ_AHEAD_AREA;
	     ++page_no) {

		recv_addr_t*	recv_addr;

		recv_addr = recv_get_rec(page_id.space(), page_no);

		const page_id_t	cur_page_id(page_id.space(), page_no);

		if (recv_addr != nullptr && !buf_page_peek(cur_page_id)) {

			mutex_enter(&recv_sys->mutex);

			if (recv_addr->state == RECV_NOT_PROCESSED) {

				recv_addr->state = RECV_BEING_READ;

				page_nos[n] = page_no;

				++n;
			}

			mutex_exit(&recv_sys->mutex);
		}
	}

	buf_read_recv_pages(false, page_id.space(), &page_nos[0], n);

	return(n);
}

/** Apply the log records to a page
@param[in,out]	recv_addr	Redo log records to apply */
static
void
recv_apply_log_rec(recv_addr_t* recv_addr)
{
	if (recv_addr->state == RECV_DISCARDED) {
		ut_a(recv_sys->n_addrs);
		--recv_sys->n_addrs;
		return;
	}

	bool			found;
	const page_id_t		page_id(recv_addr->space, recv_addr->page_no);

	const page_size_t	page_size =
			fil_space_get_page_size(recv_addr->space, &found);

	if (!found
	    || recv_sys->missing_ids.find(recv_addr->space)
	    != recv_sys->missing_ids.end()) {

		/* Tablespace was discarded or dropped after changes were
		made to it. Or, we have ignored redo log for this tablespace
		earlier and somehow it has been found now. We can't apply
		this redo log out of order. */

		recv_addr->state = RECV_PROCESSED;

		ut_a(recv_sys->n_addrs > 0);
		--recv_sys->n_addrs;

		/* If the tablespace has been explicitly deleted, we
		can safely ignore it. */

		if (recv_sys->deleted.find(recv_addr->space)
		    == recv_sys->deleted.end()) {

			recv_sys->missing_ids.insert(recv_addr->space);
		}

	} else if (recv_addr->state == RECV_NOT_PROCESSED) {

		mutex_exit(&recv_sys->mutex);

		if (buf_page_peek(page_id)) {

			mtr_t	mtr;

			mtr_start(&mtr);

			buf_block_t*	block;

			block = buf_page_get(
				page_id, page_size, RW_X_LATCH, &mtr);

			buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

			recv_recover_page(false, block);

			mtr_commit(&mtr);

		} else {
			recv_read_in_area(page_id);
		}

		mutex_enter(&recv_sys->mutex);
	}
}

/** Empties the hash table of stored log records, applying them to appropriate
pages.
@param[in]	allow_ibuf	if true, ibuf operations are allowed during
				the application; if false, no ibuf operations
				are allowed, and after the application all
				file pages are flushed to disk and invalidated
				in buffer pool: this alternative means that
				no new log records can be generated during
				the application; the caller must in this case
				own the log mutex */
void
recv_apply_hashed_log_recs(bool allow_ibuf)
{
	for (;;) {

		mutex_enter(&recv_sys->mutex);

		if (!recv_sys->apply_batch_on) {

			break;
		}

		mutex_exit(&recv_sys->mutex);

		os_thread_sleep(500000);
	}

	ut_ad(!allow_ibuf == log_mutex_own());

	if (!allow_ibuf) {
		recv_no_ibuf_operations = true;
	}

	recv_sys->apply_log_recs = true;
	recv_sys->apply_batch_on = true;

	auto	batch_size = recv_sys->n_addrs;

	ib::info()
		<< "Applying a batch of "
		<< batch_size
		<< " redo log records ...";

	static const size_t	PCT = 10;

	size_t	pct = PCT;
	size_t	applied = 0;
	auto	unit = batch_size / PCT;

	if (unit <= PCT) {
		pct = 100;
		unit = batch_size;
	}

	for (const auto& space : *recv_sys->spaces) {

		fil_tablespace_open_for_recovery(space.first);

		for (auto pages : space.second.m_pages) {

			ut_ad(pages.second->space == space.first);

			recv_apply_log_rec(pages.second);

			++applied;

			if (unit == 0 || (applied % unit) == 0) {
				ib::info() << pct << "%";
				pct += PCT;
			}
		}
	}

	/* Wait until all the pages have been processed */

	while (recv_sys->n_addrs != 0) {

		mutex_exit(&recv_sys->mutex);

		os_thread_sleep(500000);

		mutex_enter(&recv_sys->mutex);
	}

	if (!allow_ibuf) {

		/* Flush all the file pages to disk and invalidate them in
		the buffer pool */

		ut_d(log_sys->disable_redo_writes = true);

		mutex_exit(&recv_sys->mutex);

		log_mutex_exit();

		/* Stop the recv_writer thread from issuing any LRU
		flush batches. */
		mutex_enter(&recv_sys->writer_mutex);

		/* Wait for any currently run batch to end. */
		buf_flush_wait_LRU_batch_end();

		os_event_reset(recv_sys->flush_end);

		recv_sys->flush_type = BUF_FLUSH_LIST;

		os_event_set(recv_sys->flush_start);

		os_event_wait(recv_sys->flush_end);

		buf_pool_invalidate();

		/* Allow batches from recv_writer thread. */
		mutex_exit(&recv_sys->writer_mutex);

		log_mutex_enter();

		ut_d(log_sys->disable_redo_writes = false);

		mutex_enter(&recv_sys->mutex);

		recv_no_ibuf_operations = false;
	}

	recv_sys->apply_log_recs = false;
	recv_sys->apply_batch_on = false;

	recv_sys_empty_hash();

	mutex_exit(&recv_sys->mutex);

	ib::info() << "Apply batch completed!";
}

#else /* !UNIV_HOTBACKUP */
static
void
recv_apply_log_rec_for_backup(recv_addr_t* recv_addr)
{
	buf_block_t*	block = back_block1;

	bool	found;

	const page_size_t&	page_size =
		fil_space_get_page_size(recv_addr->space, &found);

	if (!found) {

		recv_addr->state = RECV_DISCARDED;

		ut_a(recv_sys->n_addrs);
		--recv_sys->n_addrs;

		return;
	}

	/* We simulate a page read made by the buffer pool, to
	make sure the recovery apparatus works ok. We must init
	the block. */

	buf_page_init_for_backup_restore(
		page_id_t(recv_addr->space, recv_addr->page_no),
		page_size, block);

	/* Extend the tablespace's last file if the page_no
	does not fall inside its bounds; we assume the last
	file is auto-extending, and mysqlbackup copied the file
	when it still was smaller */

	fil_space_t*	space = fil_space_get(recv_addr->space);

	bool	success;

	success = fil_space_extend(space, recv_addr->page_no + 1);

	if (!success) {
		ib::fatal()
			<< "Cannot extend tablespace "
			<< recv_addr->space << " to hold "
			<< recv_addr->page_no << " pages";
	}

	/* Read the page from the tablespace file using the
	fil0fil.cc routines */

	const page_id_t	page_id(recv_addr->space, recv_addr->page_no);

	dberr_t	err;

	if (page_size.is_compressed()) {

		err = fil_io(
			IORequestRead, true,
			page_id,
			page_size, 0, page_size.physical(),
			block->page.zip.data, NULL);

		if (err == DB_SUCCESS && !buf_zip_decompress(block, TRUE)) {

			ut_error;
		}
	} else {

		err = fil_io(
			IORequestRead, true,
			page_id, page_size, 0,
			page_size.logical(),
			block->frame, NULL);
	}

	if (err != DB_SUCCESS) {

		ib::fatal()
			<< "Cannot read from tablespace "
			<< recv_addr->space << " page number "
			<< recv_addr->page_no;
	}

	/* Apply the log records to this page */
	recv_recover_page(false, block);

	/* Write the page back to the tablespace file using the
	fil0fil.cc routines */

	buf_flush_init_for_writing(
		block->frame, buf_block_get_page_zip(block),
		mach_read_from_8(block->frame + FIL_PAGE_LSN),
		fsp_is_checksum_disabled(block->page.id.space()));

	if (page_size.is_compressed()) {

		error = fil_io(
			IORequestWrite, true, page_id,
			page_size, 0, page_size.physical(),
			block->page.zip.data, NULL);
	} else {
		error = fil_io(
			IORequestWrite, true, page_id,
			page_size, 0, page_size.logical(),
			block->frame, NULL);
	}
}

/** Applies log records in the hash table to a backup. */
void
recv_apply_log_recs_for_backup()
{
	recv_sys->apply_log_recs = true;
	recv_sys->apply_batch_on = true;

	ib::info()
		<< "Starting an apply batch of log records to the database...";

	for (auto space : recv_sys->spaces) {

		for (auto page : *space) {

			recv_apply_log_rec_for_backup(page.second);
		}
	}

	recv_sys_empty_hash();
}

#endif /* !UNIV_HOTBACKUP */

/** Tries to parse a single log record.
@param[out]	type		log record type
@param[in]	ptr		pointer to a buffer
@param[in]	end_ptr		end of the buffer
@param[out]	space_id	tablespace identifier
@param[out]	page_no		page number
@param[out]	body		start of log record body
@return length of the record, or 0 if the record was not complete */
static
ulint
recv_parse_log_rec(
	mlog_id_t*	type,
	byte*		ptr,
	byte*		end_ptr,
	space_id_t*	space_id,
	page_no_t*	page_no,
	byte**		body)
{
	byte*	new_ptr;

	*body = nullptr;

	UNIV_MEM_INVALID(type, sizeof *type);
	UNIV_MEM_INVALID(space_id, sizeof *space_id);
	UNIV_MEM_INVALID(page_no, sizeof *page_no);
	UNIV_MEM_INVALID(body, sizeof *body);

	if (ptr == end_ptr) {

		return(0);
	}

	switch (*ptr) {

#ifdef UNIV_LOG_LSN_DEBUG
	case MLOG_LSN | MLOG_SINGLE_REC_FLAG:
	case MLOG_LSN:

		new_ptr = mlog_parse_initial_log_record(
			ptr, end_ptr, type, space_id, page_no);

		if (new_ptr != nullptr) {

			const lsn_t	lsn = static_cast<lsn_t>(
				*space_id) << 32 | *page_no;

			ut_a(lsn == recv_sys->recovered_lsn);
		}

		*type = MLOG_LSN;
		return(new_ptr - ptr);
#endif /* UNIV_LOG_LSN_DEBUG */

	case MLOG_MULTI_REC_END:
	case MLOG_DUMMY_RECORD:
		*page_no = FIL_NULL;
		*space_id = SPACE_UNKNOWN;
		*type = static_cast<mlog_id_t>(*ptr);
		return(1);

	case MLOG_MULTI_REC_END | MLOG_SINGLE_REC_FLAG:
	case MLOG_DUMMY_RECORD | MLOG_SINGLE_REC_FLAG:
		recv_sys->found_corrupt_log = true;
		return(0);

	case MLOG_TABLE_DYNAMIC_META:
	case MLOG_TABLE_DYNAMIC_META | MLOG_SINGLE_REC_FLAG:

		table_id_t	id;
		uint64		version;

		*page_no = FIL_NULL;
		*space_id = SPACE_UNKNOWN;

		new_ptr = mlog_parse_initial_dict_log_record(
			ptr, end_ptr, type, &id, &version);

		if (new_ptr != nullptr) {
			new_ptr = recv_sys->metadata_recover->parseMetadataLog(
				id, version, new_ptr, end_ptr);
		}

		return(new_ptr == nullptr ? 0 : new_ptr - ptr);
	}

	new_ptr = mlog_parse_initial_log_record(
		ptr, end_ptr, type, space_id, page_no);

	*body = new_ptr;

	if (new_ptr == nullptr) {

		return(0);
	}

	new_ptr = recv_parse_or_apply_log_rec_body(
		*type, new_ptr, end_ptr, *space_id, *page_no,
		nullptr, nullptr, new_ptr - ptr);

	if (new_ptr == nullptr) {

		return(0);
	}

	return(new_ptr - ptr);
}

/** Parse and store a single log record entry.
@param[in]	ptr		Start of buffer
@param[in]	end_ptr		End of buffer
@return true if end of processing */
static
bool
recv_single_rec(
	byte*		ptr,
	byte*		end_ptr)
{
	ut_ad(log_mutex_own());

	/* The mtr did not modify multiple pages */

	lsn_t	old_lsn = recv_sys->recovered_lsn;

	/* Try to parse a log record, fetching its type, space id,
	page no, and a pointer to the body of the log record */

	byte*		body;
	mlog_id_t	type;
	page_no_t	page_no;
	space_id_t	space_id;

	ulint	len = recv_parse_log_rec(
		&type, ptr, end_ptr, &space_id, &page_no, &body);

	if (recv_sys->found_corrupt_log) {

		recv_report_corrupt_log( ptr, type, space_id, page_no);

	} else if (len == 0 || recv_sys->found_corrupt_fs) {

		return(true);
	}

	lsn_t	new_recovered_lsn;

	new_recovered_lsn = recv_calc_lsn_on_data_add(old_lsn, len);

	if (new_recovered_lsn > recv_sys->scanned_lsn) {

		/* The log record filled a log block, and we
		require that also the next log block should
		have been scanned in */

		return(true);
	}

	recv_previous_parsed_rec_type = type;
	recv_previous_parsed_rec_is_multi = 0;
	recv_previous_parsed_rec_offset = recv_sys->recovered_offset;

	recv_sys->recovered_offset += len;
	recv_sys->recovered_lsn = new_recovered_lsn;

	switch (type) {
	case MLOG_DUMMY_RECORD:
		/* Do nothing */
		break;

#ifdef UNIV_LOG_LSN_DEBUG
	case MLOG_LSN:
		/* Do not add these records to the hash table.
		The page number and space id fields are misused
		for something else. */
		break;
#endif /* UNIV_LOG_LSN_DEBUG */

	default:

		if (space_id == TRX_SYS_SPACE
		    || fil_tablespace_lookup_for_recovery(space_id)) {

			recv_add_to_hash_table(
				type, space_id, page_no, body,
				ptr + len, old_lsn, recv_sys->recovered_lsn);

		} else {

			recv_sys->missing_ids.insert(space_id);
		}

		/* fall through */

	case MLOG_INDEX_LOAD:
	case MLOG_FILE_OPEN:
	case MLOG_FILE_DELETE:
	case MLOG_FILE_RENAME2:
	case MLOG_FILE_CREATE2:
	case MLOG_TABLE_DYNAMIC_META:

		/* These were already handled by
		recv_parse_log_rec() and
		recv_parse_or_apply_log_rec_body(). */

		DBUG_PRINT("ib_log",
			   ("scan " LSN_PF ": log rec %s"
			    " len " ULINTPF " " PAGE_ID_PF,
			    old_lsn, get_mlog_string(type),
			    len, space_id, page_no));
		break;
	}

	return(false);
}

/** Parse and store a multiple record log entry.
@param[in]	ptr		Start of buffer
@param[in]	end_ptr		End of buffer
@return true if end of processing */
static
bool
recv_multi_rec(byte* ptr, byte*	end_ptr)
{
	ut_ad(log_mutex_own());

	/* Check that all the records associated with the single mtr
	are included within the buffer */

	ulint	n_recs = 0;
	ulint	total_len = 0;

	for (;;) {

		mlog_id_t	type;
		byte*		body;
		page_no_t	page_no;
		space_id_t	space_id;

		ulint	len = recv_parse_log_rec(
			&type, ptr, end_ptr, &space_id, &page_no, &body);

		if (recv_sys->found_corrupt_log) {

			recv_report_corrupt_log(ptr, type, space_id, page_no);

			return(true);

		} else if (len == 0) {

			return(true);

		} else if ((*ptr & MLOG_SINGLE_REC_FLAG)) {

			recv_sys->found_corrupt_log = true;

			recv_report_corrupt_log(ptr, type, space_id, page_no);

			return(true);

		} else if (recv_sys->found_corrupt_fs) {

			return(true);
		}

		recv_previous_parsed_rec_type = type;

		recv_previous_parsed_rec_offset
			= recv_sys->recovered_offset + total_len;

		recv_previous_parsed_rec_is_multi = 1;

		total_len += len;
		++n_recs;

		ptr += len;

		if (type == MLOG_MULTI_REC_END) {

			DBUG_PRINT("ib_log",
				("scan " LSN_PF
				 ": multi-log end total_len " ULINTPF
				 " n=" ULINTPF,
				 recv_sys->recovered_lsn,
				 total_len, n_recs));

			break;
		}

		DBUG_PRINT("ib_log",
			   ("scan " LSN_PF ": multi-log rec %s len "
			    ULINTPF " " PAGE_ID_PF,
			    recv_sys->recovered_lsn,
			    get_mlog_string(type),
			    len, space_id, page_no));
	}

	lsn_t	new_recovered_lsn = recv_calc_lsn_on_data_add(
		recv_sys->recovered_lsn, total_len);

	if (new_recovered_lsn > recv_sys->scanned_lsn) {

		/* The log record filled a log block, and we require
		that also the next log block should have been scanned in */

		return(true);
	}

	/* Add all the records to the hash table */

	ptr = recv_sys->buf + recv_sys->recovered_offset;

	for (;;) {

		lsn_t	old_lsn = recv_sys->recovered_lsn;

		/* This will apply MLOG_FILE_ records. */

		mlog_id_t	type;
		byte*		body;
		page_no_t	page_no;
		space_id_t	space_id;

		ulint	len = recv_parse_log_rec(
			&type, ptr, end_ptr, &space_id, &page_no, &body);

		if (recv_sys->found_corrupt_log
		    && !recv_report_corrupt_log(
			    ptr, type, space_id, page_no)) {

			return(true);

		} else if (recv_sys->found_corrupt_fs) {

			return(true);
		}

		ut_a(len != 0);
		ut_a(!(*ptr & MLOG_SINGLE_REC_FLAG));

		recv_sys->recovered_offset += len;

		recv_sys->recovered_lsn
			= recv_calc_lsn_on_data_add(old_lsn, len);

		switch (type) {
		case MLOG_MULTI_REC_END:

			/* Found the end mark for the records */
			return(false);

#ifdef UNIV_LOG_LSN_DEBUG
		case MLOG_LSN:
			/* Do not add these records to the hash table.
			The page number and space id fields are misused
			for something else. */
			break;
#endif /* UNIV_LOG_LSN_DEBUG */

		case MLOG_FILE_OPEN:
		case MLOG_FILE_DELETE:
		case MLOG_FILE_CREATE2:
		case MLOG_FILE_RENAME2:
		case MLOG_TABLE_DYNAMIC_META:
			/* case MLOG_TRUNCATE: Disabled for WL6378 */
			/* These were already handled by
			recv_parse_or_apply_log_rec_body(). */
			break;

		default:

			if (space_id == TRX_SYS_SPACE
			    || fil_tablespace_lookup_for_recovery(space_id)) {

				recv_add_to_hash_table(
					type, space_id, page_no,
					body, ptr + len, old_lsn,
					new_recovered_lsn);

			} else {

				recv_sys->missing_ids.insert(space_id);
			}
		}

		ptr += len;
	}

	return(false);
}

/** Parse log records from a buffer and optionally store them to a
hash table to wait merging to file pages.
@param[in]	checkpoint_lsn	the LSN of the latest checkpoint */
static
void
recv_parse_log_recs(lsn_t checkpoint_lsn)
{
	ut_ad(log_mutex_own());
	ut_ad(recv_sys->parse_start_lsn != 0);

	for (;;) {

		byte*	ptr = recv_sys->buf + recv_sys->recovered_offset;

		byte*	end_ptr = recv_sys->buf + recv_sys->len;

		if (ptr == end_ptr) {

			return;
		}

		bool	single_rec;

		switch (*ptr) {
#ifdef UNIV_LOG_LSN_DEBUG
		case MLOG_LSN:
#endif /* UNIV_LOG_LSN_DEBUG */
		case MLOG_DUMMY_RECORD:
			single_rec = true;
			break;
		default:
			single_rec = !!(*ptr & MLOG_SINGLE_REC_FLAG);
		}

		if (single_rec) {

			if (recv_single_rec(ptr, end_ptr)) {

				return;
			}

		} else if (recv_multi_rec(ptr, end_ptr)) {

			return;
		}
	}
}

/** Adds data from a new log block to the parsing buffer of recv_sys if
recv_sys->parse_start_lsn is non-zero.
@param[in]	log_block		log block
@param[in]	scanned_lsn		lsn of how far we were able
					to find data in this log block
@return true if more data added */
static
bool
recv_sys_add_to_parsing_buf(
	const byte*	log_block,
	lsn_t		scanned_lsn)
{
	ut_ad(scanned_lsn >= recv_sys->scanned_lsn);

	if (!recv_sys->parse_start_lsn) {
		/* Cannot start parsing yet because no start point for
		it found */

		return(false);
	}

	ulint	more_len;
	ulint	data_len = log_block_get_data_len(log_block);

	if (recv_sys->parse_start_lsn >= scanned_lsn) {

		return(false);

	} else if (recv_sys->scanned_lsn >= scanned_lsn) {

		return(false);

	} else if (recv_sys->parse_start_lsn > recv_sys->scanned_lsn) {

		more_len = (ulint) (scanned_lsn - recv_sys->parse_start_lsn);

	} else {
		more_len = (ulint) (scanned_lsn - recv_sys->scanned_lsn);
	}

	if (more_len == 0) {

		return(false);
	}

	ut_ad(data_len >= more_len);

	ulint	start_offset = data_len - more_len;

	if (start_offset < LOG_BLOCK_HDR_SIZE) {
		start_offset = LOG_BLOCK_HDR_SIZE;
	}

	ulint	end_offset = data_len;

	if (end_offset > OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
		end_offset = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;
	}

	ut_ad(start_offset <= end_offset);

	if (start_offset < end_offset) {
		ut_memcpy(recv_sys->buf + recv_sys->len,
			  log_block + start_offset, end_offset - start_offset);

		recv_sys->len += end_offset - start_offset;

		ut_a(recv_sys->len <= RECV_PARSING_BUF_SIZE);
	}

	return(true);
}

/** Moves the parsing buffer data left to the buffer start. */
static
void
recv_reset_buffer()
{
	ut_memmove(recv_sys->buf, recv_sys->buf + recv_sys->recovered_offset,
		   recv_sys->len - recv_sys->recovered_offset);

	recv_sys->len -= recv_sys->recovered_offset;

	recv_sys->recovered_offset = 0;
}

/** Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.  Unless
UNIV_HOTBACKUP is defined, this function will apply log records
automatically when the hash table becomes full.
@param[in]	max_memory	we let the hash table of recs to grow to
				this size, at the maximum
@param[in]	buf		buffer containing a log segment or garbage
@param[in]	len		buffer length
@param[in]	checkpoint_lsn	latest checkpoint LSN
@param[in]	start_lsn	buffer start lsn
@param[in,out]	contiguous_lsn	it is known that all log groups contain
				contiguous log data up to this lsn
@param[out]	read_upto_lsn	scanning succeeded up to this lsn
@return true if not able to scan any more in this log group */
static
bool
recv_scan_log_recs(
	ulint		max_memory,
	const byte*	buf,
	ulint		len,
	lsn_t		checkpoint_lsn,
	lsn_t		start_lsn,
	lsn_t*		contiguous_lsn,
	lsn_t*		read_upto_lsn)
{
	const byte*	log_block	= buf;
	lsn_t		scanned_lsn	= start_lsn;
	bool		finished	= false;
	bool		more_data	= false;

	ut_ad(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(len % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(len >= OS_FILE_LOG_BLOCK_SIZE);

	do {
		ut_ad(!finished);

		ulint	no = log_block_get_hdr_no(log_block);

		ulint	expected_no = log_block_convert_lsn_to_no(scanned_lsn);

		if (no != expected_no) {

			/* Garbage or an incompletely written log block.

			We will not report any error, because this can
			happen when InnoDB was killed while it was
			writing redo log. We simply treat this as an
			abrupt end of the redo log. */

			finished = true;

			break;
		}

		if (!log_block_checksum_is_ok(log_block)) {

			ib::error()
				<< "Log block " << no <<
				" at lsn " << scanned_lsn << " has valid"
				" header, but checksum field contains "
				<< log_block_get_checksum(log_block)
				<< ", should be "
				<< log_block_calc_checksum(log_block);

			/* Garbage or an incompletely written log block.

			This could be the result of killing the server
			while it was writing this log block. We treat
			this as an abrupt end of the redo log. */

			finished = true;

			break;
		}

		if (log_block_get_flush_bit(log_block)) {

			/* This block was a start of a log flush operation:
			we know that the previous flush operation must have
			been completed for all log groups before this block
			can have been flushed to any of the groups. Therefore,
			we know that log data is contiguous up to scanned_lsn
			in all non-corrupt log groups. */

			if (scanned_lsn > *contiguous_lsn) {

				*contiguous_lsn = scanned_lsn;
			}
		}

		ulint	data_len = log_block_get_data_len(log_block);

		if (scanned_lsn + data_len > recv_sys->scanned_lsn
		    && log_block_get_checkpoint_no(log_block)
		    < recv_sys->scanned_checkpoint_no
		    && (recv_sys->scanned_checkpoint_no
			- log_block_get_checkpoint_no(log_block)
			> 0x80000000UL)) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */

			finished = true;

			break;
		}

		if (!recv_sys->parse_start_lsn
		    && log_block_get_first_rec_group(log_block) > 0) {

			/* We found a point from which to start the parsing
			of log records */

			recv_sys->parse_start_lsn = scanned_lsn
				+ log_block_get_first_rec_group(log_block);

			recv_sys->scanned_lsn = recv_sys->parse_start_lsn;

			recv_sys->recovered_lsn = recv_sys->parse_start_lsn;
		}

		scanned_lsn += data_len;

		if (scanned_lsn > recv_sys->scanned_lsn) {

			if (srv_read_only_mode) {

				ib::warn()
					<< "Recovery skipped,"
					<< " --innodb-read-only set!";

				return(true);

			} else if (!recv_needed_recovery) {

				ib::info()
					<< "Log scan progressed past the"
					<< " checkpoint LSN "
					<< recv_sys->scanned_lsn;

				recv_init_crash_recovery();
			}

			/* We were able to find more log data: add it to the
			parsing buffer if parse_start_lsn is already
			non-zero */

			if (recv_sys->len + 4 * OS_FILE_LOG_BLOCK_SIZE
			    >= RECV_PARSING_BUF_SIZE) {

				ib::error()
					<< "Log parsing buffer overflow."
					" Recovery may have failed!";

				recv_sys->found_corrupt_log = true;

#ifndef UNIV_HOTBACKUP
				if (srv_force_recovery == 0) {

					ib::error()
						<< "Set innodb_force_recovery"
						" to ignore this error.";
					return(true);
				}
#endif /* !UNIV_HOTBACKUP */

			} else if (!recv_sys->found_corrupt_log) {

				more_data = recv_sys_add_to_parsing_buf(
					log_block, scanned_lsn);
			}

			recv_sys->scanned_lsn = scanned_lsn;

			recv_sys->scanned_checkpoint_no
				= log_block_get_checkpoint_no(log_block);
		}

		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {

			/* Log data for this group ends here */
			finished = true;

			break;

		} else {
			log_block += OS_FILE_LOG_BLOCK_SIZE;
		}

	} while (log_block < buf + len);

	*read_upto_lsn = scanned_lsn;

	if (recv_needed_recovery
	    || (recv_is_from_backup && !recv_is_making_a_backup)) {

		++recv_scan_print_counter;

		if (finished || (recv_scan_print_counter % 80) == 0) {

			ib::info()
				<< "Doing recovery: scanned up to"
				" log sequence number " << scanned_lsn;
		}
	}

	if (more_data && !recv_sys->found_corrupt_log) {

		/* Try to parse more log records */

		recv_parse_log_recs(checkpoint_lsn);

		if (recv_heap_used() > max_memory) {

			recv_apply_hashed_log_recs(false);
		}

		if (recv_sys->recovered_offset > RECV_PARSING_BUF_SIZE / 4) {

			/* Move parsing buffer data to the buffer start */

			recv_reset_buffer();
		}
	}

	return(finished);
}

#ifndef UNIV_HOTBACKUP
/** Reads a specified log segment to a buffer.
@param[in,out]	buf		buffer where to read
@param[in,out]	group		log group
@param[in]	start_lsn	read area start
@param[in]	end_lsn		read area end */
static
void
recv_read_log_seg(
	byte*		buf,
	log_group_t*	group,
	lsn_t		start_lsn,
	lsn_t		end_lsn)
{
	ut_ad(log_mutex_own());

	do {
		lsn_t	source_offset;

		source_offset = log_group_calc_lsn_offset(start_lsn, group);

		ut_a(end_lsn - start_lsn <= ULINT_MAX);

		ulint	len;

		len = (ulint) (end_lsn - start_lsn);

		ut_ad(len != 0);

		if ((source_offset % group->file_size) + len
		    > group->file_size) {

			/* If the above condition is true then len
			(which is ulint) is > the expression below,
			so the typecast is ok */
			len = (ulint) (group->file_size -
				(source_offset % group->file_size));
		}

		++log_sys->n_log_ios;

		MONITOR_INC(MONITOR_LOG_IO);

		ut_a(source_offset / UNIV_PAGE_SIZE <= PAGE_NO_MAX);

		const page_no_t	page_no = static_cast<page_no_t>(
			source_offset / univ_page_size.physical());

		fil_io(IORequestLogRead, true,
		       page_id_t(group->space_id, page_no),
		       univ_page_size,
		       (ulint) (source_offset % univ_page_size.physical()),
		       len, buf, NULL);

		start_lsn += len;
		buf += len;

	} while (start_lsn != end_lsn);
}

/** Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.
@param[in,out]	group			log group
@param[in,out]	contiguous_lsn		log sequence number
					until which all redo log has been
					scanned */
static
void
recv_recovery_begin(
	log_group_t*	group,
	lsn_t*		contiguous_lsn)
{
	mutex_enter(&recv_sys->mutex);

	recv_sys->len = 0;
	recv_sys->recovered_offset = 0;
	recv_sys->n_addrs = 0;
	recv_sys_empty_hash();
	srv_start_lsn = *contiguous_lsn;
	recv_sys->parse_start_lsn = *contiguous_lsn;
	recv_sys->scanned_lsn = *contiguous_lsn;
	recv_sys->recovered_lsn = *contiguous_lsn;
	recv_sys->scanned_checkpoint_no = 0;
	recv_previous_parsed_rec_type = MLOG_SINGLE_REC_FLAG;
	recv_previous_parsed_rec_offset	= 0;
	recv_previous_parsed_rec_is_multi = 0;
	ut_ad(recv_max_page_lsn == 0);

	mutex_exit(&recv_sys->mutex);

	ulint	max_mem	= UNIV_PAGE_SIZE
		* (buf_pool_get_n_pages()
		   - (recv_n_pool_free_frames * srv_buf_pool_instances));

	*contiguous_lsn = ut_uint64_align_down(
		*contiguous_lsn, OS_FILE_LOG_BLOCK_SIZE);

	lsn_t	start_lsn = *contiguous_lsn;

	lsn_t	checkpoint_lsn	= start_lsn;

	bool	finished = false;

	while (!finished) {

		lsn_t	end_lsn = start_lsn + RECV_SCAN_SIZE;

		recv_read_log_seg(log_sys->buf, group, start_lsn, end_lsn);

		finished = recv_scan_log_recs(
			 max_mem,
			 log_sys->buf,
			 RECV_SCAN_SIZE,
			 checkpoint_lsn,
			 start_lsn,
			 contiguous_lsn,
			 &group->scanned_lsn);

		start_lsn = end_lsn;
	}

	DBUG_PRINT("ib_log",
		   ("scan " LSN_PF " completed for log group " ULINTPF,
		    group->scanned_lsn, group->id));
}

/** Initialize crash recovery environment. Can be called iff
recv_needed_recovery == false. */
static
void
recv_init_crash_recovery()
{
	ut_ad(!srv_read_only_mode);
	ut_a(!recv_needed_recovery);

	recv_needed_recovery = true;

	ib::info() << "Database was not shutdown normally!";
	ib::info() << "Starting crash recovery.";

	/* Open the tablespace ID to file name mapping file. Required for
	redo log apply and dblwr buffer page restore. */

	fil_tablespace_open_init_for_recovery(true);

	buf_dblwr_process();

	if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {

		/* Spawn the background thread to flush dirty pages
		from the buffer pools. */

		os_thread_create(
			recv_writer_thread_key,
			recv_writer_thread);
	}
}

/** Start recovering from a redo log checkpoint.
@see recv_recovery_from_checkpoint_finish
@param[in]	flush_lsn	FIL_PAGE_FILE_FLUSH_LSN
				of first system tablespace page
@return error code or DB_SUCCESS */
dberr_t
recv_recovery_from_checkpoint_start(lsn_t flush_lsn)
{
	/* Initialize red-black tree for fast insertions into the
	flush_list during recovery process. */
	buf_flush_init_flush_rbt();

	if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {

		ib::info()
			<< "The user has set SRV_FORCE_NO_LOG_REDO on,"
			<< " skipping log redo";

		return(DB_SUCCESS);
	}

	recv_recovery_on = true;

	log_mutex_enter();

	/* Look for the latest checkpoint from any of the log groups */

	dberr_t		err;
	log_group_t*	max_cp_group;
	ulint		max_cp_field;

	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

	if (err != DB_SUCCESS) {

		log_mutex_exit();

		return(err);
	}

	log_group_header_read(max_cp_group, max_cp_field);

	byte*		buf;

	buf = log_sys->checkpoint_buf;

	lsn_t		checkpoint_lsn;

	checkpoint_lsn = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);

	uint64_t	checkpoint_no;

	checkpoint_no = mach_read_from_8(buf + LOG_CHECKPOINT_NO);

	/* Read the first log file header to print a note if this is
	a recovery from a restored InnoDB Hot Backup */

	byte		log_hdr_buf[LOG_FILE_HDR_SIZE];
	const page_id_t	page_id(max_cp_group->space_id, 0);

	fil_io(IORequestLogRead, true, page_id, univ_page_size, 0,
	       LOG_FILE_HDR_SIZE, log_hdr_buf, max_cp_group);

	if (0 == ut_memcmp(log_hdr_buf + LOG_HEADER_CREATOR,
		(byte*)"ibbackup", (sizeof "ibbackup") - 1)) {

		if (srv_read_only_mode) {
			log_mutex_exit();

			ib::error()
				<< "Cannot restore from mysqlbackup,"
				" InnoDB running in read-only mode!";

			return(DB_ERROR);
		}

		/* This log file was created by mysqlbackup --restore: print
		a note to the user about it */

		ib::info()
			<< "The log file was created by mysqlbackup"
			" --apply-log at "
			<< log_hdr_buf + LOG_HEADER_CREATOR
			<< ". The following crash recovery is part of a"
			" normal restore.";

		/* Replace the label. */
		ut_ad(LOG_HEADER_CREATOR_END - LOG_HEADER_CREATOR
		      >= sizeof LOG_HEADER_CREATOR_CURRENT);

		memset(log_hdr_buf + LOG_HEADER_CREATOR, 0,
		       LOG_HEADER_CREATOR_END - LOG_HEADER_CREATOR);

		strcpy(reinterpret_cast<char*>(log_hdr_buf)
		       + LOG_HEADER_CREATOR, LOG_HEADER_CREATOR_CURRENT);

		/* Write to the log file to wipe over the label */
		fil_io(IORequestLogWrite, true, page_id,
		       univ_page_size, 0, OS_FILE_LOG_BLOCK_SIZE, log_hdr_buf,
		       max_cp_group);

	} else if (0 == ut_memcmp(log_hdr_buf + LOG_HEADER_CREATOR,
		(byte*)LOG_HEADER_CREATOR_CLONE,
		(sizeof LOG_HEADER_CREATOR_CLONE) - 1)) {

		recv_sys->is_cloned_db = true;
		ib::info() << "Opening cloned database";
	}

	/* Start reading the log groups from the checkpoint LSN up. The
	variable contiguous_lsn contains an LSN up to which the log is
	known to be contiguously written to all log groups. */

	ut_ad(RECV_SCAN_SIZE <= log_sys->buf_size);

	ut_ad(UT_LIST_GET_LEN(log_sys->log_groups) == 1);

	log_group_t*	group;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	ut_ad(recv_sys->n_addrs == 0);

	lsn_t		contiguous_lsn;

	contiguous_lsn = checkpoint_lsn;

	switch (group->format) {
	case LOG_HEADER_FORMAT_5_7_9:
		log_mutex_exit();
		return(recv_log_recover_5_7(checkpoint_lsn));

	case LOG_HEADER_FORMAT_CURRENT:
		break;

	default:
		ut_ad(0);
		recv_sys->found_corrupt_log = true;
		log_mutex_exit();
		return(DB_ERROR);
	}

	/* NOTE: we always do a 'recovery' at startup, but only if
	there is something wrong we will print a message to the
	user about recovery: */

	if (checkpoint_lsn != flush_lsn) {

		if (checkpoint_lsn < flush_lsn) {

			ib::warn()
				<< " Are you sure you are using the"
				" right ib_logfiles to start up the database?"
				" Log sequence number in the ib_logfiles is "
				<< checkpoint_lsn << ", less than the"
				" log sequence number in the first system"
				" tablespace file header, "
				<< flush_lsn << ".";
		}

		if (!recv_needed_recovery) {

			ib::info()
				<< "The log sequence number " << flush_lsn
				<< " in the system tablespace does not match"
				" the log sequence number " << checkpoint_lsn
				<< " in the ib_logfiles!";

			if (srv_read_only_mode) {

				ib::error()
					<< "Can't initiate database"
					<< " recovery, running in"
					<< " read-only-mode.";

				log_mutex_exit();

				return(DB_READ_ONLY);
			}

			recv_init_crash_recovery();
		}

	} else if (!srv_read_only_mode) {
		/* Read the UNDO tablespace locations only. */
		fil_tablespace_open_init_for_recovery(false);
	}

	contiguous_lsn = checkpoint_lsn;

	recv_recovery_begin(group, &contiguous_lsn);

	log_sys->lsn = recv_sys->recovered_lsn;

	/* We currently have only one log group */

	if (group->scanned_lsn < checkpoint_lsn
	    || group->scanned_lsn < recv_max_page_lsn) {

		ib::error()
			<< "We scanned the log up to " << group->scanned_lsn
			<< ". A checkpoint was at " << checkpoint_lsn << " and"
			" the maximum LSN on a database page was "
			<< recv_max_page_lsn << ". It is possible that the"
			" database is now corrupt!";
	}

	if (recv_sys->recovered_lsn < checkpoint_lsn) {

		log_mutex_exit();

		/* No harm in trying to do RO access. */
		if (!srv_read_only_mode) {
			ut_error;
		}

		return(DB_ERROR);
	}

	if ((recv_sys->found_corrupt_log && srv_force_recovery == 0)
	    || recv_sys->found_corrupt_fs) {

		log_mutex_exit();

		return(DB_ERROR);
	}

	/* Synchronize the uncorrupted log groups to the most up-to-date log
	group; we also copy checkpoint info to groups */

	log_sys->next_checkpoint_lsn = checkpoint_lsn;
	log_sys->next_checkpoint_no = checkpoint_no + 1;

	recv_synchronize_groups();

	if (!recv_needed_recovery) {
		ut_a(checkpoint_lsn == recv_sys->recovered_lsn);
	} else {
		srv_start_lsn = recv_sys->recovered_lsn;
	}

	ut_memcpy(log_sys->buf, recv_sys->last_block, OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_free = (ulint) log_sys->lsn % OS_FILE_LOG_BLOCK_SIZE;
	log_sys->buf_next_to_write = log_sys->buf_free;
	log_sys->write_lsn = log_sys->lsn;

	log_sys->last_checkpoint_lsn = checkpoint_lsn;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE,
		    log_sys->lsn - log_sys->last_checkpoint_lsn);

	log_sys->next_checkpoint_no = checkpoint_no + 1;

	mutex_enter(&recv_sys->mutex);

	recv_sys->apply_log_recs = true;

	mutex_exit(&recv_sys->mutex);

	log_mutex_exit();

	/* The database is now ready to start almost normal processing of user
	transactions: transaction rollbacks and the application of the log
	records in the hash table can be run in background. */

	return(DB_SUCCESS);
}

/** Complete the recovery from the latest checkpoint.
@param[in]	aborting	true if the server has to abort due to an error
@return recovered persistent metadata */
MetadataRecover*
recv_recovery_from_checkpoint_finish(bool aborting)
{
	/* Make sure that the recv_writer thread is done. This is
	required because it grabs various mutexes and we want to
	ensure that when we enable sync_order_checks there is no
	mutex currently held by any thread. */
	mutex_enter(&recv_sys->writer_mutex);

	/* Free the resources of the recovery system */
	recv_recovery_on = false;

	/* By acquring the mutex we ensure that the recv_writer thread
	won't trigger any more LRU batches. Now wait for currently
	in progress batches to finish. */
	buf_flush_wait_LRU_batch_end();

	mutex_exit(&recv_sys->writer_mutex);

	ulint	count = 0;

	while (recv_writer_thread_active) {

		++count;

		os_thread_sleep(100000);

		if (srv_print_verbose_log && count > 600) {

			ib::info()
				<< "Waiting for recv_writer to"
				" finish flushing of buffer pool";
			count = 0;
		}
	}

	MetadataRecover*	metadata;
       
	if (!aborting) {

		metadata = recv_sys->metadata_recover;

		recv_sys->metadata_recover = nullptr;
	} else {
		metadata = nullptr;
	}

	recv_sys_free();

	if (!aborting) {
		/* Validate a few system page types that were left uninitialized
		by older versions of MySQL. */
		mtr_t	mtr;

		mtr.start();

		buf_block_t*	block;

		/* Bitmap page types will be reset in buf_dblwr_check_block()
		without redo logging. */

		block = buf_page_get(
			page_id_t(IBUF_SPACE_ID, FSP_IBUF_HEADER_PAGE_NO),
			univ_page_size, RW_X_LATCH, &mtr);

		fil_block_check_type(block, FIL_PAGE_TYPE_SYS, &mtr);

		/* Already MySQL 3.23.53 initialized FSP_IBUF_TREE_ROOT_PAGE_NO
		to FIL_PAGE_INDEX. No need to reset that one. */

		block = buf_page_get(
			page_id_t(TRX_SYS_SPACE, TRX_SYS_PAGE_NO),
			univ_page_size, RW_X_LATCH, &mtr);

		fil_block_check_type(block, FIL_PAGE_TYPE_TRX_SYS, &mtr);

		block = buf_page_get(
			page_id_t(TRX_SYS_SPACE, FSP_FIRST_RSEG_PAGE_NO),
			univ_page_size, RW_X_LATCH, &mtr);

		fil_block_check_type(block, FIL_PAGE_TYPE_SYS, &mtr);

		block = buf_page_get(
			page_id_t(TRX_SYS_SPACE, FSP_DICT_HDR_PAGE_NO),
			univ_page_size, RW_X_LATCH, &mtr);

		fil_block_check_type(block, FIL_PAGE_TYPE_SYS, &mtr);

		mtr.commit();

		fil_tablespace_open_create();
	}

	/* Free up the flush_rbt. */
	buf_flush_free_flush_rbt();

	return(metadata);
}

/** Resets the logs. The contents of log files will be lost!
@param[in]	lsn		reset to this lsn rounded up to be divisible
				by OS_FILE_LOG_BLOCK_SIZE, after which we
				add LOG_BLOCK_HDR_SIZE */
void
recv_reset_logs(lsn_t lsn)
{
	ut_ad(log_mutex_own());

	log_sys->lsn = ut_uint64_align_up(lsn, OS_FILE_LOG_BLOCK_SIZE);

	for (auto group = UT_LIST_GET_FIRST(log_sys->log_groups);
	     group != nullptr;
	     group = UT_LIST_GET_NEXT(log_groups, group)) {

		group->lsn = log_sys->lsn;
		group->lsn_offset = LOG_FILE_HDR_SIZE;
	}

	log_sys->buf_next_to_write = 0;
	log_sys->write_lsn = log_sys->lsn;

	log_sys->next_checkpoint_no = 0;
	log_sys->last_checkpoint_lsn = 0;

	log_block_init(log_sys->buf, log_sys->lsn);
	log_block_set_first_rec_group(log_sys->buf, LOG_BLOCK_HDR_SIZE);

	log_sys->buf_free = LOG_BLOCK_HDR_SIZE;
	log_sys->lsn += LOG_BLOCK_HDR_SIZE;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE,
		    (log_sys->lsn - log_sys->last_checkpoint_lsn));

	log_mutex_exit();

	/* Reset the checkpoint fields in logs */

	log_make_checkpoint_at(LSN_MAX, true);

	log_mutex_enter();
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_HOTBACKUP

/** Creates new log files after a backup has been restored.
@param[in]	log_dir		log file directory path
@param[in]	n_log_files	number of log files
@param[in]	log_file_size	log file size
@param[in]	lsn		new start lsn, must be divisible
				by OS_FILE_LOG_BLOCK_SIZE */
void
recv_reset_log_files_for_backup(
	const char*	log_dir,
	ulint		n_log_files,
	lsn_t		log_file_size,
	lsn_t		lsn)
{
	byte*		buf;
	bool		success;
	os_file_t	log_file;
	ulint		log_dir_len;
	char		name[5000];

	log_dir_len = strlen(log_dir);
	/* full path name of ib_logfile consists of log dir path + basename
	+ number. This must fit in the name buffer.
	*/
	ut_a(log_dir_len + strlen(ib_logfile_basename) + 11  < sizeof(name));

	buf = ut_zalloc_nokey(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);

	for (ulint i = 0; i < n_log_files; i++) {

		sprintf(name, "%s%s%lu", log_dir,
			ib_logfile_basename, (ulong) i);

		log_file = os_file_create_simple(innodb_log_file_key,
						 name, OS_FILE_CREATE,
						 OS_FILE_READ_WRITE,
						 srv_read_only_mode, &success);
		if (!success) {
			ib::fatal()
				<< "Cannot create " << name
				<< ". Check that the file does not exist yet.";
		}

		ib::info() << "Setting log file size to " << log_file_size;

		success = os_file_set_size(
			name, log_file, log_file_size,
			srv_read_only_mode, true);

		if (!success) {
			ib::fatal() << "Cannot set " << name << " size to "
				<< log_file_size;
		}

		os_file_flush(log_file);
		os_file_close(log_file);
	}

	/* We pretend there is a checkpoint at LSN + LOG_BLOCK_HDR_SIZE */

	log_reset_first_header_and_checkpoint(buf, lsn);

	log_block_init_in_old_format(buf + LOG_FILE_HDR_SIZE, lsn);

	log_block_set_first_rec_group(
		buf + LOG_FILE_HDR_SIZE, LOG_BLOCK_HDR_SIZE);

	sprintf(name, "%s%s%lu", log_dir, ib_logfile_basename, (ulong)0);

	log_file = os_file_create_simple(
		innodb_log_file_key, name,
		OS_FILE_OPEN, OS_FILE_READ_WRITE, srv_read_only_mode,
		&success);

	if (!success) {
		ib::fatal() << "Cannot open " << name << ".";
	}

	IORequest	request(IORequest::WRITE);

	dberr_t	err = os_file_write(
		request, name, log_file, buf, 0,
		LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);

	ut_a(err == DB_SUCCESS);

	os_file_flush(log_file);
	os_file_close(log_file);

	ut_free(buf);
}
#endif /* UNIV_HOTBACKUP */

/** Find a doublewrite copy of a page.
@param[in]	space_id	tablespace identifier
@param[in]	page_no		page number
@return	page frame
@retval nullptr if no page was found */
const byte*
recv_dblwr_t::find_page(space_id_t space_id, page_no_t page_no)
{
	typedef std::vector<const byte*, ut_allocator<const byte*> >
		matches_t;

	matches_t	matches;
	const byte*	result = 0;

	for (auto i = pages.begin(); i != pages.end(); ++i) {
		if (page_get_space_id(*i) == space_id
		    && page_get_page_no(*i) == page_no) {
			matches.push_back(*i);
		}
	}

	for (auto i = deferred.begin(); i != deferred.end(); ++i) {

		if (page_get_space_id(i->m_page) == space_id
		    && page_get_page_no(i->m_page) == page_no) {

			matches.push_back(i->m_page);
		}
	}

	if (matches.size() == 1) {
		result = matches[0];
	} else if (matches.size() > 1) {

		lsn_t max_lsn	= 0;
		lsn_t page_lsn	= 0;

		for (matches_t::iterator i = matches.begin();
		     i != matches.end();
		     ++i) {

			page_lsn = mach_read_from_8(*i + FIL_PAGE_LSN);

			if (page_lsn > max_lsn) {
				max_lsn = page_lsn;
				result = *i;
			}
		}
	}

	return(result);
}

#ifdef UNIV_DEBUG
/** Return string name of the redo log record type.
@param[in]	type	record log record enum
@return string name of record log record */
const char*
get_mlog_string(mlog_id_t type)
{
	switch (type) {
	case MLOG_SINGLE_REC_FLAG:
		return("MLOG_SINGLE_REC_FLAG");

	case MLOG_1BYTE:
		return("MLOG_1BYTE");

	case MLOG_2BYTES:
		return("MLOG_2BYTES");

	case MLOG_4BYTES:
		return("MLOG_4BYTES");

	case MLOG_8BYTES:
		return("MLOG_8BYTES");

	case MLOG_REC_INSERT:
		return("MLOG_REC_INSERT");

	case MLOG_REC_CLUST_DELETE_MARK:
		return("MLOG_REC_CLUST_DELETE_MARK");

	case MLOG_REC_SEC_DELETE_MARK:
		return("MLOG_REC_SEC_DELETE_MARK");

	case MLOG_REC_UPDATE_IN_PLACE:
		return("MLOG_REC_UPDATE_IN_PLACE");

	case MLOG_REC_DELETE:
		return("MLOG_REC_DELETE");

	case MLOG_LIST_END_DELETE:
		return("MLOG_LIST_END_DELETE");

	case MLOG_LIST_START_DELETE:
		return("MLOG_LIST_START_DELETE");

	case MLOG_LIST_END_COPY_CREATED:
		return("MLOG_LIST_END_COPY_CREATED");

	case MLOG_PAGE_REORGANIZE:
		return("MLOG_PAGE_REORGANIZE");

	case MLOG_PAGE_CREATE:
		return("MLOG_PAGE_CREATE");

	case MLOG_UNDO_INSERT:
		return("MLOG_UNDO_INSERT");

	case MLOG_UNDO_ERASE_END:
		return("MLOG_UNDO_ERASE_END");

	case MLOG_UNDO_INIT:
		return("MLOG_UNDO_INIT");

	case MLOG_UNDO_HDR_REUSE:
		return("MLOG_UNDO_HDR_REUSE");

	case MLOG_UNDO_HDR_CREATE:
		return("MLOG_UNDO_HDR_CREATE");

	case MLOG_REC_MIN_MARK:
		return("MLOG_REC_MIN_MARK");

	case MLOG_IBUF_BITMAP_INIT:
		return("MLOG_IBUF_BITMAP_INIT");

#ifdef UNIV_LOG_LSN_DEBUG
	case MLOG_LSN:
		return("MLOG_LSN");
#endif /* UNIV_LOG_LSN_DEBUG */

	case MLOG_INIT_FILE_PAGE:
		return("MLOG_INIT_FILE_PAGE");

	case MLOG_WRITE_STRING:
		return("MLOG_WRITE_STRING");

	case MLOG_MULTI_REC_END:
		return("MLOG_MULTI_REC_END");

	case MLOG_DUMMY_RECORD:
		return("MLOG_DUMMY_RECORD");

	case MLOG_FILE_DELETE:
		return("MLOG_FILE_DELETE");

	case MLOG_COMP_REC_MIN_MARK:
		return("MLOG_COMP_REC_MIN_MARK");

	case MLOG_COMP_PAGE_CREATE:
		return("MLOG_COMP_PAGE_CREATE");

	case MLOG_COMP_REC_INSERT:
		return("MLOG_COMP_REC_INSERT");

	case MLOG_COMP_REC_CLUST_DELETE_MARK:
		return("MLOG_COMP_REC_CLUST_DELETE_MARK");

	case MLOG_COMP_REC_SEC_DELETE_MARK:
		return("MLOG_COMP_REC_SEC_DELETE_MARK");

	case MLOG_COMP_REC_UPDATE_IN_PLACE:
		return("MLOG_COMP_REC_UPDATE_IN_PLACE");

	case MLOG_COMP_REC_DELETE:
		return("MLOG_COMP_REC_DELETE");

	case MLOG_COMP_LIST_END_DELETE:
		return("MLOG_COMP_LIST_END_DELETE");

	case MLOG_COMP_LIST_START_DELETE:
		return("MLOG_COMP_LIST_START_DELETE");

	case MLOG_COMP_LIST_END_COPY_CREATED:
		return("MLOG_COMP_LIST_END_COPY_CREATED");

	case MLOG_COMP_PAGE_REORGANIZE:
		return("MLOG_COMP_PAGE_REORGANIZE");

	case MLOG_FILE_CREATE2:
		return("MLOG_FILE_CREATE2");

	case MLOG_ZIP_WRITE_NODE_PTR:
		return("MLOG_ZIP_WRITE_NODE_PTR");

	case MLOG_ZIP_WRITE_BLOB_PTR:
		return("MLOG_ZIP_WRITE_BLOB_PTR");

	case MLOG_ZIP_WRITE_HEADER:
		return("MLOG_ZIP_WRITE_HEADER");

	case MLOG_ZIP_PAGE_COMPRESS:
		return("MLOG_ZIP_PAGE_COMPRESS");

	case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:
		return("MLOG_ZIP_PAGE_COMPRESS_NO_DATA");

	case MLOG_ZIP_PAGE_REORGANIZE:
		return("MLOG_ZIP_PAGE_REORGANIZE");

	case MLOG_FILE_RENAME2:
		return("MLOG_FILE_RENAME2");

	case MLOG_FILE_OPEN:
		return("MLOG_FILE_OPEN");

	case MLOG_PAGE_CREATE_RTREE:
		return("MLOG_PAGE_CREATE_RTREE");

	case MLOG_COMP_PAGE_CREATE_RTREE:
		return("MLOG_COMP_PAGE_CREATE_RTREE");

	case MLOG_INIT_FILE_PAGE2:
		return("MLOG_INIT_FILE_PAGE2");

	case MLOG_INDEX_LOAD:
		return("MLOG_INDEX_LOAD");

	/* Disabled for WL6378
	case MLOG_TRUNCATE:
		return("MLOG_TRUNCATE");
	*/

	case MLOG_TABLE_DYNAMIC_META:
		return("MLOG_TABLE_DYNAMIC_META");

	case MLOG_PAGE_CREATE_SDI:
		return("MLOG_PAGE_CREATE_SDI");

	case MLOG_COMP_PAGE_CREATE_SDI:
		return("MLOG_COMP_PAGE_CREATE_SDI");
	}

	DBUG_ASSERT(0);

	return(nullptr);
}
#endif /* UNIV_DEBUG */
