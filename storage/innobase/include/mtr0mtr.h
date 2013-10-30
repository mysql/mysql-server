/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.
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
#include "dyn0buf.h"

/**
Starts a mini-transaction. */
#define mtr_start(m)		(m)->start()

/**
Starts a synchronous mini-transaction */
#define mtr_start_sync(m)	(m)->start(true)

/**
Starts a asynchronous read-only mini-transaction */
#define mtr_start_ro(m)	(m)->start(true, true)

/**
Commits a mini-transaction. */
#define mtr_commit(m)		(m)->commit()

/**
Sets and returns a savepoint in mtr.
@return	savepoint */
#define mtr_set_savepoint(m)	(m)->get_savepoint()

/**
Releases the (index tree) s-latch stored in an mtr memo after a
savepoint. */
#define mtr_release_s_latch_at_savepoint(m, s, l)			\
				(m)->release_s_latch_at_savepoint((s), (l))

/**
Gets the logging mode of a mini-transaction.
@return	logging mode: MTR_LOG_NONE, ... */
#define mtr_get_log_mode(m)	(m)->get_log_mode()

/**
Changes the logging mode of a mini-transaction.
@return	old mode */
#define mtr_set_log_mode(m, d)	(m)->set_log_mode((d))

/**
Reads 1 - 4 bytes from a file page buffered in the buffer pool.
@return	value read */
#define mtr_read_ulint(p, t, m)	(m)->read_ulint((p), (t))

/**
Releases an object in the memo stack.
@return true if released */
#define mtr_memo_release(m, o, t)					\
				(m)->memo_release((o), (t))

#ifdef UNIV_DEBUG
/**
Checks if memo contains the given item.
@return	TRUE if contains */
#define mtr_memo_contains(m, o, t)					\
				(m)->memo_contains((m)->get_memo(), (o), (t))

/**
Checks if memo contains the given page.
@return	TRUE if contains */
#define mtr_memo_contains_page(m, p, t)					\
				(m)->memo_contains_page(		\
					(m)->get_memo(), (p), (t))
#endif /* UNIV_DEBUG */

/**
Prints info of an mtr handle. */
#define mtr_print(m)		(m)->print()

/**
Returns the log object of a mini-transaction buffer.
@return	log */
#define mtr_get_log(m)		(m)->get_log()

/**
Pushes an object to an mtr memo stack. */
#define mtr_memo_push(m, o, t)	(m)->memo_push(o, t)

/**
This macro locks an rw-lock in s-mode. */
#define mtr_s_lock(l, m)	(m)->s_lock((l), __FILE__, __LINE__)

/**
This macro locks an rw-lock in x-mode. */
#define mtr_x_lock(l, m)	(m)->x_lock((l), __FILE__, __LINE__)

/**
This macro locks an rw-lock in sx-mode. */
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

/**
Checks if a mini-transaction is dirtying a clean page.
@param b	block being x-fixed
@return true if the mtr is dirtying a clean page. */
#define mtr_block_dirtied(b)	mtr_t::is_block_dirtied((b))

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

		/** Number of pages that have been freed in this
		mini-transaction */
		ib_uint32_t	m_n_freed_pages;

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
		/** State of the transaction */
		mtr_state_t	m_state;

		/** For checking corruption. */
		ulint		m_magic_n;
#endif /* UNIV_DEBUG */

		/** Owning mini-transaction */
		mtr_t*		m_mtr;
	};

	mtr_t() { }

	~mtr_t() { }

	/**
	Starts a mini-transaction.
	@param sync		true if it is a synchronouse mini-transaction
	@param read_only	true if read only mini-transaction */
	void start(bool sync = true, bool read_only = false);

	/**
	@return true if it is an asynchronouse mini-transaction. */
	bool is_async() const
	{
		return(!m_sync);
	}

	/**
	For the mini-transaction to do a synchornous commit. */
	void set_sync()
	{
		m_sync = true;
	}

	/**
	Commits a mini-transaction. */
	void commit();

	/**
	Returns current size of the buffer.
	@return	savepoint */
	ulint get_savepoint()
		__attribute__((warn_unused_result))
	{
		ut_ad(is_active());
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(m_impl.m_memo.size());
	}

	/**
	Releases the (index tree) s-latch stored in an mtr memo after a
	savepoint.
	@param savepoint	value returned by @see set_savepoint.
	@param lock		latch to release */
	inline void release_s_latch_at_savepoint(
		ulint		savepoint,
		rw_lock_t*	lock);

	/**
	Releases the block in an mtr memo after a savepoint. */
	inline void release_block_at_savepoint(
		ulint		savepoint,
		buf_block_t*	block);

	/**
	SX-latches the not yet latched block after a savepoint. */
	inline void sx_latch_at_savepoint(ulint savepoint, buf_block_t* block);

	/**
	X-latches the not yet latched block after a savepoint. */
	inline void x_latch_at_savepoint(ulint savepoint, buf_block_t*	block);

	/**
	Gets the logging mode of a mini-transaction.
	@return	logging mode: MTR_LOG_NONE, ... */
	inline mtr_log_t get_log_mode() const
		__attribute__((warn_unused_result));

	/**
	Changes the logging mode of a mini-transaction.
	@param mode	 logging mode: MTR_LOG_NONE, ...
	@return	old mode */
	inline mtr_log_t set_log_mode(mtr_log_t mode);

	/**
	Reads 1 - 4 bytes from a file page buffered in the buffer pool.
	@param ptr	pointer from where to read
	@param type)	MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES
	@return	value read */
	inline ulint read_ulint(const byte* ptr, mlog_id_t type) const
		__attribute__((warn_unused_result));

	/**
	NOTE! Use the macro above!
	Locks a lock in s-mode.
	@param lock	rw-lock
	@param file	file name from where called
	@param line	line number in file */
	inline void s_lock(rw_lock_t* lock, const char* file, ulint line);

	/**
	NOTE! Use the macro above!
	Locks a lock in x-mode.
	@param lock	rw-lock
	@param file	file name from where called
	@param line	line number in file */
	inline void x_lock(rw_lock_t* lock, const char*	file, ulint line);

	/**
	Locks a lock in sx-mode. */
	inline void sx_lock(rw_lock_t* lock, const char* file, ulint line);

	/**
	Releases an object in the memo stack.
	@param object	object
	@param type	object type: MTR_MEMO_S_LOCK, ...
	@return bool if lock released */
	bool memo_release(const void* object, ulint type);

	/**
	Set the state to modified. */
	void set_modified()
	{
		m_impl.m_modifications = true;
	}

	/**
	Set the state to not-modified. This will not log the changes */
	void discard_modifications()
	{
		m_impl.m_modifications = false;
	}

	/**
	@return the commit LSN */
	lsn_t commit_lsn() const;

	/**
	Note that we are inside the change buffer code */
	void enter_ibuf()
	{
		m_impl.m_inside_ibuf = true;
	}

	/**
	Note that we have exited from the change buffer code */
	void exit_ibuf()
	{
		m_impl.m_inside_ibuf = false;
	}

	/**
	@return true if we are inside the change buffer code */
	bool is_inside_ibuf() const
	{
		return(m_impl.m_inside_ibuf);
	}

	/**
	Note that some  pages were freed */
	void add_freed_pages()
	{
		++m_impl.m_n_freed_pages;
	}

	/**
	@return true if mini-transaction freed pages */
	bool has_freed_pages() const
	{
		return(get_freed_page_count() > 0);
	}

	/**
	@return the number of freed pages */
	ulint get_freed_page_count() const
	{
		return(m_impl.m_n_freed_pages);
	}

#ifdef UNIV_DEBUG
	/**
	Checks if memo contains the given item.
	@param memo	memo stack
	@param object,	object to search
	@param type	type of object
	@return	true if contains */
	static bool memo_contains(
		mtr_buf_t*	memo,
		const void*	object,
		ulint		type)
		__attribute__((warn_unused_result));

	/**
	Checks if memo contains the given page.
	@param memo	memo stack
	@param ptr	pointer to buffer frame
	@param type	type of object
	@return	true if contains */
	static bool memo_contains_page(
		mtr_buf_t*	memo,
		const byte*	ptr,
		ulint		type)
		__attribute__((warn_unused_result));

	/**
	Checks if memo contains the given item.
	@param object		object to search
	@param flags		specify types of object (can be ORred) of
				MTR_MEMO_PAGE_S_FIX ... values
	@return true if contains */
	bool memo_contains_flagged(const void* ptr, ulint flags) const;

	/**
	Checks if memo contains the given page.
	@param ptr		buffer frame
	@param flags		specify types of object with OR of
				MTR_MEMO_PAGE_S_FIX... values
	@return true if contains */
	bool memo_contains_page_flagged(const byte* ptr, ulint flags) const;

	/**
	Prints info of an mtr handle. */
	void print() const;

	/*
	@return true if the mini-transaction is active */
	bool is_active() const
	{
		return(m_impl.m_state == MTR_STATE_ACTIVE);
	}

	/**
	@return true if the mini-transaction has committed */
	bool has_committed() const
	{
		return(m_impl.m_state == MTR_STATE_COMMITTED);
	}

	/**
	@return true if the mini-transaction is committing */
	bool is_committing() const
	{
		return(m_impl.m_state == MTR_STATE_COMMITTING);
	}

	/**
	@return true if mini-transaction contains modifications. */
	bool has_modifications() const
	{
		return(m_impl.m_modifications);
	}

	/**
	const version of the gettor
	@return the memo stack */
	const mtr_buf_t* get_memo() const
	{
		return(&m_impl.m_memo);
	}

	/**
	@return the memo stack */
	mtr_buf_t* get_memo()
	{
		return(&m_impl.m_memo);
	}
#endif /* UNIV_DEBUG */

	/**
	@return true if a record was added to the mini-transaction */
	bool is_dirty() const
	{
		return(m_impl.m_made_dirty);
	}

	/**
	Note that a record has been added to the log */
	void added_rec()
	{
		++m_impl.m_n_log_recs;
	}

	/**
	Returns the log object of a mini-transaction buffer.
	@return	const log */
	const mtr_buf_t* get_log() const
	{
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(&m_impl.m_log);
	}

	/**
	Returns the log object of a mini-transaction buffer.
	@return	log */
	mtr_buf_t* get_log()
	{
		ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

		return(&m_impl.m_log);
	}

	/**
	Pushes an object to an mtr memo stack.
	@param object	object
	@param type	object type: MTR_MEMO_S_LOCK, ... */
	inline void memo_push(void* object, mtr_memo_type_t type);

	/**
	Checks if a mini-transaction is dirtying a clean page.
	@param block	block being x-fixed
	@return true if the mtr is dirtying a clean page. */
	static bool is_block_dirtied(const buf_block_t* block)
		__attribute__((warn_unused_result));

private:

	class Command;

	friend class Command;

private:
	Impl			m_impl;

	/** LSN at commit time */
	volatile lsn_t		m_commit_lsn;

	/** true if it is synchronouse mini-transaction */
	bool			m_sync;
};

#ifndef UNIV_NONINL
#include "mtr0mtr.ic"
#endif /* UNIV_NOINL */

#endif /* mtr0mtr_h */
