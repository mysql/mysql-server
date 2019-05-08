/*****************************************************************************

Copyright (c) 2013, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/sync0policy.h
 Policies for mutexes.

 Created 2012-08-21 Sunny Bains.
 ***********************************************************************/

#ifndef sync0policy_h
#define sync0policy_h

#include "os0thread.h"
#include "srv0mon.h"
#include "sync0types.h"
#include "univ.i"
#include "ut0rnd.h"

#ifndef UNIV_LIBRARY
#ifdef UNIV_DEBUG

#define MUTEX_MAGIC_N 979585UL

template <typename Mutex>
class MutexDebug {
 public:
  /** For passing context to SyncDebug */
  struct Context : public latch_t {
    /** Constructor */
    Context()
        : m_mutex(),
          m_filename(),
          m_line(),
          m_thread_id(os_thread_id_t(ULINT_UNDEFINED)) {
      /* No op */
    }

    /** Create the context for SyncDebug
    @param[in]	id	ID of the latch to track */
    Context(latch_id_t id) : latch_t(id) { /* No op */
    }

    /** Set to locked state
    @param[in]	mutex		The mutex to acquire
    @param[in]	filename	File name from where to acquire
    @param[in]	line		Line number in filename */
    void locked(const Mutex *mutex, const char *filename,
                ulint line) UNIV_NOTHROW {
      m_mutex = mutex;

      m_thread_id = os_thread_get_curr_id();

      m_filename = filename;

      m_line = line;
    }

    /** Reset to unlock state */
    void release() UNIV_NOTHROW {
      m_mutex = NULL;

      m_thread_id = os_thread_id_t(ULINT_UNDEFINED);

      m_filename = NULL;

      m_line = ULINT_UNDEFINED;
    }

    /** Print information about the latch
    @return the string representation */
    virtual std::string to_string() const UNIV_NOTHROW {
      std::ostringstream msg;

      msg << m_mutex->policy().to_string();

      if (m_thread_id != os_thread_id_t(ULINT_UNDEFINED)) {
        msg << " addr: " << m_mutex << " acquired: " << locked_from().c_str();

      } else {
        msg << "Not locked";
      }

      return (msg.str());
    }

    /** @return the name of the file and line number in the file
    from where the mutex was acquired "filename:line" */
    virtual std::string locked_from() const {
      std::ostringstream msg;

      msg << sync_basename(m_filename) << ":" << m_line;

      return (std::string(msg.str()));
    }

    /** Mutex to check for lock order violation */
    const Mutex *m_mutex;

    /** Filename from where enter was called */
    const char *m_filename;

    /** Line mumber in filename */
    ulint m_line;

    /** Thread ID of the thread that own(ed) the mutex */
    os_thread_id_t m_thread_id;
  };

  /** Constructor. */
  MutexDebug() : m_magic_n(), m_context() UNIV_NOTHROW { /* No op */
  }

  /* Destructor */
  virtual ~MutexDebug() {}

  /** Mutex is being destroyed. */
  void destroy() UNIV_NOTHROW {
    ut_ad(m_context.m_thread_id == os_thread_id_t(ULINT_UNDEFINED));

    m_magic_n = 0;

    m_context.m_thread_id = 0;
  }

  /** Called when the mutex is "created". Note: Not from the constructor
  but when the mutex is initialised.
  @param[in]	id              Mutex ID */
  void init(latch_id_t id) UNIV_NOTHROW;

  /** Called when an attempt is made to lock the mutex
  @param[in]	mutex		Mutex instance to be locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void enter(const Mutex *mutex, const char *filename, ulint line) UNIV_NOTHROW;

  /** Called when the mutex is locked
  @param[in]	mutex		Mutex instance that was locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void locked(const Mutex *mutex, const char *filename,
              ulint line) UNIV_NOTHROW;

  /** Called when the mutex is released
  @param[in]	mutex		Mutex that was released */
  void release(const Mutex *mutex) UNIV_NOTHROW;

  /** @return true if thread owns the mutex */
  bool is_owned() const UNIV_NOTHROW {
    return (os_thread_eq(m_context.m_thread_id, os_thread_get_curr_id()));
  }

  /** @return the name of the file from the mutex was acquired */
  const char *get_enter_filename() const UNIV_NOTHROW {
    return (m_context.m_filename);
  }

  /** @return the name of the file from the mutex was acquired */
  ulint get_enter_line() const UNIV_NOTHROW { return (m_context.m_line); }

  /** @return id of the thread that was trying to acquire the mutex */
  os_thread_id_t get_thread_id() const UNIV_NOTHROW {
    return (m_context.m_thread_id);
  }

  /** Magic number to check for memory corruption. */
  ulint m_magic_n;

  /** Latch state of the mutex owner */
  Context m_context;
};
#endif /* UNIV_DEBUG */

/* Do nothing */
template <typename Mutex>
struct NoPolicy {
  /** Default constructor. */
  NoPolicy() {}

  void init(const Mutex &, latch_id_t, const char *, uint32_t) UNIV_NOTHROW {}
  void destroy() UNIV_NOTHROW {}
  void enter(const Mutex &, const char *, ulint line) UNIV_NOTHROW {}
  void add(uint32_t, uint32_t) UNIV_NOTHROW {}
  void locked(const Mutex &, const char *, ulint) UNIV_NOTHROW {}
  void release(const Mutex &) UNIV_NOTHROW {}
  std::string to_string() const { return (""); }
  latch_id_t get_id() const;
};

/** Collect the metrics per mutex instance, no aggregation. */
template <typename Mutex>
struct GenericPolicy
#ifdef UNIV_DEBUG
    : public MutexDebug<Mutex>
#endif /* UNIV_DEBUG */
{
 public:
  typedef Mutex MutexType;

  /** Constructor. */
  GenericPolicy() UNIV_NOTHROW :
#ifdef UNIV_DEBUG
      MutexDebug<MutexType>(),
#endif /* UNIV_DEBUG */
      m_count(),
      m_id() {
  }

  /** Destructor */
  ~GenericPolicy() {}

  /** Called when the mutex is "created". Note: Not from the constructor
  but when the mutex is initialised.
  @param[in]	mutex		Mutex instance to track
  @param[in]	id              Mutex ID
  @param[in]	filename	File where mutex was created
  @param[in]	line		Line in filename */
  void init(const MutexType &mutex, latch_id_t id, const char *filename,
            uint32_t line) UNIV_NOTHROW {
    m_id = id;

    latch_meta_t &meta = sync_latch_get_meta(id);

    ut_ad(meta.get_id() == id);

    meta.get_counter()->single_register(&m_count);

    sync_file_created_register(this, filename, line);

    ut_d(MutexDebug<MutexType>::init(m_id));
  }

  /** Called when the mutex is destroyed. */
  void destroy() UNIV_NOTHROW {
    latch_meta_t &meta = sync_latch_get_meta(m_id);

    meta.get_counter()->single_deregister(&m_count);

    sync_file_created_deregister(this);

    ut_d(MutexDebug<MutexType>::destroy());
  }

  /** Called after a successful mutex acquire.
  @param[in]	n_spins		Number of times the thread did
                                  spins while trying to acquire the mutex
  @param[in]	n_waits		Number of times the thread waited
                                  in some type of OS queue */
  void add(uint32_t n_spins, uint32_t n_waits) UNIV_NOTHROW {
    /* Currently global on/off. Keeps things simple and fast */

    if (!m_count.m_enabled) {
      return;
    }

    m_count.m_spins += n_spins;
    m_count.m_waits += n_waits;

    ++m_count.m_calls;
  }

  /** Called when an attempt is made to lock the mutex
  @param[in]	mutex		Mutex instance to be locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void enter(const MutexType &mutex, const char *filename,
             ulint line) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::enter(&mutex, filename, line));
  }

  /** Called when the mutex is locked
  @param[in]	mutex		Mutex instance that is locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void locked(const MutexType &mutex, const char *filename,
              ulint line) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::locked(&mutex, filename, line));
  }

  /** Called when the mutex is released
  @param[in]	mutex		Mutex instance that is released */
  void release(const MutexType &mutex) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::release(&mutex));
  }

  /** Print the information about the latch
  @return the string representation */
  std::string print() const UNIV_NOTHROW;

  /** @return the latch ID */
  latch_id_t get_id() const UNIV_NOTHROW { return (m_id); }

  /** @return the string representation */
  std::string to_string() const;

 private:
  typedef latch_meta_t::CounterType Counter;

  /** The user visible counters, registered with the meta-data.  */
  Counter::Count m_count;

  /** Latch meta data ID */
  latch_id_t m_id;
};

/** Track agregate metrics policy, used by the page mutex. There are just
too many of them to count individually. */
template <typename Mutex>
class BlockMutexPolicy
#ifdef UNIV_DEBUG
    : public MutexDebug<Mutex>
#endif /* UNIV_DEBUG */
{
 public:
  typedef Mutex MutexType;
  typedef typename latch_meta_t::CounterType::Count Count;

  /** Default constructor. */
  BlockMutexPolicy()
      :
#ifdef UNIV_DEBUG
        MutexDebug<MutexType>(),
#endif /* UNIV_DEBUG */
        m_count(),
        m_id() {
    /* Do nothing */
  }

  /** Destructor */
  ~BlockMutexPolicy() {}

  /** Called when the mutex is "created". Note: Not from the constructor
  but when the mutex is initialised.
  @param[in]	mutex		Mutex instance to track
  @param[in]	id              Mutex ID
  @param[in]	filename	File where mutex was created
  @param[in]	line		Line in filename */
  void init(const MutexType &mutex, latch_id_t id, const char *filename,
            uint32_t line) UNIV_NOTHROW {
    /* It can be LATCH_ID_BUF_BLOCK_MUTEX or
    LATCH_ID_BUF_POOL_ZIP. Unfortunately, they
    are mapped to the same mutex type in the
    buffer pool code. */

    m_id = id;

    latch_meta_t &meta = sync_latch_get_meta(m_id);

    ut_ad(meta.get_id() == id);

    m_count = meta.get_counter()->sum_register();

    ut_d(MutexDebug<MutexType>::init(m_id));
  }

  /** Called when the mutex is destroyed. */
  void destroy() UNIV_NOTHROW {
    latch_meta_t &meta = sync_latch_get_meta(m_id);

    ut_ad(meta.get_id() == m_id);

    meta.get_counter()->sum_deregister(m_count);

    m_count = NULL;

    ut_d(MutexDebug<MutexType>::destroy());
  }

  /** Called after a successful mutex acquire.
  @param[in]	n_spins		Number of times the thread did
                                  spins while trying to acquire the mutex
  @param[in]	n_waits		Number of times the thread waited
                                  in some type of OS queue */
  void add(uint32_t n_spins, uint32_t n_waits) UNIV_NOTHROW {
    if (!m_count->m_enabled) {
      return;
    }

    m_count->m_spins += n_spins;
    m_count->m_waits += n_waits;

    ++m_count->m_calls;
  }

  /** Called when the mutex is locked
  @param[in]	mutex		Mutex instance that is locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void locked(const MutexType &mutex, const char *filename,
              ulint line) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::locked(&mutex, filename, line));
  }

  /** Called when the mutex is released
  @param[in]	mutex		Mutex instance that is released */
  void release(const MutexType &mutex) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::release(&mutex));
  }

  /** Called when an attempt is made to lock the mutex
  @param[in]	mutex		Mutex instance to be locked
  @param[in]	filename	Filename from where it was called
  @param[in]	line		Line number from where it was called */
  void enter(const MutexType &mutex, const char *filename,
             ulint line) UNIV_NOTHROW {
    ut_d(MutexDebug<MutexType>::enter(&mutex, filename, line));
  }

  /** Print the information about the latch
  @return the string representation */
  std::string print() const UNIV_NOTHROW;

  /** @return the latch ID */
  latch_id_t get_id() const { return (m_id); }

  /** @return the string representation */
  std::string to_string() const;

 private:
  typedef latch_meta_t::CounterType Counter;

  /** The user visible counters, registered with the meta-data.  */
  Counter::Count *m_count;

  /** Latch meta data ID */
  latch_id_t m_id;
};

#include "sync0policy.ic"

#endif /* !UNIV_LIBRARY */
#endif /* sync0policy_h */
