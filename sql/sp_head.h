/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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

/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_class.h"                          // THD, set_var.h: THD
#include "set_var.h"                            // Item
#include "sp_pcontext.h"                        // sp_pcontext

#include <stddef.h>

/**
  @defgroup Stored_Routines Stored Routines
  @ingroup Runtime_Environment
  @{
*/

class sp_instr;
class sp_branch_instr;
class sp_lex_branch_instr;

///////////////////////////////////////////////////////////////////////////

/**
  Stored_program_creation_ctx -- base class for creation context of stored
  programs (stored routines, triggers, events).
*/

class Stored_program_creation_ctx : public Default_object_creation_ctx
{
public:
  const CHARSET_INFO *get_db_cl()
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

  Stored_program_creation_ctx(const CHARSET_INFO *client_cs,
                              const CHARSET_INFO *connection_cl,
                              const CHARSET_INFO *db_cl)
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
  const CHARSET_INFO *m_db_cl;
};

///////////////////////////////////////////////////////////////////////////

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
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_parser_data provides a scope for attributes used at the SP-parsing
  stage only.
*/
class sp_parser_data
{
private:
  struct Backpatch_info
  {
    sp_label *label;
    sp_branch_instr *instr;
  };

public:
  sp_parser_data() :
    m_expr_start_ptr(NULL),
    m_current_stmt_start_ptr(NULL),
    m_option_start_ptr(NULL),
    m_param_start_ptr(NULL),
    m_param_end_ptr(NULL),
    m_body_start_ptr(NULL),
    m_cont_level(0),
    m_saved_memroot(NULL),
    m_saved_free_list(NULL)
  { }

  ///////////////////////////////////////////////////////////////////////

  /**
    Start parsing a stored program body statement.

    This method switches THD::mem_root and THD::free_list in order to parse
    SP-body. The current values are kept to be restored after the body
    statement is parsed.

    @param thd  Thread context.
    @param sp   Stored Program being parsed.
  */
  void start_parsing_sp_body(THD *thd, sp_head *sp);

  /**
    Finish parsing of a stored program body statement.

    This method switches THD::mem_root and THD::free_list back when SP-body
    parsing is completed.

    @param thd  Thread context.
  */
  void finish_parsing_sp_body(THD *thd)
  {
    /*
      In some cases the parser detects a syntax error and calls
      LEX::cleanup_lex_after_parse_error() method only after finishing parsing
      the whole routine. In such a situation sp_head::restore_thd_mem_root() will
      be called twice - the first time as part of normal parsing process and the
      second time by cleanup_lex_after_parse_error().

      To avoid ruining active arena/mem_root state in this case we skip
      restoration of old arena/mem_root if this method has been already called for
      this routine.
    */
    if (!is_parsing_sp_body())
      return;

    thd->mem_root= m_saved_memroot;
    thd->free_list= m_saved_free_list;

    m_saved_memroot= NULL;
    m_saved_free_list= NULL;
  }

  /**
    @retval true if SP-body statement is being parsed.
    @retval false otherwise.
  */
  bool is_parsing_sp_body() const
  { return m_saved_memroot != NULL; }

  ///////////////////////////////////////////////////////////////////////

  void process_new_sp_instr(THD *thd, sp_instr *i);

  ///////////////////////////////////////////////////////////////////////

  /**
    Retrieve expression start pointer in the query string.

    This function is named 'pop' to highlight that it changes the internal
    state, and two subsequent calls may not return same value.

    @note It's true only in the debug mode, but this check is very useful in
    the parser to ensure we "pop" every "pushed" pointer, because we have
    lots of branches, and it's pretty easy to forget something somewhere.
  */
  const char *pop_expr_start_ptr()
  {
#ifndef DBUG_OFF
    DBUG_ASSERT(m_expr_start_ptr);
    const char *p= m_expr_start_ptr;
    m_expr_start_ptr= NULL;
    return p;
#else
    return m_expr_start_ptr;
#endif
  }

  /**
    Remember expression start pointer in the query string.

    This function is named 'push' to highlight that the pointer must be
    retrieved (pop) later.

    @sa the note for pop_expr_start_ptr().
  */
  void push_expr_start_ptr(const char *expr_start_ptr)
  {
    DBUG_ASSERT(!m_expr_start_ptr);
    m_expr_start_ptr= expr_start_ptr;
  }

  ///////////////////////////////////////////////////////////////////////

  const char *get_current_stmt_start_ptr() const
  { return m_current_stmt_start_ptr; }

  void set_current_stmt_start_ptr(const char *stmt_start_ptr)
  { m_current_stmt_start_ptr= stmt_start_ptr; }

  ///////////////////////////////////////////////////////////////////////

  const char *get_option_start_ptr() const
  { return m_option_start_ptr; }

  void set_option_start_ptr(const char *option_start_ptr)
  { m_option_start_ptr= option_start_ptr; }

  ///////////////////////////////////////////////////////////////////////

  const char *get_parameter_start_ptr() const
  { return m_param_start_ptr; }

  void set_parameter_start_ptr(const char *ptr)
  { m_param_start_ptr= ptr; }

  const char *get_parameter_end_ptr() const
  { return m_param_end_ptr; }

  void set_parameter_end_ptr(const char *ptr)
  { m_param_end_ptr= ptr; }

  ///////////////////////////////////////////////////////////////////////

  const char *get_body_start_ptr() const
  { return m_body_start_ptr; }

  void set_body_start_ptr(const char *ptr)
  { m_body_start_ptr= ptr; }

  ///////////////////////////////////////////////////////////////////////

  void push_lex(LEX *lex)
  { m_lex_stack.push_front(lex); }

  LEX *pop_lex()
  { return m_lex_stack.pop(); }

  ///////////////////////////////////////////////////////////////////////
  // Backpatch-list operations.
  ///////////////////////////////////////////////////////////////////////

  /**
    Put the instruction on the backpatch list, associated with the label.

    @param i      The SP-instruction.
    @param label  The label.

    @return Error flag.
  */
  bool add_backpatch_entry(sp_branch_instr *i, sp_label *label);

  /**
    Update all instruction with the given label in the backpatch list
    to the given instruction pointer.

    @param label  The label.
    @param dest   The instruction pointer.
  */
  void do_backpatch(sp_label *label, uint dest);

  ///////////////////////////////////////////////////////////////////////
  // Backpatch operations for supporting CONTINUE handlers.
  ///////////////////////////////////////////////////////////////////////

  /**
    Start a new backpatch level for the SP-instruction requiring continue
    destination. If the SP-instruction is NULL, the level is just increased.

    @note Only subclasses of sp_lex_branch_instr need backpatching of
    continue destinations (and no other classes do):
      - sp_instr_jump_if_not
      - sp_instr_set_case_expr
      - sp_instr_jump_case_when

    That's why the methods below accept sp_lex_branch_instr to make this
    relationship clear. And these two functions are the only places where
    set_cont_dest() is used, so set_cont_dest() is also a member of
    sp_lex_branch_instr.

    @todo These functions should probably be declared in a separate
    interface class, but currently we try to minimize the sp_instr
    hierarchy.

    @return false always.
  */
  bool new_cont_backpatch()
  {
    ++m_cont_level;
    return false;
  }

  /**
    Add a SP-instruction to the current level.

    @param i    The SP-instruction.

    @return Error flag.
  */
  bool add_cont_backpatch_entry(sp_lex_branch_instr *i);

  /**
    Backpatch (and pop) the current level to the given instruction pointer.

    @param dest The instruction pointer.
  */
  void do_cont_backpatch(uint dest);

private:
  /// Start of the expression query string (any but SET-expression).
  const char *m_expr_start_ptr;

  /// Start of the current statement's query string.
  const char *m_current_stmt_start_ptr;

  /// Start of the SET-expression query string.
  const char *m_option_start_ptr;

  /**
    Stack of LEX-objects. It's needed to handle processing of
    sub-statements.
  */
  List<LEX> m_lex_stack;

  /**
    Position in the CREATE PROCEDURE- or CREATE FUNCTION-statement's query
    string corresponding to the start of parameter declarations (stored
    procedure or stored function parameters).
  */
  const char *m_param_start_ptr;

  /**
    Position in the CREATE PROCEDURE- or CREATE FUNCTION-statement's query
    string corresponding to the end of parameter declarations (stored
    procedure or stored function parameters).
  */
  const char *m_param_end_ptr;

  /**
    Position in the CREATE-/ALTER-stored-program statement's query string
    corresponding to the start of the first SQL-statement.
  */
  const char *m_body_start_ptr;

  /// Instructions needing backpatching
  List<Backpatch_info> m_backpatch;

  /**
    We need a special list for backpatching of instructions with a continue
    destination (in the case of a continue handler catching an error in
    the test), since it would otherwise interfere with the normal backpatch
    mechanism - e.g. jump_if_not instructions have two different destinations
    which are to be patched differently.
    Since these occur in a more restricted way (always the same "level" in
    the code), we don't need the label.
  */
  List<sp_lex_branch_instr> m_cont_backpatch;

  /// The current continue backpatch level
  uint m_cont_level;

  /**********************************************************************
    The following attributes are used to store THD values during parsing
    of stored program body.

    @sa start_parsing_sp_body()
    @sa finish_parsing_sp_body()
  **********************************************************************/

  /// THD's memroot.
  MEM_ROOT *m_saved_memroot;

  /// THD's free-list.
  Item *m_saved_free_list;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_head represents one instance of a stored program. It might be of any type
  (stored procedure, function, trigger, event).
*/
class sp_head : private Query_arena
{
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

public:
  /************************************************************************
    Public attributes.
  ************************************************************************/

  /// Stored program type.
  enum_sp_type m_type;

  /// Stored program flags.
  uint m_flags;

  /**
    Definition of the RETURN-field (from the RETURNS-clause).
    It's used (and valid) for stored functions only.
  */
  Create_field m_return_field_def;

  /// Attributes used during the parsing stage only.
  sp_parser_data m_parser_data;

  /// Stored program characteristics.
  st_sp_chistics *m_chistics;

  /**
    The value of sql_mode system variable at the CREATE-time.

    It should be stored along with the character sets in the
    Stored_program_creation_ctx.
  */
  sql_mode_t m_sql_mode;

  /// Fully qualified name (<db name>.<sp name>).
  LEX_STRING m_qname;

  bool m_explicit_name;         ///< Prepend the db name? */

  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_params;
  LEX_STRING m_body;
  LEX_STRING m_body_utf8;
  LEX_STRING m_defstr;
  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;

  longlong m_created;
  longlong m_modified;

  /// Recursion level of the current SP instance. The levels are numbered from 0.
  ulong m_recursion_level;

  /**
    A list of diferent recursion level instances for the same procedure.
    For every recursion level we have a sp_head instance. This instances
    connected in the list. The list ordered by increasing recursion level
    (m_recursion_level).
  */
  sp_head *m_next_cached_sp;

  /// Pointer to the first element of the above list
  sp_head *m_first_instance;

  /**
    Pointer to the first free (non-INVOKED) routine in the list of
    cached instances for this SP. This pointer is set only for the first
    SP in the list of instances (see above m_first_cached_sp pointer).
    The pointer equal to 0 if we have no free instances.
    For non-first instance value of this pointer meaningless (point to itself);
  */
  sp_head *m_first_free_instance;

  /**
    Pointer to the last element in the list of instances of the SP.
    For non-first instance value of this pointer meaningless (point to itself);
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

  /*
    Security context for stored routine which should be run under
    definer privileges.
  */
  Security_context m_security_ctx;

  /////////////////////////////////////////////////////////////////////////
  // Trigger-specific public attributes.
  /////////////////////////////////////////////////////////////////////////

  /**
    List of all items (Item_trigger_field objects) representing fields in
    old/new version of row in trigger. We use this list for checking whenever
    all such fields are valid at trigger creation time and for binding these
    fields to TABLE object at table open (although for latter pointer to table
    being opened is probably enough).
  */
  SQL_I_List<Item_trigger_field> m_trg_table_fields;

  /// Trigger characteristics.
  st_trg_chistics m_trg_chistics;

  /// The Table_triggers_list instance, where this trigger belongs to.
  class Table_triggers_list *m_trg_list;

public:
  static void *operator new(size_t size) throw ();
  static void operator delete(void *ptr, size_t size) throw ();

  ~sp_head();

  /// Is this routine being executed?
  bool is_invoked() const
  { return m_flags & IS_INVOKED; }

  /**
    Get the value of the SP cache version, as remembered
    when the routine was inserted into the cache.
  */
  ulong sp_cache_version() const
  { return m_sp_cache_version; }

  /// Set the value of the SP cache version.
  void set_sp_cache_version(ulong sp_cache_version)
  { m_sp_cache_version= sp_cache_version; }

  Stored_program_creation_ctx *get_creation_ctx()
  { return m_creation_ctx; }

  void set_creation_ctx(Stored_program_creation_ctx *creation_ctx)
  { m_creation_ctx= creation_ctx->clone(mem_root); }

  /// Set the body-definition start position.
  void set_body_start(THD *thd, const char *begin_ptr);

  /// Set the statement-definition (body-definition) end position.
  void set_body_end(THD *thd);

  /**
    Execute trigger stored program.

    - changes security context for triggers
    - switch to new memroot
    - call sp_head::execute
    - restore old memroot
    - restores security context

    @param thd               Thread context
    @param db                database name
    @param table             table name
    @param grant_info        GRANT_INFO structure to be filled with
                             information about definer's privileges
                             on subject table

    @todo
      - TODO: we should create sp_rcontext once per command and reuse it
      on subsequent executions of a trigger.

    @return Error status.
  */
  bool execute_trigger(THD *thd,
                       const LEX_STRING *db_name,
                       const LEX_STRING *table_name,
                       GRANT_INFO *grant_info);

  /**
    Execute a function.

     - evaluate parameters
     - changes security context for SUID routines
     - switch to new memroot
     - call sp_head::execute
     - restore old memroot
     - evaluate the return value
     - restores security context

    @param thd               Thread context.
    @param argp              Passed arguments (these are items from containing
                             statement?)
    @param argcount          Number of passed arguments. We need to check if
                             this is correct.
    @param return_value_fld  Save result here.

    @todo
      We should create sp_rcontext once per command and reuse
      it on subsequent executions of a function/trigger.

    @todo
      In future we should associate call arena/mem_root with
      sp_rcontext and allocate all these objects (and sp_rcontext
      itself) on it directly rather than juggle with arenas.

    @return Error status.
  */
  bool execute_function(THD *thd, Item **args, uint argcount,
                        Field *return_fld);

  /**
    Execute a procedure.

    The function does the following steps:
     - Set all parameters
     - changes security context for SUID routines
     - call sp_head::execute
     - copy back values of INOUT and OUT parameters
     - restores security context

    @param thd  Thread context.
    @param args List of values passed as arguments.

    @return Error status.
  */

  bool execute_procedure(THD *thd, List<Item> *args);

  /**
    Implement SHOW CREATE statement for stored routines.

    @param thd  Thread context.
    @param type         Stored routine type
                        (SP_TYPE_PROCEDURE or SP_TYPE_FUNCTION)

    @return Error status.
  */
  bool show_create_routine(THD *thd, enum_sp_type type);

  /**
    Add instruction to SP.

    @param thd    Thread context.
    @param instr  Instruction.

    @return Error status.
  */
  bool add_instr(THD *thd, sp_instr *instr);

  uint instructions()
  { return m_instructions.elements(); }

  sp_instr *last_instruction()
  { return *m_instructions.back(); }

  /**
    Reset LEX-object during parsing, before we parse a sub statement.

    @param thd  Thread context.

    @return Error status.
  */
  bool reset_lex(THD *thd);

  /**
    Restore LEX-object during parsing, after we have parsed a sub statement.

    @param thd  Thread context.

    @return Error status.
  */
  bool restore_lex(THD *thd);

  char *name(uint *lenp = 0) const
  {
    if (lenp)
      *lenp= (uint) m_name.length;
    return m_name.str;
  }

  char *create_string(THD *thd, ulong *lenp);

  /**
    Create Field-object corresponding to the RETURN field of a stored function.
    This operation makes sense for stored functions only.

    @param field_max_length the max length (in the sense of Item classes).
    @param field_name       the field name (item name).
    @param table            the field's table.

    @return newly created and initialized Field-instance,
    or NULL in case of error.
  */
  Field *create_result_field(uint field_max_length,
                             const char *field_name,
                             TABLE *table);

  void set_info(longlong created,
                longlong modified,
		st_sp_chistics *chistics,
                sql_mode_t sql_mode);

  void set_definer(const char *definer, uint definerlen);
  void set_definer(const LEX_STRING *user_name, const LEX_STRING *host_name);

  /**
    Do some minimal optimization of the code:
      -# Mark used instructions
      -# While doing this, shortcut jumps to jump instructions
      -# Compact the code, removing unused instructions.

    This is the main mark and move loop; it relies on the following methods
    in sp_instr and its subclasses:

      - opt_mark()         :  Mark instruction as reachable
      - opt_shortcut_jump():  Shortcut jumps to the final destination;
                             used by opt_mark().
      - opt_move()         :  Update moved instruction
      - set_destination()  :  Set the new destination (jump instructions only)
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

  /**
    Get SP-instruction at given index.

    NOTE: it is important to have *unsigned* int here, sometimes we get (-1)
    passed here, so it get's converted to MAX_INT, and the result of the
    function call is NULL.
  */
  sp_instr *get_instr(uint i)
  {
    return (i < (uint) m_instructions.elements()) ? m_instructions.at(i) : NULL;
  }

  /**
    Add tables used by routine to the table list.

      Converts multi-set of tables used by this routine to table list and adds
      this list to the end of table list specified by 'query_tables_last_ptr'.

      Elements of list will be allocated in PS memroot, so this list will be
      persistent between PS executions.

    @param[in] thd                        Thread context
    @param[in,out] query_tables_last_ptr  Pointer to the next_global member of
                                          last element of the list where tables
                                          will be added (or to its root).
    @param[in] belong_to_view             Uppermost view which uses this routine,
                                          NULL if none.

    @retval true  if some elements were added
    @retval false otherwise.
  */
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
  /**
    Return the routine instructions as a result set.
    @return Error status.
  */
  bool show_routine_code(THD *thd);
#endif

  /*
    This method is intended for attributes of a routine which need
    to propagate upwards to the Query_tables_list of the caller (when
    a property of a sp_head needs to "taint" the calling statement).
  */
  void propagate_attributes(Query_tables_list *prelocking_ctx)
  {
    /*
      If this routine needs row-based binary logging, the entire top statement
      too (we cannot switch from statement-based to row-based only for this
      routine, as in statement-based the top-statement may be binlogged and
      the sub-statements not).
    */
    DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags(): 0x%x",
                        prelocking_ctx->get_stmt_unsafe_flags()));
    DBUG_PRINT("info", ("sp_head(0x%p=%s)->unsafe_flags: 0x%x",
                        this, name(), unsafe_flags));
    prelocking_ctx->set_stmt_unsafe_flags(unsafe_flags);
  }

  /**
    @return root parsing context for this stored program.
  */
  sp_pcontext *get_root_parsing_context() const
  { return const_cast<sp_pcontext *> (m_root_parsing_ctx); }

  /**
    @return SP-persistent mem-root. Instructions and expressions are stored in
    its memory between executions.
  */
  MEM_ROOT *get_persistent_mem_root() const
  { return const_cast<MEM_ROOT *> (&main_mem_root); }

  /**
    @return currently used mem-root.
  */
  MEM_ROOT *get_current_mem_root() const
  { return const_cast<MEM_ROOT *> (mem_root); }

  /**
    Check if a user has access right to a SP.

    @param      thd          Thread context.
    @param[out] full_access  Set to 1 if the user has SELECT
                             to the 'mysql.proc' table or is
                             the owner of the stored program.

    @return Error status.
  */
  bool check_show_access(THD *thd, bool *full_access);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /**
    Change routine security context, and check if there is an EXECUTE privilege in
    new context. If there is no EXECUTE privilege, change the context back and
    return an error.

    @param      thd      Thread context.
    @param[out] save_ctx Where to save the old security context.

    @todo Cache if the definer has the rights to use the object on the first usage
    and reset the cache only if someone does a GRANT statement that 'may' affect
    this.

    @return Error status.
  */
  bool set_security_ctx(THD *thd, Security_context **save_ctx);
#endif

private:
  /// Use sp_start_parsing() to create instances of sp_head.
  sp_head(enum_sp_type type);

  /// SP-persistent memory root (for instructions and expressions).
  MEM_ROOT main_mem_root;

  /// Root parsing context (topmost BEGIN..END block) of this SP.
  sp_pcontext *m_root_parsing_ctx;

  /// The SP-instructions.
  Dynamic_array<sp_instr *> m_instructions;

  /**
    Multi-set representing optimized list of tables to be locked by this
    routine. Does not include tables which are used by invoked routines.

    @note
    For prelocking-free SPs this multiset is constructed too.
    We do so because the same instance of sp_head may be called both
    in prelocked mode and in non-prelocked mode.
  */
  HASH m_sptabs;

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

  /// Snapshot of several system variables at CREATE-time.
  Stored_program_creation_ctx *m_creation_ctx;

  /// Flags of LEX::enum_binlog_stmt_unsafe.
  uint32 unsafe_flags;

private:
  /// Copy sp name from parser.
  void init_sp_name(THD *thd, sp_name *spname);

  /**
    Execute the routine. The main instruction jump loop is there.
    Assume the parameters already set.

    @param thd                  Thread context.
    @param merge_da_on_success  Flag specifying if Warning Info should be
                                propagated to the caller on Completion
                                Condition or not.

    @todo
      - Will write this SP statement into binlog separately
      (TODO: consider changing the condition to "not inside event union")

    @return Error status.
  */
  bool execute(THD *thd, bool merge_da_on_success);

  /**
    Perform a forward flow analysis in the generated code.
    Mark reachable instructions, for the optimizer.
  */
  void opt_mark();

  /**
    Merge the list of tables used by some query into the multi-set of
    tables used by routine.

    @param thd                 Thread context.
    @param table               Table list.
    @param lex_for_tmp_check   LEX of the query for which we are merging
                               table list.

    @note
      This method will use LEX provided to check whenever we are creating
      temporary table and mark it as such in target multi-set.

    @return Error status.
  */
  bool merge_table_list(THD *thd, TABLE_LIST *table, LEX *lex_for_tmp_check);

  friend sp_head *sp_start_parsing(THD *, enum_sp_type, sp_name *);

  // Prevent use of copy constructor and assignment operator.
  sp_head(const sp_head &);
  void operator=(sp_head &);
};

///////////////////////////////////////////////////////////////////////////
//
// "Instructions"...
//
///////////////////////////////////////////////////////////////////////////

/**
  sp_printable defines an interface which should be implemented if a class wants
  report some internal information about its state.
*/
class sp_printable
{
public:
  virtual void print(String *str) = 0;

  virtual ~sp_printable()
  { }
};

///////////////////////////////////////////////////////////////////////////

/**
  An interface for all SP-instructions with destinations that
  need to be updated by the SP-optimizer.
*/
class sp_branch_instr
{
public:
  /**
    Update the destination; used by the SP-instruction-optimizer.

    @param old_dest current (old) destination (instruction pointer).
    @param new_dest new destination (instruction pointer).
  */
  virtual void set_destination(uint old_dest, uint new_dest) = 0;

  /**
    Update all instruction with the given label in the backpatch list to
    the specified instruction pointer.

    @param dest     destination instruction pointer.
  */
  virtual void backpatch(uint dest) = 0;

  virtual ~sp_branch_instr()
  { }
};

/**
  Base class for every SP-instruction. sp_instr defines interface and provides
  base implementation.
*/
class sp_instr : public Query_arena,
                 public Sql_alloc,
                 public sp_printable
{
public:
  sp_instr(uint ip, sp_pcontext *ctx)
   :Query_arena(0, STMT_INITIALIZED_FOR_SP),
    m_marked(false),
    m_ip(ip),
    m_parsing_ctx(ctx)
  { }

  virtual ~sp_instr()
  { free_items(); }

  /**
    Execute this instruction

    @param thd         Thread context
    @param[out] nextp  index of the next instruction to execute. (For most
                       instructions this will be the instruction following this
                       one). Note that this parameter is undefined in case of
                       errors, use get_cont_dest() to find the continuation
                       instruction for CONTINUE error handlers.

    @return Error status.
  */
  virtual bool execute(THD *thd, uint *nextp) = 0;

  uint get_ip() const
  { return m_ip; }

  /**
    Get the continuation destination (instruction pointer for the CONTINUE
    HANDLER) of this instruction.
    @return the continuation destination
  */
  virtual uint get_cont_dest() const
  { return get_ip() + 1; }

  sp_pcontext *get_parsing_ctx() const
  { return m_parsing_ctx; }

  ///////////////////////////////////////////////////////////////////////////
  // The following operations are used solely for SP-code-optimizer.
  ///////////////////////////////////////////////////////////////////////////

  /**
    Mark this instruction as reachable during optimization and return the
    index to the next instruction. Jump instruction will add their
    destination to the leads list.
  */
  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    m_marked= true;
    return get_ip() + 1;
  }

  /**
    Short-cut jumps to jumps during optimization. This is used by the
    jump instructions' opt_mark() methods. 'start' is the starting point,
    used to prevent the mark sweep from looping for ever. Return the
    end destination.
  */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  { return get_ip(); }

  /**
    Inform the instruction that it has been moved during optimization.
    Most instructions will simply update its index, but jump instructions
    must also take care of their destination pointers. Forward jumps get
    pushed to the backpatch list 'ibp'.
  */
  virtual void opt_move(uint dst, List<sp_branch_instr> *ibp)
  { m_ip= dst; }

  bool opt_is_marked() const
  { return m_marked; }

protected:
  /// Show if this instruction is reachable within the SP
  /// (used by SP-optimizer).
  bool m_marked;

  /// Instruction pointer.
  uint m_ip;

  /// Instruction parsing context.
  sp_pcontext *m_parsing_ctx;

private:
  // Prevent use of copy constructor and assignment operator.
  sp_instr(const sp_instr &);
  void operator= (sp_instr &);
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_lex_instr is a class providing the interface and base implementation
  for SP-instructions, whose execution is based on expression evaluation.

  sp_lex_instr keeps LEX-object to be able to evaluate the expression.

  sp_lex_instr also provides possibility to re-parse the original query
  string if for some reason the LEX-object is not valid any longer.
*/
class sp_lex_instr : public sp_instr
{
public:
  sp_lex_instr(uint ip, sp_pcontext *ctx, LEX *lex, bool is_lex_owner)
   :sp_instr(ip, ctx),
    m_lex(NULL),
    m_is_lex_owner(false),
    m_first_execution(true),
    m_prelocking_tables(NULL),
    m_lex_query_tables_own_last(NULL)
  {
    set_lex(lex, is_lex_owner);
  }

  virtual ~sp_lex_instr()
  { free_lex(); }

  /**
    Make a few attempts to execute the instruction.

    Basically, this operation does the following things:
      - install Reprepare_observer to catch metadata changes (if any);
      - calls reset_lex_and_exec_core() to execute the instruction;
      - if the execution fails due to a change in metadata, re-parse the
        instruction's SQL-statement and repeat execution.

    @param      thd           Thread context.
    @param[out] nextp         Next instruction pointer
    @param      open_tables   Flag to specify if the function should check read
                              access to tables in LEX's table list and open and
                              lock them (used in instructions which need to
                              calculate some expression and don't execute
                              complete statement).

    @return Error status.
  */
  bool validate_lex_and_execute_core(THD *thd, uint *nextp, bool open_tables);

private:
  /**
    Prepare LEX and thread for execution of instruction, if requested open
    and lock LEX's tables, execute instruction's core function, perform
    cleanup afterwards.

    @param thd           thread context
    @param nextp[out]    next instruction pointer
    @param open_tables   if TRUE then check read access to tables in LEX's table
                         list and open and lock them (used in instructions which
                         need to calculate some expression and don't execute
                         complete statement).

    @note
      We are not saving/restoring some parts of THD which may need this because
      we do this once for whole routine execution in sp_head::execute().

    @return Error status.
  */
  bool reset_lex_and_exec_core(THD *thd, uint *nextp, bool open_tables);

  /**
    (Re-)parse the query corresponding to this instruction and return a new
    LEX-object.

    @param thd  Thread context.
    @param sp   The stored program.

    @return new LEX-object or NULL in case of failure.
  */
  LEX *parse_expr(THD *thd, sp_head *sp);

  /**
     Set LEX-object.

     Previously assigned LEX-object (if any) will be properly cleaned up
     and destroyed.

     @param lex          LEX-object to be used by this instance of sp_lex_instr.
     @param is_lex_owner the flag specifying if this instance sp_lex_instr
                         owns (and thus deletes when needed) passed LEX-object.
  */
  void set_lex(LEX *lex, bool is_lex_owner);

  /**
     Cleanup and destroy assigned LEX-object if needed.
  */
  void free_lex();

public:
  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp)
  { return validate_lex_and_execute_core(thd, nextp, true); }

protected:
  /////////////////////////////////////////////////////////////////////////
  // Interface (virtual) methods.
  /////////////////////////////////////////////////////////////////////////

  /**
    Execute core function of instruction after all preparations
    (e.g. setting of proper LEX, saving part of the thread context).

    @param thd  Thread context.
    @param nextp[out]    next instruction pointer

    @return Error flag.
  */
  virtual bool exec_core(THD *thd, uint *nextp) = 0;

  /**
    @retval false if the object (i.e. LEX-object) is valid and exec_core() can be
    just called.

    @retval true if the object is not valid any longer, exec_core() can not be
    called. The original query string should be re-parsed and a new LEX-object
    should be used.
  */
  virtual bool is_invalid() const = 0;

  /**
    Invalidate the object.
  */
  virtual void invalidate() = 0;

  /**
    Return the query string, which can be passed to the parser. I.e. the
    operation should return a valid SQL-statement query string.

    @param[out] sql_query SQL-statement query string.
  */
  virtual void get_query(String *sql_query) const;

  /**
    @return the expression query string. This string can not be passed directly
    to the parser as it is most likely not a valid SQL-statement.

    @note as it can be seen in the get_query() implementation, get_expr_query()
    might return EMPTY_STR. EMPTY_STR means that no query-expression is
    available. That happens when class provides different implementation of
    get_query(). Strictly speaking, this is a drawback of the current class
    hierarchy.
  */
  virtual LEX_STRING get_expr_query() const
  { return EMPTY_STR; }

  /**
    Callback function which is called after the statement query string is
    successfully parsed, and the thread context has not been switched to the
    outer context. The thread context contains new LEX-object corresponding to
    the parsed query string.

    @param thd  Thread context.

    @return Error flag.
  */
  virtual bool on_after_expr_parsing(THD *thd)
  { return false; }

  /**
    Destroy items in the free list before re-parsing the statement query
    string (and thus, creating new items).

    @param thd  Thread context.
  */
  virtual void cleanup_before_parsing(THD *thd);

private:
  /// LEX-object.
  LEX *m_lex;

  /**
    Indicates whether this sp_lex_instr instance is responsible for
    LEX-object deletion.
  */
  bool m_is_lex_owner;

  /**
    Indicates whether exec_core() has not been already called on the current
    LEX-object.
  */
  bool m_first_execution;

  /*****************************************************************************
    Support for being able to execute this statement in two modes:
    a) inside prelocked mode set by the calling procedure or its ancestor.
    b) outside of prelocked mode, when this statement enters/leaves
       prelocked mode itself.
  *****************************************************************************/

  /**
    List of additional tables this statement needs to lock when it
    enters/leaves prelocked mode on its own.
  */
  TABLE_LIST *m_prelocking_tables;

  /**
    The value m_lex->query_tables_own_last should be set to this when the
    statement enters/leaves prelocked mode on its own.
  */
  TABLE_LIST **m_lex_query_tables_own_last;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_stmt represents almost all conventional SQL-statements, which are
  supported outside stored programs.

  SET-statements, which deal with SP-variable or NEW/OLD trigger pseudo-rows are
  not represented by this instruction.
*/
class sp_instr_stmt : public sp_lex_instr
{
public:
  sp_instr_stmt(uint ip,
                LEX *lex,
                LEX_STRING query)
   :sp_lex_instr(ip, lex->get_sp_current_parsing_ctx(), lex, true),
    m_query(query),
    m_valid(true)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool is_invalid() const
  { return !m_valid; }

  virtual void invalidate()
  { m_valid= false; }

  virtual void get_query(String *sql_query) const
  { sql_query->append(m_query.str, m_query.length); }

private:
  /// Complete query of the SQL-statement.
  LEX_STRING m_query;

  /// Specify if the stored LEX-object is up-to-date.
  bool m_valid;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_set represents SET-statememnts, which deal with SP-variables.
*/
class sp_instr_set : public sp_lex_instr
{
public:
  sp_instr_set(uint ip,
               LEX *lex,
	       uint offset,
               Item *value_item,
               LEX_STRING value_query,
               bool is_lex_owner)
   :sp_lex_instr(ip, lex->get_sp_current_parsing_ctx(), lex, is_lex_owner),
    m_offset(offset),
    m_value_item(value_item),
    m_value_query(value_query)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool is_invalid() const
  { return m_value_item == NULL; }

  virtual void invalidate()
  { m_value_item= NULL; }

  virtual bool on_after_expr_parsing(THD *thd)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_value_item= thd->lex->select_lex.item_list.head();

    return false;
  }

  virtual LEX_STRING get_expr_query() const
  { return m_value_query; }

private:
  /// Frame offset.
  uint m_offset;

  /// Value expression item of the SET-statement.
  Item *m_value_item;

  /// SQL-query corresponding to the value expression.
  LEX_STRING m_value_query;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_set_trigger_field represents SET-statements, which deal with NEW/OLD
  trigger pseudo-rows.
*/
class sp_instr_set_trigger_field : public sp_lex_instr
{
public:
  sp_instr_set_trigger_field(uint ip,
                             LEX *lex,
                             LEX_STRING trigger_field_name,
                             Item_trigger_field *trigger_field,
                             Item *value_item,
                             LEX_STRING value_query)
   :sp_lex_instr(ip, lex->get_sp_current_parsing_ctx(), lex, true),
    m_trigger_field_name(trigger_field_name),
    m_trigger_field(trigger_field),
    m_value_item(value_item),
    m_value_query(value_query)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool is_invalid() const
  { return m_value_item == NULL; }

  virtual void invalidate()
  { m_value_item= NULL; }

  virtual bool on_after_expr_parsing(THD *thd);

  virtual void cleanup_before_parsing(THD *thd);

  virtual LEX_STRING get_expr_query() const
  { return m_value_query; }

private:
  /// Trigger field name ("field_name" of the "NEW.field_name").
  LEX_STRING m_trigger_field_name;

  /// Item corresponding to the NEW/OLD trigger field.
  Item_trigger_field *m_trigger_field;

  /// Value expression item of the SET-statement.
  Item *m_value_item;

  /// SQL-query corresponding to the value expression.
  LEX_STRING m_value_query;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_freturn represents RETURN statement in stored functions.
*/
class sp_instr_freturn : public sp_lex_instr
{
public:
  sp_instr_freturn(uint ip,
                   LEX *lex,
		   Item *expr_item,
                   LEX_STRING expr_query,
                   enum enum_field_types return_field_type)
   :sp_lex_instr(ip, lex->get_sp_current_parsing_ctx(), lex, true),
    m_expr_item(expr_item),
    m_expr_query(expr_query),
    m_return_field_type(return_field_type)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    m_marked= true;
    return UINT_MAX;
  }

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool is_invalid() const
  { return m_expr_item == NULL; }

  virtual void invalidate()
  {
    // it's already deleted.
    m_expr_item= NULL;
  }

  virtual bool on_after_expr_parsing(THD *thd)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_expr_item= thd->lex->select_lex.item_list.head();

    return false;
  }

  virtual LEX_STRING get_expr_query() const
  { return m_expr_query; }

private:
  /// RETURN-expression item.
  Item *m_expr_item;

  /// SQL-query corresponding to the RETURN-expression.
  LEX_STRING m_expr_query;

  /// RETURN-field type code.
  enum enum_field_types m_return_field_type;
};

///////////////////////////////////////////////////////////////////////////

/**
  This is base class for all kinds of jump instructions.

  @note this is the only class, we directly construct instances of, that has
  subclasses. We also redefine sp_instr_jump behavior in those subclasses.

  @todo later we will consider introducing a new class, which will be the base
  for sp_instr_jump, sp_instr_set_case_expr and sp_instr_jump_case_when.
  Something like sp_regular_branch_instr (similar to sp_lex_branch_instr).
*/
class sp_instr_jump : public sp_instr,
                      public sp_branch_instr
{
public:
  sp_instr_jump(uint ip, sp_pcontext *ctx)
   :sp_instr(ip, ctx),
    m_dest(0),
    m_optdest(NULL)
  { }

  sp_instr_jump(uint ip, sp_pcontext *ctx, uint dest)
   :sp_instr(ip, ctx),
    m_dest(dest),
    m_optdest(NULL)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp)
  {
    *nextp= m_dest;
    return false;
  }

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start);

  virtual void opt_move(uint dst, List<sp_branch_instr> *ibp);

  /////////////////////////////////////////////////////////////////////////
  // sp_branch_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_destination(uint old_dest, uint new_dest)
  {
    if (m_dest == old_dest)
      m_dest= new_dest;
  }

  virtual void backpatch(uint dest)
  {
    /* Calling backpatch twice is a logic flaw in jump resolution. */
    DBUG_ASSERT(m_dest == 0);
    m_dest= dest;
  }

protected:
  /// Where we will go.
  uint m_dest;

  // The following attribute is used by SP-optimizer.
  sp_instr *m_optdest;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_lex_branch_instr is a base class for SP-instructions, which might perform
  conditional jump depending on the value of an SQL-expression.
*/
class sp_lex_branch_instr : public sp_lex_instr,
                            public sp_branch_instr
{
protected:
  sp_lex_branch_instr(uint ip, sp_pcontext *ctx, LEX *lex,
                      Item *expr_item, LEX_STRING expr_query)
   :sp_lex_instr(ip, ctx, lex, true),
    m_dest(0),
    m_cont_dest(0),
    m_optdest(NULL),
    m_cont_optdest(NULL),
    m_expr_item(expr_item),
    m_expr_query(expr_query)
  { }

  sp_lex_branch_instr(uint ip, sp_pcontext *ctx, LEX *lex,
                      Item *expr_item, LEX_STRING expr_query,
                      uint dest)
   :sp_lex_instr(ip, ctx, lex, true),
    m_dest(dest),
    m_cont_dest(0),
    m_optdest(NULL),
    m_cont_optdest(NULL),
    m_expr_item(expr_item),
    m_expr_query(expr_query)
  { }

public:
  void set_cont_dest(uint cont_dest)
  { m_cont_dest= cont_dest; }

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  virtual void opt_move(uint dst, List<sp_branch_instr> *ibp);

  virtual uint get_cont_dest() const
  { return m_cont_dest; }

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_invalid() const
  { return m_expr_item == NULL; }

  virtual void invalidate()
  { m_expr_item= NULL; /* it's already deleted. */ }

  virtual LEX_STRING get_expr_query() const
  { return m_expr_query; }

  /////////////////////////////////////////////////////////////////////////
  // sp_branch_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_destination(uint old_dest, uint new_dest)
  {
    if (m_dest == old_dest)
      m_dest= new_dest;

    if (m_cont_dest == old_dest)
      m_cont_dest= new_dest;
  }

  virtual void backpatch(uint dest)
  {
    /* Calling backpatch twice is a logic flaw in jump resolution. */
    DBUG_ASSERT(m_dest == 0);
    m_dest= dest;
  }

protected:
  /// Where we will go.
  uint m_dest;

  /// Where continue handlers will go.
  uint m_cont_dest;

  // The following attributes are used by SP-optimizer.
  sp_instr *m_optdest;
  sp_instr *m_cont_optdest;

  /// Expression item.
  Item *m_expr_item;

  /// SQL-query corresponding to the expression.
  LEX_STRING m_expr_query;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_jump_if_not implements SP-instruction, which does the jump if its
  SQL-expression is false.
*/
class sp_instr_jump_if_not : public sp_lex_branch_instr
{
public:
  sp_instr_jump_if_not(uint ip,
                       LEX *lex,
                       Item *expr_item,
                       LEX_STRING expr_query)
   :sp_lex_branch_instr(ip, lex->get_sp_current_parsing_ctx(), lex,
                        expr_item, expr_query)
  { }

  sp_instr_jump_if_not(uint ip,
                       LEX *lex,
                       Item *expr_item,
                       LEX_STRING expr_query,
                       uint dest)
   :sp_lex_branch_instr(ip, lex->get_sp_current_parsing_ctx(), lex,
                        expr_item, expr_query, dest)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool on_after_expr_parsing(THD *thd)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_expr_item= thd->lex->select_lex.item_list.head();

    return false;
  }
};

///////////////////////////////////////////////////////////////////////////
// Instructions used for the "simple CASE" implementation.
///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_set_case_expr is used in the "simple CASE" implementation to evaluate
  and store the CASE-expression in the runtime context.
*/
class sp_instr_set_case_expr : public sp_lex_branch_instr
{
public:
  sp_instr_set_case_expr(uint ip,
                         LEX *lex,
                         uint case_expr_id,
                         Item *case_expr_item,
                         LEX_STRING case_expr_query)
   :sp_lex_branch_instr(ip, lex->get_sp_current_parsing_ctx(), lex,
                        case_expr_item, case_expr_query),
    m_case_expr_id(case_expr_id)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  virtual void opt_move(uint dst, List<sp_branch_instr> *ibp);

  /////////////////////////////////////////////////////////////////////////
  // sp_branch_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  /*
    NOTE: set_destination() and backpatch() are overriden here just because the
    m_dest attribute is not used by this class, so there is no need to do
    anything about it.

    @todo These operations probably should be left as they are (i.e. do not
    override them here). The m_dest attribute would be set and not used, but
    that should not be a big deal.

    @todo This also indicates deficiency of the current SP-istruction class
    hierarchy.
  */

  virtual void set_destination(uint old_dest, uint new_dest)
  {
    if (m_cont_dest == old_dest)
      m_cont_dest= new_dest;
  }

  virtual void backpatch(uint dest)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool on_after_expr_parsing(THD *thd)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_expr_item= thd->lex->select_lex.item_list.head();

    return false;
  }

private:
  /// Identifier (index) of the CASE-expression in the runtime context.
  uint m_case_expr_id;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_jump_case_when instruction is used in the "simple CASE"
  implementation. It's a jump instruction with the following condition:
    (CASE-expression = WHEN-expression)
  CASE-expression is retrieved from sp_rcontext;
  WHEN-expression is kept by this instruction.
*/
class sp_instr_jump_case_when : public sp_lex_branch_instr
{
public:
  sp_instr_jump_case_when(uint ip,
                          LEX *lex,
                          int case_expr_id,
                          Item *when_expr_item,
                          LEX_STRING when_expr_query)
   :sp_lex_branch_instr(ip, lex->get_sp_current_parsing_ctx(), lex,
                        when_expr_item, when_expr_query),
    m_case_expr_id(case_expr_id)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual void invalidate()
  {
    // Items should be already deleted in lex-keeper.
    m_case_expr_item= NULL;
    m_eq_item= NULL;
    m_expr_item= NULL; // it's a WHEN-expression.
  }

  virtual bool on_after_expr_parsing(THD *thd)
  { return build_expr_items(thd); }

private:
  /**
    Build CASE-expression item tree:
      Item_func_eq(case-expression, when-i-expression)

    This function is used for the following form of CASE statement:
      CASE case-expression
        WHEN when-1-expression THEN ...
        WHEN when-2-expression THEN ...
        ...
        WHEN when-n-expression THEN ...
      END CASE

    The thing is that after the parsing we have an item (item tree) for the
    case-expression and for each when-expression. Here we build jump
    conditions: expressions like (case-expression = when-i-expression).

    @param thd  Thread context.

    @return Error flag.
  */
  bool build_expr_items(THD *thd);

private:
  /// Identifier (index) of the CASE-expression in the runtime context.
  int m_case_expr_id;

  /// Item representing the CASE-expression.
  Item_case_expr *m_case_expr_item;

  /**
    Item corresponding to the main item of the jump-condition-expression:
    it's the equal function (=) in the (case-expression = when-i-expression)
    expression.
  */
  Item *m_eq_item;
};

///////////////////////////////////////////////////////////////////////////
// SQL-condition handler instructions.
///////////////////////////////////////////////////////////////////////////

class sp_instr_hpush_jump : public sp_instr_jump
{
public:
  sp_instr_hpush_jump(uint ip,
                      sp_pcontext *ctx,
                      sp_handler *handler)
   :sp_instr_jump(ip, ctx),
    m_handler(handler),
    m_opt_hpop(0),
    m_frame(ctx->current_var_count())
  {
    DBUG_ASSERT(m_handler->condition_values.elements == 0);
  }

  virtual ~sp_instr_hpush_jump()
  {
    m_handler->condition_values.empty();
    m_handler= NULL;
  }

  void add_condition(sp_condition_value *condition_value)
  { m_handler->condition_values.push_back(condition_value); }

  sp_handler *get_handler()
  { return m_handler; }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

  /** Override sp_instr_jump's shortcut; we stop here. */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  { return get_ip(); }

  /////////////////////////////////////////////////////////////////////////
  // sp_branch_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void backpatch(uint dest)
  {
    DBUG_ASSERT(!m_dest || !m_opt_hpop);
    if (!m_dest)
      m_dest= dest;
    else
      m_opt_hpop= dest;
  }

private:
  /// Handler.
  sp_handler *m_handler;

  /// hpop marking end of handler scope.
  uint m_opt_hpop;

  // This attribute is needed for SHOW PROCEDURE CODE only (i.e. it's needed in
  // debug version only). It's used in print().
  uint m_frame;
};

///////////////////////////////////////////////////////////////////////////

class sp_instr_hpop : public sp_instr
{
public:
  sp_instr_hpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

private:
  /// How many handlers this instruction should pop.
  uint m_count;
};

///////////////////////////////////////////////////////////////////////////

class sp_instr_hreturn : public sp_instr_jump
{
public:
  sp_instr_hreturn(uint ip, sp_pcontext *ctx)
   :sp_instr_jump(ip, ctx),
    m_frame(ctx->current_var_count())
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

  /** Override sp_instr_jump's shortcut; we stop here. */
  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  { return get_ip(); }

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads);

private:
  // This attribute is needed for SHOW PROCEDURE CODE only (i.e. it's needed in
  // debug version only). It's used in print().
  uint m_frame;
};

///////////////////////////////////////////////////////////////////////////
// Cursor implementation.
///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_cpush corresponds to DECLARE CURSOR, implements DECLARE CURSOR and
  OPEN.

  This is the most important instruction in cursor implementation. It is created
  and added to sp_head when DECLARE CURSOR is being parsed. The arena of this
  instruction contains LEX-object for the cursor's SELECT-statement.

  This instruction is actually used to open the cursor.

  execute() operation "implements" DECLARE CURSOR statement -- it merely pushes
  a new cursor object into the stack in sp_rcontext object.

  exec_core() operation implements OPEN statement. It is important to implement
  OPEN statement in this instruction, because OPEN may lead to re-parsing of the
  SELECT-statement. So, the original Arena and parsing context must be used.
*/
class sp_instr_cpush : public sp_lex_instr
{
public:
  sp_instr_cpush(uint ip,
                 sp_pcontext *ctx,
                 LEX *cursor_lex,
                 LEX_STRING cursor_query,
                 int cursor_idx)
   :sp_lex_instr(ip, ctx, cursor_lex, true),
    m_cursor_query(cursor_query),
    m_valid(true),
    m_cursor_idx(cursor_idx)
  {
    // Cursor can't be stored in Query Cache, so we should prevent opening QC
    // for try to write results which are absent.

    cursor_lex->safe_to_cache_query= false;
  }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // Query_arena implementation.
  /////////////////////////////////////////////////////////////////////////

  /**
    This call is used to cleanup the instruction when a sensitive
    cursor is closed. For now stored procedures always use materialized
    cursors and the call is not used.
  */
  virtual void cleanup_stmt()
  { /* no op */ }

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

  /////////////////////////////////////////////////////////////////////////
  // sp_lex_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool exec_core(THD *thd, uint *nextp);

  virtual bool is_invalid() const
  { return !m_valid; }

  virtual void invalidate()
  { m_valid= false; }

  virtual void get_query(String *sql_query) const
  { sql_query->append(m_cursor_query.str, m_cursor_query.length); }

private:
  /// This attribute keeps the cursor SELECT statement.
  LEX_STRING m_cursor_query;

  /// Flag if the LEX-object of this instruction is valid or not.
  /// The LEX-object is not valid when metadata have changed.
  bool m_valid;

  /// Used to identify the cursor in the sp_rcontext.
  int m_cursor_idx;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_cpop instruction is added at the end of BEGIN..END block.
  It's used to remove declared cursors so that they are not visible any longer.
*/
class sp_instr_cpop : public sp_instr
{
public:
  sp_instr_cpop(uint ip, sp_pcontext *ctx, uint count)
   :sp_instr(ip, ctx),
    m_count(count)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

private:
  uint m_count;
};

///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_copen represents OPEN statement (opens the cursor).
  However, the actual implementation is in sp_instr_cpush::exec_core().
*/
class sp_instr_copen : public sp_instr
{
public:
  sp_instr_copen(uint ip, sp_pcontext *ctx, int cursor_idx)
   :sp_instr(ip, ctx),
    m_cursor_idx(cursor_idx)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

private:
  /// Used to identify the cursor in the sp_rcontext.
  int m_cursor_idx;
};

///////////////////////////////////////////////////////////////////////////

/**
  The instruction corresponds to the CLOSE statement.
  It just forwards the close-call to the appropriate sp_cursor object in the
  sp_rcontext.
*/
class sp_instr_cclose : public sp_instr
{
public:
  sp_instr_cclose(uint ip, sp_pcontext *ctx, int cursor_idx)
   :sp_instr(ip, ctx),
    m_cursor_idx(cursor_idx)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

private:
  /// Used to identify the cursor in the sp_rcontext.
  int m_cursor_idx;
};

///////////////////////////////////////////////////////////////////////////

/**
  The instruction corresponds to the FETCH statement.
  It just forwards the close-call to the appropriate sp_cursor object in the
  sp_rcontext.
*/
class sp_instr_cfetch : public sp_instr
{
public:
  sp_instr_cfetch(uint ip, sp_pcontext *ctx, int cursor_idx)
   :sp_instr(ip, ctx),
    m_cursor_idx(cursor_idx)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp);

  void add_to_varlist(sp_variable *var)
  { m_varlist.push_back(var); }

private:
  /// List of SP-variables to store fetched values.
  List<sp_variable> m_varlist;

  /// Used to identify the cursor in the sp_rcontext.
  int m_cursor_idx;
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  sp_instr_error just throws an SQL-condition if the execution flow comes to it.
  It's used in the CASE implementation to perform runtime-check that the
  CASE-expression is handled by some WHEN/ELSE clause.
*/
class sp_instr_error : public sp_instr
{
public:
  sp_instr_error(uint ip, sp_pcontext *ctx, int errcode)
   :sp_instr(ip, ctx),
    m_errcode(errcode)
  { }

  /////////////////////////////////////////////////////////////////////////
  // sp_printable implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual void print(String *str);

  /////////////////////////////////////////////////////////////////////////
  // sp_instr implementation.
  /////////////////////////////////////////////////////////////////////////

  virtual bool execute(THD *thd, uint *nextp)
  {
    my_message(m_errcode, ER(m_errcode), MYF(0));
    *nextp= get_ip() + 1;
    return true;
  }

  virtual uint opt_mark(sp_head *sp, List<sp_instr> *leads)
  {
    m_marked= true;
    return UINT_MAX;
  }

private:
  /// The error code, which should be raised by this instruction.
  int m_errcode;
};

///////////////////////////////////////////////////////////////////////////

/**
  @} (end of group Stored_Routines)
*/

#endif /* _SP_HEAD_H_ */
