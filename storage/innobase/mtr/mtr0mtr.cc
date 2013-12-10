/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
#include "page0types.h"
#include "mtr0log.h"
#include "log0log.h"

#include "log0recv.h"

#ifdef UNIV_NONINL
#include "mtr0mtr.ic"
#endif /* UNIV_NONINL */

/**
Iterate over a memo block in reverse. */
template <typename Functor>
struct Iterate {

	/** Release specific object */
	explicit Iterate(Functor& functor)
		:
		m_functor(functor)
	{
		/* Do nothing */
	}

	/**
	@return false if the functor returns false. */
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

	/**
	@return false if the object was found. */
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

/**
Releases latches and decrements the buffer fix count.
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

/**
Unfix a page, does not release the latches on the page.
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
/**
Releases latches represented by a slot.
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

/**
Release the latches acquired by the mini-transaction. */
struct ReleaseLatches {

	/**
	@return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {
			memo_latch_release(slot);
		}

		return(true);
	}
};

/**
Release the latches and blocks acquired by the mini-transaction. */
struct ReleaseAll {

	/**
	@return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {
			memo_slot_release(slot);
		}

		return(true);
	}
};

/**
Check that all slots have been handled. */
struct DebugCheck {

	/**
	@return true always. */
	bool operator()(const mtr_memo_slot_t* slot) const
	{
		ut_a(slot->object == NULL);
		return(true);
	}
};

/**
Release a resource acquired by the mini-transaction. */
struct ReleaseBlocks {

	/** Release specific object */
	ReleaseBlocks(lsn_t start_lsn, lsn_t end_lsn)
		:
		m_end_lsn(end_lsn),
		m_start_lsn(start_lsn)
	{
		/* Do nothing */
	}

	/**
	Add the modified page to the buffer flush list. */
	void add_dirty_page_to_flush_list(mtr_memo_slot_t* slot) const
	{
		ut_ad(m_end_lsn > 0);
		ut_ad(m_start_lsn > 0);

		buf_block_t*	block;

		block = reinterpret_cast<buf_block_t*>(slot->object);

		buf_flush_note_modification(block, m_start_lsn, m_end_lsn);
	}

	/**
	@return true always. */
	bool operator()(mtr_memo_slot_t* slot) const
	{
		if (slot->object != NULL) {

			if (slot->type == MTR_MEMO_PAGE_X_FIX
			    || slot->type == MTR_MEMO_PAGE_SX_FIX) {

				add_dirty_page_to_flush_list(slot);
			}
		}

		return(true);
	}

	/** Mini-transaction REDO start LSN */
	lsn_t		m_end_lsn;

	/** Mini-transaction REDO end LSN */
	lsn_t		m_start_lsn;
};

class mtr_t::Command {
public:
	/**
	Command takes ownership of the m_impl member of mtr and is responsible
	for deleting it.
	@param mtr	mini-transaction instance */
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

	/**
	Destructor */
	~Command()
	{
		ut_ad(m_impl == 0);
	}

	/**
	Write the redo log record, add dirty pages to the flush list and
	release the resources. */
	void execute();

	/**
	Release the blocks used in this mini-transaction. */
	void release_blocks();

	/**
	Release the latches acquired by the mini-transaction. */
	void release_latches();

	/**
	Release both the latches and blocks used in the mini-transaction. */
	void release_all();

	/**
	Release the resources */
	void release_resources();

private:
	/**
	Write the redo log record */
	void write();

private:
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

/**
Checks if a mini-transaction is dirtying a clean page.
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

/**
Write the block contents to the REDO log */
struct mtr_write_log_t {
	/**
	@return true - never fails */
	bool operator()(const mtr_buf_t::block_t* block) const
	{
		log_write_low(block->begin(), block->used());

		return(true);
	}
};

/**
Starts a mini-transaction.
@param sync		true if it is a synchronouse mini-transaction
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
	m_impl.m_n_freed_pages = 0;

	ut_d(m_impl.m_state = MTR_STATE_ACTIVE);
	ut_d(m_impl.m_magic_n = MTR_MAGIC_N);
}

/**
Release the resources */

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

	ut_d(m_impl->m_state = MTR_STATE_COMMITTED);

	m_impl = 0;
}

/**
Commits a mini-transaction. */

void
mtr_t::commit()
{
	ut_ad(is_active());
	ut_ad(!is_inside_ibuf());
	ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
	ut_d(m_impl.m_state = MTR_STATE_COMMITTING);

	/* This is a dirty read, for debugging. */
	ut_ad(!recv_no_log_write);

	if (m_impl.m_modifications
	    && (m_impl.m_n_log_recs > 0
		|| m_impl.m_log_mode == MTR_LOG_NO_REDO)) {

		ut_ad(!srv_read_only_mode);

		Command	cmd(this);

		cmd.execute();

	} else {
		Command	cmd(this);

		cmd.release_all();
		cmd.release_resources();
	}
}

/**
@return the commit lsn */

lsn_t
mtr_t::commit_lsn() const
{
	ut_a(m_commit_lsn != 0);

	return(m_commit_lsn);
}

/**
Releases an object in the memo stack.
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

/**
Write the redo log record */

void
mtr_t::Command::write()
{
	byte*	data = m_impl->m_log.front()->begin();

	if (m_impl->m_n_log_recs > 1) {
		mlog_catenate_ulint(
			&m_impl->m_log, MLOG_MULTI_REC_END, MLOG_1BYTE);
	} else {
		*data = (byte)((ulint) *data | MLOG_SINGLE_REC_FLAG);
	}

	bool	own_mutex;

	if (m_impl->m_log.is_small()) {
		ulint	len = (m_impl->m_log_mode != MTR_LOG_NO_REDO)
			? m_impl->m_log.front()->used() : 0;

		m_end_lsn = log_reserve_and_write_fast(data, len, &m_start_lsn);

		if (m_end_lsn > 0) {
			return;
		}

		own_mutex = true;
	} else {
		own_mutex = false;
	}

	/* Open the database log for log_write_low */
	m_start_lsn = log_reserve_and_open(m_impl->m_log.size(), own_mutex);

	if (m_impl->m_log_mode == MTR_LOG_ALL) {
		mtr_write_log_t	write_log;

		m_impl->m_log.for_each_block(write_log);

	} else {
		ut_ad(m_impl->m_log_mode == MTR_LOG_NONE
		      || m_impl->m_log_mode == MTR_LOG_NO_REDO);

		/* Do nothing */
	}

	m_end_lsn = log_close();
}

/**
Release the latches and blocks acquired by this mini-transaction */

void
mtr_t::Command::release_all()
{
	ReleaseAll release;
	Iterate<ReleaseAll> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);

	/* Note that we have released the latches. */
	m_locks_released = 1;
}

/**
Release the latches acquired by this mini-transaction */

void
mtr_t::Command::release_latches()
{
	ReleaseLatches release;
	Iterate<ReleaseLatches> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);

	/* Note that we have released the latches. */
	m_locks_released = 1;
}

/**
Release the blocks used in this mini-transaction */

void
mtr_t::Command::release_blocks()
{
	ReleaseBlocks release(m_start_lsn, m_end_lsn);
	Iterate<ReleaseBlocks> iterator(release);

	m_impl->m_memo.for_each_block_in_reverse(iterator);
}

/**
Write the redo log record, add dirty pages to the flush list and release
the resources. */

void
mtr_t::Command::execute()
{
	write();

	if (m_impl->m_made_dirty) {
		log_flush_order_mutex_enter();
	}

	/* It is now safe to release the log mutex because the
	flush_order mutex will ensure that we are the first one
	to insert into the flush list. */
	log_release();

	m_impl->m_mtr->m_commit_lsn = m_end_lsn;

	release_blocks();

	if (m_impl->m_made_dirty) {
		log_flush_order_mutex_exit();
	}

	release_latches();

	release_resources();
}

#ifdef UNIV_DEBUG
/**
Checks if memo contains the given item.
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

/**
Checks if memo contains the given page.
@param memo		info
@param ptr		record
@param type		type of
@return	true if contains */

bool
mtr_t::memo_contains_page(mtr_buf_t* memo, const byte* ptr, ulint type)
{
	return(memo_contains(memo, buf_block_align(ptr), type));
}

/**
Debug check for flags */
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

/**
Checks if memo contains the given item.
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

/**
Checks if memo contains the given page.
@param ptr		buffer frame
@param flags		specify types of object with OR of
			MTR_MEMO_PAGE_S_FIX... values
@return true if contains */

bool
mtr_t::memo_contains_page_flagged(
	const byte*	ptr,
	ulint		flags) const
{
	return(memo_contains_flagged(buf_block_align(ptr), flags));
}

/**
Prints info of an mtr handle. */

void
mtr_t::print() const
{
	ib_logf(IB_LOG_LEVEL_INFO,
		"Mini-transaction handle: memo size %lu bytes "
		"log size %lu bytes",
		m_impl.m_memo.size(), get_log()->size());
}

#endif /* UNIV_DEBUG */
