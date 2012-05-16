/*
   Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"         // NO_EMBEDDED_ACCESS_CHECKS
#include "sql_priv.h"
#include "unireg.h"
#include "sql_prepare.h"
#include "sql_cache.h"         // query_cache_*
#include "probes_mysql.h"
#include "sql_show.h"          // append_identifier
#include "sql_db.h"            // mysql_opt_change_db, mysql_change_db
#include "sql_table.h"         // prepare_create_field
#include "sql_acl.h"           // *_ACL
#include "sql_array.h"         // Dynamic_array
#include "log_event.h"         // append_query_string, Query_log_event

#include "sp_head.h"
#include "sp.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp_cache.h"
#include "set_var.h"
#include "sql_parse.h"         // cleanup_items
#include "sql_base.h"          // close_thread_tables
#include "transaction.h"       // trans_commit_stmt
#include "opt_trace.h"         // opt_trace_disable_etc
#include "global_threads.h"

#include <my_user.h>           // parse_user

#include <algorithm>

// Sufficient max length of printed destinations and frame offsets (all uints).
#define SP_INSTR_UINT_MAXLEN  8
#define SP_STMT_PRINT_MAXLEN 40

/**
  SP_TABLE represents all instances of one table in an optimized multi-set of
  tables used by a stored program.
*/
struct SP_TABLE
{
  /*
    Multi-set key:
      db_name\0table_name\0alias\0 - for normal tables
      db_name\0table_name\0        - for temporary tables
    Note that in both cases we don't take last '\0' into account when
    we count length of key.
  */
  LEX_STRING qname;
  uint db_length, table_name_length;
  bool temp;               /* true if corresponds to a temporary table */
  thr_lock_type lock_type; /* lock type used for prelocking */
  uint lock_count;
  uint query_lock_count;
  uint8 trg_event_map;
};


///////////////////////////////////////////////////////////////////////////
// Static function implementations.
///////////////////////////////////////////////////////////////////////////


uchar *sp_table_key(const uchar *ptr, size_t *plen, my_bool first)
{
  SP_TABLE *tab= (SP_TABLE *)ptr;
  *plen= tab->qname.length;
  return (uchar *)tab->qname.str;
}


/**
  Helper function which operates on a THD object to set the query start_time to
  the current time.

  @param thd  Thread context.
*/
static void reset_start_time_for_sp(THD *thd)
{
  if (thd->in_sub_stmt)
    return;

  /*
    First investigate if there is a cached time stamp
  */
  if (thd->user_time.tv_sec || thd->user_time.tv_usec)
    thd->start_time= thd->user_time;
  else
    my_micro_time_to_timeval(my_micro_time(), &thd->start_time);
}


/**
  Merge contents of two hashes representing sets of routines used
  by statements or by other routines.

  @param dst   hash to which elements should be added
  @param src   hash from which elements merged

  @note
    This procedure won't create new Sroutine_hash_entry objects,
    instead it will simply add elements from source to destination
    hash. Thus time of life of elements in destination hash becomes
    dependant on time of life of elements from source hash. It also
    won't touch lists linking elements in source and destination
    hashes.

    @return Error status.
*/

static bool sp_update_sp_used_routines(HASH *dst, HASH *src)
{
  for (uint i= 0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)my_hash_element(src, i);
    if (!my_hash_search(dst, (uchar *)rt->mdl_request.key.ptr(),
                        rt->mdl_request.key.length()))
    {
      if (my_hash_insert(dst, (uchar *)rt))
        return true;
    }
  }
  return false;
}


/**
  Return a string representation of the Item value.

  @param thd  Thread context.
  @param str  String buffer for representation of the value.

  @note
    If the item has a string result type, the string is escaped
    according to its character set.

  @retval NULL      on error
  @retval non-NULL  a pointer to valid a valid string on success
*/

static String *sp_get_item_value(THD *thd, Item *item, String *str)
{
  switch (item->result_type()) {
  case REAL_RESULT:
  case INT_RESULT:
  case DECIMAL_RESULT:
    if (item->field_type() != MYSQL_TYPE_BIT)
      return item->val_str(str);
    else {/* Bit type is handled as binary string */}
  case STRING_RESULT:
    {
      String *result= item->val_str(str);

      if (!result)
        return NULL;

      {
        char buf_holder[STRING_BUFFER_USUAL_SIZE];
        String buf(buf_holder, sizeof(buf_holder), result->charset());
        const CHARSET_INFO *cs= thd->variables.character_set_client;

        /* We must reset length of the buffer, because of String specificity. */
        buf.length(0);

        buf.append('_');
        buf.append(result->charset()->csname);
        if (cs->escape_with_backslash_is_dangerous)
          buf.append(' ');
        append_query_string(thd, cs, result, &buf);
        buf.append(" COLLATE '");
        buf.append(item->collation.collation->name);
        buf.append('\'');
        str->copy(buf);

        return str;
      }
    }

  case ROW_RESULT:
  default:
    return NULL;
  }
}


static int cmp_splocal_locations(Item_splocal * const *a,
                                 Item_splocal * const *b)
{
  return (int)((*a)->pos_in_query - (*b)->pos_in_query);
}


/*
  StoredRoutinesBinlogging
  This paragraph applies only to statement-based binlogging. Row-based
  binlogging does not need anything special like this.

  Top-down overview:

  1. Statements

  Statements that have is_update_query(stmt) == TRUE are written into the
  binary log verbatim.
  Examples:
    UPDATE tbl SET tbl.x = spfunc_w_side_effects()
    UPDATE tbl SET tbl.x=1 WHERE spfunc_w_side_effect_that_returns_false(tbl.y)

  Statements that have is_update_query(stmt) == FALSE (e.g. SELECTs) are not
  written into binary log. Instead we catch function calls the statement
  makes and write it into binary log separately (see #3).

  2. PROCEDURE calls

  CALL statements are not written into binary log. Instead
  * Any FUNCTION invocation (in SET, IF, WHILE, OPEN CURSOR and other SP
    instructions) is written into binlog separately.

  * Each statement executed in SP is binlogged separately, according to rules
    in #1, with the exception that we modify query string: we replace uses
    of SP local variables with NAME_CONST('spvar_name', <spvar-value>) calls.
    This substitution is done in subst_spvars().

  3. FUNCTION calls

  In sp_head::execute_function(), we check
   * If this function invocation is done from a statement that is written
     into the binary log.
   * If there were any attempts to write events to the binary log during
     function execution (grep for start_union_events and stop_union_events)

   If the answers are No and Yes, we write the function call into the binary
   log as "SELECT spfunc(<param1value>, <param2value>, ...)"


  4. Miscellaneous issues.

  4.1 User variables.

  When we call mysql_bin_log.write() for an SP statement, thd->user_var_events
  must hold set<{var_name, value}> pairs for all user variables used during
  the statement execution.
  This set is produced by tracking user variable reads during statement
  execution.

  For SPs, this has the following implications:
  1) thd->user_var_events may contain events from several SP statements and
     needs to be valid after execution of these statements was finished. In
     order to achieve that, we
     * Allocate user_var_events array elements on appropriate mem_root (grep
       for user_var_events_alloc).
     * Use is_query_in_union() to determine if user_var_event is created.

  2) We need to empty thd->user_var_events after we have wrote a function
     call. This is currently done by making
     reset_dynamic(&thd->user_var_events);
     calls in several different places. (TODO consider moving this into
     mysql_bin_log.write() function)

  4.2 Auto_increment storage in binlog

  As we may write two statements to binlog from one single logical statement
  (case of "SELECT func1(),func2()": it is binlogged as "SELECT func1()" and
  then "SELECT func2()"), we need to reset auto_increment binlog variables
  after each binlogged SELECT. Otherwise, the auto_increment value of the
  first SELECT would be used for the second too.
*/


/**
  Replace thd->query{_length} with a string that one can write to
  the binlog.

  The binlog-suitable string is produced by replacing references to SP local
  variables with NAME_CONST('sp_var_name', value) calls.

  @param thd        Current thread.
  @param instr      Instruction (we look for Item_splocal instances in
                    instr->free_list)
  @param query_str  Original query string

  @retval false on success.
  thd->query{_length} either has been appropriately replaced or there
  is no need for replacements.

  @retval true in case of out of memory error.
*/

static bool subst_spvars(THD *thd, sp_instr *instr, LEX_STRING *query_str)
{
  Dynamic_array<Item_splocal*> sp_vars_uses;
  char *pbuf, *cur, buffer[512];
  String qbuf(buffer, sizeof(buffer), &my_charset_bin);
  int prev_pos, res, buf_len;

  /* Find all instances of Item_splocal used in this statement */
  for (Item *item= instr->free_list; item; item= item->next)
  {
    if (item->is_splocal())
    {
      Item_splocal *item_spl= (Item_splocal*)item;
      if (item_spl->pos_in_query)
        sp_vars_uses.append(item_spl);
    }
  }

  if (!sp_vars_uses.elements())
    return false;

  /* Sort SP var refs by their occurrences in the query */
  sp_vars_uses.sort(cmp_splocal_locations);

  /*
    Construct a statement string where SP local var refs are replaced
    with "NAME_CONST(name, value)"
  */
  qbuf.length(0);
  cur= query_str->str;
  prev_pos= res= 0;
  thd->query_name_consts= 0;

  for (Item_splocal **splocal= sp_vars_uses.front(); 
       splocal <= sp_vars_uses.back(); splocal++)
  {
    Item *val;

    char str_buffer[STRING_BUFFER_USUAL_SIZE];
    String str_value_holder(str_buffer, sizeof(str_buffer),
                            &my_charset_latin1);
    String *str_value;

    /* append the text between sp ref occurrences */
    res|= qbuf.append(cur + prev_pos, (*splocal)->pos_in_query - prev_pos);
    prev_pos= (*splocal)->pos_in_query + (*splocal)->len_in_query;

    res|= (*splocal)->fix_fields(thd, (Item **) splocal);
    if (res)
      break;

    if ((*splocal)->limit_clause_param)
    {
      res|= qbuf.append_ulonglong((*splocal)->val_uint());
      if (res)
        break;
      continue;
    }

    /* append the spvar substitute */
    res|= qbuf.append(STRING_WITH_LEN(" NAME_CONST('"));
    res|= qbuf.append((*splocal)->m_name);
    res|= qbuf.append(STRING_WITH_LEN("',"));

    if (res)
      break;

    val= (*splocal)->this_item();
    str_value= sp_get_item_value(thd, val, &str_value_holder);
    if (str_value)
      res|= qbuf.append(*str_value);
    else
      res|= qbuf.append(STRING_WITH_LEN("NULL"));
    res|= qbuf.append(')');
    if (res)
      break;

    thd->query_name_consts++;
  }
  if (res ||
      qbuf.append(cur + prev_pos, query_str->length - prev_pos))
    return true;

  /*
    Allocate additional space at the end of the new query string for the
    query_cache_send_result_to_client function.

    The query buffer layout is:
       buffer :==
            <statement>   The input statement(s)
            '\0'          Terminating null char
            <length>      Length of following current database name (size_t)
            <db_name>     Name of current database
            <flags>       Flags struct
  */
  buf_len= qbuf.length() + 1 + sizeof(size_t) + thd->db_length + 
           QUERY_CACHE_FLAGS_SIZE + 1;
  if ((pbuf= (char *) alloc_root(thd->mem_root, buf_len)))
  {
    memcpy(pbuf, qbuf.ptr(), qbuf.length());
    pbuf[qbuf.length()]= 0;
    memcpy(pbuf+qbuf.length()+1, (char *) &thd->db_length, sizeof(size_t));
  }
  else
    return true;

  thd->set_query(pbuf, qbuf.length());

  return false;
}

///////////////////////////////////////////////////////////////////////////
// sp_name implementation.
///////////////////////////////////////////////////////////////////////////

/**
  Create temporary sp_name object from MDL key.

  @note The lifetime of this object is bound to the lifetime of the MDL_key.
        This should be fine as sp_name objects created by this constructor
        are mainly used for SP-cache lookups.

  @param key         MDL key containing database and routine name.
  @param qname_buff  Buffer to be used for storing quoted routine name
                     (should be at least 2*NAME_LEN+1+1 bytes).
*/

sp_name::sp_name(const MDL_key *key, char *qname_buff)
{
  m_db.str= (char*)key->db_name();
  m_db.length= key->db_name_length();
  m_name.str= (char*)key->name();
  m_name.length= key->name_length();
  m_qname.str= qname_buff;
  if (m_db.length)
  {
    strxmov(qname_buff, m_db.str, ".", m_name.str, NullS);
    m_qname.length= m_db.length + 1 + m_name.length;
  }
  else
  {
    strmov(qname_buff, m_name.str);
    m_qname.length= m_name.length;
  }
  m_explicit_name= false;
}


/**
  Init the qualified name from the db and name.
*/
void sp_name::init_qname(THD *thd)
{
  const uint dot= !!m_db.length;
  /* m_qname format: [database + dot] + name + '\0' */
  m_qname.length= m_db.length + dot + m_name.length;
  if (!(m_qname.str= (char*) thd->alloc(m_qname.length + 1)))
    return;
  sprintf(m_qname.str, "%.*s%.*s%.*s",
          (int) m_db.length, (m_db.length ? m_db.str : ""),
          dot, ".",
          (int) m_name.length, m_name.str);
}

///////////////////////////////////////////////////////////////////////////
// sp_head implementation.
///////////////////////////////////////////////////////////////////////////

void *sp_head::operator new(size_t size) throw()
{
  MEM_ROOT own_root;

  init_sql_alloc(&own_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);

  sp_head *sp= (sp_head *) alloc_root(&own_root, size);
  if (!sp)
    return NULL;

  sp->main_mem_root= own_root;
  DBUG_PRINT("info", ("mem_root 0x%lx", (ulong) &sp->mem_root));
  return sp;
}

void sp_head::operator delete(void *ptr, size_t size) throw()
{
  if (!ptr)
    return;

  sp_head *sp= (sp_head *) ptr;

  /* Make a copy of main_mem_root as free_root will free the sp */
  MEM_ROOT own_root= sp->main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to 0x%lx",
                      (ulong) &sp->mem_root, (ulong) &own_root));
  free_root(&own_root, MYF(0));
}


sp_head::sp_head(enum_sp_type type)
 :Query_arena(&main_mem_root, STMT_INITIALIZED_FOR_SP),
  m_type(type),
  m_flags(0),
  m_chistics(NULL),
  m_sql_mode(0),
  m_explicit_name(false),
  m_created(0),
  m_modified(0),
  m_recursion_level(0),
  m_next_cached_sp(NULL),
  m_first_instance(NULL),
  m_first_free_instance(NULL),
  m_last_cached_sp(NULL),
  m_trg_list(NULL),
  m_root_parsing_ctx(NULL),
  m_sp_cache_version(0),
  m_creation_ctx(NULL),
  unsafe_flags(0)
{
  m_first_instance= this;
  m_first_free_instance= this;
  m_last_cached_sp= this;

  m_return_field_def.charset = NULL;

  /*
    FIXME: the only use case when name is NULL is events, and it should
    be rewritten soon. Remove the else part and replace 'if' with
    an assert when this is done.
  */

  m_db= NULL_STR;
  m_name= NULL_STR;
  m_qname= NULL_STR;

  m_params= NULL_STR;

  m_defstr= NULL_STR;
  m_body= NULL_STR;
  m_body_utf8= NULL_STR;

  my_hash_init(&m_sptabs, system_charset_info, 0, 0, 0, sp_table_key, 0, 0);
  my_hash_init(&m_sroutines, system_charset_info, 0, 0, 0, sp_sroutine_key,
               0, 0);
}


void sp_head::init_sp_name(THD *thd, sp_name *spname)
{
  /* Must be initialized in the parser. */

  DBUG_ASSERT(spname && spname->m_db.str && spname->m_db.length);

  /* We have to copy strings to get them into the right memroot. */

  m_db.length= spname->m_db.length;
  m_db.str= strmake_root(thd->mem_root, spname->m_db.str, spname->m_db.length);

  m_name.length= spname->m_name.length;
  m_name.str= strmake_root(thd->mem_root, spname->m_name.str,
                           spname->m_name.length);

  m_explicit_name= spname->m_explicit_name;

  if (spname->m_qname.length == 0)
    spname->init_qname(thd);

  m_qname.length= spname->m_qname.length;
  m_qname.str= (char*) memdup_root(thd->mem_root,
                                   spname->m_qname.str,
                                   spname->m_qname.length + 1);
}


void sp_head::set_body_start(THD *thd, const char *begin_ptr)
{
  m_parser_data.set_body_start_ptr(begin_ptr);

  thd->m_parser_state->m_lip.body_utf8_start(thd, begin_ptr);
}


void sp_head::set_body_end(THD *thd)
{
  Lex_input_stream *lip= & thd->m_parser_state->m_lip; /* shortcut */
  const char *end_ptr= lip->get_cpp_ptr(); /* shortcut */

  /* Make the string of parameters. */

  {
    const char *p_start= m_parser_data.get_parameter_start_ptr();
    const char *p_end= m_parser_data.get_parameter_end_ptr();

    if (p_start && p_end)
    {
      m_params.length= p_end - p_start;
      m_params.str= thd->strmake(p_start, m_params.length);
    }
  }

  /* Remember end pointer for further dumping of whole statement. */

  thd->lex->stmt_definition_end= end_ptr;

  /* Make the string of body (in the original character set). */

  m_body.length= end_ptr - m_parser_data.get_body_start_ptr();
  m_body.str= thd->strmake(m_parser_data.get_body_start_ptr(), m_body.length);
  trim_whitespace(thd->charset(), & m_body);

  /* Make the string of UTF-body. */

  lip->body_utf8_append(end_ptr);

  m_body_utf8.length= lip->get_body_utf8_length();
  m_body_utf8.str= thd->strmake(lip->get_body_utf8_str(), m_body_utf8.length);
  trim_whitespace(thd->charset(), & m_body_utf8);

  /*
    Make the string of whole stored-program-definition query (in the
    original character set).
  */

  m_defstr.length= end_ptr - lip->get_cpp_buf();
  m_defstr.str= thd->strmake(lip->get_cpp_buf(), m_defstr.length);
  trim_whitespace(thd->charset(), & m_defstr);
}


sp_head::~sp_head()
{
  LEX *lex;
  sp_instr *i;

  // Parsing of SP-body must have been already finished.
  DBUG_ASSERT(!m_parser_data.is_parsing_sp_body());

  for (uint ip = 0 ; (i = get_instr(ip)) ; ip++)
    delete i;

  delete m_root_parsing_ctx;

  free_items();

  /*
    If we have non-empty LEX stack then we just came out of parser with
    error. Now we should delete all auxiliary LEXes and restore original
    THD::lex. It is safe to not update LEX::ptr because further query
    string parsing and execution will be stopped anyway.
  */
  while ((lex= (LEX *) m_parser_data.pop_lex()))
  {
    THD *thd= lex->thd;
    thd->lex->sphead= NULL;
    lex_end(thd->lex);
    delete thd->lex;
    thd->lex= lex;
  }

  my_hash_free(&m_sptabs);
  my_hash_free(&m_sroutines);

  delete m_next_cached_sp;
}


Field *sp_head::create_result_field(uint field_max_length,
                                    const char *field_name,
                                    TABLE *table)
{
  uint field_length= !m_return_field_def.length ?
                     field_max_length : m_return_field_def.length;

  Field *field=
    ::make_field(table->s,                     /* TABLE_SHARE ptr */
                 (uchar*) 0,                   /* field ptr */
                 field_length,                 /* field [max] length */
                 (uchar*) "",                  /* null ptr */
                 0,                            /* null bit */
                 m_return_field_def.pack_flag,
                 m_return_field_def.sql_type,
                 m_return_field_def.charset,
                 m_return_field_def.geom_type,
                 Field::NONE,                  /* unreg check */
                 m_return_field_def.interval,
                 field_name ? field_name : (const char *) m_name.str);

  if (field)
    field->init(table);

  return field;
}


bool sp_head::execute(THD *thd, bool merge_da_on_success)
{
  char saved_cur_db_name_buf[NAME_LEN+1];
  LEX_STRING saved_cur_db_name=
    { saved_cur_db_name_buf, sizeof(saved_cur_db_name_buf) };
  bool cur_db_changed= FALSE;
  bool err_status= FALSE;
  uint ip= 0;
  sql_mode_t save_sql_mode;
  bool save_abort_on_warning;
  Query_arena *old_arena;
  /* per-instruction arena */
  MEM_ROOT execute_mem_root;
  Query_arena execute_arena(&execute_mem_root, STMT_INITIALIZED_FOR_SP),
              backup_arena;
  query_id_t old_query_id;
  TABLE *old_derived_tables;
  LEX *old_lex;
  Item_change_list old_change_list;
  String old_packet;
  Object_creation_ctx *saved_creation_ctx;
  Diagnostics_area *da= thd->get_stmt_da();
  Warning_info sp_wi(da->warning_info_id(), false);

  /*
    Just reporting a stack overrun error
    (@sa check_stack_overrun()) requires stack memory for error
    message buffer. Thus, we have to put the below check
    relatively close to the beginning of the execution stack,
    where available stack margin is still big. As long as the check
    has to be fairly high up the call stack, the amount of memory
    we "book" for has to stay fairly high as well, and hence
    not very accurate. The number below has been calculated
    by trial and error, and reflects the amount of memory necessary
    to execute a single stored procedure instruction, be it either
    an SQL statement, or, heaviest of all, a CALL, which involves
    parsing and loading of another stored procedure into the cache
    (@sa db_load_routine() and Bug#10100).
    At the time of measuring, a recursive SP invocation required
    3232 bytes of stack on 32 bit Linux, 6016 bytes on 64 bit Mac
    and 11152 on 64 bit Solaris sparc.
    The same with db_load_routine() required circa 7k bytes and
    14k bytes accordingly. Hence, here we book the stack with some
    reasonable margin.

    Reverting back to 8 * STACK_MIN_SIZE until further fix.
    8 * STACK_MIN_SIZE is required on some exotic platforms.
  */
  if (check_stack_overrun(thd, 8 * STACK_MIN_SIZE, (uchar*)&old_packet))
    return true;

  opt_trace_disable_if_no_security_context_access(thd);

  /* init per-instruction memroot */
  init_sql_alloc(&execute_mem_root, MEM_ROOT_BLOCK_SIZE, 0);

  DBUG_ASSERT(!(m_flags & IS_INVOKED));
  m_flags|= IS_INVOKED;
  m_first_instance->m_first_free_instance= m_next_cached_sp;
  if (m_next_cached_sp)
  {
    DBUG_PRINT("info",
               ("first free for 0x%lx ++: 0x%lx->0x%lx  level: %lu  flags %x",
                (ulong)m_first_instance, (ulong) this,
                (ulong) m_next_cached_sp,
                m_next_cached_sp->m_recursion_level,
                m_next_cached_sp->m_flags));
  }
  /*
    Check that if there are not any instances after this one then
    pointer to the last instance points on this instance or if there are
    some instances after this one then recursion level of next instance
    greater then recursion level of current instance on 1
  */
  DBUG_ASSERT((m_next_cached_sp == 0 &&
               m_first_instance->m_last_cached_sp == this) ||
              (m_recursion_level + 1 == m_next_cached_sp->m_recursion_level));

  /*
    NOTE: The SQL Standard does not specify the context that should be
    preserved for stored routines. However, at SAP/Walldorf meeting it was
    decided that current database should be preserved.
  */

  if (m_db.length &&
      (err_status= mysql_opt_change_db(thd, &m_db, &saved_cur_db_name, FALSE,
                                       &cur_db_changed)))
  {
    goto done;
  }

  thd->is_slave_error= 0;
  old_arena= thd->stmt_arena;

  /* Push a new warning information area. */
  da->copy_sql_conditions_to_wi(thd, &sp_wi);
  da->push_warning_info(&sp_wi);

  /*
    Switch query context. This has to be done early as this is sometimes
    allocated trough sql_alloc
  */
  saved_creation_ctx= m_creation_ctx->set_n_backup(thd);

  /*
    We have to save/restore this info when we are changing call level to
    be able properly do close_thread_tables() in instructions.
  */
  old_query_id= thd->query_id;
  old_derived_tables= thd->derived_tables;
  thd->derived_tables= 0;
  save_sql_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= m_sql_mode;
  save_abort_on_warning= thd->abort_on_warning;
  thd->abort_on_warning= 0;
  /**
    When inside a substatement (a stored function or trigger
    statement), clear the metadata observer in THD, if any.
    Remember the value of the observer here, to be able
    to restore it when leaving the substatement.

    We reset the observer to suppress errors when a substatement
    uses temporary tables. If a temporary table does not exist
    at start of the main statement, it's not prelocked
    and thus is not validated with other prelocked tables.

    Later on, when the temporary table is opened, metadata
    versions mismatch, expectedly.

    The proper solution for the problem is to re-validate tables
    of substatements (Bug#12257, Bug#27011, Bug#32868, Bug#33000),
    but it's not implemented yet.
  */
  thd->push_reprepare_observer(NULL);

  /*
    It is also more efficient to save/restore current thd->lex once when
    do it in each instruction
  */
  old_lex= thd->lex;
  /*
    We should also save Item tree change list to avoid rollback something
    too early in the calling query.
  */
  thd->change_list.move_elements_to(&old_change_list);
  /*
    Cursors will use thd->packet, so they may corrupt data which was prepared
    for sending by upper level. OTOH cursors in the same routine can share this
    buffer safely so let use use routine-local packet instead of having own
    packet buffer for each cursor.

    It is probably safe to use same thd->convert_buff everywhere.
  */
  old_packet.swap(thd->packet);

  /*
    Switch to per-instruction arena here. We can do it since we cleanup
    arena after every instruction.
  */
  thd->set_n_backup_active_arena(&execute_arena, &backup_arena);

  /*
    Save callers arena in order to store instruction results and out
    parameters in it later during sp_eval_func_item()
  */
  thd->sp_runtime_ctx->callers_arena= &backup_arena;

#if defined(ENABLED_PROFILING)
  /* Discard the initial part of executing routines. */
  thd->profiling.discard_current_query();
#endif
  do
  {
    sp_instr *i;

#if defined(ENABLED_PROFILING)
    /*
     Treat each "instr" of a routine as discrete unit that could be profiled.
     Profiling only records information for segments of code that set the
     source of the query, and almost all kinds of instructions in s-p do not.
    */
    thd->profiling.finish_current_query();
    thd->profiling.start_new_query("continuing inside routine");
#endif

    /* get_instr returns NULL when we're done. */
    i = get_instr(ip);
    if (i == NULL)
    {
#if defined(ENABLED_PROFILING)
      thd->profiling.discard_current_query();
#endif
      break;
    }

    /* Reset number of warnings for this query. */
    thd->get_stmt_da()->reset_for_next_command();

    DBUG_PRINT("execute", ("Instruction %u", ip));

    /*
      We need to reset start_time to allow for time to flow inside a stored
      procedure. This is only done for SP since time is suppose to be constant
      during execution of triggers and functions.
    */
    reset_start_time_for_sp(thd);

    /*
      We have to set thd->stmt_arena before executing the instruction
      to store in the instruction free_list all new items, created
      during the first execution (for example expanding of '*' or the
      items made during other permanent subquery transformations).
    */
    thd->stmt_arena= i;

    /*
      Will write this SP statement into binlog separately.
      TODO: consider changing the condition to "not inside event union".
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
      thd->user_var_events_alloc= thd->mem_root;

    err_status= i->execute(thd, &ip);

    if (i->free_list)
      cleanup_items(i->free_list);

    /*
      If we've set thd->user_var_events_alloc to mem_root of this SP
      statement, clean all the events allocated in it.
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    {
      reset_dynamic(&thd->user_var_events);
      thd->user_var_events_alloc= NULL;//DEBUG
    }

    /* we should cleanup free_list and memroot, used by instruction */
    thd->cleanup_after_query();
    free_root(&execute_mem_root, MYF(0));

    /*
      Find and process SQL handlers unless it is a fatal error (fatal
      errors are not catchable by SQL handlers) or the connection has been
      killed during execution.
    */
    if (!thd->is_fatal_error && !thd->killed_errno() &&
        thd->sp_runtime_ctx->handle_sql_condition(thd, &ip, i))
    {
      err_status= FALSE;
    }

    /* Reset sp_rcontext::end_partial_result_set flag. */
    thd->sp_runtime_ctx->end_partial_result_set= FALSE;

  } while (!err_status && !thd->killed && !thd->is_fatal_error);

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
  thd->profiling.start_new_query("tail end of routine");
#endif

  /* Restore query context. */

  m_creation_ctx->restore_env(thd, saved_creation_ctx);

  /* Restore arena. */

  thd->restore_active_arena(&execute_arena, &backup_arena);

  thd->sp_runtime_ctx->pop_all_cursors(); // To avoid memory leaks after an error

  /* Restore all saved */
  old_packet.swap(thd->packet);
  DBUG_ASSERT(thd->change_list.is_empty());
  old_change_list.move_elements_to(&thd->change_list);
  thd->lex= old_lex;
  thd->set_query_id(old_query_id);
  DBUG_ASSERT(!thd->derived_tables);
  thd->derived_tables= old_derived_tables;
  thd->variables.sql_mode= save_sql_mode;
  thd->abort_on_warning= save_abort_on_warning;
  thd->pop_reprepare_observer();

  thd->stmt_arena= old_arena;
  state= STMT_EXECUTED;

  /*
    Restore the caller's original warning information area:
      - warnings generated during trigger execution should not be
        propagated to the caller on success;
      - if there was an exception during execution, warning info should be
        propagated to the caller in any case.
  */
  da->pop_warning_info();

  if (err_status || merge_da_on_success)
  {
    /*
      If a routine body is empty or if a routine did not generate any warnings,
      do not duplicate our own contents by appending the contents of the called
      routine. We know that the called routine did not change its warning info.

      On the other hand, if the routine body is not empty and some statement in
      the routine generates a warning or uses tables, warning info is guaranteed
      to have changed. In this case we know that the routine warning info
      contains only new warnings, and thus we perform a copy.
    */
    if (da->warning_info_changed(&sp_wi))
    {
      /*
        If the invocation of the routine was a standalone statement,
        rather than a sub-statement, in other words, if it's a CALL
        of a procedure, rather than invocation of a function or a
        trigger, we need to clear the current contents of the caller's
        warning info.

        This is per MySQL rules: if a statement generates a warning,
        warnings from the previous statement are flushed.  Normally
        it's done in push_warning(). However, here we don't use
        push_warning() to avoid invocation of condition handlers or
        escalation of warnings to errors.
      */
      da->opt_clear_warning_info(thd->query_id);
      da->copy_sql_conditions_from_wi(thd, &sp_wi);
      da->remove_marked_sql_conditions();
    }
  }

 done:
  DBUG_PRINT("info", ("err_status: %d  killed: %d  is_slave_error: %d  report_error: %d",
                      err_status, thd->killed, thd->is_slave_error,
                      thd->is_error()));

  if (thd->killed)
    err_status= TRUE;
  /*
    If the DB has changed, the pointer has changed too, but the
    original thd->db will then have been freed
  */
  if (cur_db_changed && thd->killed != THD::KILL_CONNECTION)
  {
    /*
      Force switching back to the saved current database, because it may be
      NULL. In this case, mysql_change_db() would generate an error.
    */

    err_status|= mysql_change_db(thd, &saved_cur_db_name, TRUE);
  }
  m_flags&= ~IS_INVOKED;
  DBUG_PRINT("info",
             ("first free for 0x%lx --: 0x%lx->0x%lx, level: %lu, flags %x",
              (ulong) m_first_instance,
              (ulong) m_first_instance->m_first_free_instance,
              (ulong) this, m_recursion_level, m_flags));
  /*
    Check that we have one of following:

    1) there are not free instances which means that this instance is last
    in the list of instances (pointer to the last instance point on it and
    there are not other instances after this one in the list)

    2) There are some free instances which mean that first free instance
    should go just after this one and recursion level of that free instance
    should be on 1 more then recursion level of this instance.
  */
  DBUG_ASSERT((m_first_instance->m_first_free_instance == 0 &&
               this == m_first_instance->m_last_cached_sp &&
               m_next_cached_sp == 0) ||
              (m_first_instance->m_first_free_instance != 0 &&
               m_first_instance->m_first_free_instance == m_next_cached_sp &&
               m_first_instance->m_first_free_instance->m_recursion_level ==
               m_recursion_level + 1));
  m_first_instance->m_first_free_instance= this;

  return err_status;
}


bool sp_head::execute_trigger(THD *thd,
                              const LEX_STRING *db_name,
                              const LEX_STRING *table_name,
                              GRANT_INFO *grant_info)
{
  sp_rcontext *parent_sp_runtime_ctx = thd->sp_runtime_ctx;
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  Query_arena backup_arena;

  DBUG_ENTER("sp_head::execute_trigger");
  DBUG_PRINT("info", ("trigger %s", m_name.str));

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_ctx= NULL;


  if (m_chistics->suid != SP_IS_NOT_SUID &&
      m_security_ctx.change_security_context(thd,
                                             &m_definer_user,
                                             &m_definer_host,
                                             &m_db,
                                             &save_ctx))
    DBUG_RETURN(true);

  /*
    Fetch information about table-level privileges for subject table into
    GRANT_INFO instance. The access check itself will happen in
    Item_trigger_field, where this information will be used along with
    information about column-level privileges.
  */

  fill_effective_table_privileges(thd,
                                  grant_info,
                                  db_name->str,
                                  table_name->str);

  /* Check that the definer has TRIGGER privilege on the subject table. */

  if (!(grant_info->privilege & TRIGGER_ACL))
  {
    char priv_desc[128];
    get_privilege_desc(priv_desc, sizeof(priv_desc), TRIGGER_ACL);

    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), priv_desc,
             thd->security_ctx->priv_user, thd->security_ctx->host_or_ip,
             table_name->str);

    m_security_ctx.restore_security_context(thd, save_ctx);
    DBUG_RETURN(true);
  }
  /*
    Optimizer trace note: we needn't explicitly test here that the connected
    user has TRIGGER privilege: assume he doesn't have it; two possibilities:
    - connected user == definer: then we threw an error just above;
    - connected user != definer: then in sp_head::execute(), when checking the
    security context we will disable tracing.
  */
#endif // NO_EMBEDDED_ACCESS_CHECKS

  /*
    Prepare arena and memroot for objects which lifetime is whole
    duration of trigger call (sp_rcontext, it's tables and items,
    sp_cursor and Item_cache holders for case expressions).  We can't
    use caller's arena/memroot for those objects because in this case
    some fixed amount of memory will be consumed for each trigger
    invocation and so statements which involve lot of them will hog
    memory.

    TODO: we should create sp_rcontext once per command and reuse it
    on subsequent executions of a trigger.
  */
  init_sql_alloc(&call_mem_root, MEM_ROOT_BLOCK_SIZE, 0);
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  sp_rcontext *trigger_runtime_ctx=
    sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

  if (!trigger_runtime_ctx)
  {
    err_status= TRUE;
    goto err_with_cleanup;
  }

  trigger_runtime_ctx->sp= this;
  thd->sp_runtime_ctx= trigger_runtime_ctx;

  err_status= execute(thd, FALSE);

err_with_cleanup:
  thd->restore_active_arena(&call_arena, &backup_arena);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_ctx);
#endif // NO_EMBEDDED_ACCESS_CHECKS

  delete trigger_runtime_ctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->sp_runtime_ctx= parent_sp_runtime_ctx;

  if (thd->killed)
    thd->send_kill_message();

  DBUG_RETURN(err_status);
}


bool sp_head::execute_function(THD *thd, Item **argp, uint argcount,
                               Field *return_value_fld)
{
  ulonglong binlog_save_options;
  bool need_binlog_call= FALSE;
  uint arg_no;
  sp_rcontext *parent_sp_runtime_ctx = thd->sp_runtime_ctx;
  char buf[STRING_BUFFER_USUAL_SIZE];
  String binlog_buf(buf, sizeof(buf), &my_charset_bin);
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  Query_arena backup_arena;

  DBUG_ENTER("sp_head::execute_function");
  DBUG_PRINT("info", ("function %s", m_name.str));

  LINT_INIT(binlog_save_options);
  // Resetting THD::where to its default value
  thd->where= THD::DEFAULT_WHERE;
  /*
    Check that the function is called with all specified arguments.

    If it is not, use my_error() to report an error, or it will not terminate
    the invoking query properly.
  */
  if (argcount != m_root_parsing_ctx->context_var_count())
  {
    /*
      Need to use my_error here, or it will not terminate the
      invoking query properly.
    */
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0),
             "FUNCTION", m_qname.str,
             m_root_parsing_ctx->context_var_count(), argcount);
    DBUG_RETURN(true);
  }
  /*
    Prepare arena and memroot for objects which lifetime is whole
    duration of function call (sp_rcontext, it's tables and items,
    sp_cursor and Item_cache holders for case expressions).
    We can't use caller's arena/memroot for those objects because
    in this case some fixed amount of memory will be consumed for
    each function/trigger invocation and so statements which involve
    lot of them will hog memory.
    TODO: we should create sp_rcontext once per command and reuse
    it on subsequent executions of a function/trigger.
  */
  init_sql_alloc(&call_mem_root, MEM_ROOT_BLOCK_SIZE, 0);
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  sp_rcontext *func_runtime_ctx= sp_rcontext::create(thd, m_root_parsing_ctx,
                                                     return_value_fld);

  if (!func_runtime_ctx)
  {
    thd->restore_active_arena(&call_arena, &backup_arena);
    err_status= TRUE;
    goto err_with_cleanup;
  }

  func_runtime_ctx->sp= this;

  /*
    We have to switch temporarily back to callers arena/memroot.
    Function arguments belong to the caller and so the may reference
    memory which they will allocate during calculation long after
    this function call will be finished (e.g. in Item::cleanup()).
  */
  thd->restore_active_arena(&call_arena, &backup_arena);

  /*
    Pass arguments.

    Note, THD::sp_runtime_ctx must not be switched before the arguments are
    passed. Values are taken from the caller's runtime context and set to the
    runtime context of this function.
  */
  for (arg_no= 0; arg_no < argcount; arg_no++)
  {
    /* Arguments must be fixed in Item_func_sp::fix_fields */
    DBUG_ASSERT(argp[arg_no]->fixed);

    err_status= func_runtime_ctx->set_variable(thd, arg_no, &(argp[arg_no]));

    if (err_status)
      goto err_with_cleanup;
  }

  /*
    If row-based binlogging, we don't need to binlog the function's call, let
    each substatement be binlogged its way.
  */
  need_binlog_call= mysql_bin_log.is_open() &&
                    (thd->variables.option_bits & OPTION_BIN_LOG) &&
                    !thd->is_current_stmt_binlog_format_row();

  /*
    Remember the original arguments for unrolled replication of functions
    before they are changed by execution.

    Note, THD::sp_runtime_ctx must not be switched before the arguments are
    logged. Values are taken from the caller's runtime context.
  */
  if (need_binlog_call)
  {
    binlog_buf.length(0);
    binlog_buf.append(STRING_WITH_LEN("SELECT "));
    append_identifier(thd, &binlog_buf, m_db.str, m_db.length);
    binlog_buf.append('.');
    append_identifier(thd, &binlog_buf, m_name.str, m_name.length);
    binlog_buf.append('(');
    for (arg_no= 0; arg_no < argcount; arg_no++)
    {
      String str_value_holder;
      String *str_value;

      if (arg_no)
        binlog_buf.append(',');

      str_value= sp_get_item_value(thd, func_runtime_ctx->get_item(arg_no),
                                   &str_value_holder);

      if (str_value)
        binlog_buf.append(*str_value);
      else
        binlog_buf.append(STRING_WITH_LEN("NULL"));
    }
    binlog_buf.append(')');
  }

  thd->sp_runtime_ctx= func_runtime_ctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx;
  if (set_security_ctx(thd, &save_security_ctx))
  {
    err_status= TRUE;
    goto err_with_cleanup;
  }
#endif

  if (need_binlog_call)
  {
    query_id_t q;
    reset_dynamic(&thd->user_var_events);
    /*
      In case of artificially constructed events for function calls
      we have separate union for each such event and hence can't use
      query_id of real calling statement as the start of all these
      unions (this will break logic of replication of user-defined
      variables). So we use artificial value which is guaranteed to
      be greater than all query_id's of all statements belonging
      to previous events/unions.
      Possible alternative to this is logging of all function invocations
      as one select and not resetting THD::user_var_events before
      each invocation.
    */
    mysql_mutex_lock(&LOCK_thread_count);
    q= global_query_id;
    mysql_mutex_unlock(&LOCK_thread_count);
    mysql_bin_log.start_union_events(thd, q + 1);
    binlog_save_options= thd->variables.option_bits;
    thd->variables.option_bits&= ~OPTION_BIN_LOG;
  }

  opt_trace_disable_if_no_stored_proc_func_access(thd, this);

  /*
    Switch to call arena/mem_root so objects like sp_cursor or
    Item_cache holders for case expressions can be allocated on it.

    TODO: In future we should associate call arena/mem_root with
          sp_rcontext and allocate all these objects (and sp_rcontext
          itself) on it directly rather than juggle with arenas.
  */
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  err_status= execute(thd, TRUE);

  thd->restore_active_arena(&call_arena, &backup_arena);

  if (need_binlog_call)
  {
    mysql_bin_log.stop_union_events(thd);
    thd->variables.option_bits= binlog_save_options;
    if (thd->binlog_evt_union.unioned_events)
    {
      int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
      Query_log_event qinfo(thd, binlog_buf.ptr(), binlog_buf.length(),
                            thd->binlog_evt_union.unioned_events_trans, FALSE, FALSE, errcode);
      if (mysql_bin_log.write_event(&qinfo) &&
          thd->binlog_evt_union.unioned_events_trans)
      {
        push_warning(thd, Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                     "Invoked ROUTINE modified a transactional table but MySQL "
                     "failed to reflect this change in the binary log");
        err_status= TRUE;
      }
      reset_dynamic(&thd->user_var_events);
      /* Forget those values, in case more function calls are binlogged: */
      thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;
      thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
    }
  }

  if (!err_status)
  {
    /* We need result only in function but not in trigger */

    if (!thd->sp_runtime_ctx->is_return_value_set())
    {
      my_error(ER_SP_NORETURNEND, MYF(0), m_name.str);
      err_status= TRUE;
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

err_with_cleanup:
  delete func_runtime_ctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->sp_runtime_ctx= parent_sp_runtime_ctx;

  /*
    If not inside a procedure and a function printing warning
    messages.
  */
  if (need_binlog_call && 
      thd->sp_runtime_ctx == NULL && !thd->binlog_evt_union.do_union)
    thd->issue_unsafe_warnings();

  DBUG_RETURN(err_status);
}


bool sp_head::execute_procedure(THD *thd, List<Item> *args)
{
  bool err_status= FALSE;
  uint params = m_root_parsing_ctx->context_var_count();
  /* Query start time may be reset in a multi-stmt SP; keep this for later. */
  ulonglong utime_before_sp_exec= thd->utime_after_lock;
  sp_rcontext *parent_sp_runtime_ctx= thd->sp_runtime_ctx;
  sp_rcontext *sp_runtime_ctx_saved= thd->sp_runtime_ctx;
  bool save_enable_slow_log= false;
  bool save_log_general= false;

  DBUG_ENTER("sp_head::execute_procedure");
  DBUG_PRINT("info", ("procedure %s", m_name.str));

  if (args->elements != params)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "PROCEDURE",
             m_qname.str, params, args->elements);
    DBUG_RETURN(true);
  }

  if (!parent_sp_runtime_ctx)
  {
    // Create a temporary old context. We need it to pass OUT-parameter values.
    parent_sp_runtime_ctx= sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

    if (!parent_sp_runtime_ctx)
      DBUG_RETURN(true);

    parent_sp_runtime_ctx->sp= 0;
    thd->sp_runtime_ctx= parent_sp_runtime_ctx;

    /* set callers_arena to thd, for upper-level function to work */
    thd->sp_runtime_ctx->callers_arena= thd;
  }

  sp_rcontext *proc_runtime_ctx=
    sp_rcontext::create(thd, m_root_parsing_ctx, NULL);

  if (!proc_runtime_ctx)
  {
    thd->sp_runtime_ctx= sp_runtime_ctx_saved;

    if (!sp_runtime_ctx_saved)
      delete parent_sp_runtime_ctx;

    DBUG_RETURN(true);
  }

  proc_runtime_ctx->sp= this;

  if (params > 0)
  {
    List_iterator<Item> it_args(*args);

    DBUG_PRINT("info",(" %.*s: eval args", (int) m_name.length, m_name.str));

    for (uint i= 0 ; i < params ; i++)
    {
      Item *arg_item= it_args++;

      if (!arg_item)
        break;

      sp_variable *spvar= m_root_parsing_ctx->find_variable(i);

      if (!spvar)
        continue;

      if (spvar->mode != sp_variable::MODE_IN)
      {
        Settable_routine_parameter *srp=
          arg_item->get_settable_routine_parameter();

        if (!srp)
        {
          my_error(ER_SP_NOT_VAR_ARG, MYF(0), i+1, m_qname.str);
          err_status= TRUE;
          break;
        }

        srp->set_required_privilege(spvar->mode == sp_variable::MODE_INOUT);
      }

      if (spvar->mode == sp_variable::MODE_OUT)
      {
        Item_null *null_item= new Item_null();

        if (!null_item ||
            proc_runtime_ctx->set_variable(thd, i, (Item **)&null_item))
        {
          err_status= TRUE;
          break;
        }
      }
      else
      {
        if (proc_runtime_ctx->set_variable(thd, i, it_args.ref()))
        {
          err_status= TRUE;
          break;
        }
      }
    }

    /*
      Okay, got values for all arguments. Close tables that might be used by
      arguments evaluation. If arguments evaluation required prelocking mode,
      we'll leave it here.
    */
    thd->lex->unit.cleanup();

    if (!thd->in_sub_stmt)
    {
      thd->get_stmt_da()->set_overwrite_status(true);
      thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }

    thd_proc_info(thd, "closing tables");
    close_thread_tables(thd);
    thd_proc_info(thd, 0);

    if (! thd->in_sub_stmt && ! thd->in_multi_stmt_transaction_mode())
      thd->mdl_context.release_transactional_locks();
    else if (! thd->in_sub_stmt)
      thd->mdl_context.release_statement_locks();

    thd->rollback_item_tree_changes();

    DBUG_PRINT("info",(" %.*s: eval args done", (int) m_name.length, 
                       m_name.str));
  }
  if (!(m_flags & LOG_SLOW_STATEMENTS) && thd->enable_slow_log)
  {
    DBUG_PRINT("info", ("Disabling slow log for the execution"));
    save_enable_slow_log= true;
    thd->enable_slow_log= FALSE;
  }
  if (!(m_flags & LOG_GENERAL_LOG) && !(thd->variables.option_bits & OPTION_LOG_OFF))
  {
    DBUG_PRINT("info", ("Disabling general log for the execution"));
    save_log_general= true;
    /* disable this bit */
    thd->variables.option_bits |= OPTION_LOG_OFF;
  }
  thd->sp_runtime_ctx= proc_runtime_ctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx= 0;
  if (!err_status)
    err_status= set_security_ctx(thd, &save_security_ctx);
#endif

  opt_trace_disable_if_no_stored_proc_func_access(thd, this);

  if (!err_status)
    err_status= execute(thd, TRUE);

  if (save_log_general)
    thd->variables.option_bits &= ~OPTION_LOG_OFF;
  if (save_enable_slow_log)
    thd->enable_slow_log= true;
  /*
    In the case when we weren't able to employ reuse mechanism for
    OUT/INOUT parameters, we should reallocate memory. This
    allocation should be done on the arena which will live through
    all execution of calling routine.
  */
  thd->sp_runtime_ctx->callers_arena= parent_sp_runtime_ctx->callers_arena;

  if (!err_status && params > 0)
  {
    List_iterator<Item> it_args(*args);

    /*
      Copy back all OUT or INOUT values to the previous frame, or
      set global user variables
    */
    for (uint i= 0 ; i < params ; i++)
    {
      Item *arg_item= it_args++;

      if (!arg_item)
        break;

      sp_variable *spvar= m_root_parsing_ctx->find_variable(i);

      if (spvar->mode == sp_variable::MODE_IN)
        continue;

      Settable_routine_parameter *srp=
        arg_item->get_settable_routine_parameter();

      DBUG_ASSERT(srp);

      if (srp->set_value(thd, parent_sp_runtime_ctx, proc_runtime_ctx->get_item_addr(i)))
      {
        err_status= TRUE;
        break;
      }

      Send_field *out_param_info= new (thd->mem_root) Send_field();
      proc_runtime_ctx->get_item(i)->make_field(out_param_info);
      out_param_info->db_name= m_db.str;
      out_param_info->table_name= m_name.str;
      out_param_info->org_table_name= m_name.str;
      out_param_info->col_name= spvar->name.str;
      out_param_info->org_col_name= spvar->name.str;

      srp->set_out_param_info(out_param_info);
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (save_security_ctx)
    m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

  if (!sp_runtime_ctx_saved)
    delete parent_sp_runtime_ctx;

  delete proc_runtime_ctx;
  thd->sp_runtime_ctx= sp_runtime_ctx_saved;
  thd->utime_after_lock= utime_before_sp_exec;

  /*
    If not insided a procedure and a function printing warning
    messages.
  */ 
  bool need_binlog_call= mysql_bin_log.is_open() &&
                         (thd->variables.option_bits & OPTION_BIN_LOG) &&
                         !thd->is_current_stmt_binlog_format_row();
  if (need_binlog_call && thd->sp_runtime_ctx == NULL &&
      !thd->binlog_evt_union.do_union)
    thd->issue_unsafe_warnings();

  DBUG_RETURN(err_status);
}


bool sp_head::reset_lex(THD *thd)
{
  LEX *oldlex= thd->lex;

  LEX *sublex= new (thd->mem_root)st_lex_local;

  if (!sublex)
    return true;

  thd->lex= sublex;
  m_parser_data.push_lex(oldlex);

  /* Reset most stuff. */
  lex_start(thd);

  /* And keep the SP stuff too */
  sublex->sphead= oldlex->sphead;
  sublex->set_sp_current_parsing_ctx(oldlex->get_sp_current_parsing_ctx());
  sublex->sp_lex_in_use= FALSE;

  /* Reset type info. */

  sublex->charset= NULL;
  sublex->length= NULL;
  sublex->dec= NULL;
  sublex->interval_list.empty();
  sublex->type= 0;

  /* Reset part of parser state which needs this. */
  thd->m_parser_state->m_yacc.reset_before_substatement();

  return false;
}


bool sp_head::restore_lex(THD *thd)
{
  LEX *sublex= thd->lex;

  sublex->set_trg_event_type_for_tables();

  LEX *oldlex= (LEX *) m_parser_data.pop_lex();

  if (!oldlex)
    return false; // Nothing to restore

  /* If this substatement is unsafe, the entire routine is too. */
  DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags: 0x%x",
                      thd->lex->get_stmt_unsafe_flags()));
  unsafe_flags|= sublex->get_stmt_unsafe_flags();

  /*
    Add routines which are used by statement to respective set for
    this routine.
  */
  if (sp_update_sp_used_routines(&m_sroutines, &sublex->sroutines))
    return true;

  /*
    Merge tables used by this statement (but not by its functions or
    procedures) to multiset of tables used by this routine.
  */
  merge_table_list(thd, sublex->query_tables, sublex);

  if (!sublex->sp_lex_in_use)
  {
    sublex->sphead= NULL;
    lex_end(sublex);
    delete sublex;
  }

  thd->lex= oldlex;
  return false;
}

void sp_head::set_info(longlong created,
                       longlong modified,
                       st_sp_chistics *chistics,
                       sql_mode_t sql_mode)
{
  m_created= created;
  m_modified= modified;
  m_chistics= (st_sp_chistics *) memdup_root(mem_root, (char*) chistics,
                                             sizeof(*chistics));
  if (m_chistics->comment.length == 0)
    m_chistics->comment.str= 0;
  else
    m_chistics->comment.str= strmake_root(mem_root,
                                          m_chistics->comment.str,
                                          m_chistics->comment.length);
  m_sql_mode= sql_mode;
}


void sp_head::set_definer(const char *definer, uint definerlen)
{
  char user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING user_name= { user_name_holder, USERNAME_LENGTH };

  char host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING host_name= { host_name_holder, HOSTNAME_LENGTH };

  parse_user(definer, definerlen, user_name.str, &user_name.length,
             host_name.str, &host_name.length);

  set_definer(&user_name, &host_name);
}


void sp_head::set_definer(const LEX_STRING *user_name,
                          const LEX_STRING *host_name)
{
  m_definer_user.str= strmake_root(mem_root, user_name->str, user_name->length);
  m_definer_user.length= user_name->length;

  m_definer_host.str= strmake_root(mem_root, host_name->str, host_name->length);
  m_definer_host.length= host_name->length;
}


bool sp_head::show_create_routine(THD *thd, enum_sp_type type)
{
  const char *col1_caption= (type == SP_TYPE_PROCEDURE) ?
                            "Procedure" : "Function";

  const char *col3_caption= (type == SP_TYPE_PROCEDURE) ?
                            "Create Procedure" : "Create Function";

  bool err_status;

  Protocol *protocol= thd->protocol;
  List<Item> fields;

  LEX_STRING sql_mode;

  bool full_access;

  DBUG_ASSERT(type == SP_TYPE_PROCEDURE || type == SP_TYPE_FUNCTION);

  if (check_show_access(thd, &full_access))
    return true;

  sql_mode_string_representation(thd, m_sql_mode, &sql_mode);

  /* Send header. */

  fields.push_back(new Item_empty_string(col1_caption, NAME_CHAR_LEN));
  fields.push_back(new Item_empty_string("sql_mode", sql_mode.length));

  {
    /*
      NOTE: SQL statement field must be not less than 1024 in order not to
      confuse old clients.
    */

    Item_empty_string *stmt_fld=
      new Item_empty_string(col3_caption,
                            std::max<size_t>(m_defstr.length, 1024U));

    stmt_fld->maybe_null= TRUE;

    fields.push_back(stmt_fld);
  }

  fields.push_back(new Item_empty_string("character_set_client",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("collation_connection",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("Database Collation",
                                         MY_CS_NAME_SIZE));

  if (protocol->send_result_set_metadata(&fields,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    return true;
  }

  /* Send data. */

  protocol->prepare_for_resend();

  protocol->store(m_name.str, m_name.length, system_charset_info);
  protocol->store(sql_mode.str, sql_mode.length, system_charset_info);

  if (full_access)
    protocol->store(m_defstr.str, m_defstr.length,
                    m_creation_ctx->get_client_cs());
  else
    protocol->store_null();


  protocol->store(m_creation_ctx->get_client_cs()->csname, system_charset_info);
  protocol->store(m_creation_ctx->get_connection_cl()->name, system_charset_info);
  protocol->store(m_creation_ctx->get_db_cl()->name, system_charset_info);

  err_status= protocol->write();

  if (!err_status)
    my_eof(thd);

  return err_status;
}


bool sp_head::add_instr(THD *thd, sp_instr *instr)
{
  m_parser_data.process_new_sp_instr(thd, instr);

  /*
    Memory root of every instruction is designated for permanent
    transformations (optimizations) made on the parsed tree during
    the first execution. It points to the memory root of the
    entire stored procedure, as their life span is equal.
  */
  instr->mem_root= get_persistent_mem_root();

  return m_instructions.append(instr);
}


void sp_head::optimize()
{
  List<sp_branch_instr> bp;
  sp_instr *i;
  uint src, dst;

  opt_mark();

  bp.empty();
  src= dst= 0;
  while ((i= get_instr(src)))
  {
    if (!i->opt_is_marked())
    {
      delete i;
      src+= 1;
    }
    else
    {
      if (src != dst)
      {
        m_instructions.set(dst, i);

        /* Move the instruction and update prev. jumps */
        sp_branch_instr *ibp;
        List_iterator_fast<sp_branch_instr> li(bp);

        while ((ibp= li++))
          ibp->set_destination(src, dst);
      }
      i->opt_move(dst, &bp);
      src+= 1;
      dst+= 1;
    }
  }

  m_instructions.elements(dst);
  bp.empty();
}


void sp_head::add_mark_lead(uint ip, List<sp_instr> *leads)
{
  sp_instr *i= get_instr(ip);

  if (i && !i->opt_is_marked())
    leads->push_front(i);
}


void sp_head::opt_mark()
{
  uint ip;
  sp_instr *i;
  List<sp_instr> leads;

  /*
    Forward flow analysis algorithm in the instruction graph:
    - first, add the entry point in the graph (the first instruction) to the
      'leads' list of paths to explore.
    - while there are still leads to explore:
      - pick one lead, and follow the path forward. Mark instruction reached.
        Stop only if the end of the routine is reached, or the path converge
        to code already explored (marked).
      - while following a path, collect in the 'leads' list any fork to
        another path (caused by conditional jumps instructions), so that these
        paths can be explored as well.
  */

  /* Add the entry point */
  i= get_instr(0);
  leads.push_front(i);

  /* For each path of code ... */
  while (leads.elements != 0)
  {
    i= leads.pop();

    /* Mark the entire path, collecting new leads. */
    while (i && !i->opt_is_marked())
    {
      ip= i->opt_mark(this, & leads);
      i= get_instr(ip);
    }
  }
}


#ifndef DBUG_OFF
bool sp_head::show_routine_code(THD *thd)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  List<Item> field_list;
  sp_instr *i;
  bool full_access;
  bool res= false;
  uint ip;

  if (check_show_access(thd, &full_access) || !full_access)
    return true;

  field_list.push_back(new Item_uint(NAME_STRING("Pos"), 0, 9));
  // 1024 is for not to confuse old clients
  field_list.push_back(new Item_empty_string("Instruction",
                                             std::max(buffer.length(), 1024U)));
  if (protocol->send_result_set_metadata(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
    return true;

  for (ip= 0; (i = get_instr(ip)) ; ip++)
  {
    /*
      Consistency check. If these are different something went wrong
      during optimization.
    */
    if (ip != i->get_ip())
    {
      const char *format= "Instruction at position %u has m_ip=%u";
      char tmp[sizeof(format) + 2*SP_INSTR_UINT_MAXLEN + 1];

      sprintf(tmp, format, ip, i->get_ip());
      /*
        Since this is for debugging purposes only, we don't bother to
        introduce a special error code for it.
      */
      push_warning(thd, Sql_condition::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR, tmp);
    }
    protocol->prepare_for_resend();
    protocol->store((longlong)ip);

    buffer.set("", 0, system_charset_info);
    i->print(&buffer);
    protocol->store(buffer.ptr(), buffer.length(), system_charset_info);
    if ((res= protocol->write()))
      break;
  }

  if (!res)
    my_eof(thd);

  return res;
}
#endif // ifndef DBUG_OFF


bool sp_head::merge_table_list(THD *thd,
                               TABLE_LIST *table,
                               LEX *lex_for_tmp_check)
{
  if (lex_for_tmp_check->sql_command == SQLCOM_DROP_TABLE &&
      lex_for_tmp_check->drop_temporary)
    return true;

  for (uint i= 0 ; i < m_sptabs.records ; i++)
  {
    SP_TABLE *tab= (SP_TABLE*) my_hash_element(&m_sptabs, i);
    tab->query_lock_count= 0;
  }

  for (; table ; table= table->next_global)
    if (!table->derived && !table->schema_table)
    {
      char tname[(NAME_LEN + 1) * 3];           // db\0table\0alias\0
      uint tlen, alen;

      tlen= table->db_length;
      memcpy(tname, table->db, tlen);
      tname[tlen++]= '\0';
      memcpy(tname+tlen, table->table_name, table->table_name_length);
      tlen+= table->table_name_length;
      tname[tlen++]= '\0';
      alen= strlen(table->alias);
      memcpy(tname+tlen, table->alias, alen);
      tlen+= alen;
      tname[tlen]= '\0';

      /*
        Upgrade the lock type because this table list will be used
        only in pre-locked mode, in which DELAYED inserts are always
        converted to normal inserts.
      */
      if (table->lock_type == TL_WRITE_DELAYED)
        table->lock_type= TL_WRITE;

      /*
        We ignore alias when we check if table was already marked as temporary
        (and therefore should not be prelocked). Otherwise we will erroneously
        treat table with same name but with different alias as non-temporary.
      */

      SP_TABLE *tab;

      if ((tab= (SP_TABLE*) my_hash_search(&m_sptabs, (uchar *)tname, tlen)) ||
          ((tab= (SP_TABLE*) my_hash_search(&m_sptabs, (uchar *)tname,
                                        tlen - alen - 1)) &&
           tab->temp))
      {
        if (tab->lock_type < table->lock_type)
          tab->lock_type= table->lock_type; // Use the table with the highest lock type
        tab->query_lock_count++;
        if (tab->query_lock_count > tab->lock_count)
          tab->lock_count++;
        tab->trg_event_map|= table->trg_event_map;
      }
      else
      {
        if (!(tab= (SP_TABLE *)thd->calloc(sizeof(SP_TABLE))))
          return false;
        if (lex_for_tmp_check->sql_command == SQLCOM_CREATE_TABLE &&
            lex_for_tmp_check->query_tables == table &&
            lex_for_tmp_check->create_info.options & HA_LEX_CREATE_TMP_TABLE)
        {
          tab->temp= true;
          tab->qname.length= tlen - alen - 1;
        }
        else
          tab->qname.length= tlen;
        tab->qname.str= (char*) thd->memdup(tname, tab->qname.length + 1);
        if (!tab->qname.str)
          return false;
        tab->table_name_length= table->table_name_length;
        tab->db_length= table->db_length;
        tab->lock_type= table->lock_type;
        tab->lock_count= tab->query_lock_count= 1;
        tab->trg_event_map= table->trg_event_map;
        if (my_hash_insert(&m_sptabs, (uchar *)tab))
          return false;
      }
    }
  return true;
}


bool sp_head::add_used_tables_to_table_list(THD *thd,
                                            TABLE_LIST ***query_tables_last_ptr,
                                            TABLE_LIST *belong_to_view)
{
  Query_arena *arena, backup;
  bool result= false;

  /*
    Use persistent arena for table list allocation to be PS/SP friendly.
    Note that we also have to copy database/table names and alias to PS/SP
    memory since current instance of sp_head object can pass away before
    next execution of PS/SP for which tables are added to prelocking list.
    This will be fixed by introducing of proper invalidation mechanism
    once new TDC is ready.
  */
  arena= thd->activate_stmt_arena_if_needed(&backup);

  for (uint i= 0; i < m_sptabs.records; i++)
  {
    char *tab_buff, *key_buff;
    SP_TABLE *stab= (SP_TABLE*) my_hash_element(&m_sptabs, i);
    if (stab->temp)
      continue;

    if (!(tab_buff= (char *)thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST)) *
                                        stab->lock_count)) ||
        !(key_buff= (char*)thd->memdup(stab->qname.str,
                                       stab->qname.length + 1)))
      return false;

    for (uint j= 0; j < stab->lock_count; j++)
    {
      TABLE_LIST *table= (TABLE_LIST *)tab_buff;

      table->db= key_buff;
      table->db_length= stab->db_length;
      table->table_name= table->db + table->db_length + 1;
      table->table_name_length= stab->table_name_length;
      table->alias= table->table_name + table->table_name_length + 1;
      table->lock_type= stab->lock_type;
      table->cacheable_table= 1;
      table->prelocking_placeholder= 1;
      table->belong_to_view= belong_to_view;
      table->trg_event_map= stab->trg_event_map;
      /*
        Since we don't allow DDL on base tables in prelocked mode it
        is safe to infer the type of metadata lock from the type of
        table lock.
      */
      table->mdl_request.init(MDL_key::TABLE, table->db, table->table_name,
                              table->lock_type >= TL_WRITE_ALLOW_WRITE ?
                              MDL_SHARED_WRITE : MDL_SHARED_READ,
                              MDL_TRANSACTION);

      /* Everyting else should be zeroed */

      **query_tables_last_ptr= table;
      table->prev_global= *query_tables_last_ptr;
      *query_tables_last_ptr= &table->next_global;

      tab_buff+= ALIGN_SIZE(sizeof(TABLE_LIST));
      result= true;
    }
  }

  if (arena)
    thd->restore_active_arena(arena, &backup);

  return result;
}


bool sp_head::check_show_access(THD *thd, bool *full_access)
{
  TABLE_LIST tables;
  memset(&tables, 0, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "proc";

  *full_access=
    ((!check_table_access(thd, SELECT_ACL, &tables, false, 1, true) &&
      (tables.grant.privilege & SELECT_ACL) != 0) ||
     (!strcmp(m_definer_user.str, thd->security_ctx->priv_user) &&
      !strcmp(m_definer_host.str, thd->security_ctx->priv_host)));

  return *full_access ?
         false :
         check_some_routine_access(thd, m_db.str, m_name.str,
                                   m_type == SP_TYPE_PROCEDURE);
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool sp_head::set_security_ctx(THD *thd, Security_context **save_ctx)
{
  *save_ctx= NULL;

  if (m_chistics->suid != SP_IS_NOT_SUID &&
      m_security_ctx.change_security_context(thd,
                                             &m_definer_user, &m_definer_host,
                                             &m_db, save_ctx))
  {
    return true;
  }

  /*
    If we changed context to run as another user, we need to check the
    access right for the new context again as someone may have revoked
    the right to use the procedure from this user.
  */

  if (*save_ctx &&
      check_routine_access(thd, EXECUTE_ACL, m_db.str, m_name.str,
                           m_type == SP_TYPE_PROCEDURE, false))
  {
    m_security_ctx.restore_security_context(thd, *save_ctx);
    *save_ctx= NULL;
    return true;
  }

  return false;
}
#endif // ! NO_EMBEDDED_ACCESS_CHECKS


///////////////////////////////////////////////////////////////////////////
// sp_parser_data implementation.
///////////////////////////////////////////////////////////////////////////


void sp_parser_data::start_parsing_sp_body(THD *thd, sp_head *sp)
{
  m_saved_memroot= thd->mem_root;
  m_saved_free_list= thd->free_list;

  thd->mem_root= sp->get_persistent_mem_root();
  thd->free_list= NULL;
}


bool sp_parser_data::add_backpatch_entry(sp_branch_instr *i,
                                         sp_label *label)
{
  Backpatch_info *bp= (Backpatch_info *)sql_alloc(sizeof(Backpatch_info));

  if (!bp)
    return true;

  bp->label= label;
  bp->instr= i;
  return m_backpatch.push_front(bp);
}


void sp_parser_data::do_backpatch(sp_label *label, uint dest)
{
  Backpatch_info *bp;
  List_iterator_fast<Backpatch_info> li(m_backpatch);

  while ((bp= li++))
  {
    if (bp->label == label)
      bp->instr->backpatch(dest);
  }
}


bool sp_parser_data::add_cont_backpatch_entry(sp_lex_branch_instr *i)
{
  i->set_cont_dest(m_cont_level);
  return m_cont_backpatch.push_front(i);
}


void sp_parser_data::do_cont_backpatch(uint dest)
{
  sp_lex_branch_instr *i;

  while ((i= m_cont_backpatch.head()) && i->get_cont_dest() == m_cont_level)
  {
    i->set_cont_dest(dest);
    m_cont_backpatch.pop();
  }

  --m_cont_level;
}


void sp_parser_data::process_new_sp_instr(THD* thd, sp_instr *i)
{
  /*
    thd->free_list should be cleaned here because it's implicitly expected
    that that process_new_sp_instr() (called from sp_head::add_instr) is
    called as the last action after parsing the SP-instruction's SQL query.

    Thus, at this point thd->free_list contains all Item-objects, created for
    this SP-instruction.

    Next SP-instruction should start its own free-list from the scratch.
  */

  i->free_list= thd->free_list;

  thd->free_list= NULL;
}


///////////////////////////////////////////////////////////////////////////
// sp_lex_instr implementation.
///////////////////////////////////////////////////////////////////////////


bool sp_lex_instr::reset_lex_and_exec_core(THD *thd,
                                           uint *nextp,
                                           bool open_tables)
{
  bool rc= false;

  /*
    The flag is saved at the entry to the following substatement.
    It's reset further in the common code part.
    It's merged with the saved parent's value at the exit of this func.
  */

  unsigned int parent_unsafe_rollback_flags=
    thd->transaction.stmt.get_unsafe_rollback_flags();
  thd->transaction.stmt.reset_unsafe_rollback_flags();

  /* Check pre-conditions. */

  DBUG_ASSERT(!thd->derived_tables);
  DBUG_ASSERT(thd->change_list.is_empty());

  /*
    Use our own lex.

    Although it is saved/restored in sp_head::execute() when we are
    entering/leaving routine, it's still should be saved/restored here,
    in order to properly behave in case of ER_NEED_REPREPARE error
    (when ER_NEED_REPREPARE happened, and we failed to re-parse the query).
  */

  LEX *lex_saved= thd->lex;
  thd->lex= m_lex;

  /* Set new query id. */

  thd->set_query_id(next_query_id());

  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
  {
    /*
      This statement will enter/leave prelocked mode on its own.
      Entering prelocked mode changes table list and related members
      of LEX, so we'll need to restore them.
    */
    if (m_lex_query_tables_own_last)
    {
      /*
        We've already entered/left prelocked mode with this statement.
        Attach the list of tables that need to be prelocked and mark m_lex
        as having such list attached.
      */
      *m_lex_query_tables_own_last= m_prelocking_tables;
      m_lex->mark_as_requiring_prelocking(m_lex_query_tables_own_last);
    }
  }

  /* Reset LEX-object before re-use. */

  reinit_stmt_before_use(thd, m_lex);

  /* Open tables if needed. */

  if (open_tables)
  {
    /*
      IF, CASE, DECLARE, SET, RETURN, have 'open_tables' true; they may
      have a subquery in parameter and are worth tracing. They don't
      correspond to a SQL command so we pretend that they are SQLCOM_SELECT.
    */
    Opt_trace_start ots(thd, m_lex->query_tables, SQLCOM_SELECT,
                        &m_lex->var_list, NULL, 0, this,
                        thd->variables.character_set_client);
    Opt_trace_object trace_command(&thd->opt_trace);
    Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

    /*
      Check whenever we have access to tables for this statement
      and open and lock them before executing instructions core function.
      If we are not opening any tables, we don't need to check permissions
      either.
    */
    if (m_lex->query_tables)
      rc= (open_temporary_tables(thd, m_lex->query_tables) ||
            check_table_access(thd, SELECT_ACL, m_lex->query_tables, false,
                               UINT_MAX, false));

    if (!rc)
      rc= open_and_lock_tables(thd, m_lex->query_tables, true, 0);

    if (!rc)
    {
      rc= exec_core(thd, nextp);
      DBUG_PRINT("info",("exec_core returned: %d", rc));
    }

    /*
      Call after unit->cleanup() to close open table
      key read.
    */

    m_lex->unit.cleanup();

    /* Here we also commit or rollback the current statement. */

    if (! thd->in_sub_stmt)
    {
      thd->get_stmt_da()->set_overwrite_status(true);
      thd->is_error() ? trans_rollback_stmt(thd) : trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }
    thd_proc_info(thd, "closing tables");
    close_thread_tables(thd);
    thd_proc_info(thd, 0);

    if (! thd->in_sub_stmt && ! thd->in_multi_stmt_transaction_mode())
      thd->mdl_context.release_transactional_locks();
    else if (! thd->in_sub_stmt)
      thd->mdl_context.release_statement_locks();
  }
  else
  {
    rc= exec_core(thd, nextp);
    DBUG_PRINT("info",("exec_core returned: %d", rc));
  }

  if (m_lex->query_tables_own_last)
  {
    /*
      We've entered and left prelocking mode when executing statement
      stored in m_lex.
      m_lex->query_tables(->next_global)* list now has a 'tail' - a list
      of tables that are added for prelocking. (If this is the first
      execution, the 'tail' was added by open_tables(), otherwise we've
      attached it above in this function).
      Now we'll save the 'tail', and detach it.
    */
    m_lex_query_tables_own_last= m_lex->query_tables_own_last;
    m_prelocking_tables= *m_lex_query_tables_own_last;
    *m_lex_query_tables_own_last= NULL;
    m_lex->mark_as_requiring_prelocking(NULL);
  }

  /* Rollback changes to the item tree during execution. */

  thd->rollback_item_tree_changes();

  /*
    Update the state of the active arena if no errors on
    open_tables stage.
  */

  if (!rc || !thd->is_error() ||
      (thd->get_stmt_da()->sql_errno() != ER_CANT_REOPEN_TABLE &&
       thd->get_stmt_da()->sql_errno() != ER_NO_SUCH_TABLE &&
       thd->get_stmt_da()->sql_errno() != ER_UPDATE_TABLE_USED))
    thd->stmt_arena->state= Query_arena::STMT_EXECUTED;

  /*
    Merge here with the saved parent's values
    what is needed from the substatement gained
  */

  thd->transaction.stmt.add_unsafe_rollback_flags(parent_unsafe_rollback_flags);

  /* Restore original lex. */

  thd->lex= lex_saved;

  /*
    Unlike for PS we should not call Item's destructors for newly created
    items after execution of each instruction in stored routine. This is
    because SP often create Item (like Item_int, Item_string etc...) when
    they want to store some value in local variable, pass return value and
    etc... So their life time should be longer than one instruction.

    cleanup_items() is called in sp_head::execute()
  */

  return rc || thd->is_error();
}

LEX *sp_lex_instr::parse_expr(THD *thd, sp_head *sp)
{
  String sql_query;
  sql_query.set_charset(system_charset_info);

  get_query(&sql_query);

  if (sql_query.length() == 0)
  {
    // The instruction has returned zero-length query string. That means, the
    // re-preparation of the instruction is not possible. We should not come
    // here in the normal life.
    DBUG_ASSERT(false);
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    return NULL;
  }

  // Prepare parser state. It can be done just before parse_sql(), do it here
  // only to simplify exit in case of failure (out-of-memory error).

  Parser_state parser_state;

  if (parser_state.init(thd, sql_query.c_ptr(), sql_query.length()))
    return NULL;

  // Cleanup current THD from previously held objects before new parsing.

  cleanup_before_parsing(thd);

  // Switch mem-roots. We need to store new LEX and its Items in the persistent
  // SP-memory (memory which is not freed between executions).

  MEM_ROOT *execution_mem_root= thd->mem_root;

  thd->mem_root= thd->sp_runtime_ctx->sp->get_persistent_mem_root();

  // Switch THD::free_list. It's used to remember the newly created set of Items
  // during parsing. We should clean those items after each execution.

  Item *execution_free_list= thd->free_list;
  thd->free_list= NULL;

  // Create a new LEX and intialize it.

  LEX *lex_saved= thd->lex;

  thd->lex= new (thd->mem_root) st_lex_local;
  lex_start(thd);

  thd->lex->sphead= sp;
  thd->lex->set_sp_current_parsing_ctx(get_parsing_ctx());
  sp->m_parser_data.set_current_stmt_start_ptr(sql_query.c_ptr());

  // Parse the just constructed SELECT-statement.

  bool parsing_failed= parse_sql(thd, &parser_state, NULL);

  if (!parsing_failed)
  {
    thd->lex->set_trg_event_type_for_tables();

    if (sp->m_type == SP_TYPE_TRIGGER)
    {
      /*
        Also let us bind these objects to Field objects in table being opened.

        We ignore errors of setup_field() here, because if even something is
        wrong we still will be willing to open table to perform some operations
        (e.g.  SELECT)... Anyway some things can be checked only during trigger
        execution.
      */

      Table_triggers_list *ttl= sp->m_trg_list;
      int event= sp->m_trg_chistics.event;
      int action_time= sp->m_trg_chistics.action_time;
      GRANT_INFO *grant_table= &ttl->subject_table_grants[event][action_time];

      for (Item_trigger_field *trg_field= sp->m_trg_table_fields.first;
           trg_field;
           trg_field= trg_field->next_trg_field)
      {
        trg_field->setup_field(thd, ttl->trigger_table, grant_table);
      }
    }

    // Call after-parsing callback.

    parsing_failed= on_after_expr_parsing(thd);

    // Append newly created Items to the list of Items, owned by this
    // instruction.

    free_list= thd->free_list;
  }

  // Restore THD::lex.

  thd->lex->sphead= NULL;
  thd->lex->set_sp_current_parsing_ctx(NULL);

  LEX *expr_lex= thd->lex;
  thd->lex= lex_saved;

  // Restore execution mem-root and THD::free_list.

  thd->mem_root= execution_mem_root;
  thd->free_list= execution_free_list;

  // That's it.

  return parsing_failed ? NULL : expr_lex;
}

bool sp_lex_instr::validate_lex_and_execute_core(THD *thd,
                                                 uint *nextp,
                                                 bool open_tables)
{
  Reprepare_observer reprepare_observer;
  int reprepare_attempt= 0;

  while (true)
  {
    if (is_invalid())
    {
      LEX *lex= parse_expr(thd, thd->sp_runtime_ctx->sp);

      if (!lex)
        return true;

      set_lex(lex, true);

      m_first_execution= true;
    }

    /*
      Install the metadata observer. If some metadata version is
      different from prepare time and an observer is installed,
      the observer method will be invoked to push an error into
      the error stack.
    */
    Reprepare_observer *stmt_reprepare_observer= NULL;

    /*
      Meta-data versions are stored in the LEX-object on the first execution.
      Thus, the reprepare observer should not be installed for the first
      execution, because it will always be triggered.

      Then, the reprepare observer should be installed for the statements, which
      are marked by CF_REEXECUTION_FRAGILE (@sa CF_REEXECUTION_FRAGILE) or if
      the SQL-command is SQLCOM_END, which means that the LEX-object is
      representing an expression, so the exact SQL-command does not matter.
    */

    if (!m_first_execution &&
        (sql_command_flags[m_lex->sql_command] & CF_REEXECUTION_FRAGILE ||
         m_lex->sql_command == SQLCOM_END))
    {
      reprepare_observer.reset_reprepare_observer();
      stmt_reprepare_observer= &reprepare_observer;
    }

    thd->push_reprepare_observer(stmt_reprepare_observer);

    bool rc= reset_lex_and_exec_core(thd, nextp, open_tables);

    thd->pop_reprepare_observer();

    m_first_execution= false;

    if (!rc)
      return false;

    /*
      Here is why we need all the checks below:
        - if the reprepare observer is not set, we've got an error, which should
          be raised to the user;
        - if we've got fatal error, it should be raised to the user;
        - if our thread got killed during execution, the error should be raised
          to the user;
        - if we've got an error, different from ER_NEED_REPREPARE, we need to
          raise it to the user;
        - we take only 3 attempts to reprepare the query, otherwise we might end
          up in the endless loop.
    */
    if (stmt_reprepare_observer &&
        !thd->is_fatal_error &&
        !thd->killed &&
        thd->get_stmt_da()->sql_errno() == ER_NEED_REPREPARE &&
        reprepare_attempt++ < 3)
    {
      DBUG_ASSERT(stmt_reprepare_observer->is_invalidated());

      thd->clear_error();
      free_lex();
      invalidate();
    }
    else
      return true;
  }
}


void sp_lex_instr::set_lex(LEX *lex, bool is_lex_owner)
{
  free_lex();

  m_lex= lex;
  m_is_lex_owner= is_lex_owner;
  m_lex_query_tables_own_last= NULL;

  if (m_lex)
    m_lex->sp_lex_in_use= true;
}


void sp_lex_instr::free_lex()
{
  if (!m_is_lex_owner || !m_lex)
    return;

  /* Prevent endless recursion. */
  m_lex->sphead= NULL;
  lex_end(m_lex);
  delete (st_lex_local *) m_lex;

  m_lex= NULL;
  m_is_lex_owner= false;
  m_lex_query_tables_own_last= NULL;
}


void sp_lex_instr::cleanup_before_parsing(THD *thd)
{
  /*
    Destroy items in the instruction's free list before re-parsing the
    statement query string (and thus, creating new items).
  */
  Item *p= free_list;
  while (p)
  {
    Item *next= p->next;
    p->delete_self();
    p= next;
  }

  free_list= NULL;

  // Remove previously stored trigger-field items.
  sp_head *sp= thd->sp_runtime_ctx->sp;

  if (sp->m_type == SP_TYPE_TRIGGER)
    sp->m_trg_table_fields.empty();
}


void sp_lex_instr::get_query(String *sql_query) const
{
  LEX_STRING expr_query= this->get_expr_query();

  if (!expr_query.str)
  {
    sql_query->length(0);
    return;
  }

  sql_query->append("SELECT ");
  sql_query->append(expr_query.str, expr_query.length);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_stmt implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_stmt::execute(THD *thd, uint *nextp)
{
  bool need_subst= false;
  bool rc= false;

  DBUG_PRINT("info", ("query: '%.*s'", (int) m_query.length, m_query.str));

  const CSET_STRING query_backup= thd->query_string;

#if defined(ENABLED_PROFILING)
  /* This SP-instr is profilable and will be captured. */
  thd->profiling.set_query_source(m_query.str, m_query.length);
#endif

  /*
    If we can't set thd->query_string at all, we give up on this statement.
  */
  if (alloc_query(thd, m_query.str, m_query.length))
    return true;

  /*
    Check whether we actually need a substitution of SP variables with
    NAME_CONST(...) (using subst_spvars()).
    If both of the following apply, we won't need to substitute:

    - general log is off

    - binary logging is off, or not in statement mode

    We don't have to substitute on behalf of the query cache as
    queries with SP vars are not cached, anyway.

    query_name_consts is used elsewhere in a special case concerning
    CREATE TABLE, but we do not need to do anything about that here.

    The slow query log is another special case: we won't know whether a
    query qualifies for the slow query log until after it's been
    executed. We assume that most queries are not slow, so we do not
    pre-emptively substitute just for the slow query log. If a query
    ends up being slow after all and we haven't done the substitution
    already for any of the above (general log etc.), we'll do the
    substitution immediately before writing to the log.
  */

  need_subst= ((thd->variables.option_bits & OPTION_LOG_OFF) &&
               (!(thd->variables.option_bits & OPTION_BIN_LOG) ||
                !mysql_bin_log.is_open() ||
                thd->is_current_stmt_binlog_format_row())) ? FALSE : TRUE;

  /*
    If we need to do a substitution but can't (OOM), give up.
  */

  if (need_subst && subst_spvars(thd, this, &m_query))
    return true;

  /*
    (the order of query cache and subst_spvars calls is irrelevant because
    queries with SP vars can't be cached)
  */
  if (unlikely((thd->variables.option_bits & OPTION_LOG_OFF)==0))
    general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());

  if (query_cache_send_result_to_client(thd, thd->query(),
                                        thd->query_length()) <= 0)
  {
    rc= validate_lex_and_execute_core(thd, nextp, false);

    if (thd->get_stmt_da()->is_eof())
    {
      /* Finalize server status flags after executing a statement. */
      thd->update_server_status();

      thd->protocol->end_statement();
    }

    query_cache_end_of_result(thd);

    if (!rc && unlikely(log_slow_applicable(thd)))
    {
      /*
        We actually need to write the slow log. Check whether we already
        called subst_spvars() above, otherwise, do it now.  In the highly
        unlikely event of subst_spvars() failing (OOM), we'll try to log
        the unmodified statement instead.
      */
      if (!need_subst)
        rc= subst_spvars(thd, this, &m_query);
      log_slow_do(thd);
    }

    /*
      With the current setup, a subst_spvars() and a mysql_rewrite_query()
      (rewriting passwords etc.) will not both happen to a query.
      If this ever changes, we give the engineer pause here so they will
      double-check whether the potential conflict they created is a
      problem.
    */
    DBUG_ASSERT((thd->query_name_consts == 0) ||
                (thd->rewritten_query.length() == 0));
  }
  else
    *nextp= get_ip() + 1;

  thd->set_query(query_backup);
  thd->query_name_consts= 0;

  if (!thd->is_error())
    thd->get_stmt_da()->reset_diagnostics_area();

  return rc || thd->is_error();
}


void sp_instr_stmt::print(String *str)
{
  /* stmt CMD "..." */
  if (str->reserve(SP_STMT_PRINT_MAXLEN + SP_INSTR_UINT_MAXLEN + 8))
    return;
  str->qs_append(STRING_WITH_LEN("stmt"));
  str->qs_append(STRING_WITH_LEN(" \""));

  /*
    Print the query string (but not too much of it), just to indicate which
    statement it is.
  */
  uint len= m_query.length;
  if (len > SP_STMT_PRINT_MAXLEN)
    len= SP_STMT_PRINT_MAXLEN-3;

  /* Copy the query string and replace '\n' with ' ' in the process */
  for (uint i= 0 ; i < len ; i++)
  {
    char c= m_query.str[i];
    if (c == '\n')
      c= ' ';
    str->qs_append(c);
  }
  if (m_query.length > SP_STMT_PRINT_MAXLEN)
    str->qs_append(STRING_WITH_LEN("...")); /* Indicate truncated string */
  str->qs_append('"');
}


bool sp_instr_stmt::exec_core(THD *thd, uint *nextp)
{
  MYSQL_QUERY_EXEC_START(thd->query(),
                         thd->thread_id,
                         (char *) (thd->db ? thd->db : ""),
                         &thd->security_ctx->priv_user[0],
                         (char *)thd->security_ctx->host_or_ip,
                         3);

  bool rc= mysql_execute_command(thd);

  MYSQL_QUERY_EXEC_DONE(rc);

  *nextp= get_ip() + 1;

  return rc;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_set implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_set::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  if (!thd->sp_runtime_ctx->set_variable(thd, m_offset, &m_value_item))
    return false;

  /* Failed to evaluate the value. Reset the variable to NULL. */

  if (thd->sp_runtime_ctx->set_variable(thd, m_offset, 0))
  {
    /* If this also failed, let's abort. */
    my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
  }

  return true;
}

void sp_instr_set::print(String *str)
{
  /* set name@offset ... */
  int rsrv = SP_INSTR_UINT_MAXLEN+6;
  sp_variable *var = m_parsing_ctx->find_variable(m_offset);

  /* 'var' should always be non-null, but just in case... */
  if (var)
    rsrv+= var->name.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("set "));
  if (var)
  {
    str->qs_append(var->name.str, var->name.length);
    str->qs_append('@');
  }
  str->qs_append(m_offset);
  str->qs_append(' ');
  m_value_item->print(str, QT_ORDINARY);
}


///////////////////////////////////////////////////////////////////////////
// sp_instr_set_trigger_field implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_set_trigger_field::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;
  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  return m_trigger_field->set_value(thd, &m_value_item);
}

void sp_instr_set_trigger_field::print(String *str)
{
  str->append(STRING_WITH_LEN("set_trigger_field "));
  m_trigger_field->print(str, QT_ORDINARY);
  str->append(STRING_WITH_LEN(":="));
  m_value_item->print(str, QT_ORDINARY);
}

bool sp_instr_set_trigger_field::on_after_expr_parsing(THD *thd)
{
  DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

  m_value_item= thd->lex->select_lex.item_list.head();

  DBUG_ASSERT(!m_trigger_field);

  m_trigger_field=
    new (thd->mem_root) Item_trigger_field(thd->lex->current_context(),
                                           Item_trigger_field::NEW_ROW,
                                           m_trigger_field_name.str,
                                           UPDATE_ACL,
                                           false);

  return m_value_item == NULL || m_trigger_field == NULL;
}

void sp_instr_set_trigger_field::cleanup_before_parsing(THD *thd)
{
  sp_lex_instr::cleanup_before_parsing(thd);

  m_trigger_field= NULL;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_jump implementation.
///////////////////////////////////////////////////////////////////////////

void sp_instr_jump::print(String *str)
{
  /* jump dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("jump "));
  str->qs_append(m_dest);
}

uint sp_instr_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_dest= opt_shortcut_jump(sp, this);
  if (m_dest != get_ip() + 1)   /* Jumping to following instruction? */
    m_marked= true;
  m_optdest= sp->get_instr(m_dest);
  return m_dest;
}

uint sp_instr_jump::opt_shortcut_jump(sp_head *sp, sp_instr *start)
{
  uint dest= m_dest;
  sp_instr *i;

  while ((i= sp->get_instr(dest)))
  {
    uint ndest;

    if (start == i || this == i)
      break;
    ndest= i->opt_shortcut_jump(sp, start);
    if (ndest == dest)
      break;
    dest= ndest;
  }
  return dest;
}

void sp_instr_jump::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  if (m_dest > get_ip())
    bp->push_back(this);      // Forward
  else if (m_optdest)
    m_dest= m_optdest->get_ip();  // Backward
  m_ip= dst;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_jump_if_not class implementation
///////////////////////////////////////////////////////////////////////////

bool sp_instr_jump_if_not::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(m_expr_item);

  Item *item= sp_prepare_func_item(thd, &m_expr_item);

  if (!item)
    return true;

  *nextp= item->val_bool() ? get_ip() + 1 : m_dest;

  return false;
}


void sp_instr_jump_if_not::print(String *str)
{
  /* jump_if_not dest(cont) ... */
  if (str->reserve(2*SP_INSTR_UINT_MAXLEN+14+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("jump_if_not "));
  str->qs_append(m_dest);
  str->qs_append('(');
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  m_expr_item->print(str, QT_ORDINARY);
}

///////////////////////////////////////////////////////////////////////////
// sp_lex_branch_instr implementation.
///////////////////////////////////////////////////////////////////////////

uint sp_lex_branch_instr::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_dest);

  if (i)
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }

  sp->add_mark_lead(m_dest, leads);

  i= sp->get_instr(m_cont_dest);

  if (i)
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }

  sp->add_mark_lead(m_cont_dest, leads);

  return get_ip() + 1;
}

void sp_lex_branch_instr::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  /*
    cont. destinations may point backwards after shortcutting jumps
    during the mark phase. If it's still pointing forwards, only
    push this for backpatching if sp_instr_jump::opt_move() will not
    do it (i.e. if the m_dest points backwards).
   */
  if (m_cont_dest > get_ip())
  {                             // Forward
    if (m_dest < get_ip())
      bp->push_back(this);
  }
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->get_ip(); // Backward

  /* This will take care of m_dest and m_ip */
  if (m_dest > get_ip())
    bp->push_back(this);      // Forward
  else if (m_optdest)
    m_dest= m_optdest->get_ip();  // Backward
  m_ip= dst;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_jump_case_when implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_jump_case_when::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(m_eq_item);

  Item *item= sp_prepare_func_item(thd, &m_eq_item);

  if (!item)
    return true;

  *nextp= item->val_bool() ? get_ip() + 1 : m_dest;

  return false;
}


void sp_instr_jump_case_when::print(String *str)
{
  /* jump_if_not dest(cont) ... */
  if (str->reserve(2*SP_INSTR_UINT_MAXLEN+14+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("jump_if_not_case_when "));
  str->qs_append(m_dest);
  str->qs_append('(');
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  m_eq_item->print(str, QT_ORDINARY);
}

bool sp_instr_jump_case_when::build_expr_items(THD *thd)
{
  // Setup CASE-expression item (m_case_expr_item).

  m_case_expr_item= new Item_case_expr(m_case_expr_id);

  if (!m_case_expr_item)
    return true;

#ifndef DBUG_OFF
  m_case_expr_item->m_sp= thd->lex->sphead;
#endif

  // Setup WHEN-expression item (m_expr_item) if it is not already set.
  //
  // This function can be called in two cases:
  //
  //   - during initial (regular) parsing of SP. In this case we don't have
  //     lex->select_lex (because it's not a SELECT statement), but
  //     m_expr_item is already set in constructor.
  //
  //   - during re-parsing after meta-data change. In this case we've just
  //     parsed aux-SELECT statement, so we need to take 1st (and the only one)
  //     item from its list.

  if (!m_expr_item)
  {
    DBUG_ASSERT(thd->lex->select_lex.item_list.elements == 1);

    m_expr_item= thd->lex->select_lex.item_list.head();
  }

  // Setup main expression item (m_expr_item).

  m_eq_item= new Item_func_eq(m_case_expr_item, m_expr_item);

  if (!m_eq_item)
    return true;

  return false;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_freturn implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_freturn::exec_core(THD *thd, uint *nextp)
{
  /*
    RETURN is a "procedure statement" (in terms of the SQL standard).
    That means, Diagnostics Area should be clean before its execution.
  */

  Diagnostics_area *da= thd->get_stmt_da();
  da->clear_warning_info(da->warning_info_id());

  /*
    Change <next instruction pointer>, so that this will be the last
    instruction in the stored function.
  */

  *nextp= UINT_MAX;

  /*
    Evaluate the value of return expression and store it in current runtime
    context.

    NOTE: It's necessary to evaluate result item right here, because we must
    do it in scope of execution the current context/block.
  */

  return thd->sp_runtime_ctx->set_return_value(thd, &m_expr_item);
}

void sp_instr_freturn::print(String *str)
{
  /* freturn type expr... */
  if (str->reserve(1024+8+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("freturn "));
  str->qs_append((uint) m_return_field_type);
  str->qs_append(' ');
  m_expr_item->print(str, QT_ORDINARY);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_hpush_jump implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_hpush_jump::execute(THD *thd, uint *nextp)
{
  *nextp= m_dest;

  return thd->sp_runtime_ctx->push_handler(m_handler, get_ip() + 1);
}


void sp_instr_hpush_jump::print(String *str)
{
  /* hpush_jump dest fsize type */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 21))
    return;

  str->qs_append(STRING_WITH_LEN("hpush_jump "));
  str->qs_append(m_dest);
  str->qs_append(' ');
  str->qs_append(m_frame);

  switch (m_handler->type) {
  case sp_handler::EXIT:
    str->qs_append(STRING_WITH_LEN(" EXIT"));
    break;
  case sp_handler::CONTINUE:
    str->qs_append(STRING_WITH_LEN(" CONTINUE"));
    break;
  default:
    // The handler type must be either CONTINUE or EXIT.
    DBUG_ASSERT(0);
  }
}


uint sp_instr_hpush_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_dest);

  if (i)
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }

  sp->add_mark_lead(m_dest, leads);

  /*
    For continue handlers, all instructions in the scope of the handler
    are possible leads. For example, the instruction after freturn might
    be executed if the freturn triggers the condition handled by the
    continue handler.

    m_dest marks the start of the handler scope. It's added as a lead
    above, so we start on m_dest+1 here.
    m_opt_hpop is the hpop marking the end of the handler scope.
  */
  if (m_handler->type == sp_handler::CONTINUE)
  {
    for (uint scope_ip= m_dest+1; scope_ip <= m_opt_hpop; scope_ip++)
      sp->add_mark_lead(scope_ip, leads);
  }

  return get_ip() + 1;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_hpop implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_hpop::execute(THD *thd, uint *nextp)
{
  thd->sp_runtime_ctx->pop_handlers(m_count);
  *nextp= get_ip() + 1;
  return false;
}

void sp_instr_hpop::print(String *str)
{
  /* hpop count */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("hpop "));
  str->qs_append(m_count);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_hreturn implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_hreturn::execute(THD *thd, uint *nextp)
{
  // NOTE: we must call sp_rcontext::exit_handler() even if m_dest is set.

  uint continue_ip= thd->sp_runtime_ctx->exit_handler(thd->get_stmt_da());

  *nextp= m_dest ? m_dest : continue_ip;

  return false;
}


void sp_instr_hreturn::print(String *str)
{
  /* hreturn framesize dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 9))
    return;
  str->qs_append(STRING_WITH_LEN("hreturn "));
  if (m_dest)
  {
    // NOTE: this is legacy: hreturn instruction for EXIT handler
    // should print out 0 as frame index.
    str->qs_append(STRING_WITH_LEN("0 "));
    str->qs_append(m_dest);
  }
  else
  {
    str->qs_append(m_frame);
  }
}


uint sp_instr_hreturn::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  if (m_dest)
  {
    /*
      This is an EXIT handler; next instruction step is in m_dest.
     */
    return m_dest;
  }

  /*
    This is a CONTINUE handler; next instruction step will come from
    the handler stack and not from opt_mark.
   */
  return UINT_MAX;
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_cpush implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_cpush::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  // sp_instr_cpush::execute() just registers the cursor in the runtime context.

  return thd->sp_runtime_ctx->push_cursor(this);
}


bool sp_instr_cpush::exec_core(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  // sp_instr_cpush::exec_core() opens the cursor (it's called from
  // sp_instr_copen::execute().

  return c ? c->open(thd) : true;
}


void sp_instr_cpush::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  uint rsrv= SP_INSTR_UINT_MAXLEN + 7 + m_cursor_query.length + 1;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cpush "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);

  str->qs_append(':');
  str->qs_append(m_cursor_query.str, m_cursor_query.length);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_cpop implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_cpop::execute(THD *thd, uint *nextp)
{
  thd->sp_runtime_ctx->pop_cursors(m_count);
  *nextp= get_ip() + 1;

  return false;
}


void sp_instr_cpop::print(String *str)
{
  /* cpop count */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("cpop "));
  str->qs_append(m_count);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_copen implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_copen::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  // Get the cursor pointer.

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  if (!c)
    return true;

  // Retrieve sp_instr_cpush instance.

  sp_instr_cpush *push_instr= c->get_push_instr();

  // Switch Statement Arena to the sp_instr_cpush object. It contains the
  // free_list of the query, so new items (if any) are stored in the right
  // free_list, and we can cleanup after each open.

  Query_arena *stmt_arena_saved= thd->stmt_arena;
  thd->stmt_arena= push_instr;

  // Switch to the cursor's lex and execute sp_instr_cpush::exec_core().
  // sp_instr_cpush::exec_core() is *not* executed during
  // sp_instr_cpush::execute(). sp_instr_cpush::exec_core() is intended to be
  // executed on cursor opening.

  bool rc= push_instr->validate_lex_and_execute_core(thd, nextp, false);

  // Cleanup the query's items.

  if (push_instr->free_list)
    cleanup_items(push_instr->free_list);

  // Restore Statement Arena.

  thd->stmt_arena= stmt_arena_saved;

  return rc;
}


void sp_instr_copen::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* copen name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+7;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("copen "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_cclose implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_cclose::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  return c ? c->close(thd) : true;
}


void sp_instr_cclose::print(String *str)
{
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* cclose name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cclose "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_cfetch implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_cfetch::execute(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_cursor *c= thd->sp_runtime_ctx->get_cursor(m_cursor_idx);

  return c ? c->fetch(thd, &m_varlist) : true;
}


void sp_instr_cfetch::print(String *str)
{
  List_iterator_fast<sp_variable> li(m_varlist);
  sp_variable *pv;
  const LEX_STRING *cursor_name= m_parsing_ctx->find_cursor(m_cursor_idx);

  /* cfetch name@offset vars... */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (cursor_name)
    rsrv+= cursor_name->length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cfetch "));
  if (cursor_name)
  {
    str->qs_append(cursor_name->str, cursor_name->length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor_idx);
  while ((pv= li++))
  {
    if (str->reserve(pv->name.length+SP_INSTR_UINT_MAXLEN+2))
      return;
    str->qs_append(' ');
    str->qs_append(pv->name.str, pv->name.length);
    str->qs_append('@');
    str->qs_append(pv->offset);
  }
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_error implementation.
///////////////////////////////////////////////////////////////////////////

void sp_instr_error::print(String *str)
{
  /* error code */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+6))
    return;
  str->qs_append(STRING_WITH_LEN("error "));
  str->qs_append(m_errcode);
}

///////////////////////////////////////////////////////////////////////////
// sp_instr_set_case_expr implementation.
///////////////////////////////////////////////////////////////////////////

bool sp_instr_set_case_expr::exec_core(THD *thd, uint *nextp)
{
  *nextp= get_ip() + 1;

  sp_rcontext *rctx= thd->sp_runtime_ctx;

  if (rctx->set_case_expr(thd, m_case_expr_id, &m_expr_item) &&
      !rctx->get_case_expr(m_case_expr_id))
  {
    // Failed to evaluate the value, the case expression is still not
    // initialized. Set to NULL so we can continue.

    Item *null_item= new Item_null();

    if (!null_item || rctx->set_case_expr(thd, m_case_expr_id, &null_item))
    {
      // If this also failed, we have to abort.
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    }

    return true;
  }

  return false;
}


void sp_instr_set_case_expr::print(String *str)
{
  /* set_case_expr (cont) id ... */
  str->reserve(2*SP_INSTR_UINT_MAXLEN+18+32); // Add some extra for expr too
  str->qs_append(STRING_WITH_LEN("set_case_expr ("));
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  str->qs_append(m_case_expr_id);
  str->qs_append(' ');
  m_expr_item->print(str, QT_ORDINARY);
}

uint sp_instr_set_case_expr::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_marked= true;

  sp_instr *i= sp->get_instr(m_cont_dest);

  if (i)
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }

  sp->add_mark_lead(m_cont_dest, leads);
  return get_ip() + 1;
}

void sp_instr_set_case_expr::opt_move(uint dst, List<sp_branch_instr> *bp)
{
  if (m_cont_dest > get_ip())
    bp->push_back(this);        // Forward
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->get_ip(); // Backward
  m_ip= dst;
}
