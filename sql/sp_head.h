/* -*- C++ -*- */
/*
   Copyright (c) 2002, 2011, Oracle and/or its affiliates.

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

#ifndef _SP_HEAD_H_
#define _SP_HEAD_H_

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_class.h"                          // THD, set_var.h: THD
#include "set_var.h"                            // Item
#include "sp.h"
#include <stddef.h>

/**
  @defgroup Stored_Routines Stored Routines
  @ingroup Runtime_Environment
  @{
*/

Item_result
sp_map_result_type(enum enum_field_types type);

Item::Type
sp_map_item_type(enum enum_field_types type);

uint
sp_get_flags_for_command(LEX *lex);

struct sp_label;
class sp_instr;
class sp_instr_opt_meta;
class sp_instr_jump_if_not;
struct sp_cond_type;
struct sp_variable;

/*************************************************************************/

/**
  Stored_program_creation_ctx -- base class for creation context of stored
  programs (stored routines, triggers, events).
*/

class Stored_program_creation_ctx :public Default_object_creation_ctx
{
public:
  CHARSET_INFO *get_db_cl()
  {
    return m_db_cl;
  }

public:
  virtual Stored_program_creation_ctx *clone(MEM_ROOT *mem_root) = 0;

protected:
  Stored_program_creation_ctx(THD *thd)
    : Default_object_creation_ctx(thd),
      m_db_cl(thd->variables.collation_database)
  { }

  Stored_program_creation_ctx(CHARSET_INFO *client_cs,
                              CHARSET_INFO *connection_cl,
                              CHARSET_INFO *db_cl)
    : Default_object_creation_ctx(client_cs, connection_cl),
      m_db_cl(db_cl)
  { }

protected:
  virtual void change_env(THD *thd) const
  {
    thd->variables.collation_database= m_db_cl;

    Default_object_creation_ctx::change_env(thd);
  }

protected:
  /**
    db_cl stores the value of the database collation. Both character set
    and collation attributes are used.

    Database collation is included into the context because it defines the
    default collation for stored-program variables.
  */
  CHARSET_INFO *m_db_cl;
};

/*************************************************************************/

class sp_name : public Sql_alloc
{
public:

  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_qname;
  bool       m_explicit_name;                   /**< Prepend the db name? */

  sp_name(LEX_STRING db, LEX_STRING name, bool use_explicit_name)
    : m_db(db), m_name(name), m_explicit_name(use_explicit_name)
  {
    m_qname.str= 0;
    m_qname.length= 0;
  }

  /** Create temporary sp_name object from MDL key. */
  sp_name(const MDL_key *key, char *qname_buff);

  // Init. the qualified name from the db and name.
  void init_qname(THD *thd);	// thd for memroot allocation

  ~sp_name()
  {}
};


bool
check_routine_name(LEX_STRING *ident);

class sp_head :private Query_arena
{
  sp_head(const sp_head &);	/**< Prevent use of these */
  void operator=(sp_head &);

  MEM_ROOT main_mem_root;
public:
  /** Possible values of m_flags */
  enum {
    HAS_RETURN= 1,              // For FUNCTIONs only: is set if has RETURN
    MULTI_RESULTS= 8,           // Is set if a procedure with SELECT(s)
    CONTAINS_DYNAMIC_SQL= 16,   // Is set if a procedure with PREPARE/EXECUTE
    IS_INVOKED= 32,             // Is set if this sp_head is being used
    HAS_SET_AUTOCOMMIT_STMT= 64,// Is set if a procedure with 'set autocommit'
    /* Is set if a procedure with COMMIT (implicit or explicit) | ROLLBACK */
    HAS_COMMIT_OR_ROLLBACK= 128,
    LOG_SLOW_STATEMENTS= 256,   // Used by events
    LOG_GENERAL_LOG= 512,        // Used by events
    HAS_SQLCOM_RESET= 1024,
    HAS_SQLCOM_FLUSH= 2048
  };

  stored_procedure_type m_type;
  uint m_flags;                 // Boolean attributes of a stored routine

  Create_field m_return_field_def; /**< This is used for FUNCTIONs only. */

  const char *m_tmp_query;	///< Temporary pointer to sub query string
  st_sp_chistics *m_chistics;
  ulonglong m_sql_mode;		///< For SHOW CREATE and execution
  LEX_STRING m_qname;		///< db.name
  bool m_explicit_name;         ///< Prepend the db name? */
  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_params;
  LEX_STRING m_body;
  LEX_STRING m_body_utf8;
  LEX_STRING m_defstr;
  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;

  /**
    Is this routine being executed?
  */
  bool is_invoked() const { return m_flags & IS_INVOKED; }

  /**
    Get the value of the SP cache version, as remembered
    when the routine was inserted into the cache.
  */
  ulong sp_cache_version() const { return m_sp_cache_version; }

  /** Set the value of the SP cache version.  */
  void set_sp_cache_version(ulong version_arg)
  {
    m_sp_cache_version= version_arg;
  }
private:
  /**
    Version of the stored routine cache at the moment when the
    routine was added to it. Is used only for functions and
    procedures, not used for triggers or events.  When sp_head is
    created, its version is 0. When it's added to the cache, the
    version is assigned the global value 'Cversion'.
    If later on Cversion is incremented, we know that the routine
    is obsolete and should not be used --
    sp_cache_flush_obsolete() will purge it.
  */
  ulong m_sp_cache_version;
  Stored_program_creation_ctx *m_creation_ctx;
  /**
    Boolean combination of (1<<flag), where flag is a member of
    LEX::enum_binlog_stmt_unsafe.
  */
  uint32 unsafe_flags;

public:
  inline Stored_program_creation_ctx *get_creation_ctx()
  {
    return m_creation_ctx;
  }

  inline void set_creation_ctx(Stored_program_creation_ctx *creation_ctx)
  {
    m_creation_ctx= creation_ctx->clone(mem_root);
  }

  longlong m_created;
  longlong m_modified;
  /** Recursion level of the current SP instance. The levels are numbered from 0 */
  ulong m_recursion_level;
  /**
    A list of diferent recursion level instances for the same procedure.
    For every recursion level we have a sp_head instance. This instances
    connected in the list. The list ordered by increasing recursion level
    (m_recursion_level).
  */
  sp_head *m_next_cached_sp;
  /**
    Pointer to the first element of the above list
  */
  sp_head *m_first_instance;
  /**
    Pointer to the first free (non-INVOKED) routine in the list of
    cached instances for this SP. This pointer is set only for the first
    SP in the list of instences (see above m_first_cached_sp pointer).
    The pointer equal to 0 if we have no free instances.
    For non-first instance value of this pointer meanless (point to itself);
  */
  sp_head *m_first_free_instance;
  /**
    Pointer to the last element in the list of instances of the SP.
    For non-first instance value of this pointer meanless (point to itself);
  */
  sp_head *m_last_cached_sp;
  /**
    Set containing names of stored routines used by this routine.
    Note that unlike elements of similar set for statement elements of this
    set are not linked in one list. Because of this we are able save memory
    by using for this set same objects that are used in 'sroutines' sets
    for statements of which this stored routine consists.
  */
  HASH m_sroutines;
  // Pointers set during parsing
  const char *m_param_begin;
  const char *m_param_end;

private:
  const char *m_body_begin;

public:
  /*
    Security context for stored routine which should be run under
    definer privileges.
  */
  Security_context m_security_ctx;

  static void *
  operator new(size_t size) throw ();

  static void
  operator delete(void *ptr, size_t size) throw ();

  sp_head();

  /// Initialize after we have reset mem_root
  void
  init(LEX *lex);

  /** Copy sp name from parser. */
  void
  init_sp_name(THD *thd, sp_name *spname);

  /** Set the body-definition start position. */
  void
  set_body_start(THD *thd, const char *begin_ptr);

  /** Set the statement-definition (body-definition) end position. */
  void
  set_stmt_end(THD *thd);

  virtual ~sp_head();

  bool
  execute_trigger(THD *thd,
                  const LEX_STRING *db_name,
                  const LEX_STRING *table_name,
                  GRANT_INFO *grant_info);

  bool
  execute_function(THD *thd, Item **args, uint argcount, Field *return_fld);

  bool
  execute_procedure(THD *thd, List<Item> *args);

  bool
  show_create_routine(THD *thd, int type);

  int
  add_instr(sp_instr *instr);

  inline uint
  instructions()
  {
    return m_instr.elements;
  }

  inline sp_instr *
  last_instruction()
  {
    sp_instr *i;

    get_dynamic(&m_instr, (uchar*)&i, m_instr.elements-1);
    return i;
  }

  /*
    Resets lex in 'thd' and keeps a copy of the old one.

    @todo Conflicting comment in sp_head.cc
  */
  bool
  reset_lex(THD *thd);

  /**
    Restores lex in 'thd' from our copy, but keeps some status from the
    one in 'thd', like ptr, tables, fields, etc.

    @todo Conflicting comment in sp_head.cc
  */
  bool
  restore_lex(THD *thd);

  /// Put the instruction on the backpatch list, associated with the label.
  int
  push_backpatch(sp_instr *, struct sp_label *);

  /// Update all instruction with this label in the backpatch list to
  /// the current position.
  void
  backpatch(struct sp_label *);

  /// Start a new cont. backpatch level. If 'i' is NULL, the level is just incr.
  int
  new_cont_backpatch(sp_instr_opt_meta *i);

  /// Add an instruction to the current level
  int
  add_cont_backpatch(sp_instr_opt_meta *i);

  /// Backpatch (and pop) the current level to the current position.
  void
  do_cont_backpatch();

  char *name(uint *lenp = 0) const
  {
    if (lenp)
      *lenp= (uint) m_name.length;
    return m_name.str;
  }

  char *create_string(THD *thd, ulong *lenp);

  Field *create_result_field(uint field_max_length, const char *field_name,
                             TABLE *table);

  bool fill_field_definition(THD *thd, LEX *lex,
                             enum enum_field_types field_type,
                             Create_field *field_def);

  void set_info(longlong created, longlong modified,
		st_sp_chistics *chistics, ulonglong sql_mode);

  void set_definer(const char *definer, uint definerlen);
  void set_definer(const LEX_STRING *user_name, const LEX_STRING *host_name);

  void reset_thd_mem_root(THD *thd);

  void restore_thd_mem_root(THD *thd);

  /**
    Optimize the code.
  */
  void optimize();

  /**
    Helper used during flow analysis during code optimization.
    See the implementation of <code>opt_mark()</code>.
    @param ip the instruction to add to the leads list
    @param leads the list of remaining paths to explore in the graph that
    represents the code, during flow analysis.
  */
  void add_mark_lead(uint ip, List<sp_instr> *leads);

  void recursion_level_error(THD *thd);

  inline sp_instr *
  get_instr(uint i)
  {
    sp_instr *ip;

    if (i < m_instr.elements)
      get_dynamic(&m_instr, (uchar*)&ip, i);
    else
      ip= NULL;
    return ip;
  }

  /* Add tables used by routine to the table list. */
  bool add_used_tables_to_table_list(THD *thd,
                                     TABLE_LIST ***query_tables_last_ptr,
                                     TABLE_LIST *belong_to_view);

  /**
    Check if this stored routine contains statements disallowed
    in a stored function or trigger, and set an appropriate error message
    if this is the case.
  */
  bool is_not_allowed_in_function(const char *where)
  {
    if (m_flags & CONTAINS_DYNAMIC_SQL)
      my_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0), "Dynamic SQL");
    else if (m_flags & MULTI_RESULTS)
      my_error(ER_SP_NO_RETSET, MYF(0), where);
    else if (m_flags & HAS_SET_AUTOCOMMIT_STMT)
      my_error(ER_SP_CANT_SET_AUTOCOMMIT, MYF(0));
    else if (m_flags & HAS_COMMIT_OR_ROLLBACK)
      my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    else if (m_flags & HAS_SQLCOM_RESET)
      my_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0), "RESET");
    else if (m_flags & HAS_SQLCOM_FLUSH)
      my_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0), "FLUSH");

    return test(m_flags &
		(CONTAINS_DYNAMIC_SQL|MULTI_RESULTS|HAS_SET_AUTOCOMMIT_STMT|
                 HAS_COMMIT_OR_ROLLBACK|HAS_SQLCOM_RESET|HAS_SQLCOM_FLUSH));
  }

#ifndef DBUG_OFF
  int show_routine_code(THD *thd);
#endif

  /*
    This method is intended for attributes of a routine which need
    to propagate upwards to the Query_tables_list of the caller (when
    a property of a sp_head needs to "taint" the calling statement).
  */
  void propagate_attributes(Query_tables_list *prelocking_ctx)
  {
    DBUG_ENTER("sp_head::propagate_attributes");
    /*
      If this routine needs row-based binary logging, the entire top statement
      too (we cannot switch from statement-based to row-based only for this
      routine, as in statement-based the top-statement may be binlogged and
      the substatements not).
    */
    DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags(): 0x%x",
                        prelocking_ctx->get_stmt_unsafe_flags()));
    DBUG_PRINT("info", ("sp_head(0x%p=%s)->unsafe_flags: 0x%x",
                        this, name(), unsafe_flags));
    prelocking_ctx->set_stmt_unsafe_flags(unsafe_flags);
    DBUG_VOID_RETURN;
  }

  sp_pcontext *get_parse_context() { return m_pcont; }

private:

  MEM_ROOT *m_thd_root;		///< Temp. store for thd's mem_root
  THD *m_thd;			///< Set if we have reset mem_root

  sp_pcontext *m_pcont;		///< Parse context
  List<LEX> m_lex;		///< Temp. store for the other lex
  DYNAMIC_ARRAY m_instr;	///< The "instructions"
  typedef struct
  {
    struct sp_label *lab;
    sp_instr *instr;
  } bp_t;
  List<bp_t> m_backpatch;	///< Instructions needing backpatching
  /**
    We need a special list for backpatching of instructions with a continue
    destination (in the case of a continue handler catching an error in
    the test), since it would otherwise interfere with the normal backpatch
    mechanism - e.g. jump_if_not instructions have two different destinations
    which are to be patched differently.
    Since these occur in a more restricted way (always the same "level" in
    the code), we don't need the label.
  */
  List<sp_instr_opt_meta> m_cont_backpatch;
  uint m_cont_level;            // The current cont. backpatch level

  /**
    Multi-set representing optimized list of tables to be locked by this
    routine. Does not include tables which are used by invoked routines.

    @note
    For prelocking-free SPs this multiset is constructed too.
    We do so because the same instance of sp_head may be called both
    in prelocked mode and in non-prelocked mode.
  */
  HASH m_sptabs;

  bool
  execute(THD *thd, bool merge_da_on_success);

  /**
    Perform a forward flow analysis in the generated code.
    Mark reachable instructions, for the optimizer.
  */
  void opt_mark();

  /**
    Merge the list of tables used by query into the multi-set of tables used
    by routine.
  */
  bool merge_table_list(THD *thd, TABLE_LIST *table, LEX *lex_for_tmp_check);
}; // class sp_head : public Sql_alloc


//
// "Instructions"...
//

class sp_instr :public Query_arena, public Sql_alloc
{
  sp_instr(const sp_instr &);	/**< Prevent use of these */
  void operator=(sp_instr &);

public:

  uint marked;
  uint m_ip;			///< My index
  sp_pcontext *m_ctx;		///< My parse context

  /// Should give each a name or type code for debugging purposes?
  sp_instr(uint ip, sp_pcontext *ctx)
    :Query_arena(0, STMT_INITIALIZED_FOR_SP), marked(0), m_ip(ip), m_ctx(ctx)
  {}

  virtual ~sp_instr()
  { free_items(); }


  /**
    Execute this instruction

   
    @param thd         Thread handle
    @param[out] nextp  index of the next instruction to execute. (For most
                       instructions this will be the instruction following this
                       one). Note that this parameter is undefined in case of
                       errors, use get_cont_dest() to find the continuation
                       instruction for CONTINUE error handlers.
   
    @retval 0      on success, 
    @retval other  if some error occured
  */

  virtual int execute(THD *thd, uint *nextp) = 0;

  /**
    Execute <code>open_and_lock_tables()</code> for this statement.
    Open and lock the tables used by this statement, as a pre-requisite
    to execute the core logic of this instruction with
    <code>exec_core()</code>.
    @param thd the current thread
    @param tables the list of tables to open and lock
    @return zero on success, non zero on failure.
  */
  int exec_open_and_lock_tables(THD *thd, TABLE_LIST *tables);

  /**
    Get the continuation destination of this instruction.
    @return the continuation destination
  */
  virtual uint get_cont_dest();

  /*
    Execute core function of instruction after all preparations (e.g.
    setting of proper LEX, saving part of the thread context have been
    done).

    Should be implemented for instructions using expressions or whole
    statements (thus having to have own LEX). Used in concert with
    sp_lex_keeper class and its descendants (there are none currently).
  */
  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str) = 0;

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {}

  /**
    Mark this instruction as reachable during optimization and return the
    index to the next instruction. Jump instruction will add their
    destination to the leads list.
  */
  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    marked= 1;
    return m_ip+1;
  }

  /**
    Short-cut jumps to jumps during optimization. This is used by the
    jump instructions' opt_mark() methods. 'start' is the starting point,
    used to prevent the mark sweep from looping for ever. Return the
    end destination.
  */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  /**
    Inform the instruction that it has been moved during optimization.
    Most instructions will simply update its index, but jump instructions
    must also take care of their destination pointers. Forward jumps get
    pushed to the backpatch list 'ibp'.
  */
  virtual void opt_move(uint dst, List<sp_instr> *ibp)
  {
    m_ip= dst;
  }

}; // class sp_instr : public Sql_alloc


/**
  Auxilary class to which instructions delegate responsibility
  for handling LEX and preparations before executing statement
  or calculating complex expression.

  Exist mainly to avoid having double hierarchy between instruction
  classes.

  @todo
    Add ability to not store LEX and do any preparations if
    expression used is simple.
*/

class sp_lex_keeper
{
  /** Prevent use of these */
  sp_lex_keeper(const sp_lex_keeper &);
  void operator=(sp_lex_keeper &);
public:

  sp_lex_keeper(LEX *lex, bool lex_resp)
    : m_lex(lex), m_lex_resp(lex_resp), 
      lex_query_tables_own_last(NULL)
  {
    lex->sp_lex_in_use= TRUE;
  }
  virtual ~sp_lex_keeper()
  {
    if (m_lex_resp)
    {
      /* Prevent endless recursion. */
      m_lex->sphead= NULL;
      lex_end(m_lex);
      delete m_lex;
    }
  }

  /**
    Prepare execution of instruction using LEX, if requested check whenever
    we have read access to tables used and open/lock them, call instruction's
    exec_core() method, perform cleanup afterwards.
   
    @todo Conflicting comment in sp_head.cc
  */
  int reset_lex_and_exec_core(THD *thd, uint *nextp, bool open_tables,
                              sp_instr* instr);

  inline uint sql_command() const
  {
    return (uint)m_lex->sql_command;
  }

  void disable_query_cache()
  {
    m_lex->safe_to_cache_query= 0;
  }
private:

  LEX *m_lex;
  /**
    Indicates whenever this sp_lex_keeper instance responsible
    for LEX deletion.
  */
  bool m_lex_resp;

  /*
    Support for being able to execute this statement in two modes:
    a) inside prelocked mode set by the calling procedure or its ancestor.
    b) outside of prelocked mode, when this statement enters/leaves
       prelocked mode itself.
  */
  
  /**
    List of additional tables this statement needs to lock when it
    enters/leaves prelocked mode on its own.
  */
  TABLE_LIST *prelocking_tables;

  /**
    The value m_lex->query_tables_own_last should be set to this when the
    statement enters/leaves prelocked mode on its own.
  */
  TABLE_LIST **lex_query_tables_own_last;
};


/**
  Call out to some prepared SQL statement.
*/
class sp_instr_stmt : public sp_instr
{
  sp_instr_stmt(const sp_instr_stmt &);	/**< Prevent use of these */
  void operator=(sp_instr_stmt &);

public:

  LEX_STRING m_query;		///< For thd->query

  sp_instr_stmt(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE)
  {
    m_query.str= 0;
    m_query.length= 0;
  }

  virtual ~sp_instr_stmt()
  {};

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_stmt : public sp_instr


class sp_instr_set : public sp_instr
{
  sp_instr_set(const sp_instr_set &);	/**< Prevent use of these */
  void operator=(sp_instr_set &);

public:

  sp_instr_set(uint ip, sp_pcontext *ctx,
	       uint offset, Item *val, enum enum_field_types type_arg,
               LEX *lex, bool lex_resp)
    : sp_instr(ip, ctx), m_offset(offset), m_value(val), m_type(type_arg),
      m_lex_keeper(lex, lex_resp)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_offset;		///< Frame offset
  Item *m_value;
  enum enum_field_types m_type;	///< The declared type
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_set : public sp_instr


/**
  Set NEW/OLD row field value instruction. Used in triggers.
*/
class sp_instr_set_trigger_field : public sp_instr
{
  sp_instr_set_trigger_field(const sp_instr_set_trigger_field &);
  void operator=(sp_instr_set_trigger_field &);

public:

  sp_instr_set_trigger_field(uint ip, sp_pcontext *ctx,
                             Item_trigger_field *trg_fld,
                             Item *val, LEX *lex)
    : sp_instr(ip, ctx),
      trigger_field(trg_fld),
      value(val), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_set_trigger_field()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:
  Item_trigger_field *trigger_field;
  Item *value;
  sp_lex_keeper m_lex_keeper;
}; // class sp_instr_trigger_field : public sp_instr


/**
  An abstract class for all instructions with destinations that
  needs to be updated by the optimizer.

  Even if not all subclasses will use both the normal destination and
  the continuation destination, we put them both here for simplicity.
*/
class sp_instr_opt_meta : public sp_instr
{
public:

  uint m_dest;			///< Where we will go
  uint m_cont_dest;             ///< Where continue handlers will go

  sp_instr_opt_meta(uint ip, sp_pcontext *ctx)
    : sp_instr(ip, ctx),
      m_dest(0), m_cont_dest(0), m_optdest(0), m_cont_optdest(0)
  {}

  sp_instr_opt_meta(uint ip, sp_pcontext *ctx, uint dest)
    : sp_instr(ip, ctx),
      m_dest(dest), m_cont_dest(0), m_optdest(0), m_cont_optdest(0)
  {}

  virtual ~sp_instr_opt_meta()
  {}

  virtual void set_destination(uint old_dest, uint new_dest)
    = 0;

  virtual uint get_cont_dest();

protected:

  sp_instr *m_optdest;		///< Used during optimization
  sp_instr *m_cont_optdest;     ///< Used during optimization

}; // class sp_instr_opt_meta : public sp_instr

class sp_instr_jump : public sp_instr_opt_meta
{
  sp_instr_jump(const sp_instr_jump &);	/**< Prevent use of these */
  void operator=(sp_instr_jump &);

public:

  sp_instr_jump(uint ip, sp_pcontext *ctx)
    : sp_instr_opt_meta(ip, ctx)
  {}

  sp_instr_jump(uint ip, sp_pcontext *ctx, uint dest)
    : sp_instr_opt_meta(ip, ctx, dest)
  {}

  virtual ~sp_instr_jump()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start);

  virtual void opt_move(uint dst, List<sp_instr> *ibp);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {
    /* Calling backpatch twice is a logic flaw in jump resolution. */
    DBUG_ASSERT(m_dest == 0);
    m_dest= dest;
  }

  /**
    Update the destination; used by the optimizer.
  */
  virtual void set_destination(uint old_dest, uint new_dest)
  {
    if (m_dest == old_dest)
      m_dest= new_dest;
  }

}; // class sp_instr_jump : public sp_instr_opt_meta


class sp_instr_jump_if_not : public sp_instr_jump
{
  sp_instr_jump_if_not(const sp_instr_jump_if_not &); /**< Prevent use of these */
  void operator=(sp_instr_jump_if_not &);

public:

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, LEX *lex)
    : sp_instr_jump(ip, ctx), m_expr(i),
      m_lex_keeper(lex, TRUE)
  {}

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, uint dest, LEX *lex)
    : sp_instr_jump(ip, ctx, dest), m_expr(i),
      m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_jump_if_not()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  /** Override sp_instr_jump's shortcut; we stop here */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  virtual void opt_move(uint dst, List<sp_instr> *ibp);

  virtual void set_destination(uint old_dest, uint new_dest)
  {
    sp_instr_jump::set_destination(old_dest, new_dest);
    if (m_cont_dest == old_dest)
      m_cont_dest= new_dest;
  }

private:

  Item *m_expr;			///< The condition
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_jump_if_not : public sp_instr_jump


class sp_instr_freturn : public sp_instr
{
  sp_instr_freturn(const sp_instr_freturn &);	/**< Prevent use of these */
  void operator=(sp_instr_freturn &);

public:

  sp_instr_freturn(uint ip, sp_pcontext *ctx,
		   Item *val, enum enum_field_types type_arg, LEX *lex)
    : sp_instr(ip, ctx), m_value(val), m_type(type_arg),
      m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_freturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    marked= 1;
    return UINT_MAX;
  }

protected:

  Item *m_value;
  enum enum_field_types m_type;
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_freturn : public sp_instr


class sp_instr_hpush_jump : public sp_instr_jump
{
  sp_instr_hpush_jump(const sp_instr_hpush_jump &); /**< Prevent use of these */
  void operator=(sp_instr_hpush_jump &);

public:

  sp_instr_hpush_jump(uint ip, sp_pcontext *ctx, int htype, uint fp)
    : sp_instr_jump(ip, ctx), m_type(htype), m_frame(fp), m_opt_hpop(0)
  {
    m_cond.empty();
  }

  virtual ~sp_instr_hpush_jump()
  {
    m_cond.empty();
  }

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  /** Override sp_instr_jump's shortcut; we stop here. */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {
    DBUG_ASSERT(!m_dest || !m_opt_hpop);
    if (!m_dest)
      m_dest= dest;
    else
      m_opt_hpop= dest;
  }

  inline void add_condition(struct sp_cond_type *cond)
  {
    m_cond.push_front(cond);
  }

private:

  int m_type;			///< Handler type
  uint m_frame;
  uint m_opt_hpop;              // hpop marking end of handler scope.
  List<struct sp_cond_type> m_cond;

}; // class sp_instr_hpush_jump : public sp_instr_jump


class sp_instr_hpop : public sp_instr
{
  sp_instr_hpop(const sp_instr_hpop &);	/**< Prevent use of these */
  void operator=(sp_instr_hpop &);

public:

  sp_instr_hpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_hpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_count;

}; // class sp_instr_hpop : public sp_instr


class sp_instr_hreturn : public sp_instr_jump
{
  sp_instr_hreturn(const sp_instr_hreturn &);	/**< Prevent use of these */
  void operator=(sp_instr_hreturn &);

public:

  sp_instr_hreturn(uint ip, sp_pcontext *ctx, uint fp)
    : sp_instr_jump(ip, ctx), m_frame(fp)
  {}

  virtual ~sp_instr_hreturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  /* This instruction will not be short cut optimized. */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

private:

  uint m_frame;

}; // class sp_instr_hreturn : public sp_instr_jump


/** This is DECLARE CURSOR */
class sp_instr_cpush : public sp_instr
{
  sp_instr_cpush(const sp_instr_cpush &); /**< Prevent use of these */
  void operator=(sp_instr_cpush &);

public:

  sp_instr_cpush(uint ip, sp_pcontext *ctx, LEX *lex, uint offset)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE), m_cursor(offset)
  {}

  virtual ~sp_instr_cpush()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  /**
    This call is used to cleanup the instruction when a sensitive
    cursor is closed. For now stored procedures always use materialized
    cursors and the call is not used.
  */
  virtual void cleanup_stmt() { /* no op */ }
private:

  sp_lex_keeper m_lex_keeper;
  uint m_cursor;                /**< Frame offset (for debugging) */

}; // class sp_instr_cpush : public sp_instr


class sp_instr_cpop : public sp_instr
{
  sp_instr_cpop(const sp_instr_cpop &); /**< Prevent use of these */
  void operator=(sp_instr_cpop &);

public:

  sp_instr_cpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_cpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_count;

}; // class sp_instr_cpop : public sp_instr


class sp_instr_copen : public sp_instr
{
  sp_instr_copen(const sp_instr_copen &); /**< Prevent use of these */
  void operator=(sp_instr_copen &);

public:

  sp_instr_copen(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_copen()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;		///< Stack index

}; // class sp_instr_copen : public sp_instr_stmt


class sp_instr_cclose : public sp_instr
{
  sp_instr_cclose(const sp_instr_cclose &); /**< Prevent use of these */
  void operator=(sp_instr_cclose &);

public:

  sp_instr_cclose(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_cclose()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;

}; // class sp_instr_cclose : public sp_instr


class sp_instr_cfetch : public sp_instr
{
  sp_instr_cfetch(const sp_instr_cfetch &); /**< Prevent use of these */
  void operator=(sp_instr_cfetch &);

public:

  sp_instr_cfetch(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {
    m_varlist.empty();
  }

  virtual ~sp_instr_cfetch()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  void add_to_varlist(struct sp_variable *var)
  {
    m_varlist.push_back(var);
  }

private:

  uint m_cursor;
  List<struct sp_variable> m_varlist;

}; // class sp_instr_cfetch : public sp_instr


class sp_instr_error : public sp_instr
{
  sp_instr_error(const sp_instr_error &); /**< Prevent use of these */
  void operator=(sp_instr_error &);

public:

  sp_instr_error(uint ip, sp_pcontext *ctx, int errcode)
    : sp_instr(ip, ctx), m_errcode(errcode)
  {}

  virtual ~sp_instr_error()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    marked= 1;
    return UINT_MAX;
  }

private:

  int m_errcode;

}; // class sp_instr_error : public sp_instr


class sp_instr_set_case_expr : public sp_instr_opt_meta
{
public:

  sp_instr_set_case_expr(uint ip, sp_pcontext *ctx, uint case_expr_id,
                         Item *case_expr, LEX *lex)
    : sp_instr_opt_meta(ip, ctx),
      m_case_expr_id(case_expr_id), m_case_expr(case_expr),
      m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_set_case_expr()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  virtual void opt_move(uint dst, List<sp_instr> *ibp);

  virtual void set_destination(uint old_dest, uint new_dest)
  {
    if (m_cont_dest == old_dest)
      m_cont_dest= new_dest;
  }

private:

  uint m_case_expr_id;
  Item *m_case_expr;
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_set_case_expr : public sp_instr_opt_meta


#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool
sp_change_security_context(THD *thd, sp_head *sp,
                           Security_context **backup);
void
sp_restore_security_context(THD *thd, Security_context *backup);

bool
set_routine_security_ctx(THD *thd, sp_head *sp, bool is_proc,
                         Security_context **save_ctx);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

TABLE_LIST *
sp_add_to_query_tables(THD *thd, LEX *lex,
		       const char *db, const char *name,
                       thr_lock_type locktype,
                       enum_mdl_type mdl_type);

Item *
sp_prepare_func_item(THD* thd, Item **it_addr);

bool
sp_eval_expr(THD *thd, Field *result_field, Item **expr_item_ptr);

/**
  @} (end of group Stored_Routines)
*/

#endif /* _SP_HEAD_H_ */
