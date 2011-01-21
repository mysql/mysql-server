/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_INSTR_CLASS_H
#define PFS_INSTR_CLASS_H

#include "mysql_com.h"                          /* NAME_LEN */

/**
  @file storage/perfschema/pfs_instr_class.h
  Performance schema instruments meta data (declarations).
*/

/**
  Maximum length of an instrument name.
  For example, 'wait/sync/mutex/sql/LOCK_open' is an instrument name.
*/
#define PFS_MAX_INFO_NAME_LENGTH 128

/**
  Maximum length of the 'full' prefix of an instrument name.
  For example, for the instrument name 'wait/sync/mutex/sql/LOCK_open',
  the full prefix is 'wait/sync/mutex/sql/', which in turn derives from
  a prefix 'wait/sync/mutex' for mutexes, and a category of 'sql' for mutexes
  of the sql layer in the server.
*/
#define PFS_MAX_FULL_PREFIX_NAME_LENGTH 32

#include <my_global.h>
#include <mysql/psi/psi.h>
#include "pfs_lock.h"
#include "pfs_stat.h"
#include "pfs_column_types.h"

struct PFS_global_param;

/**
  @addtogroup Performance_schema_buffers
  @{
*/

extern my_bool pfs_enabled;

/** Key, naming a synch instrument (mutex, rwlock, cond). */
typedef unsigned int PFS_sync_key;
/** Key, naming a thread instrument. */
typedef unsigned int PFS_thread_key;
/** Key, naming a file instrument. */
typedef unsigned int PFS_file_key;

struct PFS_thread;

extern uint mutex_class_start;
extern uint rwlock_class_start;
extern uint cond_class_start;
extern uint file_class_start;
extern uint table_class_start;
extern uint max_instrument_class;

/** Information for all instrumentation. */
struct PFS_instr_class
{
  /** Instrument name. */
  char m_name[PFS_MAX_INFO_NAME_LENGTH];
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** Instrument flags. */
  int m_flags;
  /** True if this instrument is enabled. */
  bool m_enabled;
  /** True if this instrument is timed. */
  bool m_timed;
  /**
    Instrument name index.
    Self index in:
    - EVENTS_WAITS_SUMMARY_*_BY_EVENT_NAME
  */
  uint m_event_name_index;

  bool is_singleton() const
  {
    return m_flags & PSI_FLAG_GLOBAL;
  }
};

struct PFS_mutex;

/** Instrumentation metadata for a MUTEX. */
struct PFS_mutex_class : public PFS_instr_class
{
  /**
    Lock statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_lock_stat;
  /** Self index in @c mutex_class_array. */
  uint m_index;
  /** Singleton instance. */
  PFS_mutex *m_singleton;
};

struct PFS_rwlock;

/** Instrumentation metadata for a RWLOCK. */
struct PFS_rwlock_class : public PFS_instr_class
{
  /**
    Read lock statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_read_lock_stat;
  /**
    Write lock statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_single_stat m_write_lock_stat;
  /** Self index in @c rwlock_class_array. */
  uint m_index;
  /** Singleton instance. */
  PFS_rwlock *m_singleton;
};

struct PFS_cond;

/** Instrumentation metadata for a COND. */
struct PFS_cond_class : public PFS_instr_class
{
  /**
    Condition usage statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_cond_stat m_cond_stat;
  /** Self index in @c cond_class_array. */
  uint m_index;
  /** Singleton instance. */
  PFS_cond *m_singleton;
};

/** Instrumentation metadata of a thread. */
struct PFS_thread_class
{
  /** Thread instrument name. */
  char m_name[PFS_MAX_INFO_NAME_LENGTH];
  /** Length in bytes of @c m_name. */
  uint m_name_length;
  /** True if this thread instrument is enabled. */
  bool m_enabled;
  /** Singleton instance. */
  PFS_thread *m_singleton;
};

/** Key identifying a table share. */
struct PFS_table_share_key
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "<enum_object_type><schema_name><0x00><object_name><0x00>"
    @see create_table_def_key
  */
  char m_hash_key[1 + NAME_LEN + 1 + NAME_LEN + 1];
  /** Length in bytes of @c m_hash_key. */
  uint m_key_length;
};

/** Table index or 'key' */
struct PFS_table_key
{
  /** Index name */
  char m_name[NAME_LEN];
  /** Length in bytes of @c m_name. */
  uint m_name_length;
};

/** Instrumentation metadata for a table share. */
struct PFS_table_share
{
public:
  uint32 get_version()
  { return m_lock.get_version(); }

  enum_object_type get_object_type()
  {
    return (enum_object_type) m_key.m_hash_key[0];
  }

  void aggregate_io(void);
  void aggregate_lock(void);

  inline void aggregate(void)
  {
    aggregate_io();
    aggregate_lock();
  }

  inline void init_refcount(void)
  {
    PFS_atomic::store_32(& m_refcount, 1);
  }

  inline int get_refcount(void)
  {
    return PFS_atomic::load_32(& m_refcount);
  }

  inline void inc_refcount(void)
  {
    PFS_atomic::add_32(& m_refcount, 1);
  }

  inline void dec_refcount(void)
  {
    PFS_atomic::add_32(& m_refcount, -1);
  }

  /** Setup object refresh version. */
  uint m_setup_objects_version;
  /** Internal lock. */
  pfs_lock m_lock;
  /** Search key. */
  PFS_table_share_key m_key;
  /** Schema name. */
  const char *m_schema_name;
  /** Length in bytes of @c m_schema_name. */
  uint m_schema_name_length;
  /** Table name. */
  const char *m_table_name;
  /** Length in bytes of @c m_table_name. */
  uint m_table_name_length;
  /** True if this table instrument is enabled. */
  bool m_enabled;
  /** True if this table instrument is timed. */
  bool m_timed;
  bool m_purge;
  /** Table statistics. */
  PFS_table_stat m_table_stat;
  /** Number of indexes. */
  uint m_key_count;
  /** Index names. */
  PFS_table_key m_keys[MAX_KEY];

private:
  /** Number of opened table handles. */
  int m_refcount;
};

/**
  Instrument controlling all table io.
  This instrument is used with table SETUP_OBJECTS.
*/
extern PFS_instr_class global_table_io_class;

/**
  Instrument controlling all table lock.
  This instrument is used with table SETUP_OBJECTS.
*/
extern PFS_instr_class global_table_lock_class;

struct PFS_file;

/** Instrumentation metadata for a file. */
struct PFS_file_class : public PFS_instr_class
{
  /** File usage statistics. */
  PFS_file_stat m_file_stat;
  /** Self index in @c file_class_array. */
  uint m_index;
  /** Singleton instance. */
  PFS_file *m_singleton;
};

void init_event_name_sizing(const PFS_global_param *param);

int init_sync_class(uint mutex_class_sizing,
                    uint rwlock_class_sizing,
                    uint cond_class_sizing);

void cleanup_sync_class();
int init_thread_class(uint thread_class_sizing);
void cleanup_thread_class();
int init_table_share(uint table_share_sizing);
void cleanup_table_share();
int init_table_share_hash();
void cleanup_table_share_hash();
int init_file_class(uint file_class_sizing);
void cleanup_file_class();

PFS_sync_key register_mutex_class(const char *name, uint name_length,
                                  int flags);

PFS_sync_key register_rwlock_class(const char *name, uint name_length,
                                   int flags);

PFS_sync_key register_cond_class(const char *name, uint name_length,
                                 int flags);

PFS_thread_key register_thread_class(const char *name, uint name_length,
                                     int flags);

PFS_file_key register_file_class(const char *name, uint name_length,
                                 int flags);

PFS_mutex_class *find_mutex_class(PSI_mutex_key key);
PFS_mutex_class *sanitize_mutex_class(PFS_mutex_class *unsafe);
PFS_rwlock_class *find_rwlock_class(PSI_rwlock_key key);
PFS_rwlock_class *sanitize_rwlock_class(PFS_rwlock_class *unsafe);
PFS_cond_class *find_cond_class(PSI_cond_key key);
PFS_cond_class *sanitize_cond_class(PFS_cond_class *unsafe);
PFS_thread_class *find_thread_class(PSI_thread_key key);
PFS_thread_class *sanitize_thread_class(PFS_thread_class *unsafe);
PFS_file_class *find_file_class(PSI_file_key key);
PFS_file_class *sanitize_file_class(PFS_file_class *unsafe);
const char *sanitize_table_schema_name(const char *unsafe);
const char *sanitize_table_object_name(const char *unsafe);
PFS_instr_class *find_table_class(uint index);
PFS_instr_class *sanitize_table_class(PFS_instr_class *unsafe);

PFS_table_share *find_or_create_table_share(PFS_thread *thread,
                                            bool temporary,
                                            const TABLE_SHARE *share);
void release_table_share(PFS_table_share *pfs);
void purge_table_share(PFS_thread *thread, PFS_table_share *pfs);
void drop_table_share(PFS_thread *thread,
                      bool temporary,
                      const char *schema_name, uint schema_name_length,
                      const char *table_name, uint table_name_length);

PFS_table_share *sanitize_table_share(PFS_table_share *unsafe);

extern ulong mutex_class_max;
extern ulong mutex_class_lost;
extern ulong rwlock_class_max;
extern ulong rwlock_class_lost;
extern ulong cond_class_max;
extern ulong cond_class_lost;
extern ulong thread_class_max;
extern ulong thread_class_lost;
extern ulong file_class_max;
extern ulong file_class_lost;
extern ulong table_share_max;
extern ulong table_share_lost;
extern PFS_table_share *table_share_array;

void reset_file_class_io();

/** @} */
#endif

