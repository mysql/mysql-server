/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TRANSACTION_INFO_INCLUDED
#define TRANSACTION_INFO_INCLUDED

#include "my_global.h"
#include "my_dbug.h"                   // DBUG_ENTER
#include "my_sys.h"                    // strmake_root
#include "xa.h"                        // XID_STATE
#include "my_alloc.h"                  // MEM_ROOT
#include "thr_malloc.h"                // init_sql_alloc
#include "sql_cache.h"                 // query_cache
#include "mdl.h"                       // MDL_savepoint
#include "handler.h"                   // handlerton
#include "rpl_transaction_ctx.h"       // Rpl_transaction_ctx
#include "rpl_transaction_write_set_ctx.h" // Transaction_write_set_ctx

class THD;

typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32        key_length;
} CHANGED_TABLE_LIST;


/**
  Either statement transaction or normal transaction - related
  thread-specific storage engine data.

  If a storage engine participates in a statement/transaction,
  an instance of this class is present in
  thd->m_transaction.m_scope_info[STMT|SESSION].ha_list. The addition
  this list is made by trans_register_ha().

  When it's time to commit or rollback, each element of ha_list
  is used to access storage engine's prepare()/commit()/rollback()
  methods, and also to evaluate if a full two phase commit is
  necessary.

  @sa General description of transaction handling in handler.cc.
*/

class Ha_trx_info
{
public:

  /**
    Register this storage engine in the given transaction context.
  */

  void register_ha(Ha_trx_info *ha_info, handlerton *ht_arg)
  {
    DBUG_ENTER("Ha_trx_info::register_ha");
    DBUG_PRINT("enter", ("ht: 0x%llx (%s)",
                         (ulonglong) ht_arg,
                         ha_legacy_type_name(ht_arg->db_type)));
    DBUG_ASSERT(m_flags == 0);
    DBUG_ASSERT(m_ht == NULL);
    DBUG_ASSERT(m_next == NULL);

    m_ht= ht_arg;
    m_flags= (int) TRX_READ_ONLY; /* Assume read-only at start. */

    m_next= ha_info;

    DBUG_VOID_RETURN;
  }

  /**
    Clear, prepare for reuse.
  */

  void reset()
  {
    DBUG_ENTER("Ha_trx_info::reset");
    m_next= NULL;
    m_ht= NULL;
    m_flags= 0;
    DBUG_VOID_RETURN;
  }

  Ha_trx_info()
  {
    reset();
  }

  void set_trx_read_write()
  {
    DBUG_ASSERT(is_started());
    m_flags|= (int) TRX_READ_WRITE;
  }

  bool is_trx_read_write() const
  {
    DBUG_ASSERT(is_started());
    return m_flags & (int) TRX_READ_WRITE;
  }

  /**
    Set the transaction flag to noop_read_write
    If the transaction has no operation dml statement.
  */
  void set_trx_noop_read_write()
  {
    DBUG_ASSERT(is_started());
    m_flags|= (int) TRX_NOOP_READ_WRITE;
  }

  /**
    Check if the stmt transaction has noop_read_write flag set.
  */
  bool is_trx_noop_read_write() const
  {
    DBUG_ASSERT(is_started());
    return m_flags & (int) TRX_NOOP_READ_WRITE;
  }

  bool is_started() const
  {
    return m_ht != NULL;
  }


  /**
    Mark this transaction read-write if the argument is read-write.
  */

  void coalesce_trx_with(const Ha_trx_info *stmt_trx)
  {
    /*
      Must be called only after the transaction has been started.
      Can be called many times, e.g. when we have many
      read-write statements in a transaction.
    */
    DBUG_ASSERT(is_started());
    if (stmt_trx->is_trx_read_write())
      set_trx_read_write();
    if (stmt_trx->is_trx_noop_read_write())
      set_trx_noop_read_write();
  }

  Ha_trx_info *next() const
  {
    DBUG_ASSERT(is_started());
    return m_next;
  }

  handlerton *ht() const
  {
    DBUG_ASSERT(is_started());
    return m_ht;
  }

private:
  enum { TRX_READ_ONLY= 0, TRX_READ_WRITE= 1, TRX_NOOP_READ_WRITE= 2 };
  /**
    Auxiliary, used for ha_list management
  */
  Ha_trx_info *m_next;

  /**
    Although a given Ha_trx_info instance is currently always used
    for the same storage engine, 'ht' is not-NULL only when the
    corresponding storage is a part of a transaction.
  */
  handlerton *m_ht;

  /**
    Transaction flags related to this engine.
    Not-null only if this instance is a part of transaction.
    May assume a combination of enum values above.
  */
  uchar       m_flags;
};

struct st_savepoint
{
  struct st_savepoint *prev;
  char                *name;
  size_t              length;
  Ha_trx_info         *ha_list;
  /** State of metadata locks before this savepoint was set. */
  MDL_savepoint        mdl_savepoint;
};

class Transaction_ctx
{
public:
  enum enum_trx_scope { STMT= 0, SESSION };

  SAVEPOINT *m_savepoints;

private:
  struct THD_TRANS
  {
    /* true is not all entries in the ht[] support 2pc */
    bool        m_no_2pc;
    int         m_rw_ha_count;
    /* storage engines that registered in this transaction */
    Ha_trx_info *m_ha_list;

  private:
    /*
      The purpose of this member variable (i.e. flag) is to keep track of
      statements which cannot be rolled back safely(completely).
      For example,

      * statements that modified non-transactional tables. The value
      MODIFIED_NON_TRANS_TABLE is set within mysql_insert, mysql_update,
      mysql_delete, etc if a non-transactional table is modified.

      * 'DROP TEMPORARY TABLE' and 'CREATE TEMPORARY TABLE' statements.
      The former sets the value DROPPED_TEMP_TABLE and the latter
      the value CREATED_TEMP_TABLE.

      The tracked statements are modified in scope of:

      * transaction, when the variable is a member of
      THD::m_transaction.m_scope_info[SESSION]

      * top-level statement or sub-statement, when the variable is a
      member of THD::m_transaction.m_scope_info[STMT]

      This member has the following life cycle:

      * m_scope_info[STMT].m_unsafe_rollback_flags is used to keep track of
      top-level statements which cannot be rolled back safely. At the end of the
      statement, the value of m_scope_info[STMT].m_unsafe_rollback_flags is
      merged with m_scope_info[SESSION].m_unsafe_rollback_flags
      and gets reset.

      * m_scope_info[SESSION].cannot_safely_rollback is reset at the end
      of transaction

      * Since we do not have a dedicated context for execution of
      a sub-statement, to keep track of non-transactional changes in a
      sub-statement, we re-use m_scope_info[STMT].m_unsafe_rollback_flags.
      At entrance into a sub-statement, a copy of the value of
      m_scope_info[STMT].m_unsafe_rollback_flags (containing the changes of the
      outer statement) is saved on stack.
      Then m_scope_info[STMT].m_unsafe_rollback_flags is reset to 0 and the
      substatement is executed. Then the new value is merged
      with the saved value.
    */

    unsigned int m_unsafe_rollback_flags;
    /*
      Define the type of statements which cannot be rolled back safely.
      Each type occupies one bit in m_unsafe_rollback_flags.
    */
    static unsigned int const MODIFIED_NON_TRANS_TABLE= 0x01;
    static unsigned int const CREATED_TEMP_TABLE= 0x02;
    static unsigned int const DROPPED_TEMP_TABLE= 0x04;

  public:
    bool cannot_safely_rollback() const
    {
      return m_unsafe_rollback_flags > 0;
    }
    unsigned int get_unsafe_rollback_flags() const
    {
      return m_unsafe_rollback_flags;
    }
    void set_unsafe_rollback_flags(unsigned int flags)
    {
      DBUG_PRINT("debug", ("set_unsafe_rollback_flags: %d", flags));
      m_unsafe_rollback_flags= flags;
    }
    void add_unsafe_rollback_flags(unsigned int flags)
    {
      DBUG_PRINT("debug", ("add_unsafe_rollback_flags: %d", flags));
      m_unsafe_rollback_flags|= flags;
    }
    void reset_unsafe_rollback_flags()
    {
      DBUG_PRINT("debug", ("reset_unsafe_rollback_flags"));
      m_unsafe_rollback_flags= 0;
    }
    void mark_modified_non_trans_table()
    {
      DBUG_PRINT("debug", ("mark_modified_non_trans_table"));
      m_unsafe_rollback_flags|= MODIFIED_NON_TRANS_TABLE;
    }
    bool has_modified_non_trans_table() const
    {
      return m_unsafe_rollback_flags & MODIFIED_NON_TRANS_TABLE;
    }
    void mark_created_temp_table()
    {
      DBUG_PRINT("debug", ("mark_created_temp_table"));
      m_unsafe_rollback_flags|= CREATED_TEMP_TABLE;
    }
    bool has_created_temp_table() const
    {
      return m_unsafe_rollback_flags & CREATED_TEMP_TABLE;
    }
    void mark_dropped_temp_table()
    {
      DBUG_PRINT("debug", ("mark_dropped_temp_table"));
      m_unsafe_rollback_flags|= DROPPED_TEMP_TABLE;
    }
    bool has_dropped_temp_table() const
    {
      return m_unsafe_rollback_flags & DROPPED_TEMP_TABLE;
    }

    void reset()
    {
      m_no_2pc= false;
      m_rw_ha_count= 0;
      reset_unsafe_rollback_flags();
    }
    bool is_empty() const { return m_ha_list == NULL; }
  };

  THD_TRANS m_scope_info[2];

  XID_STATE m_xid_state;

  /*
    Tables changed in transaction (that must be invalidated in query cache).
    List contain only transactional tables, that not invalidated in query
    cache (instead of full list of changed in transaction tables).
  */
  CHANGED_TABLE_LIST* m_changed_tables;
  MEM_ROOT m_mem_root; // Transaction-life memory allocation pool

public:
  /*
    (Mostly) binlog-specific fields use while flushing the caches
    and committing transactions.
    We don't use bitfield any more in the struct. Modification will
    be lost when concurrently updating multiple bit fields. It will
    cause a race condition in a multi-threaded application. And we
    already caught a race condition case between xid_written and
    ready_preempt in MYSQL_BIN_LOG::ordered_commit.
  */
  struct
  {
    bool enabled;                   // see ha_enable_transaction()
    bool pending;                   // Is the transaction commit pending?
    bool xid_written;               // The session wrote an XID
    bool real_commit;               // Is this a "real" commit?
    bool commit_low;                // see MYSQL_BIN_LOG::ordered_commit
    bool run_hooks;                 // Call the after_commit hook
#ifndef DBUG_OFF
    bool ready_preempt;             // internal in MYSQL_BIN_LOG::ordered_commit
#endif
  } m_flags;
  /* Binlog-specific logical timestamps. */
  /*
    Store for the transaction's commit parent sequence_number.
    The value specifies this transaction dependency with a "parent"
    transaction.
    The member is assigned, when the transaction is about to commit
    in binlog to a value of the last committed transaction's sequence_number.
    This and last_committed as numbers are kept ever incremented
    regardless of binary logs being rotated or when transaction
    is logged in multiple pieces.
    However the logger to the binary log may convert them
    according to its specification.
  */
  int64 last_committed;
  /*
    The transaction's private logical timestamp assigned at the
    transaction prepare phase. The timestamp enumerates transactions
    in the binary log. The value is gained through incrementing (stepping) a
    global clock.
    Eventually the value is considered to increase max_committed_transaction
    system clock when the transaction has committed.
  */
  int64 sequence_number;

  void store_commit_parent(int64 last_arg)
  {
    last_committed= last_arg;
  }

  Transaction_ctx();
  virtual ~Transaction_ctx()
  {
    free_root(&m_mem_root, MYF(0));
  }

  void cleanup()
  {
    DBUG_ENTER("Transaction_ctx::cleanup");
    m_changed_tables= NULL;
    m_savepoints= NULL;
    m_xid_state.cleanup();
    m_rpl_transaction_ctx.cleanup();
    m_transaction_write_set_ctx.clear_write_set();
    free_root(&m_mem_root,MYF(MY_KEEP_PREALLOC));
    DBUG_VOID_RETURN;
  }

  bool is_active(enum_trx_scope scope) const
  {
    return m_scope_info[scope].m_ha_list != NULL;
  }

  void push_unsafe_rollback_warnings(THD *thd);

  void merge_unsafe_rollback_flags()
  {
    /*
      Merge m_scope_info[STMT].unsafe_rollback_flags to
      m_scope_info[SESSION].unsafe_rollback_flags. If the statement
      cannot be rolled back safely, the transaction including
      this statement definitely cannot rolled back safely.
    */
    m_scope_info[SESSION].add_unsafe_rollback_flags(
      m_scope_info[STMT].get_unsafe_rollback_flags());
  }

  void init_mem_root_defaults(ulong trans_alloc_block_size,
                              ulong trans_prealloc_size)
  {
    reset_root_defaults(&m_mem_root,
                        trans_alloc_block_size,
                        trans_prealloc_size);
  }

  MEM_ROOT* transaction_memroot()
  {
    return &m_mem_root;
  }

  void* allocate_memory(unsigned int size)
  {
    return alloc_root(&m_mem_root, size);
  }

  void claim_memory_ownership()
  {
    claim_root(&m_mem_root);
  }

  void free_memory(myf root_alloc_flags)
  {
    free_root(&m_mem_root, root_alloc_flags);
  }

  char* strmake(const char *str, size_t len)
  {
    return strmake_root(&m_mem_root, str, len);
  }

  void invalidate_changed_tables_in_cache()
  {
    if (m_changed_tables)
      query_cache.invalidate(m_changed_tables);
  }

  bool add_changed_table(const char *key, long key_length);

  Ha_trx_info* ha_trx_info(enum_trx_scope scope)
  {
    return m_scope_info[scope].m_ha_list;
  }

  const Ha_trx_info* ha_trx_info(enum_trx_scope scope) const
  {
    return m_scope_info[scope].m_ha_list;
  }

  void set_ha_trx_info(enum_trx_scope scope, Ha_trx_info *trx_info)
  {
    m_scope_info[scope].m_ha_list= trx_info;
  }

  XID_STATE *xid_state()
  {
    return &m_xid_state;
  }

  const XID_STATE *xid_state() const
  {
    return &m_xid_state;
  }

  bool cannot_safely_rollback(enum_trx_scope scope) const
  {
    return m_scope_info[scope].cannot_safely_rollback();
  }

  unsigned int get_unsafe_rollback_flags(enum_trx_scope scope) const
  {
    return m_scope_info[scope].get_unsafe_rollback_flags();
  }

  void set_unsafe_rollback_flags(enum_trx_scope scope, unsigned int flags)
  {
    m_scope_info[scope].set_unsafe_rollback_flags(flags);
  }

  void add_unsafe_rollback_flags(enum_trx_scope scope, unsigned int flags)
  {
    m_scope_info[scope].add_unsafe_rollback_flags(flags);
  }

  void reset_unsafe_rollback_flags(enum_trx_scope scope)
  {
    m_scope_info[scope].reset_unsafe_rollback_flags();
  }

  void mark_modified_non_trans_table(enum_trx_scope scope)
  {
    m_scope_info[scope].mark_modified_non_trans_table();
  }

  bool has_modified_non_trans_table(enum_trx_scope scope) const
  {
    return m_scope_info[scope].has_modified_non_trans_table();
  }

  void mark_created_temp_table(enum_trx_scope scope)
  {
    m_scope_info[scope].mark_created_temp_table();
  }

  bool has_created_temp_table(enum_trx_scope scope) const
  {
    return m_scope_info[scope].has_created_temp_table();
  }

  void mark_dropped_temp_table(enum_trx_scope scope)
  {
    m_scope_info[scope].mark_dropped_temp_table();
  }

  bool has_dropped_temp_table(enum_trx_scope scope) const
  {
    return m_scope_info[scope].has_dropped_temp_table();
  }

  void reset(enum_trx_scope scope)
  {
    m_scope_info[scope].reset();
  }

  bool is_empty(enum_trx_scope scope) const
  {
    return m_scope_info[scope].is_empty();
  }

  void set_no_2pc(enum_trx_scope scope, bool value)
  {
    m_scope_info[scope].m_no_2pc= value;
  }

  bool no_2pc(enum_trx_scope scope) const
  {
    return m_scope_info[scope].m_no_2pc;
  }

  int rw_ha_count(enum_trx_scope scope) const
  {
    return m_scope_info[scope].m_rw_ha_count;
  }

  void set_rw_ha_count(enum_trx_scope scope, int value)
  {
    m_scope_info[scope].m_rw_ha_count= value;
  }

  void reset_scope(enum_trx_scope scope)
  {
    m_scope_info[scope].m_ha_list= 0;
    m_scope_info[scope].m_no_2pc=0;
    m_scope_info[scope].m_rw_ha_count= 0;
  }

private:
  CHANGED_TABLE_LIST* changed_table_dup(const char *key, long key_length);

  /* routings to adding tables to list of changed in transaction tables */
  bool list_include(CHANGED_TABLE_LIST** prev,
                    CHANGED_TABLE_LIST* curr,
                    CHANGED_TABLE_LIST* new_table)
  {
    if (new_table)
    {
      *prev= new_table;
      (*prev)->next= curr;
      return false;
    }
    else
      return true;
  }

public:
  Rpl_transaction_ctx *get_rpl_transaction_ctx()
  {
    return &m_rpl_transaction_ctx;
  }

  const Rpl_transaction_ctx *get_rpl_transaction_ctx() const
  {
    return &m_rpl_transaction_ctx;
  }

  Rpl_transaction_write_set_ctx *get_transaction_write_set_ctx()
  {
    return &m_transaction_write_set_ctx;
  }

  const Rpl_transaction_write_set_ctx *get_transaction_write_set_ctx() const
  {
    return &m_transaction_write_set_ctx;
  }

private:
  Rpl_transaction_ctx m_rpl_transaction_ctx;
  Rpl_transaction_write_set_ctx m_transaction_write_set_ctx;
};

#endif
