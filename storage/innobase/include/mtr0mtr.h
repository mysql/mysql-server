/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.
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
@file include/mtr0mtr.h
Mini-transaction buffer

Created 11/26/1995 Heikki Tuuri
*******************************************************/

#ifndef mtr0mtr_h
#define mtr0mtr_h

#include "univ.i"
#include "log0types.h"
#include "mtr0types.h"
#include "buf0types.h"
#include "trx0types.h"
#include "dyn0buf.h"

/** Start a mini-transaction. */
#define mtr_start(m)		(m)->start()

/** Start a synchronous mini-transaction */
#define mtr_start_sync(m)	(m)->start(true)

/** Start an asynchronous read-only mini-transaction */
#define mtr_start_ro(m)	(m)->start(true, true)

/** Commit a mini-transaction. */
#define mtr_commit(m)		(m)->commit()

/** Set and return a savepoint in mtr.
@return	savepoint */
#define mtr_set_savepoint(m)	(m)->get_savepoint()

/** Release the (index tree) s-latch stored in an mtr memo after a
savepoint. */
#define mtr_release_s_latch_at_savepoint(m, s, l)			\
				(m)->release_s_latch_at_savepoint((s), (l))

/** Get the logging mode of a mini-transaction.
@return	logging mode: MTR_LOG_NONE, ... */
#define mtr_get_log_mode(m)	(m)->get_log_mode()

/** Change the logging mode of a mini-transaction.
@return	old mode */
#define mtr_set_log_mode(m, d)	(m)->set_log_mode((d))

/** Get the flush observer of a mini-transaction.
@return flush observer object */
#define mtr_get_flush_observer(m)	(m)->get_flush_observer()

/** Set the flush observer of a mini-transaction. */
#define mtr_set_flush_observer(m, d)	(m)->set_flush_observer((d))

/** Read 1 - 4 bytes from a file page buffered in the buffer pool.
@return	value read */
#define mtr_read_ulint(p, t, m)	(m)->read_ulint((p), (t))

/** Release an object in the memo stack.
@return true if released */
#define mtr_memo_release(m, o, t)					\
				(m)->memo_release((o), (t))

#ifdef UNIV_DEBUG

/** Check if memo contains the given item ignore if table is intrinsic
@return TRUE if contains or table is intrinsic. */
#define mtr_is_block_fix(m, o, t, table)				\
	(mtr_memo_contains(m, o, t)					\
	 || dict_table_is_intrinsic(table))

/** Check if memo contains the given page ignore if table is intrinsic
@return TRUE if contains or table is intrinsic. */
#define mtr_is_page_fix(m, p, t, table)					\
	(mtr_memo_contains_page(m, p, t)				\
	 || dict_table_is_intrinsic(table))

/** Check if memo contains the given item.
@return	TRUE if contains */
#define mtr_memo_contains(m, o, t)					\
				(m)->memo_contains((m)->get_memo(), (o), (t))

/** Check if memo contains the given page.
@return	TRUE if contains */
#define mtr_memo_contains_page(m, p, t)					\
	(m)->memo_contains_page_flagged((p), (t))
#endif /* UNIV_DEBUG */

/** Print info of an mtr handle. */
#define mtr_print(m)		(m)->print()

/** Return the log object of a mini-transaction buffer.
@return	log */
#define mtr_get_log(m)		(m)->get_log()

/** Push an object to an mtr memo stack. */
#define mtr_memo_push(m, o, t)	(m)->memo_push(o, t)

/** Lock an rw-lock in s-mode. */
#define mtr_s_lock(l, m)	(m)->s_lock((l), __FILE__, __LINE__)

/** Lock an rw-lock in x-mode. */
#define mtr_x_lock(l, m)	(m)->x_lock((l), __FILE__, __LINE__)

/** Lock a tablespace in x-mode. */
#define mtr_x_lock_space(s, m)	(m)->x_lock_space((s), __FILE__, __LINE__)

/** Lock an rw-lock in sx-mode. */
#define mtr_sx_lock(l, m)	(m)->sx_lock((l), __FILE__, __LINE__)

#define mtr_memo_contains_flagged(m, p, l)				\
				(m)->memo_contains_flagged((p), (l))

#define mtr_memo_contains_page_flagged(m, p, l)				\
				(m)->memo_contains_page_flagged((p), (l))

#define mtr_release_block_at_savepoint(m, s, b)				\
				(m)->release_block_at_savepoint((s), (b))

#define mtr_block_sx_latch_at_savepoint(m, s, b)			\
				(m)->sx_latch_at_savepoint((s), (b))

#define mtr_block_x_latch_at_savepoint(m, s, b)				\
				(m)->x_latch_at_savepoint((s), (b))

/** Check if a mini-transaction is dirtying a clean page.
@param b	block being x-fixed
@return true if the mtr is dirtying a clean page. */
#define mtr_block_dirtied(b)	mtr_t::is_block_dirtied((b))

/** Forward declaration of a tablespace object */
struct fil_space_t;

/** Append records to the system-wide redo log buffer.
@param[in]	log	redo log records */
void
mtr_write_log(
	const mtr_buf_t*	log);

/** Mini-transaction memo stack slot. */
struct mtr_memo_slot_t {
	/** pointer to the object */
	void*		object;

	/** type of the stored object (MTR_MEMO_S_LOCK, ...) */
	ulint		type;
};

/** Mini-transaction handle and buffer */
struct mtr_t {

	/** State variables of the mtr */
	struct Impl {

		/** memo stack for locks etc. */
		mtr_buf_t	m_memo;

		/** mini-transaction log */
		mtr_buf_t	m_log;

		/** true if mtr has made at least one buffer pool page dirty */
		bool		m_made_dirty;

		/** true if inside ibuf changes */
		bool		m_inside_ibuf;

		/** true if the mini-transaction modified buffer pool pages */
		bool		m_modifications;

		/** Count of how many page initial log records have been
		written to the mtr log */
		ib_uint32_t	m_n_log_recs;

		/** specifies which operations should be logged; default
		value MTR_LOG_ALL */
		mtr_log_t	m_log_mode;
#ifdef UNIV_DEBUG
		/** Persistent user tablespace associated with the
		mini-transaction, or 0 (TRX_SYS_SPACE) if none yet */
		ulint		m_user_space_id;
#endif /* UNIV_DEBUG */
		/** User tablespace that is being modified by the
		mini-transaction */
		fil_space_t*	m_user_space;
		/** Undo tablespace that is being modified by the
		mini-transaction */
		fil_space_t*	m_undo_space;
		/** System tablespace if it is being modified by the
		mini-transaction */
		fil_space_t*	m_sys_space;

		/** State of the transaction */
		mtr_state_t	m_state;

		/** Flush Observer */
		FlushObserver*	m_flush_observer;

#ifdef UNIV_DEBUG
		/** For checking corruption. */
		ulint		m_magic_n;
#endif /* UNIV_DEBUG */

		/** Owning mini-transaction */
		mtr_t*		m_mtr;
	};

	mtr_t()
	{
		m_impl.m_state = MTR_STATE_INIT;
	}

	~mtr_t() { }

	/** Release the free extents that was reserved using
	fsp_reserve_free_extents().  This is equivalent to calling
	fil_space_release_free_extents().  This is intended for use
	with index pages.
	@param[in]	n_reserved	number of reserved extents */
	void release_free_extents(ulint n_reserved);

	/** Start a mini-transaction.
	@param sync		true if it is a synchronous mini-transaction
	@param read_only	true if read only mini-transaction */
	void start(bool sync = true, bool read_only = false);

	/** @return whether this is an asynchronous mini-transaction. */
	bool is_async() const
	{
		return(!m_sync);
	}

	/** Request a future commit to be synchronous. */
	void set_sync()
	{
		m_sync = true;
	}

	/** Commit the mini-transaction. */
	void commit();

	/** Commit a mini-transaction that did not modify any pages,
	but generated some redo log on a higher level, such as
	MLOG_FILE_NAME records and a MLOG_CHECKPOINT marker.
	The caller must invoke log_mutex_enter() and log_mutex_exit().
	This is to be used at log_checkpoint().
	@param[in]	checkpoint_lsn		the LSN of the log checkpoint
	@param[in]	write_mlog_checkpoint	Write MLOG_CHECKPOINT marker
						if it is enabled. */
	void commit_checkpoint(
		lsn_t	checkpoint_lsn,
		bool	write_mlog_checkpoint);

	/** Return current size of the buffer.
	@return	savepoint */
	ulint get_savepoint() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_ad(is_active());
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(m_impl.m_memo.size());
	}

	/** Release the (index tree) s-latch stored in an mtr memo after a
	savepoint.
	@param savepoint	value returned by @see set_savepoint.
	@param lock		latch to release */
	inline void release_s_latch_at_savepoint(
		ulint		savepoint,
		rw_lock_t*	lock);

	/** Release the block in an mtr memo after a savepoint. */
	inline void release_block_at_savepoint(
		ulint		savepoint,
		buf_block_t*	block);

	/** SX-latch a not yet latched block after a savepoint. */
	inline void sx_latch_at_savepoint(ulint savepoint, buf_block_t* block);

	/** X-latch a not yet latched block after a savepoint. */
	inline void x_latch_at_savepoint(ulint savepoint, buf_block_t*	block);

	/** Get the logging mode.
	@return	logging mode */
	inline mtr_log_t get_log_mode() const
		MY_ATTRIBUTE((warn_unused_result));

	/** Change the logging mode.
	@param mode	 logging mode
	@return	old mode */
	inline mtr_log_t set_log_mode(mtr_log_t mode);

	/** Note that the mini-transaction is modifying the system tablespace
	(for example, for the change buffer or for undo logs)
	@return the system tablespace */
	fil_space_t* set_sys_modified()
	{
		if (!m_impl.m_sys_space) {
			lookup_sys_space();
		}
		return(m_impl.m_sys_space);
	}

	/** Copy the tablespaces associated with the mini-transaction
	(needed for generating MLOG_FILE_NAME records)
	@param[in]	mtr	mini-transaction that may modify
	the same set of tablespaces as this one */
	void set_spaces(const mtr_t& mtr)
	{
		ut_ad(m_impl.m_user_space_id == TRX_SYS_SPACE);
		ut_ad(!m_impl.m_user_space);
		ut_ad(!m_impl.m_undo_space);
		ut_ad(!m_impl.m_sys_space);

		ut_d(m_impl.m_user_space_id = mtr.m_impl.m_user_space_id);
		m_impl.m_user_space = mtr.m_impl.m_user_space;
		m_impl.m_undo_space = mtr.m_impl.m_undo_space;
		m_impl.m_sys_space = mtr.m_impl.m_sys_space;
	}

	/** Set the tablespace associated with the mini-transaction
	(needed for generating a MLOG_FILE_NAME record)
	@param[in]	space_id	user or system tablespace ID
	@return	the tablespace */
	fil_space_t* set_named_space(ulint space_id)
	{
		ut_ad(m_impl.m_user_space_id == TRX_SYS_SPACE);
		ut_d(m_impl.m_user_space_id = space_id);
		if (space_id == TRX_SYS_SPACE) {
			return(set_sys_modified());
		} else {
			lookup_user_space(space_id);
			return(m_impl.m_user_space);
		}
	}

	/** Set the tablespace associated with the mini-transaction
	(needed for generating a MLOG_FILE_NAME record)
	@param[in]	space	user or system tablespace */
	void set_named_space(fil_space_t* space);

#ifdef UNIV_DEBUG
	/** Check the tablespace associated with the mini-transaction
	(needed for generating a MLOG_FILE_NAME record)
	@param[in]	space	tablespace
	@return whether the mini-transaction is associated with the space */
	bool is_named_space(ulint space) const;
#endif /* UNIV_DEBUG */

	/** Read 1 - 4 bytes from a file page buffered in the buffer pool.
	@param ptr	pointer from where to read
	@param type)	MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES
	@return	value read */
	inline ulint read_ulint(const byte* ptr, mlog_id_t type) const
		MY_ATTRIBUTE((warn_unused_result));

	/** Locks a rw-latch in S mode.
	NOTE: use mtr_s_lock().
	@param lock	rw-lock
	@param file	file name from where called
	@param line	line number in file */
	inline void s_lock(rw_lock_t* lock, const char* file, ulint line);

	/** Locks a rw-latch in X mode.
	NOTE: use mtr_x_lock().
	@param lock	rw-lock
	@param file	file name from where called
	@param line	line number in file */
	inline void x_lock(rw_lock_t* lock, const char*	file, ulint line);

	/** Locks a rw-latch in X mode.
	NOTE: use mtr_sx_lock().
	@param lock	rw-lock
	@param file	file name from where called
	@param line	line number in file */
	inline void sx_lock(rw_lock_t* lock, const char* file, ulint line);

	/** Acquire a tablespace X-latch.
	NOTE: use mtr_x_lock_space().
	@param[in]	space_id	tablespace ID
	@param[in]	file		file name from where called
	@param[in]	line		line number in file
	@return the tablespace object (never NULL) */
	fil_space_t* x_lock_space(
		ulint		space_id,
		const char*	file,
		ulint		line);

	/** Release an object in the memo stack.
	@param object	object
	@param type	object type: MTR_MEMO_S_LOCK, ...
	@return bool if lock released */
	bool memo_release(const void* object, ulint type);
	/** Release a page latch.
	@param[in]	ptr	pointer to within a page frame
	@param[in]	type	object type: MTR_MEMO_PAGE_X_FIX, ... */
	void release_page(const void* ptr, mtr_memo_type_t type);

	/** Note that the mini-transaction has modified data. */
	void set_modified()
	{
		m_impl.m_modifications = true;
	}

	/** Set the state to not-modified. This will not log the
	changes.  This is only used during redo log apply, to avoid
	logging the changes. */
	void discard_modifications()
	{
		m_impl.m_modifications = false;
	}

	/** Get the LSN of commit().
	@return the commit LSN
	@retval 0 if the transaction only modified temporary tablespaces */
	lsn_t commit_lsn() const
	{
		ut_ad(has_committed());
		return(m_commit_lsn);
	}

	/** Note that we are inside the change buffer code. */
	void enter_ibuf()
	{
		m_impl.m_inside_ibuf = true;
	}

	/** Note that we have exited from the change buffer code. */
	void exit_ibuf()
	{
		m_impl.m_inside_ibuf = false;
	}

	/** @return true if we are inside the change buffer code */
	bool is_inside_ibuf() const
	{
		return(m_impl.m_inside_ibuf);
	}

	/*
	@return true if the mini-transaction is active */
	bool is_active() const
	{
		return(m_impl.m_state == MTR_STATE_ACTIVE);
	}

	/** Get flush observer
	@return flush observer */
	FlushObserver* get_flush_observer() const
	{
		return(m_impl.m_flush_observer);
	}

	/** Set flush observer
	@param[in]	observer	flush observer */
	void set_flush_observer(FlushObserver*	observer)
	{
		ut_ad(observer == NULL
		      || m_impl.m_log_mode == MTR_LOG_NO_REDO);

		m_impl.m_flush_observer = observer;
	}

#ifdef UNIV_DEBUG
	/** Check if memo contains the given item.
	@param memo	memo stack
	@param object,	object to search
	@param type	type of object
	@return	true if contains */
	static bool memo_contains(
		mtr_buf_t*	memo,
		const void*	object,
		ulint		type)
		MY_ATTRIBUTE((warn_unused_result));

	/** Check if memo contains the given item.
	@param object		object to search
	@param flags		specify types of object (can be ORred) of
				MTR_MEMO_PAGE_S_FIX ... values
	@return true if contains */
	bool memo_contains_flagged(const void* ptr, ulint flags) const;

	/** Check if memo contains the given page.
	@param[in]	ptr	pointer to within buffer frame
	@param[in]	flags	specify types of object with OR of
				MTR_MEMO_PAGE_S_FIX... values
	@return	the block
	@retval	NULL	if not found */
	buf_block_t* memo_contains_page_flagged(
		const byte*	ptr,
		ulint		flags) const;

	/** Mark the given latched page as modified.
	@param[in]	ptr	pointer to within buffer frame */
	void memo_modify_page(const byte* ptr);

	/** Print info of an mtr handle. */
	void print() const;

	/** @return true if the mini-transaction has committed */
	bool has_committed() const
	{
		return(m_impl.m_state == MTR_STATE_COMMITTED);
	}

	/** @return true if the mini-transaction is committing */
	bool is_committing() const
	{
		return(m_impl.m_state == MTR_STATE_COMMITTING);
	}

	/** @return true if mini-transaction contains modifications. */
	bool has_modifications() const
	{
		return(m_impl.m_modifications);
	}

	/** @return the memo stack */
	const mtr_buf_t* get_memo() const
	{
		return(&m_impl.m_memo);
	}

	/** @return the memo stack */
	mtr_buf_t* get_memo()
	{
		return(&m_impl.m_memo);
	}
#endif /* UNIV_DEBUG */

	/** @return true if a record was added to the mini-transaction */
	bool is_dirty() const
	{
		return(m_impl.m_made_dirty);
	}

	/** Note that a record has been added to the log */
	void added_rec()
	{
		++m_impl.m_n_log_recs;
	}

	/** Get the buffered redo log of this mini-transaction.
	@return	redo log */
	const mtr_buf_t* get_log() const
	{
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(&m_impl.m_log);
	}

	/** Get the buffered redo log of this mini-transaction.
	@return	redo log */
	mtr_buf_t* get_log()
	{
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(&m_impl.m_log);
	}

	/** Push an object to an mtr memo stack.
	@param object	object
	@param type	object type: MTR_MEMO_S_LOCK, ... */
	inline void memo_push(void* object, mtr_memo_type_t type);

	/** Check if this mini-transaction is dirtying a clean page.
	@param block	block being x-fixed
	@return true if the mtr is dirtying a clean page. */
	static bool is_block_dirtied(const buf_block_t* block)
		MY_ATTRIBUTE((warn_unused_result));

private:
	/** Look up the system tablespace. */
	void lookup_sys_space();
	/** Look up the user tablespace.
	@param[in]	space_id	tablespace ID  */
	void lookup_user_space(ulint space_id);

	class Command;

	friend class Command;

private:
	Impl			m_impl;

	/** LSN at commit time */
	volatile lsn_t		m_commit_lsn;

	/** true if it is synchronous mini-transaction */
	bool			m_sync;
};

#ifndef UNIV_NONINL
#include "mtr0mtr.ic"
#endif /* UNIV_NOINL */

#endif /* mtr0mtr_h */
