/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file mtr/mtr0mtr.cc
Mini-transaction buffer

Created 11/26/1995 Heikki Tuuri
*******************************************************/

#include "mtr0mtr.h"

#include "buf0buf.h"
#include "buf0flu.h"
#include "fsp0sysspace.h"
#include "page0types.h"
#include "mtr0log.h"
#include "log0log.h"
#include "row0trunc.h"

#include "log0recv.h"

#ifdef UNIV_NONINL
#include "mtr0mtr.ic"
#endif /* UNIV_NONINL */

/** Iterate over a memo block in reverse. */
template <typename Functor>
struct Iterate {

	/** Release specific object */
	explicit Iterate(Functor& functor)
		:
		m_functor(functor)
	{
		/* Do nothing */
	}

	/** @return false if the functor returns false. */
	bool operator()(mtr_buf_t::block_t* block)
	{
		const mtr_memo_slot_t*	start =
			reinterpret_cast<const mtr_memo_slot_t*>(
				block->begin());

		mtr_memo_slot_t*	slot =
			reinterpret_cast<mtr_memo_slot_t*>(
				block->end());

		ut_ad(!(block->used() % sizeof(*slot)));

		while (slot-- != start) {

			if (!m_functor(slot)) {
				return(false);
			}
		}

		return(true);
	}

	Functor&	m_functor;
};

/** Find specific object */
struct Find {

	/** Constructor */
	Find(const void* object, ulint type)
		:
		m_slot(),
		m_type(type),
		m_object(object)
	{
		ut_a(object != NULL);
	}

	/** @return false if the object was found. */
	bool operator()(mtr_memo_slot_t* slot)
	{
		if (m_object == slot->object && m_type == slot->type) {
			m_slot = slot;
			return(false);
		}

		return(true);
	}

	/** Slot if found */
	mtr_memo_slot_t*m_slot;

	/** Type of the object to look for */
	ulint		m_type;

	/** The object instance to look for */
	const void*	m_object;
};

/** Find a page frame */
struct FindPage
{
	/** Constructor
	@param[in]	ptr	pointer to within a page frame
	@param[in]	flags	MTR_MEMO flags to look for */
	FindPage(const void* ptr, ulint flags)
		: m_ptr(ptr), m_flags(flags), m_slot(NULL)
	{
		/* We can only look for page-related flags. */
		ut_ad(!(flags & ~(MTR_MEMO_PAGE_S_FIX
				  | MTR_MEMO_PAGE_X_FIX
				  | MTR_MEMO_PAGE_SX_FIX
				  | MTR_MEMO_BUF_FIX
				  | MTR_MEMO_MODIFY)));
	}

	/** Visit a memo entry.
	@param[in]	slot	memo entry to visit
	@retval	false	if a page was found
	@retval	true	if the iteration should continue */
	bool operator()(mtr_memo_slot_t* slot)
	{
		ut_ad(m_slot == NULL);

		if (!(m_flags & slot->type) || slot->object == NULL) {
			return(true);
		}

		buf_block_t* block = reinterpret_cast<buf_block_t*>(
			slot->object);

		if (m_ptr < block->frame
		    || m_ptr >= block->frame + block->page.size.logical()) {
			return(true);
		}

		m_slot = slot;
		return(false);
	}

	/** @return the slot that was found */
	mtr_memo_slot_t* get_slot() const
	{
		ut_ad(m_slot != NULL);
		return(m_slot);
	}
	/** @return the block that was found */
	buf_block_t* get_block() const
	{
		return(reinterpret_cast<buf_block_t*>(get_slot()->object));
	}
private:
	/** Pointer inside a page frame to look for */
	const void*const	m_ptr;
	/** MTR_MEMO flags to look for */
	const ulint		m_flags;
	/** The slot corresponding to m_ptr */
	mtr_memo_slot_t*	m_slot;
};

/** Release latches and decrement the buffer fix count.
@param slot	memo slot */
static
void
memo_slot_release(mtr_memo_slot_t* slot)
{
	switch (slot->type) {
	case MTR_MEMO_BUF_FIX:
	case MTR_MEMO_PAGE_S_FIX:
	case MTR_MEMO_PAGE_SX_FIX:
	case MTR_MEMO_PAGE_X_FIX: {

		buf_block_t*	block;

		block = reinterpret_cast<buf_block_t*>(slot->object);

		buf_block_unfix(block);
		buf_page_release_latch(block, slot->type);
		break;
	}

	case MTR_MEMO_S_LOCK:
		rw_lock_s_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		break;

	case MTR_MEMO_SX_LOCK:
		rw_lock_sx_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		break;

	case MTR_MEMO_X_LOCK:
		rw_lock_x_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		break;

#ifdef UNIV_DEBUG
	default:
		ut_ad(slot->type == MTR_MEMO_MODIFY);
#endif /* UNIV_DEBUG */
	}

	slot->object = NULL;
}

/** Unfix a page, do not release the latches on the page.
@param slot	memo slot */
static
void
memo_block_unfix(mtr_memo_slot_t* slot)
{
	switch (slot->type) {
	case MTR_MEMO_BUF_FIX:
	case MTR_MEMO_PAGE_S_FIX:
	case MTR_MEMO_PAGE_X_FIX:
	case MTR_MEMO_PAGE_SX_FIX: {
		buf_block_unfix(reinterpret_cast<buf_block_t*>(slot->object));
		break;
	}

	case MTR_MEMO_S_LOCK:
	case MTR_MEMO_X_LOCK:
	case MTR_MEMO_SX_LOCK:
		break;
#ifdef UNIV_DEBUG
	default:
#endif /* UNIV_DEBUG */
		break;
	}
}
/** Release latches represented by a slot.
@param slot	memo slot */
static
void
memo_latch_release(mtr_memo_slot_t* slot)
{
	switch (slot->type) {
	case MTR_MEMO_BUF_FIX:
	case MTR_MEMO_PAGE_S_FIX:
	case MTR_MEMO_PAGE_SX_FIX:
	case MTR_MEMO_PAGE_X_FIX: {
		buf_block_t*	block;

		block = reinterpret_cast<buf_block_t*>(slot->object);

		memo_block_unfix(slot);

		buf_page_release_latch(block, slot->type);

		slot->object = NULL;
		break;
	}

	case MTR_MEMO_S_LOCK:
		rw_lock_s_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		slot->object = NULL;
		break;

	case MTR_MEMO_X_LOCK:
		rw_lock_x_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		slot->object = NULL;
		break;

	case MTR_MEMO_SX_LOCK:
		rw_lock_sx_unlock(reinterpret_cast<rw_lock_t*>(slot->object));
		slot->object = NULL;
		break;

#ifdef UNIV_DEBUG
	default:
		ut_ad(slot->type == MTR_MEMO_MODIFY);

		slot->object = NULL;
#endif /* UNIV_DEBUG */
	}
}

/** Release the latches acquired by the mini-transaction. */
struct ReleaseLatches {

	/** @return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {
			memo_latch_release(slot);
		}

		return(true);
	}
};

/** Release the latches and blocks acquired by the mini-transaction. */
struct ReleaseAll {
	/** @return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {
			memo_slot_release(slot);
		}

		return(true);
	}
};

/** Check that all slots have been handled. */
struct DebugCheck {
	/** @return true always. */
	bool operator()(const mtr_memo_slot_t* slot) const
	{
		ut_a(slot->object == NULL);
		return(true);
	}
};

/** Release a resource acquired by the mini-transaction. */
struct ReleaseBlocks {
	/** Release specific object */
	ReleaseBlocks(lsn_t start_lsn, lsn_t end_lsn, FlushObserver* observer)
		:
		m_end_lsn(end_lsn),
		m_start_lsn(start_lsn),
		m_flush_observer(observer)
	{
		/* Do nothing */
	}

	/** Add the modified page to the buffer flush list. */
	void add_dirty_page_to_flush_list(mtr_memo_slot_t* slot) const
	{
		ut_ad(m_end_lsn > 0);
		ut_ad(m_start_lsn > 0);

		buf_block_t*	block;

		block = reinterpret_cast<buf_block_t*>(slot->object);

		buf_flush_note_modification(block, m_start_lsn,
					    m_end_lsn, m_flush_observer);
	}

	/** @return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {

			if (slot->type == MTR_MEMO_PAGE_X_FIX
			    || slot->type == MTR_MEMO_PAGE_SX_FIX) {

				add_dirty_page_to_flush_list(slot);

			} else if (slot->type == MTR_MEMO_BUF_FIX) {

				buf_block_t*	block;
				block = reinterpret_cast<buf_block_t*>(
					slot->object);
				if (block->made_dirty_with_no_latch) {
					add_dirty_page_to_flush_list(slot);
					block->made_dirty_with_no_latch = false;
				}
			}
		}

		return(true);
	}

	/** Mini-transaction REDO start LSN */
	lsn_t		m_end_lsn;

	/** Mini-transaction REDO end LSN */
	lsn_t		m_start_lsn;

	/** Flush observer */
	FlushObserver*	m_flush_observer;
};

class mtr_t::Command {
public:
	/** Constructor.
	Takes ownership of the mtr->m_impl, is responsible for deleting it.
	@param[in,out]	mtr	mini-transaction */
	explicit Command(mtr_t* mtr)
		:
		m_locks_released()
	{
		init(mtr);
	}

	void init(mtr_t* mtr)
	{
		m_impl = &mtr->m_impl;
		m_sync = mtr->m_sync;
	}

	/** Destructor */
	~Command()
	{
		ut_ad(m_impl == 0);
	}

	/** Write the redo log record, add dirty pages to the flush list and
	release the resources. */
	void execute();

	/** Release the blocks used in this mini-transaction. */
	void release_blocks();

	/** Release the latches acquired by the mini-transaction. */
	void release_latches();

	/** Release both the latches and blocks used in the mini-transaction. */
	void release_all();

	/** Release the resources */
	void release_resources();

	/** Append the redo log records to the redo log buffer.
	@param[in]	len	number of bytes to write */
	void finish_write(ulint len);

private:
	/** Prepare to write the mini-transaction log to the redo log buffer.
	@return number of bytes to write in finish_write() */
	ulint prepare_write();

	/** true if it is a sync mini-transaction. */
	bool			m_sync;

	/** The mini-transaction state. */
	mtr_t::Impl*		m_impl;

	/** Set to 1 after the user thread releases the latches. The log
	writer thread must wait for this to be set to 1. */
	volatile ulint		m_locks_released;

	/** Start lsn of the possible log entry for this mtr */
	lsn_t			m_start_lsn;

	/** End lsn of the possible log entry for this mtr */
	lsn_t			m_end_lsn;
};

/** Check if a mini-transaction is dirtying a clean page.
@return true if the mtr is dirtying a clean page. */
bool
mtr_t::is_block_dirtied(const buf_block_t* block)
{
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_ad(block->page.buf_fix_count > 0);

	/* It is OK to read oldest_modification because no
	other thread can be performing a write of it and it
	is only during write that the value is reset to 0. */
	return(block->page.oldest_modification == 0);
}

/** Write the block contents to the REDO log */
struct mtr_write_log_t {
	/** Append a block to the redo log buffer.
	@return whether the appending should continue */
	bool operator()(const mtr_buf_t::block_t* block) const
	{
		log_write_low(block->begin(), block->used());
		return(true);
	}
};

/** Append records to the system-wide redo log buffer.
@param[in]	log	redo log records */
void
mtr_write_log(
	const mtr_buf_t*	log)
{
	const ulint	len = log->size();
	mtr_write_log_t	write_log;

	DBUG_PRINT("ib_log",
		   (ULINTPF " extra bytes written at " LSN_PF,
		    len, log_sys->lsn));

	log_reserve_and_open(len);
	log->for_each_block(write_log);
	log_close();
}

/** Start a mini-transaction.
@param sync		true if it is a synchronous mini-transaction
@param read_only	true if read only mini-transaction */
void
mtr_t::start(bool sync, bool read_only)
{
	UNIV_MEM_INVALID(this, sizeof(*this));

	UNIV_MEM_INVALID(&m_impl, sizeof(m_impl));

	m_sync =  sync;

	m_commit_lsn = 0;

	new(&m_impl.m_log) mtr_buf_t();
	new(&m_impl.m_memo) mtr_buf_t();

	m_impl.m_mtr = this;
	m_impl.m_log_mode = MTR_LOG_ALL;
	m_impl.m_inside_ibuf = false;
	m_impl.m_modifications = false;
	m_impl.m_made_dirty = false;
	m_impl.m_n_log_recs = 0;
	m_impl.m_state = MTR_STATE_ACTIVE;
	ut_d(m_impl.m_user_space_id = TRX_SYS_SPACE);
	m_impl.m_user_space = NULL;
	m_impl.m_undo_space = NULL;
	m_impl.m_sys_space = NULL;
	m_impl.m_flush_observer = NULL;

	ut_d(m_impl.m_magic_n = MTR_MAGIC_N);
}

/** Release the resources */
void
mtr_t::Command::release_resources()
{
	ut_ad(m_impl->m_magic_n == MTR_MAGIC_N);

	/* Currently only used in commit */
	ut_ad(m_impl->m_state == MTR_STATE_COMMITTING);

#ifdef UNIV_DEBUG
	DebugCheck		release;
	Iterate<DebugCheck>	iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);
#endif /* UNIV_DEBUG */

	/* Reset the mtr buffers */
	m_impl->m_log.erase();

	m_impl->m_memo.erase();

	m_impl->m_state = MTR_STATE_COMMITTED;

	m_impl = 0;
}

/** Commit a mini-transaction. */
void
mtr_t::commit()
{
	ut_ad(is_active());
	ut_ad(!is_inside_ibuf());
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	m_impl.m_state = MTR_STATE_COMMITTING;

	/* This is a dirty read, for debugging. */
	ut_ad(!recv_no_log_write);

	Command	cmd(this);

	if (m_impl.m_modifications
	    && (m_impl.m_n_log_recs > 0
		|| m_impl.m_log_mode == MTR_LOG_NO_REDO)) {

		ut_ad(!srv_read_only_mode
		      || m_impl.m_log_mode == MTR_LOG_NO_REDO);

		cmd.execute();
	} else {
		cmd.release_all();
		cmd.release_resources();
	}
}

/** Commit a mini-transaction that did not modify any pages,
but generated some redo log on a higher level, such as
MLOG_FILE_NAME records and a MLOG_CHECKPOINT marker.
The caller must invoke log_mutex_enter() and log_mutex_exit().
This is to be used at log_checkpoint().
@param[in]	checkpoint_lsn		the LSN of the log checkpoint
@param[in]	write_mlog_checkpoint	Write MLOG_CHECKPOINT marker
					if it is enabled. */
void
mtr_t::commit_checkpoint(
	lsn_t	checkpoint_lsn,
	bool	write_mlog_checkpoint)
{
	ut_ad(log_mutex_own());
	ut_ad(is_active());
	ut_ad(!is_inside_ibuf());
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_ad(get_log_mode() == MTR_LOG_ALL);
	ut_ad(!m_impl.m_made_dirty);
	ut_ad(m_impl.m_memo.size() == 0);
	ut_ad(!srv_read_only_mode);
	ut_d(m_impl.m_state = MTR_STATE_COMMITTING);
	ut_ad(write_mlog_checkpoint || m_impl.m_n_log_recs > 1);

	/* This is a dirty read, for debugging. */
	ut_ad(!recv_no_log_write);

	switch (m_impl.m_n_log_recs) {
	case 0:
		break;
	case 1:
		*m_impl.m_log.front()->begin() |= MLOG_SINGLE_REC_FLAG;
		break;
	default:
		mlog_catenate_ulint(
			&m_impl.m_log, MLOG_MULTI_REC_END, MLOG_1BYTE);
	}

	if (write_mlog_checkpoint) {
		byte*	ptr = m_impl.m_log.push<byte*>(SIZE_OF_MLOG_CHECKPOINT);
#if SIZE_OF_MLOG_CHECKPOINT != 9
# error SIZE_OF_MLOG_CHECKPOINT != 9
#endif
		*ptr = MLOG_CHECKPOINT;
		mach_write_to_8(ptr + 1, checkpoint_lsn);
	}

	Command	cmd(this);
	cmd.finish_write(m_impl.m_log.size());
	cmd.release_resources();

	if (write_mlog_checkpoint) {
		DBUG_PRINT("ib_log",
			   ("MLOG_CHECKPOINT(" LSN_PF ") written at " LSN_PF,
			    checkpoint_lsn, log_sys->lsn));
	}
}

#ifdef UNIV_DEBUG
/** Check if a tablespace is associated with the mini-transaction
(needed for generating a MLOG_FILE_NAME record)
@param[in]	space	tablespace
@return whether the mini-transaction is associated with the space */
bool
mtr_t::is_named_space(ulint space) const
{
	ut_ad(!m_impl.m_sys_space
	      || m_impl.m_sys_space->id == TRX_SYS_SPACE);
	ut_ad(!m_impl.m_undo_space
	      || m_impl.m_undo_space->id != TRX_SYS_SPACE);
	ut_ad(!m_impl.m_user_space
	      || m_impl.m_user_space->id != TRX_SYS_SPACE);
	ut_ad(!m_impl.m_sys_space
	      || m_impl.m_sys_space != m_impl.m_user_space);
	ut_ad(!m_impl.m_sys_space
	      || m_impl.m_sys_space != m_impl.m_undo_space);
	ut_ad(!m_impl.m_user_space
	      || m_impl.m_user_space != m_impl.m_undo_space);

	switch (get_log_mode()) {
	case MTR_LOG_NONE:
	case MTR_LOG_NO_REDO:
		return(true);
	case MTR_LOG_ALL:
	case MTR_LOG_SHORT_INSERTS:
		return(m_impl.m_user_space_id == space
		       || is_predefined_tablespace(space));
	}

	ut_error;
	return(false);
}
#endif /* UNIV_DEBUG */

/** Acquire a tablespace X-latch.
NOTE: use mtr_x_lock_space().
@param[in]	space_id	tablespace ID
@param[in]	file		file name from where called
@param[in]	line		line number in file
@return the tablespace object (never NULL) */
fil_space_t*
mtr_t::x_lock_space(ulint space_id, const char* file, ulint line)
{
	fil_space_t*	space;

	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_ad(is_active());

	if (space_id == TRX_SYS_SPACE) {
		space = m_impl.m_sys_space;

		if (!space) {
			space = m_impl.m_sys_space = fil_space_get(space_id);
		}
	} else if ((space = m_impl.m_user_space) && space_id == space->id) {
	} else if ((space = m_impl.m_undo_space) && space_id == space->id) {
	} else if (get_log_mode() == MTR_LOG_NO_REDO) {
		space = fil_space_get(space_id);
		ut_ad(space->purpose == FIL_TYPE_TEMPORARY
		      || space->purpose == FIL_TYPE_IMPORT
		      || space->redo_skipped_count > 0
		      || srv_is_tablespace_truncated(space->id));
	} else {
		/* called from trx_rseg_create() */
		space = m_impl.m_undo_space = fil_space_get(space_id);
	}

	ut_ad(space);
	ut_ad(space->id == space_id);
	x_lock(&space->latch, file, line);
	ut_ad(space->purpose == FIL_TYPE_TEMPORARY
	      || space->purpose == FIL_TYPE_IMPORT
	      || space->purpose == FIL_TYPE_TABLESPACE);
	return(space);
}

/** Look up the system tablespace. */
void
mtr_t::lookup_sys_space()
{
	ut_ad(!m_impl.m_sys_space);
	m_impl.m_sys_space = fil_space_get(TRX_SYS_SPACE);
	ut_ad(m_impl.m_sys_space);
}

/** Look up the user tablespace.
@param[in]	space_id	tablespace ID */
void
mtr_t::lookup_user_space(ulint space_id)
{
	ut_ad(space_id != TRX_SYS_SPACE);
	ut_ad(m_impl.m_user_space_id == space_id);
	ut_ad(!m_impl.m_user_space);
	m_impl.m_user_space = fil_space_get(space_id);
	ut_ad(m_impl.m_user_space);
}

/** Set the tablespace associated with the mini-transaction
(needed for generating a MLOG_FILE_NAME record)
@param[in]	space	user or system tablespace */
void
mtr_t::set_named_space(fil_space_t* space)
{
	ut_ad(m_impl.m_user_space_id == TRX_SYS_SPACE);
	ut_d(m_impl.m_user_space_id = space->id);
	if (space->id == TRX_SYS_SPACE) {
		ut_ad(m_impl.m_sys_space == NULL
		      || m_impl.m_sys_space == space);
		m_impl.m_sys_space = space;
	} else {
		m_impl.m_user_space = space;
	}
}

/** Release an object in the memo stack.
@return true if released */
bool
mtr_t::memo_release(const void* object, ulint type)
{
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_ad(is_active());

	/* We cannot release a page that has been written to in the
	middle of a mini-transaction. */
	ut_ad(!m_impl.m_modifications || type != MTR_MEMO_PAGE_X_FIX);

	Find		find(object, type);
	Iterate<Find>	iterator(find);

	if (!m_impl.m_memo.for_each_block_in_reverse(iterator)) {
		memo_slot_release(find.m_slot);
		return(true);
	}

	return(false);
}

/** Release a page latch.
@param[in]	ptr	pointer to within a page frame
@param[in]	type	object type: MTR_MEMO_PAGE_X_FIX, ... */
void
mtr_t::release_page(const void* ptr, mtr_memo_type_t type)
{
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_ad(is_active());

	/* We cannot release a page that has been written to in the
	middle of a mini-transaction. */
	ut_ad(!m_impl.m_modifications || type != MTR_MEMO_PAGE_X_FIX);

	FindPage		find(ptr, type);
	Iterate<FindPage>	iterator(find);

	if (!m_impl.m_memo.for_each_block_in_reverse(iterator)) {
		memo_slot_release(find.get_slot());
		return;
	}

	/* The page was not found! */
	ut_ad(0);
}

/** Prepare to write the mini-transaction log to the redo log buffer.
@return number of bytes to write in finish_write() */
ulint
mtr_t::Command::prepare_write()
{
	switch (m_impl->m_log_mode) {
	case MTR_LOG_SHORT_INSERTS:
		ut_ad(0);
		/* fall through (write no redo log) */
	case MTR_LOG_NO_REDO:
	case MTR_LOG_NONE:
		ut_ad(m_impl->m_log.size() == 0);
		log_mutex_enter();
		m_end_lsn = m_start_lsn = log_sys->lsn;
		return(0);
	case MTR_LOG_ALL:
		break;
	}

	ulint	len	= m_impl->m_log.size();
	ulint	n_recs	= m_impl->m_n_log_recs;
	ut_ad(len > 0);
	ut_ad(n_recs > 0);

	if (len > log_sys->buf_size / 2) {
		log_buffer_extend((len + 1) * 2);
	}

	ut_ad(m_impl->m_n_log_recs == n_recs);

	fil_space_t*	space = m_impl->m_user_space;

	if (space != NULL && is_system_or_undo_tablespace(space->id)) {
		/* Omit MLOG_FILE_NAME for predefined tablespaces. */
		space = NULL;
	}

	log_mutex_enter();

	if (fil_names_write_if_was_clean(space, m_impl->m_mtr)) {
		/* This mini-transaction was the first one to modify
		this tablespace since the latest checkpoint, so
		some MLOG_FILE_NAME records were appended to m_log. */
		ut_ad(m_impl->m_n_log_recs > n_recs);
		mlog_catenate_ulint(
			&m_impl->m_log, MLOG_MULTI_REC_END, MLOG_1BYTE);
		len = m_impl->m_log.size();
	} else {
		/* This was not the first time of dirtying a
		tablespace since the latest checkpoint. */

		ut_ad(n_recs == m_impl->m_n_log_recs);

		if (n_recs <= 1) {
			ut_ad(n_recs == 1);

			/* Flag the single log record as the
			only record in this mini-transaction. */
			*m_impl->m_log.front()->begin()
				|= MLOG_SINGLE_REC_FLAG;
		} else {
			/* Because this mini-transaction comprises
			multiple log records, append MLOG_MULTI_REC_END
			at the end. */

			mlog_catenate_ulint(
				&m_impl->m_log, MLOG_MULTI_REC_END,
				MLOG_1BYTE);
			len++;
		}
	}

	/* check and attempt a checkpoint if exceeding capacity */
	log_margin_checkpoint_age(len);

	return(len);
}

/** Append the redo log records to the redo log buffer
@param[in] len	number of bytes to write */
void
mtr_t::Command::finish_write(
	ulint	len)
{
	ut_ad(m_impl->m_log_mode == MTR_LOG_ALL);
	ut_ad(log_mutex_own());
	ut_ad(m_impl->m_log.size() == len);
	ut_ad(len > 0);

	if (m_impl->m_log.is_small()) {
		const mtr_buf_t::block_t*	front = m_impl->m_log.front();
		ut_ad(len <= front->used());

		m_end_lsn = log_reserve_and_write_fast(
			front->begin(), len, &m_start_lsn);

		if (m_end_lsn > 0) {
			return;
		}
	}

	/* Open the database log for log_write_low */
	m_start_lsn = log_reserve_and_open(len);

	mtr_write_log_t	write_log;
	m_impl->m_log.for_each_block(write_log);

	m_end_lsn = log_close();
}

/** Release the latches and blocks acquired by this mini-transaction */
void
mtr_t::Command::release_all()
{
	ReleaseAll release;
	Iterate<ReleaseAll> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);

	/* Note that we have released the latches. */
	m_locks_released = 1;
}

/** Release the latches acquired by this mini-transaction */
void
mtr_t::Command::release_latches()
{
	ReleaseLatches release;
	Iterate<ReleaseLatches> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);

	/* Note that we have released the latches. */
	m_locks_released = 1;
}

/** Release the blocks used in this mini-transaction */
void
mtr_t::Command::release_blocks()
{
	ReleaseBlocks release(m_start_lsn, m_end_lsn, m_impl->m_flush_observer);
	Iterate<ReleaseBlocks> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);
}

/** Write the redo log record, add dirty pages to the flush list and release
the resources. */
void
mtr_t::Command::execute()
{
	ut_ad(m_impl->m_log_mode != MTR_LOG_NONE);

	if (const ulint len = prepare_write()) {
		finish_write(len);
	}

	if (m_impl->m_made_dirty) {
		log_flush_order_mutex_enter();
	}

	/* It is now safe to release the log mutex because the
	flush_order mutex will ensure that we are the first one
	to insert into the flush list. */
	log_mutex_exit();

	m_impl->m_mtr->m_commit_lsn = m_end_lsn;

	release_blocks();

	if (m_impl->m_made_dirty) {
		log_flush_order_mutex_exit();
	}

	release_latches();

	release_resources();
}

/** Release the free extents that was reserved using
fsp_reserve_free_extents().  This is equivalent to calling
fil_space_release_free_extents().  This is intended for use
with index pages.
@param[in]	n_reserved	number of reserved extents */
void
mtr_t::release_free_extents(ulint n_reserved)
{
	fil_space_t*	space;

	ut_ad(m_impl.m_undo_space == NULL);

	if (m_impl.m_user_space != NULL) {

		ut_ad(m_impl.m_user_space->id
		      == m_impl.m_user_space_id);
		ut_ad(memo_contains(get_memo(), &m_impl.m_user_space->latch,
				    MTR_MEMO_X_LOCK));

		space = m_impl.m_user_space;
	} else {

		ut_ad(m_impl.m_sys_space->id == TRX_SYS_SPACE);
		ut_ad(memo_contains(get_memo(), &m_impl.m_sys_space->latch,
				    MTR_MEMO_X_LOCK));

		space = m_impl.m_sys_space;
	}

	space->release_free_extents(n_reserved);
}

#ifdef UNIV_DEBUG
/** Check if memo contains the given item.
@return	true if contains */
bool
mtr_t::memo_contains(
	mtr_buf_t*	memo,
	const void*	object,
	ulint		type)
{
	Find		find(object, type);
	Iterate<Find>	iterator(find);

	return(!memo->for_each_block_in_reverse(iterator));
}

/** Debug check for flags */
struct FlaggedCheck {
	FlaggedCheck(const void* ptr, ulint flags)
		:
		m_ptr(ptr),
		m_flags(flags)
	{
		// Do nothing
	}

	bool operator()(const mtr_memo_slot_t* slot) const
	{
		if (m_ptr == slot->object && (m_flags & slot->type)) {
			return(false);
		}

		return(true);
	}

	const void*	m_ptr;
	ulint		m_flags;
};

/** Check if memo contains the given item.
@param object		object to search
@param flags		specify types of object (can be ORred) of
			MTR_MEMO_PAGE_S_FIX ... values
@return true if contains */
bool
mtr_t::memo_contains_flagged(const void* ptr, ulint flags) const
{
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_ad(is_committing() || is_active());

	FlaggedCheck		check(ptr, flags);
	Iterate<FlaggedCheck>	iterator(check);

	return(!m_impl.m_memo.for_each_block_in_reverse(iterator));
}

/** Check if memo contains the given page.
@param[in]	ptr	pointer to within buffer frame
@param[in]	flags	specify types of object with OR of
			MTR_MEMO_PAGE_S_FIX... values
@return	the block
@retval	NULL	if not found */
buf_block_t*
mtr_t::memo_contains_page_flagged(
	const byte*	ptr,
	ulint		flags) const
{
	FindPage		check(ptr, flags);
	Iterate<FindPage>	iterator(check);

	return(m_impl.m_memo.for_each_block_in_reverse(iterator)
	       ? NULL : check.get_block());
}

/** Mark the given latched page as modified.
@param[in]	ptr	pointer to within buffer frame */
void
mtr_t::memo_modify_page(const byte* ptr)
{
	buf_block_t*	block = memo_contains_page_flagged(
		ptr, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX);
	ut_ad(block != NULL);

	if (!memo_contains(get_memo(), block, MTR_MEMO_MODIFY)) {
		memo_push(block, MTR_MEMO_MODIFY);
	}
}

/** Print info of an mtr handle. */
void
mtr_t::print() const
{
	ib::info() << "Mini-transaction handle: memo size "
		<< m_impl.m_memo.size() << " bytes log size "
		<< get_log()->size() << " bytes";
}

#endif /* UNIV_DEBUG */
