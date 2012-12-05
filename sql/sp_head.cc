/*
   Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql_priv.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif
#include "sp_head.h"
#include "sp.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp_cache.h"

/*
  Sufficient max length of printed destinations and frame offsets (all uints).
*/
#define SP_INSTR_UINT_MAXLEN  8
#define SP_STMT_PRINT_MAXLEN 40


#include <my_user.h>

extern "C" uchar *sp_table_key(const uchar *ptr, size_t *plen, my_bool first);

/**
  Helper function which operates on a THD object to set the query start_time to
  the current time.

  @param[in, out] thd The session object

*/

static void reset_start_time_for_sp(THD *thd)
{
  /*
    Do nothing if the context is a trigger or function because time should be
    constant during the execution of those.
  */
  if (!thd->in_sub_stmt)
  {
    /*
      First investigate if there is a cached time stamp
    */
    if (thd->user_time)
    {
      thd->start_time= thd->user_time;
    }
    else
    {
      my_micro_time_and_time(&thd->start_time);
    }
  }
}

Item_result
sp_map_result_type(enum enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
    return INT_RESULT;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    return DECIMAL_RESULT;
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    return REAL_RESULT;
  default:
    return STRING_RESULT;
  }
}


Item::Type
sp_map_item_type(enum enum_field_types type)
{
  switch (type) {
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
    return Item::INT_ITEM;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    return Item::DECIMAL_ITEM;
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    return Item::REAL_ITEM;
  default:
    return Item::STRING_ITEM;
  }
}


/**
  Return a string representation of the Item value.

  @param thd     thread handle
  @param str     string buffer for representation of the value

  @note
    If the item has a string result type, the string is escaped
    according to its character set.

  @retval
    NULL      on error
  @retval
    non-NULL  a pointer to valid a valid string on success
*/

static String *
sp_get_item_value(THD *thd, Item *item, String *str)
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
        CHARSET_INFO *cs= thd->variables.character_set_client;

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


/**
   Returns a combination of:
   - sp_head::MULTI_RESULTS: added if the 'cmd' is a command that might
     result in multiple result sets being sent back.
   - sp_head::CONTAINS_DYNAMIC_SQL: added if 'cmd' is one of PREPARE,
     EXECUTE, DEALLOCATE.
*/

uint
sp_get_flags_for_command(LEX *lex)
{
  uint flags;

  switch (lex->sql_command) {
  case SQLCOM_SELECT:
    if (lex->result)
    {
      flags= 0;                      /* This is a SELECT with INTO clause */
      break;
    }
    /* fallthrough */
  case SQLCOM_ANALYZE:
  case SQLCOM_BACKUP_TABLE:
  case SQLCOM_OPTIMIZE:
  case SQLCOM_PRELOAD_KEYS:
  case SQLCOM_ASSIGN_TO_KEYCACHE:
  case SQLCOM_CHECKSUM:
  case SQLCOM_CHECK:
  case SQLCOM_HA_READ:
  case SQLCOM_SHOW_AUTHORS:
  case SQLCOM_SHOW_BINLOGS:
  case SQLCOM_SHOW_BINLOG_EVENTS:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_COLUMN_TYPES:
  case SQLCOM_SHOW_CONTRIBUTORS:
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_CREATE_DB:
  case SQLCOM_SHOW_CREATE_FUNC:
  case SQLCOM_SHOW_CREATE_PROC:
  case SQLCOM_SHOW_CREATE_EVENT:
  case SQLCOM_SHOW_CREATE_TRIGGER:
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_ERRORS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_FUNC_CODE:
  case SQLCOM_SHOW_GRANTS:
  case SQLCOM_SHOW_ENGINE_STATUS:
  case SQLCOM_SHOW_ENGINE_LOGS:
  case SQLCOM_SHOW_ENGINE_MUTEX:
  case SQLCOM_SHOW_EVENTS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_MASTER_STAT:
  case SQLCOM_SHOW_NEW_MASTER:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_PRIVILEGES:
  case SQLCOM_SHOW_PROCESSLIST:
  case SQLCOM_SHOW_PROC_CODE:
  case SQLCOM_SHOW_SLAVE_HOSTS:
  case SQLCOM_SHOW_SLAVE_STAT:
  case SQLCOM_SHOW_STATUS:
  case SQLCOM_SHOW_STATUS_FUNC:
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_WARNS:
  case SQLCOM_REPAIR:
  case SQLCOM_RESTORE_TABLE:
    flags= sp_head::MULTI_RESULTS;
    break;
  /*
    EXECUTE statement may return a result set, but doesn't have to.
    We can't, however, know it in advance, and therefore must add
    this statement here. This is ok, as is equivalent to a result-set
    statement within an IF condition.
  */
  case SQLCOM_EXECUTE:
    flags= sp_head::MULTI_RESULTS | sp_head::CONTAINS_DYNAMIC_SQL;
    break;
  case SQLCOM_PREPARE:
  case SQLCOM_DEALLOCATE_PREPARE:
    flags= sp_head::CONTAINS_DYNAMIC_SQL;
    break;
  case SQLCOM_CREATE_TABLE:
    if (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)
      flags= 0;
    else
      flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  case SQLCOM_DROP_TABLE:
    if (lex->drop_temporary)
      flags= 0;
    else
      flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  case SQLCOM_FLUSH:
    flags= sp_head::HAS_SQLCOM_FLUSH;
    break;
  case SQLCOM_RESET:
    flags= sp_head::HAS_SQLCOM_RESET;
    break;
  case SQLCOM_CREATE_INDEX:
  case SQLCOM_CREATE_DB:
  case SQLCOM_CREATE_VIEW:
  case SQLCOM_CREATE_TRIGGER:
  case SQLCOM_CREATE_USER:
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_GRANT:
  case SQLCOM_REVOKE:
  case SQLCOM_BEGIN:
  case SQLCOM_RENAME_TABLE:
  case SQLCOM_RENAME_USER:
  case SQLCOM_DROP_INDEX:
  case SQLCOM_DROP_DB:
  case SQLCOM_REVOKE_ALL:
  case SQLCOM_DROP_USER:
  case SQLCOM_DROP_VIEW:
  case SQLCOM_DROP_TRIGGER:
  case SQLCOM_TRUNCATE:
  case SQLCOM_COMMIT:
  case SQLCOM_ROLLBACK:
  case SQLCOM_LOAD:
  case SQLCOM_LOAD_MASTER_DATA:
  case SQLCOM_LOCK_TABLES:
  case SQLCOM_CREATE_PROCEDURE:
  case SQLCOM_CREATE_SPFUNCTION:
  case SQLCOM_ALTER_PROCEDURE:
  case SQLCOM_ALTER_FUNCTION:
  case SQLCOM_DROP_PROCEDURE:
  case SQLCOM_DROP_FUNCTION:
  case SQLCOM_CREATE_EVENT:
  case SQLCOM_ALTER_EVENT:
  case SQLCOM_DROP_EVENT:
  case SQLCOM_INSTALL_PLUGIN:
  case SQLCOM_UNINSTALL_PLUGIN:
    flags= sp_head::HAS_COMMIT_OR_ROLLBACK;
    break;
  default:
    flags= 0;
    break;
  }
  return flags;
}

/**
  Prepare an Item for evaluation (call of fix_fields).

  @param thd       thread handler
  @param it_addr   pointer on item refernce

  @retval
    NULL      error
  @retval
    non-NULL  prepared item
*/

Item *
sp_prepare_func_item(THD* thd, Item **it_addr)
{
  DBUG_ENTER("sp_prepare_func_item");
  it_addr= (*it_addr)->this_item_addr(thd, it_addr);

  if (!(*it_addr)->fixed &&
      ((*it_addr)->fix_fields(thd, it_addr) ||
       (*it_addr)->check_cols(1)))
  {
    DBUG_PRINT("info", ("fix_fields() failed"));
    DBUG_RETURN(NULL);
  }
  DBUG_RETURN(*it_addr);
}


/**
  Evaluate an expression and store the result in the field.

  @param thd                    current thread object
  @param result_field           the field to store the result
  @param expr_item_ptr          the root item of the expression

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_eval_expr(THD *thd, Field *result_field, Item **expr_item_ptr)
{
  Item *expr_item;
  enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  bool save_abort_on_warning= thd->abort_on_warning;
  bool save_stmt_modified_non_trans_table= 
    thd->transaction.stmt.modified_non_trans_table;

  DBUG_ENTER("sp_eval_expr");

  if (!*expr_item_ptr)
    goto error;

  if (!(expr_item= sp_prepare_func_item(thd, expr_item_ptr)))
    goto error;

  /*
    Set THD flags to emit warnings/errors in case of overflow/type errors
    during saving the item into the field.

    Save original values and restore them after save.
  */
  
  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  thd->abort_on_warning=
    thd->variables.sql_mode &
    (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES);
  thd->transaction.stmt.modified_non_trans_table= FALSE;

  /* Save the value in the field. Convert the value if needed. */

  expr_item->save_in_field(result_field, 0);

  thd->count_cuted_fields= save_count_cuted_fields;
  thd->abort_on_warning= save_abort_on_warning;
  thd->transaction.stmt.modified_non_trans_table= save_stmt_modified_non_trans_table;

  if (!thd->is_error())
    DBUG_RETURN(FALSE);

error:
  /*
    In case of error during evaluation, leave the result field set to NULL.
    Sic: we can't do it in the beginning of the function because the 
    result field might be needed for its own re-evaluation, e.g. case of 
    set x = x + 1;
  */
  result_field->set_null();
  DBUG_RETURN (TRUE);
}


/*
 *
 *  sp_name
 *
 */

sp_name::sp_name(THD *thd, char *key, uint key_len)
{
  m_sroutines_key.str= key;
  m_sroutines_key.length= key_len;
  m_qname.str= ++key;
  m_qname.length= key_len - 1;
  if ((m_name.str= strchr(m_qname.str, '.')))
  {
    m_db.length= m_name.str - key;
    m_db.str= strmake_root(thd->mem_root, key, m_db.length);
    m_name.str++;
    m_name.length= m_qname.length - m_db.length - 1;
  }
  else
  {
    m_name.str= m_qname.str;
    m_name.length= m_qname.length;
    m_db.str= 0;
    m_db.length= 0;
  }
  m_explicit_name= false;
}


/**
  Init the qualified name from the db and name.
*/
void
sp_name::init_qname(THD *thd)
{
  const uint dot= !!m_db.length;
  /* m_sroutines format: m_type + [database + dot] + name + nul */
  m_sroutines_key.length= 1 + m_db.length + dot + m_name.length;
  if (!(m_sroutines_key.str= (char*) thd->alloc(m_sroutines_key.length + 1)))
    return;
  m_qname.length= m_sroutines_key.length - 1;
  m_qname.str= m_sroutines_key.str + 1;
  sprintf(m_qname.str, "%.*s%.*s%.*s",
          (int) m_db.length, (m_db.length ? m_db.str : ""),
          dot, ".",
          (int) m_name.length, m_name.str);
}


/**
  Check that the name 'ident' is ok.  It's assumed to be an 'ident'
  from the parser, so we only have to check length and trailing spaces.
  The former is a standard requirement (and 'show status' assumes a
  non-empty name), the latter is a mysql:ism as trailing spaces are
  removed by get_field().

  @retval
    TRUE    bad name
  @retval
    FALSE   name is ok
*/

bool
check_routine_name(LEX_STRING *ident)
{
  if (!ident || !ident->str || !ident->str[0] ||
      ident->str[ident->length-1] == ' ')
  { 
    my_error(ER_SP_WRONG_NAME, MYF(0), ident->str);
    return TRUE;
  }
  if (check_string_char_length(ident, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), ident->str);
    return TRUE;
  }

  return FALSE;
}


/*
 *
 *  sp_head
 *
 */

void *
sp_head::operator new(size_t size) throw()
{
  DBUG_ENTER("sp_head::operator new");
  MEM_ROOT own_root;
  sp_head *sp;

  init_sql_alloc(&own_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  sp= (sp_head *) alloc_root(&own_root, size);
  if (sp == NULL)
    DBUG_RETURN(NULL);
  sp->main_mem_root= own_root;
  DBUG_PRINT("info", ("mem_root 0x%lx", (ulong) &sp->mem_root));
  DBUG_RETURN(sp);
}

void 
sp_head::operator delete(void *ptr, size_t size) throw()
{
  DBUG_ENTER("sp_head::operator delete");
  MEM_ROOT own_root;

  if (ptr == NULL)
    DBUG_VOID_RETURN;

  sp_head *sp= (sp_head *) ptr;

  /* Make a copy of main_mem_root as free_root will free the sp */
  own_root= sp->main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to 0x%lx",
                      (ulong) &sp->mem_root, (ulong) &own_root));
  free_root(&own_root, MYF(0));

  DBUG_VOID_RETURN;
}


sp_head::sp_head()
  :Query_arena(&main_mem_root, INITIALIZED_FOR_SP),
   m_flags(0), m_recursion_level(0), m_next_cached_sp(0),
   m_cont_level(0)
{
  const LEX_STRING str_reset= { NULL, 0 };

  m_first_instance= this;
  m_first_free_instance= this;
  m_last_cached_sp= this;

  m_return_field_def.charset = NULL;
  /*
    FIXME: the only use case when name is NULL is events, and it should
    be rewritten soon. Remove the else part and replace 'if' with
    an assert when this is done.
  */
  m_db= m_name= m_qname= str_reset;

  DBUG_ENTER("sp_head::sp_head");

  m_backpatch.empty();
  m_cont_backpatch.empty();
  m_lex.empty();
  hash_init(&m_sptabs, system_charset_info, 0, 0, 0, sp_table_key, 0, 0);
  hash_init(&m_sroutines, system_charset_info, 0, 0, 0, sp_sroutine_key, 0, 0);

  m_body_utf8.str= NULL;
  m_body_utf8.length= 0;

  DBUG_VOID_RETURN;
}


void
sp_head::init(LEX *lex)
{
  DBUG_ENTER("sp_head::init");

  lex->spcont= m_pcont= new sp_pcontext();

  if (!lex->spcont)
    DBUG_VOID_RETURN;

  /*
    Altough trg_table_fields list is used only in triggers we init for all
    types of stored procedures to simplify reset_lex()/restore_lex() code.
  */
  lex->trg_table_fields.empty();
  my_init_dynamic_array(&m_instr, sizeof(sp_instr *), 16, 8);

  m_param_begin= NULL;
  m_param_end= NULL;

  m_body_begin= NULL ;

  m_qname.str= NULL;
  m_qname.length= 0;

  m_explicit_name= false;

  m_db.str= NULL;
  m_db.length= 0;

  m_name.str= NULL;
  m_name.length= 0;

  m_params.str= NULL;
  m_params.length= 0;

  m_body.str= NULL;
  m_body.length= 0;

  m_defstr.str= NULL;
  m_defstr.length= 0;

  m_sroutines_key.str= NULL;
  m_sroutines_key.length= 0;

  m_return_field_def.charset= NULL;

  DBUG_VOID_RETURN;
}


void
sp_head::init_sp_name(THD *thd, sp_name *spname)
{
  DBUG_ENTER("sp_head::init_sp_name");

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

  m_sroutines_key.length= spname->m_sroutines_key.length;
  m_sroutines_key.str= (char*) memdup_root(thd->mem_root,
                                           spname->m_sroutines_key.str,
                                           spname->m_sroutines_key.length + 1);
  m_sroutines_key.str[0]= static_cast<char>(m_type);

  m_qname.length= m_sroutines_key.length - 1;
  m_qname.str= m_sroutines_key.str + 1;

  DBUG_VOID_RETURN;
}


void
sp_head::set_body_start(THD *thd, const char *begin_ptr)
{
  m_body_begin= begin_ptr;
  thd->m_parser_state->m_lip.body_utf8_start(thd, begin_ptr);
}


void
sp_head::set_stmt_end(THD *thd)
{
  Lex_input_stream *lip= & thd->m_parser_state->m_lip; /* shortcut */
  const char *end_ptr= lip->get_cpp_ptr(); /* shortcut */

  /* Make the string of parameters. */

  if (m_param_begin && m_param_end)
  {
    m_params.length= m_param_end - m_param_begin;
    m_params.str= thd->strmake(m_param_begin, m_params.length);
  }

  /* Remember end pointer for further dumping of whole statement. */

  thd->lex->stmt_definition_end= end_ptr;

  /* Make the string of body (in the original character set). */

  m_body.length= end_ptr - m_body_begin;
  m_body.str= thd->strmake(m_body_begin, m_body.length);
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


static TYPELIB *
create_typelib(MEM_ROOT *mem_root, Create_field *field_def, List<String> *src)
{
  TYPELIB *result= NULL;
  CHARSET_INFO *cs= field_def->charset;
  DBUG_ENTER("create_typelib");

  if (src->elements)
  {
    result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
    result->count= src->elements;
    result->name= "";
    if (!(result->type_names=(const char **)
          alloc_root(mem_root,(sizeof(char *)+sizeof(int))*(result->count+1))))
      DBUG_RETURN(0);
    result->type_lengths= (uint*)(result->type_names + result->count+1);
    List_iterator<String> it(*src);
    String conv;
    for (uint i=0; i < result->count; i++)
    {
      uint32 dummy;
      uint length;
      String *tmp= it++;

      if (String::needs_conversion(tmp->length(), tmp->charset(),
      				   cs, &dummy))
      {
        uint cnv_errs;
        conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);

        length= conv.length();
        result->type_names[i]= (char*) strmake_root(mem_root, conv.ptr(),
                                                    length);
      }
      else
      {
        length= tmp->length();
        result->type_names[i]= strmake_root(mem_root, tmp->ptr(), length);
      }

      // Strip trailing spaces.
      length= cs->cset->lengthsp(cs, result->type_names[i], length);
      result->type_lengths[i]= length;
      ((uchar *)result->type_names[i])[length]= '\0';
    }
    result->type_names[result->count]= 0;
    result->type_lengths[result->count]= 0;
  }
  DBUG_RETURN(result);
}


int
sp_head::create(THD *thd)
{
  DBUG_ENTER("sp_head::create");
  DBUG_PRINT("info", ("type: %d name: %s params: %s body: %s",
		      m_type, m_name.str, m_params.str, m_body.str));

  DBUG_RETURN(sp_create_routine(thd, m_type, this));
}

sp_head::~sp_head()
{
  LEX *lex;
  sp_instr *i;
  DBUG_ENTER("sp_head::~sp_head");

  /* sp_head::restore_thd_mem_root() must already have been called. */
  DBUG_ASSERT(m_thd == NULL);

  for (uint ip = 0 ; (i = get_instr(ip)) ; ip++)
    delete i;
  delete_dynamic(&m_instr);
  m_pcont->destroy();
  free_items();

  /*
    If we have non-empty LEX stack then we just came out of parser with
    error. Now we should delete all auxilary LEXes and restore original
    THD::lex. It is safe to not update LEX::ptr because further query
    string parsing and execution will be stopped anyway.
  */
  while ((lex= (LEX *)m_lex.pop()))
  {
    THD *thd= lex->thd;
    lex_end(thd->lex);
    delete thd->lex;
    thd->lex= lex;
  }

  hash_free(&m_sptabs);
  hash_free(&m_sroutines);

  delete m_next_cached_sp;

  DBUG_VOID_RETURN;
}


/**
  This is only used for result fields from functions (both during
  fix_length_and_dec() and evaluation).
*/

Field *
sp_head::create_result_field(uint field_max_length, const char *field_name,
                             TABLE *table)
{
  uint field_length;
  Field *field;

  DBUG_ENTER("sp_head::create_result_field");

  field_length= !m_return_field_def.length ?
                field_max_length : m_return_field_def.length;

  field= ::make_field(table->s,                     /* TABLE_SHARE ptr */
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
  
  DBUG_RETURN(field);
}


int cmp_splocal_locations(Item_splocal * const *a, Item_splocal * const *b)
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
     needs to be valid after exection of these statements was finished. In 
     order to achieve that, we
     * Allocate user_var_events array elements on appropriate mem_root (grep
       for user_var_events_alloc).
     * Use is_query_in_union() to determine if user_var_event is created.
     
  2) We need to empty thd->user_var_events after we have wrote a function
     call. This is currently done by making 
     reset_dynamic(&thd->user_var_events);
     calls in several different places. (TODO cosider moving this into
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

  @return
    - FALSE  on success.
    thd->query{_length} either has been appropriately replaced or there
    is no need for replacements.
    - TRUE   out of memory error.
*/

static bool
subst_spvars(THD *thd, sp_instr *instr, LEX_STRING *query_str)
{
  DBUG_ENTER("subst_spvars");

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
    DBUG_RETURN(FALSE);
    
  /* Sort SP var refs by their occurences in the query */
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
       splocal < sp_vars_uses.back(); splocal++)
  {
    Item *val;

    char str_buffer[STRING_BUFFER_USUAL_SIZE];
    String str_value_holder(str_buffer, sizeof(str_buffer),
                            &my_charset_latin1);
    String *str_value;
    
    /* append the text between sp ref occurences */
    res|= qbuf.append(cur + prev_pos, (*splocal)->pos_in_query - prev_pos);
    prev_pos= (*splocal)->pos_in_query + (*splocal)->len_in_query;
    
    /* append the spvar substitute */
    res|= qbuf.append(STRING_WITH_LEN(" NAME_CONST('"));
    res|= qbuf.append((*splocal)->m_name.str, (*splocal)->m_name.length);
    res|= qbuf.append(STRING_WITH_LEN("',"));
    res|= (*splocal)->fix_fields(thd, (Item **) splocal);

    if (res)
      break;

    val= (*splocal)->this_item();
    DBUG_PRINT("info", ("print 0x%lx", (long) val));
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
  res|= qbuf.append(cur + prev_pos, query_str->length - prev_pos);
  if (res)
    DBUG_RETURN(TRUE);

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
    DBUG_RETURN(TRUE);

  thd->set_query(pbuf, qbuf.length());

  DBUG_RETURN(FALSE);
}


/*
  Return appropriate error about recursion limit reaching

  SYNOPSIS
    sp_head::recursion_level_error()
    thd		Thread handle

  NOTE
    For functions and triggers we return error about prohibited recursion.
    For stored procedures we return about reaching recursion limit.
*/

void sp_head::recursion_level_error(THD *thd)
{
  if (m_type == TYPE_ENUM_PROCEDURE)
  {
    my_error(ER_SP_RECURSION_LIMIT, MYF(0),
             static_cast<int>(thd->variables.max_sp_recursion_depth),
             m_name.str);
  }
  else
    my_error(ER_SP_NO_RECURSION, MYF(0));
}


/**
  Execute the routine. The main instruction jump loop is there.
  Assume the parameters already set.
  @todo
    - Will write this SP statement into binlog separately 
    (TODO: consider changing the condition to "not inside event union")

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_head::execute(THD *thd)
{
  DBUG_ENTER("sp_head::execute");
  char saved_cur_db_name_buf[NAME_LEN+1];
  LEX_STRING saved_cur_db_name=
    { saved_cur_db_name_buf, sizeof(saved_cur_db_name_buf) };
  bool cur_db_changed= FALSE;
  sp_rcontext *ctx;
  bool err_status= FALSE;
  uint ip= 0;
  ulong save_sql_mode;
  bool save_abort_on_warning;
  Query_arena *old_arena;
  /* per-instruction arena */
  MEM_ROOT execute_mem_root;
  Query_arena execute_arena(&execute_mem_root, INITIALIZED_FOR_SP),
              backup_arena;
  query_id_t old_query_id;
  TABLE *old_derived_tables;
  LEX *old_lex;
  Item_change_list old_change_list;
  String old_packet;
  Reprepare_observer *save_reprepare_observer= thd->m_reprepare_observer;

  Object_creation_ctx *saved_creation_ctx;

  /* Use some extra margin for possible SP recursion and functions */
  if (check_stack_overrun(thd, 8 * STACK_MIN_SIZE, (uchar*)&old_packet))
    DBUG_RETURN(TRUE);

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

  if ((ctx= thd->spcont))
    ctx->clear_handler();
  thd->is_slave_error= 0;
  old_arena= thd->stmt_arena;

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
  thd->m_reprepare_observer= 0;

  /*
    It is also more efficient to save/restore current thd->lex once when
    do it in each instruction
  */
  old_lex= thd->lex;
  /*
    We should also save Item tree change list to avoid rollback something
    too early in the calling query.
  */
  old_change_list= thd->change_list;
  thd->change_list.empty();
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
  thd->spcont->callers_arena= &backup_arena;

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  /* Discard the initial part of executing routines. */
  thd->profiling.discard_current_query();
#endif
  do
  {
    sp_instr *i;
    uint hip;			// Handler ip

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
    /* 
     Treat each "instr" of a routine as discrete unit that could be profiled.
     Profiling only records information for segments of code that set the
     source of the query, and almost all kinds of instructions in s-p do not.
    */
    thd->profiling.finish_current_query();
    thd->profiling.start_new_query("continuing inside routine");
#endif

    i = get_instr(ip);	// Returns NULL when we're done.
    if (i == NULL)
    {
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
      thd->profiling.discard_current_query();
#endif
      break;
    }

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
      Will write this SP statement into binlog separately 
      (TODO: consider changing the condition to "not inside event union")
    */
    if (thd->prelocked_mode == NON_PRELOCKED)
      thd->user_var_events_alloc= thd->mem_root;

    err_status= i->execute(thd, &ip);

    if (i->free_list)
      cleanup_items(i->free_list);
    
    /* 
      If we've set thd->user_var_events_alloc to mem_root of this SP
      statement, clean all the events allocated in it.
    */
    if (thd->prelocked_mode == NON_PRELOCKED)
    {
      reset_dynamic(&thd->user_var_events);
      thd->user_var_events_alloc= NULL;//DEBUG
    }

    /* we should cleanup free_list and memroot, used by instruction */
    thd->cleanup_after_query();
    free_root(&execute_mem_root, MYF(0));    

    /*
      Check if an exception has occurred and a handler has been found
      Note: We have to check even if err_status == FALSE, since warnings (and
      some errors) don't return a non-zero value. We also have to check even
      if thd->killed != 0, since some errors return with this even when a
      handler has been found (e.g. "bad data").
    */
    if (ctx)
    {
      uint hf;

      switch (ctx->found_handler(&hip, &hf)) {
      case SP_HANDLER_NONE:
	break;
      case SP_HANDLER_CONTINUE:
        thd->restore_active_arena(&execute_arena, &backup_arena);
        thd->set_n_backup_active_arena(&execute_arena, &backup_arena);
        ctx->push_hstack(i->get_cont_dest());
        // Fall through
      default:
	ip= hip;
	err_status= FALSE;
	ctx->clear_handler();
	ctx->enter_handler(hip);
        thd->clear_error();
        thd->is_fatal_error= 0;
	thd->killed= THD::NOT_KILLED;
        thd->mysys_var->abort= 0;
	continue;
      }
    }
  } while (!err_status && !thd->killed);

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  thd->profiling.finish_current_query();
  thd->profiling.start_new_query("tail end of routine");
#endif

  /* Restore query context. */

  m_creation_ctx->restore_env(thd, saved_creation_ctx);

  /* Restore arena. */

  thd->restore_active_arena(&execute_arena, &backup_arena);

  thd->spcont->pop_all_cursors(); // To avoid memory leaks after an error

  /* Restore all saved */
  old_packet.swap(thd->packet);
  DBUG_ASSERT(thd->change_list.is_empty());
  thd->change_list= old_change_list;
  /* To avoid wiping out thd->change_list on old_change_list destruction */
  old_change_list.empty();
  thd->lex= old_lex;
  thd->query_id= old_query_id;
  DBUG_ASSERT(!thd->derived_tables);
  thd->derived_tables= old_derived_tables;
  thd->variables.sql_mode= save_sql_mode;
  thd->abort_on_warning= save_abort_on_warning;
  thd->m_reprepare_observer= save_reprepare_observer;

  thd->stmt_arena= old_arena;
  state= EXECUTED;

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
    ther are not other instances after this one in the list)

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

  DBUG_RETURN(err_status);
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
/**
  set_routine_security_ctx() changes routine security context, and
  checks if there is an EXECUTE privilege in new context.  If there is
  no EXECUTE privilege, it changes the context back and returns a
  error.

  @param thd         thread handle
  @param sp          stored routine to change the context for
  @param is_proc     TRUE is procedure, FALSE if function
  @param save_ctx    pointer to an old security context

  @todo
    - Cache if the definer has the right to use the object on the
    first usage and only reset the cache if someone does a GRANT
    statement that 'may' affect this.

  @retval
    TRUE   if there was a error, and the context wasn't changed.
  @retval
    FALSE  if the context was changed.
*/

bool
set_routine_security_ctx(THD *thd, sp_head *sp, bool is_proc,
                         Security_context **save_ctx)
{
  *save_ctx= 0;
  if (sp->m_chistics->suid != SP_IS_NOT_SUID &&
      sp->m_security_ctx.change_security_context(thd, &sp->m_definer_user,
                                                 &sp->m_definer_host,
                                                 &sp->m_db,
                                                 save_ctx))
    return TRUE;

  /*
    If we changed context to run as another user, we need to check the
    access right for the new context again as someone may have revoked
    the right to use the procedure from this user.

    TODO:
      Cache if the definer has the right to use the object on the
      first usage and only reset the cache if someone does a GRANT
      statement that 'may' affect this.
  */
  if (*save_ctx &&
      check_routine_access(thd, EXECUTE_ACL,
                           sp->m_db.str, sp->m_name.str, is_proc, FALSE))
  {
    sp->m_security_ctx.restore_security_context(thd, *save_ctx);
    *save_ctx= 0;
    return TRUE;
  }

  return FALSE;
}
#endif // ! NO_EMBEDDED_ACCESS_CHECKS


/**
  Execute trigger stored program.

  - changes security context for triggers
  - switch to new memroot
  - call sp_head::execute
  - restore old memroot
  - restores security context

  @param thd               Thread handle
  @param db                database name
  @param table             table name
  @param grant_info        GRANT_INFO structure to be filled with
                           information about definer's privileges
                           on subject table

  @todo
    - TODO: we should create sp_rcontext once per command and reuse it
    on subsequent executions of a trigger.

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_head::execute_trigger(THD *thd,
                         const LEX_STRING *db_name,
                         const LEX_STRING *table_name,
                         GRANT_INFO *grant_info)
{
  sp_rcontext *octx = thd->spcont;
  sp_rcontext *nctx = NULL;
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::INITIALIZED_FOR_SP);
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
    DBUG_RETURN(TRUE);

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
    DBUG_RETURN(TRUE);
  }
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

  if (!(nctx= new sp_rcontext(m_pcont, 0, octx)) ||
      nctx->init(thd))
  {
    err_status= TRUE;
    goto err_with_cleanup;
  }

#ifndef DBUG_OFF
  nctx->sp= this;
#endif

  thd->spcont= nctx;

  err_status= execute(thd);

err_with_cleanup:
  thd->restore_active_arena(&call_arena, &backup_arena);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_ctx);
#endif // NO_EMBEDDED_ACCESS_CHECKS

  delete nctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->spcont= octx;

  if (thd->killed)
    thd->send_kill_message();

  DBUG_RETURN(err_status);
}


/**
  Execute a function.

   - evaluate parameters
   - changes security context for SUID routines
   - switch to new memroot
   - call sp_head::execute
   - restore old memroot
   - evaluate the return value
   - restores security context

  @param thd               Thread handle
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

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_head::execute_function(THD *thd, Item **argp, uint argcount,
                          Field *return_value_fld)
{
  ulonglong binlog_save_options;
  bool need_binlog_call;
  uint arg_no;
  sp_rcontext *octx = thd->spcont;
  sp_rcontext *nctx = NULL;
  char buf[STRING_BUFFER_USUAL_SIZE];
  String binlog_buf(buf, sizeof(buf), &my_charset_bin);
  bool err_status= FALSE;
  MEM_ROOT call_mem_root;
  Query_arena call_arena(&call_mem_root, Query_arena::INITIALIZED_FOR_SP);
  Query_arena backup_arena;
  DBUG_ENTER("sp_head::execute_function");
  DBUG_PRINT("info", ("function %s", m_name.str));

  LINT_INIT(binlog_save_options);

  /*
    Check that the function is called with all specified arguments.

    If it is not, use my_error() to report an error, or it will not terminate
    the invoking query properly.
  */
  if (argcount != m_pcont->context_var_count())
  {
    /*
      Need to use my_error here, or it will not terminate the
      invoking query properly.
    */
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0),
             "FUNCTION", m_qname.str, m_pcont->context_var_count(), argcount);
    DBUG_RETURN(TRUE);
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

  if (!(nctx= new sp_rcontext(m_pcont, return_value_fld, octx)) ||
      nctx->init(thd))
  {
    thd->restore_active_arena(&call_arena, &backup_arena);
    err_status= TRUE;
    goto err_with_cleanup;
  }

  /*
    We have to switch temporarily back to callers arena/memroot.
    Function arguments belong to the caller and so the may reference
    memory which they will allocate during calculation long after
    this function call will be finished (e.g. in Item::cleanup()).
  */
  thd->restore_active_arena(&call_arena, &backup_arena);

#ifndef DBUG_OFF
  nctx->sp= this;
#endif

  /* Pass arguments. */
  for (arg_no= 0; arg_no < argcount; arg_no++)
  {
    /* Arguments must be fixed in Item_func_sp::fix_fields */
    DBUG_ASSERT(argp[arg_no]->fixed);

    if ((err_status= nctx->set_variable(thd, arg_no, &(argp[arg_no]))))
      goto err_with_cleanup;
  }

  /*
    If row-based binlogging, we don't need to binlog the function's call, let
    each substatement be binlogged its way.
  */
  need_binlog_call= mysql_bin_log.is_open() &&
    (thd->options & OPTION_BIN_LOG) && !thd->current_stmt_binlog_row_based;

  /*
    Remember the original arguments for unrolled replication of functions
    before they are changed by execution.
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

      str_value= sp_get_item_value(thd, nctx->get_item(arg_no),
                                   &str_value_holder);

      if (str_value)
        binlog_buf.append(*str_value);
      else
        binlog_buf.append(STRING_WITH_LEN("NULL"));
    }
    binlog_buf.append(')');
  }
  thd->spcont= nctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx;
  if (set_routine_security_ctx(thd, this, FALSE, &save_security_ctx))
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
      variables). So we use artifical value which is guaranteed to
      be greater than all query_id's of all statements belonging
      to previous events/unions.
      Possible alternative to this is logging of all function invocations
      as one select and not resetting THD::user_var_events before
      each invocation.
    */
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    q= global_query_id;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    mysql_bin_log.start_union_events(thd, q + 1);
    binlog_save_options= thd->options;
    thd->options&= ~OPTION_BIN_LOG;
  }

  /*
    Switch to call arena/mem_root so objects like sp_cursor or
    Item_cache holders for case expressions can be allocated on it.

    TODO: In future we should associate call arena/mem_root with
          sp_rcontext and allocate all these objects (and sp_rcontext
          itself) on it directly rather than juggle with arenas.
  */
  thd->set_n_backup_active_arena(&call_arena, &backup_arena);

  err_status= execute(thd);

  thd->restore_active_arena(&call_arena, &backup_arena);

  if (need_binlog_call)
  {
    mysql_bin_log.stop_union_events(thd);
    thd->options= binlog_save_options;
    if (thd->binlog_evt_union.unioned_events)
    {
      int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
      Query_log_event qinfo(thd, binlog_buf.ptr(), binlog_buf.length(),
                            thd->binlog_evt_union.unioned_events_trans, FALSE, errcode);
      if (mysql_bin_log.write(&qinfo) &&
          thd->binlog_evt_union.unioned_events_trans)
      {
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
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

    if (!nctx->is_return_value_set())
    {
      my_error(ER_SP_NORETURNEND, MYF(0), m_name.str);
      err_status= TRUE;
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

err_with_cleanup:
  delete nctx;
  call_arena.free_items();
  free_root(&call_mem_root, MYF(0));
  thd->spcont= octx;

  DBUG_RETURN(err_status);
}


/**
  Execute a procedure. 

  The function does the following steps:
   - Set all parameters 
   - changes security context for SUID routines
   - call sp_head::execute
   - copy back values of INOUT and OUT parameters
   - restores security context

  @param thd    Thread handle
  @param args   List of values passed as arguments.

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_head::execute_procedure(THD *thd, List<Item> *args)
{
  bool err_status= FALSE;
  uint params = m_pcont->context_var_count();
  sp_rcontext *save_spcont, *octx;
  sp_rcontext *nctx = NULL;
  bool save_enable_slow_log= false;
  bool save_log_general= false;
  DBUG_ENTER("sp_head::execute_procedure");
  DBUG_PRINT("info", ("procedure %s", m_name.str));

  if (args->elements != params)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "PROCEDURE",
             m_qname.str, params, args->elements);
    DBUG_RETURN(TRUE);
  }

  save_spcont= octx= thd->spcont;
  if (! octx)
  {				// Create a temporary old context
    if (!(octx= new sp_rcontext(m_pcont, NULL, octx)) ||
        octx->init(thd))
    {
      delete octx; /* Delete octx if it was init() that failed. */
      DBUG_RETURN(TRUE);
    }
    
#ifndef DBUG_OFF
    octx->sp= 0;
#endif
    thd->spcont= octx;

    /* set callers_arena to thd, for upper-level function to work */
    thd->spcont->callers_arena= thd;
  }

  if (!(nctx= new sp_rcontext(m_pcont, NULL, octx)) ||
      nctx->init(thd))
  {
    delete nctx; /* Delete nctx if it was init() that failed. */
    thd->spcont= save_spcont;
    DBUG_RETURN(TRUE);
  }
#ifndef DBUG_OFF
  nctx->sp= this;
#endif

  if (params > 0)
  {
    List_iterator<Item> it_args(*args);

    DBUG_PRINT("info",(" %.*s: eval args", (int) m_name.length, m_name.str));

    for (uint i= 0 ; i < params ; i++)
    {
      Item *arg_item= it_args++;

      if (!arg_item)
        break;

      sp_variable_t *spvar= m_pcont->find_variable(i);

      if (!spvar)
        continue;

      if (spvar->mode != sp_param_in)
      {
        Settable_routine_parameter *srp=
          arg_item->get_settable_routine_parameter();

        if (!srp)
        {
          my_error(ER_SP_NOT_VAR_ARG, MYF(0), i+1, m_qname.str);
          err_status= TRUE;
          break;
        }

        srp->set_required_privilege(spvar->mode == sp_param_inout);
      }

      if (spvar->mode == sp_param_out)
      {
        Item_null *null_item= new Item_null();

        if (!null_item ||
            nctx->set_variable(thd, i, (Item **)&null_item))
        {
          err_status= TRUE;
          break;
        }
      }
      else
      {
        if (nctx->set_variable(thd, i, it_args.ref()))
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
    if (!thd->in_sub_stmt)
    {
      thd->lex->unit.cleanup();
      close_thread_tables(thd);            
      thd->rollback_item_tree_changes();
    }

    DBUG_PRINT("info",(" %.*s: eval args done", (int) m_name.length, 
                       m_name.str));
  }
  if (!(m_flags & LOG_SLOW_STATEMENTS) && thd->enable_slow_log)
  {
    DBUG_PRINT("info", ("Disabling slow log for the execution"));
    save_enable_slow_log= true;
    thd->enable_slow_log= FALSE;
  }
  if (!(m_flags & LOG_GENERAL_LOG) && !(thd->options & OPTION_LOG_OFF))
  {
    DBUG_PRINT("info", ("Disabling general log for the execution"));
    save_log_general= true;
    /* disable this bit */
    thd->options |= OPTION_LOG_OFF;
  }
  thd->spcont= nctx;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *save_security_ctx= 0;
  if (!err_status)
    err_status= set_routine_security_ctx(thd, this, TRUE, &save_security_ctx);
#endif

  if (!err_status)
    err_status= execute(thd);

  if (save_log_general)
    thd->options &= ~OPTION_LOG_OFF;
  if (save_enable_slow_log)
    thd->enable_slow_log= true;
  /*
    In the case when we weren't able to employ reuse mechanism for
    OUT/INOUT paranmeters, we should reallocate memory. This
    allocation should be done on the arena which will live through
    all execution of calling routine.
  */
  thd->spcont->callers_arena= octx->callers_arena;

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

      sp_variable_t *spvar= m_pcont->find_variable(i);

      if (spvar->mode == sp_param_in)
        continue;

      Settable_routine_parameter *srp=
        arg_item->get_settable_routine_parameter();

      DBUG_ASSERT(srp);

      if (srp->set_value(thd, octx, nctx->get_item_addr(i)))
      {
        err_status= TRUE;
        break;
      }
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (save_security_ctx)
    m_security_ctx.restore_security_context(thd, save_security_ctx);
#endif

  if (!save_spcont)
    delete octx;

  delete nctx;
  thd->spcont= save_spcont;
  DBUG_RETURN(err_status);
}


/**
  Reset lex during parsing, before we parse a sub statement.

  @param thd Thread handler.

  @return Error state
    @retval true An error occurred.
    @retval false Success.
*/

bool
sp_head::reset_lex(THD *thd)
{
  DBUG_ENTER("sp_head::reset_lex");
  LEX *sublex;
  LEX *oldlex= thd->lex;

  sublex= new (thd->mem_root)st_lex_local;
  if (sublex == 0)
    DBUG_RETURN(TRUE);

  thd->lex= sublex;
  (void)m_lex.push_front(oldlex);

  /* Reset most stuff. */
  lex_start(thd);

  /* And keep the SP stuff too */
  sublex->sphead= oldlex->sphead;
  sublex->spcont= oldlex->spcont;
  /* And trigger related stuff too */
  sublex->trg_chistics= oldlex->trg_chistics;
  sublex->trg_table_fields.empty();
  sublex->sp_lex_in_use= FALSE;

  /* Reset type info. */

  sublex->charset= NULL;
  sublex->length= NULL;
  sublex->dec= NULL;
  sublex->interval_list.empty();
  sublex->type= 0;

  DBUG_RETURN(FALSE);
}


/**
  Restore lex during parsing, after we have parsed a sub statement.

  @param thd Thread handle

  @return
    @retval TRUE failure
    @retval FALSE success
*/

bool
sp_head::restore_lex(THD *thd)
{
  DBUG_ENTER("sp_head::restore_lex");
  LEX *sublex= thd->lex;
  LEX *oldlex;

  sublex->set_trg_event_type_for_tables();

  oldlex= (LEX *)m_lex.pop();
  if (! oldlex)
    DBUG_RETURN(FALSE);			// Nothing to restore

  oldlex->trg_table_fields.push_back(&sublex->trg_table_fields);

  /*
    If this substatement needs row-based, the entire routine does too (we
    cannot switch from statement-based to row-based only for this
    substatement).
  */
  if (sublex->is_stmt_unsafe())
    m_flags|= BINLOG_ROW_BASED_IF_MIXED;

  /*
    Add routines which are used by statement to respective set for
    this routine.
  */
  if (sp_update_sp_used_routines(&m_sroutines, &sublex->sroutines))
    DBUG_RETURN(TRUE);
  /*
    Merge tables used by this statement (but not by its functions or
    procedures) to multiset of tables used by this routine.
  */
  merge_table_list(thd, sublex->query_tables, sublex);
  if (! sublex->sp_lex_in_use)
  {
    lex_end(sublex);
    delete sublex;
  }
  thd->lex= oldlex;
  DBUG_RETURN(FALSE);
}

/**
  Put the instruction on the backpatch list, associated with the label.
*/
int
sp_head::push_backpatch(sp_instr *i, sp_label_t *lab)
{
  bp_t *bp= (bp_t *)sql_alloc(sizeof(bp_t));

  if (!bp)
    return 1;
  bp->lab= lab;
  bp->instr= i;
  return m_backpatch.push_front(bp);
}

/**
  Update all instruction with this label in the backpatch list to
  the current position.
*/
void
sp_head::backpatch(sp_label_t *lab)
{
  bp_t *bp;
  uint dest= instructions();
  List_iterator_fast<bp_t> li(m_backpatch);

  DBUG_ENTER("sp_head::backpatch");
  while ((bp= li++))
  {
    if (bp->lab == lab)
    {
      DBUG_PRINT("info", ("backpatch: (m_ip %d, label 0x%lx <%s>) to dest %d",
                          bp->instr->m_ip, (ulong) lab, lab->name, dest));
      bp->instr->backpatch(dest, lab->ctx);
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Prepare an instance of Create_field for field creation (fill all necessary
  attributes).

  @param[in]  thd          Thread handle
  @param[in]  lex          Yacc parsing context
  @param[in]  field_type   Field type
  @param[out] field_def    An instance of create_field to be filled

  @retval
    FALSE  on success
  @retval
    TRUE   on error
*/

bool
sp_head::fill_field_definition(THD *thd, LEX *lex,
                               enum enum_field_types field_type,
                               Create_field *field_def)
{
  LEX_STRING cmt = { 0, 0 };
  uint unused1= 0;
  int unused2= 0;

  if (field_def->init(thd, (char*) "", field_type, lex->length, lex->dec,
                      lex->type, (Item*) 0, (Item*) 0, &cmt, 0,
                      &lex->interval_list,
                      lex->charset ? lex->charset :
                                     thd->variables.collation_database,
                      lex->uint_geom_type))
    return TRUE;

  if (field_def->interval_list.elements)
    field_def->interval= create_typelib(mem_root, field_def,
                                        &field_def->interval_list);

  sp_prepare_create_field(thd, field_def);

  if (prepare_create_field(field_def, &unused1, &unused2, &unused2,
                           HA_CAN_GEOMETRY))
  {
    return TRUE;
  }

  return FALSE;
}


int
sp_head::new_cont_backpatch(sp_instr_opt_meta *i)
{
  m_cont_level+= 1;
  if (i)
  {
    /* Use the cont. destination slot to store the level */
    i->m_cont_dest= m_cont_level;
    if (m_cont_backpatch.push_front(i))
      return 1;
  }
  return 0;
}

int
sp_head::add_cont_backpatch(sp_instr_opt_meta *i)
{
  i->m_cont_dest= m_cont_level;
  return m_cont_backpatch.push_front(i);
}

void
sp_head::do_cont_backpatch()
{
  uint dest= instructions();
  uint lev= m_cont_level--;
  sp_instr_opt_meta *i;

  while ((i= m_cont_backpatch.head()) && i->m_cont_dest == lev)
  {
    i->m_cont_dest= dest;
    (void)m_cont_backpatch.pop();
  }
}

void
sp_head::set_info(longlong created, longlong modified,
		  st_sp_chistics *chistics, ulong sql_mode)
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


void
sp_head::set_definer(const char *definer, uint definerlen)
{
  char user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING user_name= { user_name_holder, USERNAME_LENGTH };

  char host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING host_name= { host_name_holder, HOSTNAME_LENGTH };

  parse_user(definer, definerlen, user_name.str, &user_name.length,
             host_name.str, &host_name.length);

  set_definer(&user_name, &host_name);
}


void
sp_head::set_definer(const LEX_STRING *user_name, const LEX_STRING *host_name)
{
  m_definer_user.str= strmake_root(mem_root, user_name->str, user_name->length);
  m_definer_user.length= user_name->length;

  m_definer_host.str= strmake_root(mem_root, host_name->str, host_name->length);
  m_definer_host.length= host_name->length;
}


void
sp_head::reset_thd_mem_root(THD *thd)
{
  DBUG_ENTER("sp_head::reset_thd_mem_root");
  m_thd_root= thd->mem_root;
  thd->mem_root= &main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to thd mem root 0x%lx",
                      (ulong) &mem_root, (ulong) &thd->mem_root));
  free_list= thd->free_list; // Keep the old list
  thd->free_list= NULL;	// Start a new one
  m_thd= thd;
  DBUG_VOID_RETURN;
}

void
sp_head::restore_thd_mem_root(THD *thd)
{
  DBUG_ENTER("sp_head::restore_thd_mem_root");

  /*
   In some cases our parser detects a syntax error and calls
   LEX::cleanup_lex_after_parse_error() method only after
   finishing parsing the whole routine. In such a situation
   sp_head::restore_thd_mem_root() will be called twice - the
   first time as part of normal parsing process and the second
   time by cleanup_lex_after_parse_error().
   To avoid ruining active arena/mem_root state in this case we
   skip restoration of old arena/mem_root if this method has been
   already called for this routine.
  */
  if (!m_thd)
    DBUG_VOID_RETURN;

  Item *flist= free_list;	// The old list
  set_query_arena(thd);         // Get new free_list and mem_root
  state= INITIALIZED_FOR_SP;

  DBUG_PRINT("info", ("mem_root 0x%lx returned from thd mem root 0x%lx",
                      (ulong) &mem_root, (ulong) &thd->mem_root));
  thd->free_list= flist;	// Restore the old one
  thd->mem_root= m_thd_root;
  m_thd= NULL;
  DBUG_VOID_RETURN;
}


/**
  Check if a user has access right to a routine.

  @param thd		    Thread handler
  @param sp		    SP
  @param full_access       Set to 1 if the user has SELECT right to the
                           'mysql.proc' able or is the owner of the routine
  @retval
    false ok
  @retval
    true  error
*/

bool check_show_routine_access(THD *thd, sp_head *sp, bool *full_access)
{
  TABLE_LIST tables;
  bzero((char*) &tables,sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "proc";
  *full_access= ((!check_table_access(thd, SELECT_ACL, &tables, 1, TRUE) &&
                  (tables.grant.privilege & SELECT_ACL) != 0) ||
                 (!strcmp(sp->m_definer_user.str,
                          thd->security_ctx->priv_user) &&
                  !strcmp(sp->m_definer_host.str,
                          thd->security_ctx->priv_host)));
  if (!*full_access)
    return check_some_routine_access(thd, sp->m_db.str, sp->m_name.str,
                                     sp->m_type == TYPE_ENUM_PROCEDURE);
  return 0;
}


/**
  Implement SHOW CREATE statement for stored routines.

  @param thd  Thread context.
  @param type         Stored routine type
                      (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION)

  @return Error status.
    @retval FALSE on success
    @retval TRUE on error
*/

bool
sp_head::show_create_routine(THD *thd, int type)
{
  const char *col1_caption= type == TYPE_ENUM_PROCEDURE ?
                            "Procedure" : "Function";

  const char *col3_caption= type == TYPE_ENUM_PROCEDURE ?
                            "Create Procedure" : "Create Function";

  bool err_status;

  Protocol *protocol= thd->protocol;
  List<Item> fields;

  LEX_STRING sql_mode;

  bool full_access;

  DBUG_ENTER("sp_head::show_create_routine");
  DBUG_PRINT("info", ("routine %s", m_name.str));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);

  if (check_show_routine_access(thd, this, &full_access))
    DBUG_RETURN(TRUE);

  sys_var_thd_sql_mode::symbolic_mode_representation(
      thd, m_sql_mode, &sql_mode);

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
                            max(m_defstr.length, 1024));

    stmt_fld->maybe_null= TRUE;

    fields.push_back(stmt_fld);
  }

  fields.push_back(new Item_empty_string("character_set_client",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("collation_connection",
                                         MY_CS_NAME_SIZE));

  fields.push_back(new Item_empty_string("Database Collation",
                                         MY_CS_NAME_SIZE));

  if (protocol->send_fields(&fields,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    DBUG_RETURN(TRUE);
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

  DBUG_RETURN(err_status);
}




/**
  Add instruction to SP.

  @param instr   Instruction
*/

int sp_head::add_instr(sp_instr *instr)
{
  instr->free_list= m_thd->free_list;
  m_thd->free_list= 0;
  /*
    Memory root of every instruction is designated for permanent
    transformations (optimizations) made on the parsed tree during
    the first execution. It points to the memory root of the
    entire stored procedure, as their life span is equal.
  */
  instr->mem_root= &main_mem_root;
  return insert_dynamic(&m_instr, (uchar*)&instr);
}


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

void sp_head::optimize()
{
  List<sp_instr> bp;
  sp_instr *i;
  uint src, dst;

  opt_mark();

  bp.empty();
  src= dst= 0;
  while ((i= get_instr(src)))
  {
    if (! i->marked)
    {
      delete i;
      src+= 1;
    }
    else
    {
      if (src != dst)
      {                         // Move the instruction and update prev. jumps
	sp_instr *ibp;
	List_iterator_fast<sp_instr> li(bp);

	set_dynamic(&m_instr, (uchar*)&i, dst);
	while ((ibp= li++))
        {
          sp_instr_opt_meta *im= static_cast<sp_instr_opt_meta *>(ibp);
          im->set_destination(src, dst);
        }
      }
      i->opt_move(dst, &bp);
      src+= 1;
      dst+= 1;
    }
  }
  m_instr.elements= dst;
  bp.empty();
}

void sp_head::add_mark_lead(uint ip, List<sp_instr> *leads)
{
  sp_instr *i= get_instr(ip);

  if (i && ! i->marked)
    leads->push_front(i);
}

void
sp_head::opt_mark()
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
    while (i && ! i->marked)
    {
      ip= i->opt_mark(this, & leads);
      i= get_instr(ip);
    }
  }
}


#ifndef DBUG_OFF
/**
  Return the routine instructions as a result set.
  @return
    0 if ok, !=0 on error.
*/
int
sp_head::show_routine_code(THD *thd)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  List<Item> field_list;
  sp_instr *i;
  bool full_access;
  int res= 0;
  uint ip;
  DBUG_ENTER("sp_head::show_routine_code");
  DBUG_PRINT("info", ("procedure: %s", m_name.str));

  if (check_show_routine_access(thd, this, &full_access) || !full_access)
    DBUG_RETURN(1);

  field_list.push_back(new Item_uint("Pos", 9));
  // 1024 is for not to confuse old clients
  field_list.push_back(new Item_empty_string("Instruction",
					     max(buffer.length(), 1024)));
  if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
    DBUG_RETURN(1);

  for (ip= 0; (i = get_instr(ip)) ; ip++)
  {
    /* 
      Consistency check. If these are different something went wrong
      during optimization.
    */
    if (ip != i->m_ip)
    {
      const char *format= "Instruction at position %u has m_ip=%u";
      char tmp[sizeof(format) + 2*SP_INSTR_UINT_MAXLEN + 1];

      sprintf(tmp, format, ip, i->m_ip);
      /*
        Since this is for debugging purposes only, we don't bother to
        introduce a special error code for it.
      */
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR, tmp);
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

  DBUG_RETURN(res);
}
#endif // ifndef DBUG_OFF


/**
  Prepare LEX and thread for execution of instruction, if requested open
  and lock LEX's tables, execute instruction's core function, perform
  cleanup afterwards.

  @param thd           thread context
  @param nextp         out - next instruction
  @param open_tables   if TRUE then check read access to tables in LEX's table
                       list and open and lock them (used in instructions which
                       need to calculate some expression and don't execute
                       complete statement).
  @param sp_instr      instruction for which we prepare context, and which core
                       function execute by calling its exec_core() method.

  @note
    We are not saving/restoring some parts of THD which may need this because
    we do this once for whole routine execution in sp_head::execute().

  @return
    0/non-0 - Success/Failure
*/

int
sp_lex_keeper::reset_lex_and_exec_core(THD *thd, uint *nextp,
                                       bool open_tables, sp_instr* instr)
{
  int res= 0;
  DBUG_ENTER("reset_lex_and_exec_core");

  /* 
    The flag is saved at the entry to the following substatement.
    It's reset further in the common code part.
    It's merged with the saved parent's value at the exit of this func.
  */
  bool parent_modified_non_trans_table= thd->transaction.stmt.modified_non_trans_table;
  thd->transaction.stmt.modified_non_trans_table= FALSE;
  DBUG_ASSERT(!thd->derived_tables);
  DBUG_ASSERT(thd->change_list.is_empty());
  /*
    Use our own lex.
    We should not save old value since it is saved/restored in
    sp_head::execute() when we are entering/leaving routine.
  */
  thd->lex= m_lex;

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id= next_query_id();
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  if (thd->prelocked_mode == NON_PRELOCKED)
  {
    /*
      This statement will enter/leave prelocked mode on its own.
      Entering prelocked mode changes table list and related members
      of LEX, so we'll need to restore them.
    */
    if (lex_query_tables_own_last)
    {
      /*
        We've already entered/left prelocked mode with this statement.
        Attach the list of tables that need to be prelocked and mark m_lex
        as having such list attached.
      */
      *lex_query_tables_own_last= prelocking_tables;
      m_lex->mark_as_requiring_prelocking(lex_query_tables_own_last);
    }
  }

  reinit_stmt_before_use(thd, m_lex);

  if (open_tables)
    res= instr->exec_open_and_lock_tables(thd, m_lex->query_tables);

  if (!res)
  {
    res= instr->exec_core(thd, nextp);
    DBUG_PRINT("info",("exec_core returned: %d", res));
  }

  m_lex->unit.cleanup();

  thd_proc_info(thd, "closing tables");
  /* Here we also commit or rollback the current statement. */
  close_thread_tables(thd);
  thd_proc_info(thd, 0);

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
    lex_query_tables_own_last= m_lex->query_tables_own_last;
    prelocking_tables= *lex_query_tables_own_last;
    *lex_query_tables_own_last= NULL;
    m_lex->mark_as_requiring_prelocking(NULL);
  }
  thd->rollback_item_tree_changes();
  /*
    Update the state of the active arena if no errors on
    open_tables stage.
  */
  if (!res || !thd->is_error() ||
      (thd->main_da.sql_errno() != ER_CANT_REOPEN_TABLE &&
       thd->main_da.sql_errno() != ER_NO_SUCH_TABLE &&
       thd->main_da.sql_errno() != ER_UPDATE_TABLE_USED))
    thd->stmt_arena->state= Query_arena::EXECUTED;

  /*
    Merge here with the saved parent's values
    what is needed from the substatement gained
  */
  thd->transaction.stmt.modified_non_trans_table |= parent_modified_non_trans_table;
  /*
    Unlike for PS we should not call Item's destructors for newly created
    items after execution of each instruction in stored routine. This is
    because SP often create Item (like Item_int, Item_string etc...) when
    they want to store some value in local variable, pass return value and
    etc... So their life time should be longer than one instruction.

    cleanup_items() is called in sp_head::execute()
  */
  DBUG_RETURN(res || thd->is_error());
}


/*
  sp_instr class functions
*/

int sp_instr::exec_open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  int result;

  /*
    Check whenever we have access to tables for this statement
    and open and lock them before executing instructions core function.
  */
  if (check_table_access(thd, SELECT_ACL, tables, UINT_MAX, FALSE)
      || open_and_lock_tables(thd, tables))
    result= -1;
  else
    result= 0;

  return result;
}

uint sp_instr::get_cont_dest()
{
  return (m_ip+1);
}


int sp_instr::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(0);
  return 0;
}

/*
  sp_instr_stmt class functions
*/

int
sp_instr_stmt::execute(THD *thd, uint *nextp)
{
  char *query;
  uint32 query_length;
  int res;
  DBUG_ENTER("sp_instr_stmt::execute");
  DBUG_PRINT("info", ("command: %d", m_lex_keeper.sql_command()));

  query= thd->query();
  query_length= thd->query_length();
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  /* This s-p instr is profilable and will be captured. */
  thd->profiling.set_query_source(m_query.str, m_query.length);
#endif
  if (!(res= alloc_query(thd, m_query.str, m_query.length)) &&
      !(res=subst_spvars(thd, this, &m_query)))
  {
    /*
      (the order of query cache and subst_spvars calls is irrelevant because
      queries with SP vars can't be cached)
    */
    if (unlikely((thd->options & OPTION_LOG_OFF)==0))
      general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());

    if (query_cache_send_result_to_client(thd,
          thd->query(), 
          thd->query_length()) <= 0)
    {
      res= m_lex_keeper.reset_lex_and_exec_core(thd, nextp, FALSE, this);

      if (thd->main_da.is_eof())
        net_end_statement(thd);

      query_cache_end_of_result(thd);

      if (!res && unlikely(thd->enable_slow_log))
        log_slow_statement(thd);
    }
    else
      *nextp= m_ip+1;
    thd->set_query(query, query_length);
    thd->query_name_consts= 0;

    if (!thd->is_error())
      thd->main_da.reset_diagnostics_area();
  }
  DBUG_RETURN(res || thd->is_error());
}


void
sp_instr_stmt::print(String *str)
{
  uint i, len;

  /* stmt CMD "..." */
  if (str->reserve(SP_STMT_PRINT_MAXLEN+SP_INSTR_UINT_MAXLEN+8))
    return;
  str->qs_append(STRING_WITH_LEN("stmt "));
  str->qs_append((uint)m_lex_keeper.sql_command());
  str->qs_append(STRING_WITH_LEN(" \""));
  len= m_query.length;
  /*
    Print the query string (but not too much of it), just to indicate which
    statement it is.
  */
  if (len > SP_STMT_PRINT_MAXLEN)
    len= SP_STMT_PRINT_MAXLEN-3;
  /* Copy the query string and replace '\n' with ' ' in the process */
  for (i= 0 ; i < len ; i++)
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


int
sp_instr_stmt::exec_core(THD *thd, uint *nextp)
{
  int res= mysql_execute_command(thd);
  *nextp= m_ip+1;
  return res;
}


/*
  sp_instr_set class functions
*/

int
sp_instr_set::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_set::execute");
  DBUG_PRINT("info", ("offset: %u", m_offset));

  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_set::exec_core(THD *thd, uint *nextp)
{
  int res= thd->spcont->set_variable(thd, m_offset, &m_value);

  if (res && thd->spcont->found_handler_here())
  {
    /*
      Failed to evaluate the value, and a handler has been found. Reset the
      variable to NULL.
    */

    if (thd->spcont->set_variable(thd, m_offset, 0))
    {
      /* If this also failed, let's abort. */

      sp_rcontext *spcont= thd->spcont;
    
      thd->spcont= NULL;           /* Avoid handlers */
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
      spcont->clear_handler();
      thd->spcont= spcont;
    }
  }

  *nextp = m_ip+1;
  return res;
}

void
sp_instr_set::print(String *str)
{
  /* set name@offset ... */
  int rsrv = SP_INSTR_UINT_MAXLEN+6;
  sp_variable_t *var = m_ctx->find_variable(m_offset);

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
  m_value->print(str, QT_ORDINARY);
}


/*
  sp_instr_set_trigger_field class functions
*/

int
sp_instr_set_trigger_field::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_set_trigger_field::execute");
  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_set_trigger_field::exec_core(THD *thd, uint *nextp)
{
  const int res= (trigger_field->set_value(thd, &value) ? -1 : 0);
  *nextp = m_ip+1;
  return res;
}

void
sp_instr_set_trigger_field::print(String *str)
{
  str->append(STRING_WITH_LEN("set_trigger_field "));
  trigger_field->print(str, QT_ORDINARY);
  str->append(STRING_WITH_LEN(":="));
  value->print(str, QT_ORDINARY);
}

/*
  sp_instr_opt_meta
*/

uint sp_instr_opt_meta::get_cont_dest()
{
  return m_cont_dest;
}


/*
 sp_instr_jump class functions
*/

int
sp_instr_jump::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_jump::execute");
  DBUG_PRINT("info", ("destination: %u", m_dest));

  *nextp= m_dest;
  DBUG_RETURN(0);
}

void
sp_instr_jump::print(String *str)
{
  /* jump dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("jump "));
  str->qs_append(m_dest);
}

uint
sp_instr_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  m_dest= opt_shortcut_jump(sp, this);
  if (m_dest != m_ip+1)		/* Jumping to following instruction? */
    marked= 1;
  m_optdest= sp->get_instr(m_dest);
  return m_dest;
}

uint
sp_instr_jump::opt_shortcut_jump(sp_head *sp, sp_instr *start)
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

void
sp_instr_jump::opt_move(uint dst, List<sp_instr> *bp)
{
  if (m_dest > m_ip)
    bp->push_back(this);	// Forward
  else if (m_optdest)
    m_dest= m_optdest->m_ip;	// Backward
  m_ip= dst;
}


/*
  sp_instr_jump_if_not class functions
*/

int
sp_instr_jump_if_not::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_jump_if_not::execute");
  DBUG_PRINT("info", ("destination: %u", m_dest));
  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_jump_if_not::exec_core(THD *thd, uint *nextp)
{
  Item *it;
  int res;

  it= sp_prepare_func_item(thd, &m_expr);
  if (! it)
  {
    res= -1;
  }
  else
  {
    res= 0;
    if (! it->val_bool())
      *nextp = m_dest;
    else
      *nextp = m_ip+1;
  }

  return res;
}


void
sp_instr_jump_if_not::print(String *str)
{
  /* jump_if_not dest(cont) ... */
  if (str->reserve(2*SP_INSTR_UINT_MAXLEN+14+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("jump_if_not "));
  str->qs_append(m_dest);
  str->qs_append('(');
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  m_expr->print(str, QT_ORDINARY);
}


uint
sp_instr_jump_if_not::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_dest)))
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }
  sp->add_mark_lead(m_dest, leads);
  if ((i= sp->get_instr(m_cont_dest)))
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }
  sp->add_mark_lead(m_cont_dest, leads);
  return m_ip+1;
}

void
sp_instr_jump_if_not::opt_move(uint dst, List<sp_instr> *bp)
{
  /*
    cont. destinations may point backwards after shortcutting jumps
    during the mark phase. If it's still pointing forwards, only
    push this for backpatching if sp_instr_jump::opt_move() will not
    do it (i.e. if the m_dest points backwards).
   */
  if (m_cont_dest > m_ip)
  {                             // Forward
    if (m_dest < m_ip)
      bp->push_back(this);
  }
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->m_ip; // Backward
  /* This will take care of m_dest and m_ip */
  sp_instr_jump::opt_move(dst, bp);
}


/*
  sp_instr_freturn class functions
*/

int
sp_instr_freturn::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_freturn::execute");
  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_freturn::exec_core(THD *thd, uint *nextp)
{
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

  return thd->spcont->set_return_value(thd, &m_value);
}

void
sp_instr_freturn::print(String *str)
{
  /* freturn type expr... */
  if (str->reserve(1024+8+32)) // Add some for the expr. too
    return;
  str->qs_append(STRING_WITH_LEN("freturn "));
  str->qs_append((uint)m_type);
  str->qs_append(' ');
  m_value->print(str, QT_ORDINARY);
}

/*
  sp_instr_hpush_jump class functions
*/

int
sp_instr_hpush_jump::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_hpush_jump::execute");
  List_iterator_fast<sp_cond_type_t> li(m_cond);
  sp_cond_type_t *p;

  while ((p= li++))
    thd->spcont->push_handler(p, m_ip+1, m_type, m_frame);

  *nextp= m_dest;
  DBUG_RETURN(0);
}


void
sp_instr_hpush_jump::print(String *str)
{
  /* hpush_jump dest fsize type */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 21))
    return;
  str->qs_append(STRING_WITH_LEN("hpush_jump "));
  str->qs_append(m_dest);
  str->qs_append(' ');
  str->qs_append(m_frame);
  switch (m_type) {
  case SP_HANDLER_NONE:
    str->qs_append(STRING_WITH_LEN(" NONE")); // This would be a bug
    break;
  case SP_HANDLER_EXIT:
    str->qs_append(STRING_WITH_LEN(" EXIT"));
    break;
  case SP_HANDLER_CONTINUE:
    str->qs_append(STRING_WITH_LEN(" CONTINUE"));
    break;
  case SP_HANDLER_UNDO:
    str->qs_append(STRING_WITH_LEN(" UNDO"));
    break;
  default:
    // This would be a bug as well
    str->qs_append(STRING_WITH_LEN(" UNKNOWN:"));
    str->qs_append(m_type);
  }
}


uint
sp_instr_hpush_jump::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_dest)))
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
  if (m_type == SP_HANDLER_CONTINUE)
  {
    for (uint scope_ip= m_dest+1; scope_ip <= m_opt_hpop; scope_ip++)
      sp->add_mark_lead(scope_ip, leads);
  }

  return m_ip+1;
}


/*
  sp_instr_hpop class functions
*/

int
sp_instr_hpop::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_hpop::execute");
  thd->spcont->pop_handlers(m_count);
  *nextp= m_ip+1;
  DBUG_RETURN(0);
}

void
sp_instr_hpop::print(String *str)
{
  /* hpop count */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("hpop "));
  str->qs_append(m_count);
}


/*
  sp_instr_hreturn class functions
*/

int
sp_instr_hreturn::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_hreturn::execute");
  if (m_dest)
    *nextp= m_dest;
  else
  {
    *nextp= thd->spcont->pop_hstack();
  }
  thd->spcont->exit_handler();
  DBUG_RETURN(0);
}


void
sp_instr_hreturn::print(String *str)
{
  /* hreturn framesize dest */
  if (str->reserve(SP_INSTR_UINT_MAXLEN*2 + 9))
    return;
  str->qs_append(STRING_WITH_LEN("hreturn "));
  str->qs_append(m_frame);
  if (m_dest)
  {
    str->qs_append(' ');
    str->qs_append(m_dest);
  }
}


uint
sp_instr_hreturn::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  marked= 1;
  
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


/*
  sp_instr_cpush class functions
*/

int
sp_instr_cpush::execute(THD *thd, uint *nextp)
{
  Query_arena backup_arena;
  DBUG_ENTER("sp_instr_cpush::execute");

  /*
    We should create cursors in the callers arena, as
    it could be (and usually is) used in several instructions.
  */
  thd->set_n_backup_active_arena(thd->spcont->callers_arena, &backup_arena);

  thd->spcont->push_cursor(&m_lex_keeper, this);

  thd->restore_active_arena(thd->spcont->callers_arena, &backup_arena);

  *nextp= m_ip+1;

  DBUG_RETURN(0);
}


void
sp_instr_cpush::print(String *str)
{
  LEX_STRING n;
  my_bool found= m_ctx->find_cursor(m_cursor, &n);
  /* cpush name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+7;

  if (found)
    rsrv+= n.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cpush "));
  if (found)
  {
    str->qs_append(n.str, n.length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor);
}


/*
  sp_instr_cpop class functions
*/

int
sp_instr_cpop::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_cpop::execute");
  thd->spcont->pop_cursors(m_count);
  *nextp= m_ip+1;
  DBUG_RETURN(0);
}


void
sp_instr_cpop::print(String *str)
{
  /* cpop count */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+5))
    return;
  str->qs_append(STRING_WITH_LEN("cpop "));
  str->qs_append(m_count);
}


/*
  sp_instr_copen class functions
*/

/**
  @todo
    Assert that we either have an error or a cursor
*/

int
sp_instr_copen::execute(THD *thd, uint *nextp)
{
  /*
    We don't store a pointer to the cursor in the instruction to be
    able to reuse the same instruction among different threads in future.
  */
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res;
  DBUG_ENTER("sp_instr_copen::execute");

  if (! c)
    res= -1;
  else
  {
    sp_lex_keeper *lex_keeper= c->get_lex_keeper();
    Query_arena *old_arena= thd->stmt_arena;

    /*
      Get the Query_arena from the cpush instruction, which contains
      the free_list of the query, so new items (if any) are stored in
      the right free_list, and we can cleanup after each open.
    */
    thd->stmt_arena= c->get_instr();
    res= lex_keeper->reset_lex_and_exec_core(thd, nextp, FALSE, this);
    /* Cleanup the query's items */
    if (thd->stmt_arena->free_list)
      cleanup_items(thd->stmt_arena->free_list);
    thd->stmt_arena= old_arena;
    /*
      Work around the fact that errors in selects are not returned properly
      (but instead converted into a warning), so if a condition handler
      caught, we have lost the result code.
    */
    if (!res)
    {
      uint dummy1, dummy2;

      if (thd->spcont->found_handler(&dummy1, &dummy2))
        res= -1;
    }
    /* TODO: Assert here that we either have an error or a cursor */
  }
  DBUG_RETURN(res);
}


int
sp_instr_copen::exec_core(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res= c->open(thd);
  *nextp= m_ip+1;
  return res;
}

void
sp_instr_copen::print(String *str)
{
  LEX_STRING n;
  my_bool found= m_ctx->find_cursor(m_cursor, &n);
  /* copen name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+7;

  if (found)
    rsrv+= n.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("copen "));
  if (found)
  {
    str->qs_append(n.str, n.length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor);
}


/*
  sp_instr_cclose class functions
*/

int
sp_instr_cclose::execute(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res;
  DBUG_ENTER("sp_instr_cclose::execute");

  if (! c)
    res= -1;
  else
    res= c->close(thd);
  *nextp= m_ip+1;
  DBUG_RETURN(res);
}


void
sp_instr_cclose::print(String *str)
{
  LEX_STRING n;
  my_bool found= m_ctx->find_cursor(m_cursor, &n);
  /* cclose name@offset */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (found)
    rsrv+= n.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cclose "));
  if (found)
  {
    str->qs_append(n.str, n.length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor);
}


/*
  sp_instr_cfetch class functions
*/

int
sp_instr_cfetch::execute(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res;
  Query_arena backup_arena;
  DBUG_ENTER("sp_instr_cfetch::execute");

  res= c ? c->fetch(thd, &m_varlist) : -1;

  *nextp= m_ip+1;
  DBUG_RETURN(res);
}


void
sp_instr_cfetch::print(String *str)
{
  List_iterator_fast<struct sp_variable> li(m_varlist);
  sp_variable_t *pv;
  LEX_STRING n;
  my_bool found= m_ctx->find_cursor(m_cursor, &n);
  /* cfetch name@offset vars... */
  uint rsrv= SP_INSTR_UINT_MAXLEN+8;

  if (found)
    rsrv+= n.length;
  if (str->reserve(rsrv))
    return;
  str->qs_append(STRING_WITH_LEN("cfetch "));
  if (found)
  {
    str->qs_append(n.str, n.length);
    str->qs_append('@');
  }
  str->qs_append(m_cursor);
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


/*
  sp_instr_error class functions
*/

int
sp_instr_error::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_error::execute");

  my_message(m_errcode, ER(m_errcode), MYF(0));
  *nextp= m_ip+1;
  DBUG_RETURN(-1);
}


void
sp_instr_error::print(String *str)
{
  /* error code */
  if (str->reserve(SP_INSTR_UINT_MAXLEN+6))
    return;
  str->qs_append(STRING_WITH_LEN("error "));
  str->qs_append(m_errcode);
}


/**************************************************************************
  sp_instr_set_case_expr class implementation
**************************************************************************/

int
sp_instr_set_case_expr::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_set_case_expr::execute");

  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_set_case_expr::exec_core(THD *thd, uint *nextp)
{
  int res= thd->spcont->set_case_expr(thd, m_case_expr_id, &m_case_expr);

  if (res &&
      !thd->spcont->get_case_expr(m_case_expr_id) &&
      thd->spcont->found_handler_here())
  {
    /*
      Failed to evaluate the value, the case expression is still not
      initialized, and a handler has been found. Set to NULL so we can continue.
    */

    Item *null_item= new Item_null();
    
    if (!null_item ||
        thd->spcont->set_case_expr(thd, m_case_expr_id, &null_item))
    {
      /* If this also failed, we have to abort. */

      sp_rcontext *spcont= thd->spcont;
    
      thd->spcont= NULL;           /* Avoid handlers */
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
      spcont->clear_handler();
      thd->spcont= spcont;
    }
  }
  else
    *nextp= m_ip+1;

  return res;
}


void
sp_instr_set_case_expr::print(String *str)
{
  /* set_case_expr (cont) id ... */
  str->reserve(2*SP_INSTR_UINT_MAXLEN+18+32); // Add some extra for expr too
  str->qs_append(STRING_WITH_LEN("set_case_expr ("));
  str->qs_append(m_cont_dest);
  str->qs_append(STRING_WITH_LEN(") "));
  str->qs_append(m_case_expr_id);
  str->qs_append(' ');
  m_case_expr->print(str, QT_ORDINARY);
}

uint
sp_instr_set_case_expr::opt_mark(sp_head *sp, List<sp_instr> *leads)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_cont_dest)))
  {
    m_cont_dest= i->opt_shortcut_jump(sp, this);
    m_cont_optdest= sp->get_instr(m_cont_dest);
  }
  sp->add_mark_lead(m_cont_dest, leads);
  return m_ip+1;
}

void
sp_instr_set_case_expr::opt_move(uint dst, List<sp_instr> *bp)
{
  if (m_cont_dest > m_ip)
    bp->push_back(this);        // Forward
  else if (m_cont_optdest)
    m_cont_dest= m_cont_optdest->m_ip; // Backward
  m_ip= dst;
}


/* ------------------------------------------------------------------ */


/*
  Structure that represent all instances of one table
  in optimized multi-set of tables used by routine.
*/

typedef struct st_sp_table
{
  /*
    Multi-set key:
      db_name\0table_name\0alias\0 - for normal tables
      db_name\0table_name\0        - for temporary tables
  */
  LEX_STRING qname;
  uint db_length, table_name_length;
  bool temp;               /* true if corresponds to a temporary table */
  thr_lock_type lock_type; /* lock type used for prelocking */
  uint lock_count;
  uint query_lock_count;
  uint8 trg_event_map;
} SP_TABLE;


uchar *sp_table_key(const uchar *ptr, size_t *plen, my_bool first)
{
  SP_TABLE *tab= (SP_TABLE *)ptr;
  *plen= tab->qname.length;
  return (uchar *)tab->qname.str;
}


/**
  Merge the list of tables used by some query into the multi-set of
  tables used by routine.

  @param thd                 thread context
  @param table               table list
  @param lex_for_tmp_check   LEX of the query for which we are merging
                             table list.

  @note
    This method will use LEX provided to check whenever we are creating
    temporary table and mark it as such in target multi-set.

  @retval
    TRUE    Success
  @retval
    FALSE   Error
*/

bool
sp_head::merge_table_list(THD *thd, TABLE_LIST *table, LEX *lex_for_tmp_check)
{
  SP_TABLE *tab;

  if (lex_for_tmp_check->sql_command == SQLCOM_DROP_TABLE &&
      lex_for_tmp_check->drop_temporary)
    return TRUE;

  for (uint i= 0 ; i < m_sptabs.records ; i++)
  {
    tab= (SP_TABLE *)hash_element(&m_sptabs, i);
    tab->query_lock_count= 0;
  }

  for (; table ; table= table->next_global)
    if (!table->derived && !table->schema_table)
    {
      /*
        Structure of key for the multi-set is "db\0table\0alias\0".
        Since "alias" part can have arbitrary length we use String
        object to construct the key. By default String will use
        buffer allocated on stack with NAME_LEN bytes reserved for
        alias, since in most cases it is going to be smaller than
        NAME_LEN bytes.
      */
      char tname_buff[(NAME_LEN + 1) * 3];
      String tname(tname_buff, sizeof(tname_buff), &my_charset_bin);
      uint temp_table_key_length;

      tname.length(0);
      tname.append(table->db, table->db_length);
      tname.append('\0');
      tname.append(table->table_name, table->table_name_length);
      tname.append('\0');
      temp_table_key_length= tname.length();
      tname.append(table->alias);
      tname.append('\0');

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
      if ((tab= (SP_TABLE *)hash_search(&m_sptabs, (uchar *)tname.ptr(),
                                        tname.length())) ||
          ((tab= (SP_TABLE *)hash_search(&m_sptabs, (uchar *)tname.ptr(),
                                         temp_table_key_length)) &&
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
	  return FALSE;
	if (lex_for_tmp_check->sql_command == SQLCOM_CREATE_TABLE &&
	    lex_for_tmp_check->query_tables == table &&
	    lex_for_tmp_check->create_info.options & HA_LEX_CREATE_TMP_TABLE)
        {
	  tab->temp= TRUE;
          tab->qname.length= temp_table_key_length;
        }
        else
          tab->qname.length= tname.length();
        tab->qname.str= (char*) thd->memdup(tname.ptr(), tab->qname.length);
        if (!tab->qname.str)
          return FALSE;
        tab->table_name_length= table->table_name_length;
        tab->db_length= table->db_length;
        tab->lock_type= table->lock_type;
        tab->lock_count= tab->query_lock_count= 1;
        tab->trg_event_map= table->trg_event_map;
	if (my_hash_insert(&m_sptabs, (uchar *)tab))
          return FALSE;
      }
    }
  return TRUE;
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
    0 if none.

  @retval
    TRUE    if some elements were added
  @retval
    FALSE   otherwise.
*/

bool
sp_head::add_used_tables_to_table_list(THD *thd,
                                       TABLE_LIST ***query_tables_last_ptr,
                                       TABLE_LIST *belong_to_view)
{
  uint i;
  Query_arena *arena, backup;
  bool result= FALSE;
  DBUG_ENTER("sp_head::add_used_tables_to_table_list");

  /*
    Use persistent arena for table list allocation to be PS/SP friendly.
    Note that we also have to copy database/table names and alias to PS/SP
    memory since current instance of sp_head object can pass away before
    next execution of PS/SP for which tables are added to prelocking list.
    This will be fixed by introducing of proper invalidation mechanism
    once new TDC is ready.
  */
  arena= thd->activate_stmt_arena_if_needed(&backup);

  for (i=0 ; i < m_sptabs.records ; i++)
  {
    char *tab_buff, *key_buff;
    TABLE_LIST *table;
    SP_TABLE *stab= (SP_TABLE *)hash_element(&m_sptabs, i);
    if (stab->temp)
      continue;

    if (!(tab_buff= (char *)thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST)) *
                                        stab->lock_count)) ||
        !(key_buff= (char*)thd->memdup(stab->qname.str,
                                       stab->qname.length)))
      DBUG_RETURN(FALSE);

    for (uint j= 0; j < stab->lock_count; j++)
    {
      table= (TABLE_LIST *)tab_buff;

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

      /* Everyting else should be zeroed */

      **query_tables_last_ptr= table;
      table->prev_global= *query_tables_last_ptr;
      *query_tables_last_ptr= &table->next_global;

      tab_buff+= ALIGN_SIZE(sizeof(TABLE_LIST));
      result= TRUE;
    }
  }

  if (arena)
    thd->restore_active_arena(arena, &backup);

  DBUG_RETURN(result);
}


/**
  Simple function for adding an explicetly named (systems) table to
  the global table list, e.g. "mysql", "proc".
*/

TABLE_LIST *
sp_add_to_query_tables(THD *thd, LEX *lex,
		       const char *db, const char *name,
		       thr_lock_type locktype)
{
  TABLE_LIST *table;

  if (!(table= (TABLE_LIST *)thd->calloc(sizeof(TABLE_LIST))))
  {
    thd->fatal_error();
    return NULL;
  }
  table->db_length= strlen(db);
  table->db= thd->strmake(db, table->db_length);
  table->table_name_length= strlen(name);
  table->table_name= thd->strmake(name, table->table_name_length);
  table->alias= thd->strdup(name);
  table->lock_type= locktype;
  table->select_lex= lex->current_select;
  table->cacheable_table= 1;
  
  lex->add_to_query_tables(table);
  return table;
}

