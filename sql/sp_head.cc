/* Copyright (C) 2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef __GNUC__
#pragma implementation
#endif

#include "mysql_priv.h"
#include "sp_head.h"
#include "sp.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp_cache.h"

Item_result
sp_map_result_type(enum enum_field_types type)
{
  switch (type)
  {
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

/*
 * Returns TRUE if the 'cmd' is a command that might result in
 * multiple result sets being sent back.
 * Note: This does not include SQLCOM_SELECT which is treated
 *       separately in sql_yacc.yy.
 */
bool
sp_multi_results_command(enum enum_sql_command cmd)
{
  switch (cmd) {
  case SQLCOM_ANALYZE:
  case SQLCOM_CHECKSUM:
  case SQLCOM_HA_READ:
  case SQLCOM_SHOW_BINLOGS:
  case SQLCOM_SHOW_BINLOG_EVENTS:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_COLUMN_TYPES:
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_CREATE_DB:
  case SQLCOM_SHOW_CREATE_FUNC:
  case SQLCOM_SHOW_CREATE_PROC:
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_ERRORS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_GRANTS:
  case SQLCOM_SHOW_INNODB_STATUS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_LOGS:
  case SQLCOM_SHOW_MASTER_STAT:
  case SQLCOM_SHOW_MUTEX_STATUS:
  case SQLCOM_SHOW_NEW_MASTER:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_PRIVILEGES:
  case SQLCOM_SHOW_PROCESSLIST:
  case SQLCOM_SHOW_SLAVE_HOSTS:
  case SQLCOM_SHOW_SLAVE_STAT:
  case SQLCOM_SHOW_STATUS:
  case SQLCOM_SHOW_STATUS_FUNC:
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_WARNS:
    return TRUE;
  default:
    return FALSE;
  }
}


/*
  Prepare Item for execution (call of fix_fields)

  SYNOPSIS
    sp_prepare_func_item()
    thd       thread handler
    it_addr   pointer on item refernce

  RETURN
    NULL  error
    prepared item
*/

static Item *
sp_prepare_func_item(THD* thd, Item **it_addr)
{
  Item *it= *it_addr;
  DBUG_ENTER("sp_prepare_func_item");
  it_addr= it->this_item_addr(thd, it_addr);

  if (!it->fixed && (*it_addr)->fix_fields(thd, 0, it_addr))
  {
    DBUG_PRINT("info", ("fix_fields() failed"));
    DBUG_RETURN(NULL);
  }
  DBUG_RETURN(*it_addr);
}


/* Evaluate a (presumed) func item. Always returns an item, the parameter
** if nothing else.
*/
Item *
sp_eval_func_item(THD *thd, Item **it_addr, enum enum_field_types type,
		  Item *reuse)
{
  DBUG_ENTER("sp_eval_func_item");
  Item *it= sp_prepare_func_item(thd, it_addr);
  uint rsize;
  DBUG_PRINT("info", ("type: %d", type));

  if (!it)
  {
    DBUG_RETURN(NULL);
  }

  /* QQ How do we do this? Is there some better way? */
  if (type == MYSQL_TYPE_NULL)
    it= new(reuse, &rsize) Item_null();
  else
  {
    switch (sp_map_result_type(type)) {
    case INT_RESULT:
      {
	longlong i= it->val_int();

	if (it->null_value)
	{
	  DBUG_PRINT("info", ("INT_RESULT: null"));
	  it= new(reuse, &rsize) Item_null();
	}
	else
	{
	  DBUG_PRINT("info", ("INT_RESULT: %d", i));
          it= new(reuse, &rsize) Item_int(i);
	}
	break;
      }
    case REAL_RESULT:
      {
	double d= it->val_real();

	if (it->null_value)
	{
	  DBUG_PRINT("info", ("REAL_RESULT: null"));
	  it= new(reuse, &rsize) Item_null();
	}
	else
	{
	  /* There's some difference between Item::new_item() and the
	   * constructor; the former crashes, the latter works... weird. */
	  uint8 decimals= it->decimals;
	  uint32 max_length= it->max_length;
	  DBUG_PRINT("info", ("REAL_RESULT: %g", d));
          it= new(reuse, &rsize) Item_float(d);
	  it->decimals= decimals;
	  it->max_length= max_length;
	}
	break;
      }
    case DECIMAL_RESULT:
      {
        my_decimal value, *val= it->val_decimal(&value);
        if (it->null_value)
          it= new(reuse, &rsize) Item_null();
        else
          it= new(reuse, &rsize) Item_decimal(val);
#ifndef DBUG_OFF
        char dbug_buff[DECIMAL_MAX_STR_LENGTH+1];
        DBUG_PRINT("info", ("DECIMAL_RESULT: %s", dbug_decimal_as_string(dbug_buff, val)));
#endif
        break;
      }
    case STRING_RESULT:
      {
	char buffer[MAX_FIELD_WIDTH];
	String tmp(buffer, sizeof(buffer), it->collation.collation);
	String *s= it->val_str(&tmp);

	if (it->null_value)
	{
	  DBUG_PRINT("info", ("default result: null"));
	  it= new(reuse, &rsize) Item_null();
	}
	else
	{
	  DBUG_PRINT("info",("default result: %*s",
                             s->length(), s->c_ptr_quick()));
	  it= new(reuse, &rsize) Item_string(thd->strmake(s->ptr(),
							  s->length()),
					     s->length(),
					     it->collation.collation);
	}
	break;
      }
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0);
    }
  }
  it->rsize= rsize;

  DBUG_RETURN(it);
}


/*
 *
 *  sp_name
 *
 */

void
sp_name::init_qname(THD *thd)
{
  m_qname.length= m_db.length+m_name.length+1;
  m_qname.str= thd->alloc(m_qname.length+1);
  sprintf(m_qname.str, "%*s.%*s",
	  m_db.length, (m_db.length ? m_db.str : ""),
	  m_name.length, m_name.str);
}

sp_name *
sp_name_current_db_new(THD *thd, LEX_STRING name)
{
  sp_name *qname;

  if (! thd->db)
    qname= new sp_name(name);
  else
  {
    LEX_STRING db;

    db.length= strlen(thd->db);
    db.str= thd->strmake(thd->db, db.length);
    qname= new sp_name(db, name);
  }
  qname->init_qname(thd);
  return qname;
}


/* ------------------------------------------------------------------ */


/*
 *
 *  sp_head
 *
 */

void *
sp_head::operator new(size_t size)
{
  DBUG_ENTER("sp_head::operator new");
  MEM_ROOT own_root;
  sp_head *sp;

  init_alloc_root(&own_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  sp= (sp_head *) alloc_root(&own_root, size);
  sp->main_mem_root= own_root;
  DBUG_PRINT("info", ("mem_root 0x%lx", (ulong) &sp->mem_root));
  DBUG_RETURN(sp);
}

void 
sp_head::operator delete(void *ptr, size_t size)
{
  DBUG_ENTER("sp_head::operator delete");
  MEM_ROOT own_root;
  sp_head *sp= (sp_head *) ptr;

  /* Make a copy of main_mem_root as free_root will free the sp */
  own_root= sp->main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to 0x%lx",
                      (ulong) &sp->mem_root, (ulong) &own_root));
  free_root(&own_root, MYF(0));

  DBUG_VOID_RETURN;
}


sp_head::sp_head()
  :Item_arena((bool)FALSE), m_returns_cs(NULL), m_has_return(FALSE),
   m_simple_case(FALSE), m_multi_results(FALSE), m_in_handler(FALSE)
{
  extern byte *
    sp_table_key(const byte *ptr, uint *plen, my_bool first);
  extern byte 
    *sp_lex_sp_key(const byte *ptr, uint *plen, my_bool first);
  DBUG_ENTER("sp_head::sp_head");

  state= INITIALIZED;
  m_backpatch.empty();
  m_lex.empty();
  hash_init(&m_sptabs, system_charset_info, 0, 0, 0, sp_table_key, 0, 0);
  hash_init(&m_spfuns, system_charset_info, 0, 0, 0, sp_lex_sp_key, 0, 0);
  hash_init(&m_spprocs, system_charset_info, 0, 0, 0, sp_lex_sp_key, 0, 0);
  DBUG_VOID_RETURN;
}


void
sp_head::init(LEX *lex)
{
  DBUG_ENTER("sp_head::init");

  lex->spcont= m_pcont= new sp_pcontext(NULL);
  /*
    Altough trg_table_fields list is used only in triggers we init for all
    types of stored procedures to simplify reset_lex()/restore_lex() code.
  */
  lex->trg_table_fields.empty();
  my_init_dynamic_array(&m_instr, sizeof(sp_instr *), 16, 8);
  m_param_begin= m_param_end= m_body_begin= 0;
  m_qname.str= m_db.str= m_name.str= m_params.str= 
    m_body.str= m_defstr.str= 0;
  m_qname.length= m_db.length= m_name.length= m_params.length=
    m_body.length= m_defstr.length= 0;
  m_returns_cs= NULL;
  DBUG_VOID_RETURN;
}

void
sp_head::init_strings(THD *thd, LEX *lex, sp_name *name)
{
  DBUG_ENTER("sp_head::init_strings");
  uint n;			/* Counter for nul trimming */ 
  /* During parsing, we must use thd->mem_root */
  MEM_ROOT *root= thd->mem_root;

  /* We have to copy strings to get them into the right memroot */
  if (name)
  {
    m_db.length= name->m_db.length;
    if (name->m_db.length == 0)
      m_db.str= NULL;
    else
      m_db.str= strmake_root(root, name->m_db.str, name->m_db.length);
    m_name.length= name->m_name.length;
    m_name.str= strmake_root(root, name->m_name.str, name->m_name.length);

    if (name->m_qname.length == 0)
      name->init_qname(thd);
    m_qname.length= name->m_qname.length;
    m_qname.str= strmake_root(root, name->m_qname.str, m_qname.length);
  }
  else if (thd->db)
  {
    m_db.length= thd->db_length;
    m_db.str= strmake_root(root, thd->db, m_db.length);
  }

  if (m_param_begin && m_param_end)
  {
    m_params.length= m_param_end - m_param_begin;
    m_params.str= strmake_root(root,
                               (char *)m_param_begin, m_params.length);
  }

  m_body.length= lex->ptr - m_body_begin;
  /* Trim nuls at the end */
  n= 0;
  while (m_body.length && m_body_begin[m_body.length-1] == '\0')
  {
    m_body.length-= 1;
    n+= 1;
  }
  m_body.str= strmake_root(root, (char *)m_body_begin, m_body.length);
  m_defstr.length= lex->ptr - lex->buf;
  m_defstr.length-= n;
  m_defstr.str= strmake_root(root, (char *)lex->buf, m_defstr.length);
  DBUG_VOID_RETURN;
}

TYPELIB *
sp_head::create_typelib(List<String> *src)
{
  TYPELIB *result= NULL;
  CHARSET_INFO *cs= m_returns_cs;
  DBUG_ENTER("sp_head::clone_typelib");
  if (src->elements)
  {
    result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
    result->count= src->elements;
    result->name= "";
    if (!(result->type_names=(const char **)
          alloc_root(mem_root,(sizeof(char *)+sizeof(int))*(result->count+1))))
      return 0;
    result->type_lengths= (unsigned int *)(result->type_names + result->count+1);
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
  return result;
}

int
sp_head::create(THD *thd)
{
  DBUG_ENTER("sp_head::create");
  int ret;

  DBUG_PRINT("info", ("type: %d name: %s params: %s body: %s",
		      m_type, m_name.str, m_params.str, m_body.str));

#ifndef DBUG_OFF
  optimize();
  {
    String s;
    sp_instr *i;
    uint ip= 0;
    while ((i = get_instr(ip)))
    {
      char buf[8];

      sprintf(buf, "%4u: ", ip);
      s.append(buf);
      i->print(&s);
      s.append('\n');
      ip+= 1;
    }
    s.append('\0');
    DBUG_PRINT("info", ("Code %s\n%s", m_qname.str, s.ptr()));
  }
#endif

  if (m_type == TYPE_ENUM_FUNCTION)
    ret= sp_create_function(thd, this);
  else
    ret= sp_create_procedure(thd, this);

  DBUG_RETURN(ret);
}

sp_head::~sp_head()
{
  destroy();
  if (m_thd)
    restore_thd_mem_root(m_thd);
}

void
sp_head::destroy()
{
  sp_instr *i;
  LEX *lex;
  DBUG_ENTER("sp_head::destroy");
  DBUG_PRINT("info", ("name: %s", m_name.str));

  for (uint ip = 0 ; (i = get_instr(ip)) ; ip++)
    delete i;
  delete_dynamic(&m_instr);
  m_pcont->destroy();
  free_items(free_list);

  /*
    If we have non-empty LEX stack then we just came out of parser with
    error. Now we should delete all auxilary LEXes and restore original
    THD::lex (In this case sp_head::restore_thd_mem_root() was not called
    too, so m_thd points to the current thread context).
    It is safe to not update LEX::ptr because further query string parsing
    and execution will be stopped anyway.
  */
  DBUG_ASSERT(m_lex.is_empty() || m_thd);
  while ((lex= (LEX *)m_lex.pop()))
  {
    delete m_thd->lex;
    m_thd->lex= lex;
  }

  hash_free(&m_sptabs);
  hash_free(&m_spfuns);
  hash_free(&m_spprocs);
  DBUG_VOID_RETURN;
}


Field *
sp_head::make_field(uint max_length, const char *name, TABLE *dummy)
{
  Field *field;
  DBUG_ENTER("sp_head::make_field");
  field= ::make_field((char *)0,
		!m_returns_len ? max_length : m_returns_len, 
		(uchar *)"", 0, m_returns_pack, m_returns, m_returns_cs,
		(enum Field::geometry_type)0, Field::NONE, 
		m_returns_typelib,
		name ? name : (const char *)m_name.str, dummy);
  DBUG_RETURN(field);
}

int
sp_head::execute(THD *thd)
{
  DBUG_ENTER("sp_head::execute");
  char olddb[128];
  bool dbchanged;
  sp_rcontext *ctx;
  int ret= 0;
  uint ip= 0;
  Item_arena *old_arena;
  query_id_t old_query_id;
  TABLE *old_derived_tables;
  LEX *old_lex;
  Item_change_list old_change_list;
  String old_packet;

  /* Use some extra margin for possible SP recursion and functions */
  if (check_stack_overrun(thd, 4*STACK_MIN_SIZE))
  {
    DBUG_RETURN(-1);
  }

  dbchanged= FALSE;
  if (m_db.length &&
      (ret= sp_use_new_db(thd, m_db.str, olddb, sizeof(olddb), 0, &dbchanged)))
    goto done;

  if ((ctx= thd->spcont))
    ctx->clear_handler();
  thd->query_error= 0;
  old_arena= thd->current_arena;
  thd->current_arena= this;

  /*
    We have to save/restore this info when we are changing call level to
    be able properly do close_thread_tables() in instructions.
  */
  old_query_id= thd->query_id;
  old_derived_tables= thd->derived_tables;
  thd->derived_tables= 0;
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

  do
  {
    sp_instr *i;
    uint hip;			// Handler ip

    i = get_instr(ip);	// Returns NULL when we're done.
    if (i == NULL)
      break;
    DBUG_PRINT("execute", ("Instruction %u", ip));
    thd->set_time();		// Make current_time() et al work
    ret= i->execute(thd, &ip);
    if (i->free_list)
      cleanup_items(i->free_list);
    // Check if an exception has occurred and a handler has been found
    // Note: We havo to check even if ret==0, since warnings (and some
    //       errors don't return a non-zero value.
    //       We also have to check even if thd->killed != 0, since some
    //       errors return with this even when a handler has been found
    //       (e.g. "bad data").
    if (ctx)
    {
      uint hf;

      switch (ctx->found_handler(&hip, &hf))
      {
      case SP_HANDLER_NONE:
	break;
      case SP_HANDLER_CONTINUE:
	ctx->save_variables(hf);
	ctx->push_hstack(ip);
        // Fall through
      default:
	ip= hip;
	ret= 0;
	ctx->clear_handler();
	ctx->in_handler= TRUE;
        thd->clear_error();
	thd->killed= THD::NOT_KILLED;
	continue;
      }
    }
  } while (ret == 0 && !thd->killed);

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

  cleanup_items(thd->current_arena->free_list);
  thd->current_arena= old_arena;

 done:
  DBUG_PRINT("info", ("ret=%d killed=%d query_error=%d",
		      ret, thd->killed, thd->query_error));

  if (thd->killed)
    ret= -1;
  /* If the DB has changed, the pointer has changed too, but the
     original thd->db will then have been freed */
  if (dbchanged)
  {
    if (! thd->killed)
      ret= sp_change_db(thd, olddb, 0);
  }
  DBUG_RETURN(ret);
}


int
sp_head::execute_function(THD *thd, Item **argp, uint argcount, Item **resp)
{
  DBUG_ENTER("sp_head::execute_function");
  DBUG_PRINT("info", ("function %s", m_name.str));
  uint csize = m_pcont->max_pvars();
  uint params = m_pcont->current_pvars();
  uint hmax = m_pcont->max_handlers();
  uint cmax = m_pcont->max_cursors();
  sp_rcontext *octx = thd->spcont;
  sp_rcontext *nctx = NULL;
  uint i;
  int ret;

  if (argcount != params)
  {
    // Need to use my_printf_error here, or it will not terminate the
    // invoking query properly.
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0),
             "FUNCTION", m_qname.str, params, argcount);
    DBUG_RETURN(-1);
  }

  // QQ Should have some error checking here? (types, etc...)
  nctx= new sp_rcontext(csize, hmax, cmax);
  for (i= 0 ; i < params && i < argcount ; i++)
  {
    sp_pvar_t *pvar = m_pcont->find_pvar(i);
    Item *it= sp_eval_func_item(thd, argp++, pvar->type, NULL);

    if (it)
      nctx->push_item(it);
    else
    {
      DBUG_RETURN(-1);
    }
  }
  // The rest of the frame are local variables which are all IN.
  // Default all variables to null (those with default clauses will
  // be set by an set instruction).
  {
    Item_null *nit= NULL;	// Re-use this, and only create if needed
    for (; i < csize ; i++)
    {
      if (! nit)
	nit= new Item_null();
      nctx->push_item(nit);
    }
  }
  thd->spcont= nctx;

  ret= execute(thd);

  if (m_type == TYPE_ENUM_FUNCTION && ret == 0)
  {
    /* We need result only in function but not in trigger */
    Item *it= nctx->get_result();

    if (it)
      *resp= it;
    else
    {
      my_error(ER_SP_NORETURNEND, MYF(0), m_name.str);
      ret= -1;
    }
  }

  nctx->pop_all_cursors();	// To avoid memory leaks after an error
  thd->spcont= octx;
  DBUG_RETURN(ret);
}

static Item_func_get_user_var *
item_is_user_var(Item *it)
{
  if (it->type() == Item::FUNC_ITEM)
  {
    Item_func *fi= static_cast<Item_func*>(it);

    if (fi->functype() == Item_func::GUSERVAR_FUNC)
      return static_cast<Item_func_get_user_var*>(fi);
  }
  return NULL;
}

int
sp_head::execute_procedure(THD *thd, List<Item> *args)
{
  DBUG_ENTER("sp_head::execute_procedure");
  DBUG_PRINT("info", ("procedure %s", m_name.str));
  int ret= 0;
  uint csize = m_pcont->max_pvars();
  uint params = m_pcont->current_pvars();
  uint hmax = m_pcont->max_handlers();
  uint cmax = m_pcont->max_cursors();
  sp_rcontext *octx = thd->spcont;
  sp_rcontext *nctx = NULL;
  my_bool tmp_octx = FALSE;	// True if we have allocated a temporary octx

  if (args->elements != params)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "PROCEDURE",
             m_qname.str, params, args->elements);
    DBUG_RETURN(-1);
  }

  if (csize > 0 || hmax > 0 || cmax > 0)
  {
    Item_null *nit= NULL;	// Re-use this, and only create if needed
    uint i;
    List_iterator<Item> li(*args);
    Item *it;

    nctx= new sp_rcontext(csize, hmax, cmax);
    if (! octx)
    {				// Create a temporary old context
      octx= new sp_rcontext(csize, hmax, cmax);
      tmp_octx= TRUE;
    }
    // QQ: Should do type checking?
    for (i = 0 ; (it= li++) && i < params ; i++)
    {
      sp_pvar_t *pvar= m_pcont->find_pvar(i);

      if (pvar)
      {
	if (pvar->mode != sp_param_in)
	{
	  if (!it->is_splocal() && !item_is_user_var(it))
	  {
	    my_error(ER_SP_NOT_VAR_ARG, MYF(0), i+1, m_qname.str);
	    ret= -1;
	    break;
	  }
	}
	if (pvar->mode == sp_param_out)
	{
	  if (! nit)
	    nit= new Item_null();
	  nctx->push_item(nit); // OUT
	}
	else
	{
	  Item *it2= sp_eval_func_item(thd, li.ref(), pvar->type, NULL);

	  if (it2)
	    nctx->push_item(it2); // IN or INOUT
	  else
	  {
	    ret= -1;		// Eval failed
	    break;
	  }
	}
      }
    }

    // The rest of the frame are local variables which are all IN.
    // Default all variables to null (those with default clauses will
    // be set by an set instruction).
    for (; i < csize ; i++)
    {
      if (! nit)
	nit= new Item_null();
      nctx->push_item(nit);
    }
    thd->spcont= nctx;
  }

  if (! ret)
    ret= execute(thd);

  if (!ret && csize > 0)
  {
    List_iterator_fast<Item> li(*args);
    Item *it;

    // Copy back all OUT or INOUT values to the previous frame, or
    // set global user variables
    for (uint i = 0 ; (it= li++) && i < params ; i++)
    {
      sp_pvar_t *pvar= m_pcont->find_pvar(i);

      if (pvar->mode != sp_param_in)
      {
	if (it->is_splocal())
	  octx->set_item(static_cast<Item_splocal *>(it)->get_offset(),
			 nctx->get_item(i));
	else
	{
	  Item_func_get_user_var *guv= item_is_user_var(it);

	  if (guv)
	  {
	    Item *item= nctx->get_item(i);
	    Item_func_set_user_var *suv;

	    suv= new Item_func_set_user_var(guv->get_name(), item);
	    /*
	      we do not check suv->fixed, because it can't be fixed after
	      creation
	    */
	    suv->fix_fields(thd, NULL, &item);
	    suv->fix_length_and_dec();
	    suv->check();
	    suv->update();
	  }
	}
      }
    }
  }

  if (tmp_octx)
    octx= NULL;
  if (nctx)
    nctx->pop_all_cursors();	// To avoid memory leaks after an error
  thd->spcont= octx;

  DBUG_RETURN(ret);
}


// Reset lex during parsing, before we parse a sub statement.
void
sp_head::reset_lex(THD *thd)
{
  DBUG_ENTER("sp_head::reset_lex");
  LEX *sublex;
  LEX *oldlex= thd->lex;

  (void)m_lex.push_front(oldlex);
  thd->lex= sublex= new st_lex;

  /* Reset most stuff. The length arguments doesn't matter here. */
  lex_start(thd, oldlex->buf, (ulong) (oldlex->end_of_query - oldlex->ptr));

  /* We must reset ptr and end_of_query again */
  sublex->ptr= oldlex->ptr;
  sublex->end_of_query= oldlex->end_of_query;
  sublex->tok_start= oldlex->tok_start;
  sublex->yylineno= oldlex->yylineno;
  /* And keep the SP stuff too */
  sublex->sphead= oldlex->sphead;
  sublex->spcont= oldlex->spcont;
  /* And trigger related stuff too */
  sublex->trg_chistics= oldlex->trg_chistics;
  sublex->trg_table_fields.empty();
  sublex->sp_lex_in_use= FALSE;
  DBUG_VOID_RETURN;
}

// Restore lex during parsing, after we have parsed a sub statement.
void
sp_head::restore_lex(THD *thd)
{
  DBUG_ENTER("sp_head::restore_lex");
  LEX *sublex= thd->lex;
  LEX *oldlex= (LEX *)m_lex.pop();

  init_stmt_after_parse(thd, sublex);
  if (! oldlex)
    return;			// Nothing to restore

  // Update some state in the old one first
  oldlex->ptr= sublex->ptr;
  oldlex->next_state= sublex->next_state;
  oldlex->trg_table_fields.push_back(&sublex->trg_table_fields);

  /*
    Add routines which are used by statement to respective sets for
    this routine
  */
  sp_merge_hash(&m_spfuns, &sublex->spfuns);
  sp_merge_hash(&m_spprocs, &sublex->spprocs);
  /*
    Merge tables used by this statement (but not by its functions or
    procedures) to multiset of tables used by this routine.
  */
  merge_table_list(thd, sublex->query_tables, sublex);
  if (! sublex->sp_lex_in_use)
    delete sublex;
  thd->lex= oldlex;
  DBUG_VOID_RETURN;
}

void
sp_head::push_backpatch(sp_instr *i, sp_label_t *lab)
{
  bp_t *bp= (bp_t *)sql_alloc(sizeof(bp_t));

  if (bp)
  {
    bp->lab= lab;
    bp->instr= i;
    (void)m_backpatch.push_front(bp);
  }
}

void
sp_head::backpatch(sp_label_t *lab)
{
  bp_t *bp;
  uint dest= instructions();
  List_iterator_fast<bp_t> li(m_backpatch);

  while ((bp= li++))
  {
    if (bp->lab == lab ||
	(bp->lab->type == SP_LAB_REF &&
	 my_strcasecmp(system_charset_info, bp->lab->name, lab->name) == 0))
    {
      if (bp->lab->type != SP_LAB_REF)
	bp->instr->backpatch(dest, lab->ctx);
      else
      {
	sp_label_t *dstlab= bp->lab->ctx->find_label(lab->name);

	if (dstlab)
	{
	  bp->lab= lab;
	  bp->instr->backpatch(dest, dstlab->ctx);
	}
      }
    }
  }
}

int
sp_head::check_backpatch(THD *thd)
{
  bp_t *bp;
  List_iterator_fast<bp_t> li(m_backpatch);

  while ((bp= li++))
  {
    if (bp->lab->type == SP_LAB_REF)
    {
      my_error(ER_SP_LILABEL_MISMATCH, MYF(0), "GOTO", bp->lab->name);
      return -1;
    }
  }
  return 0;
}

void
sp_head::set_info(char *definer, uint definerlen,
		  longlong created, longlong modified,
		  st_sp_chistics *chistics, ulong sql_mode)
{
  char *p= strchr(definer, '@');
  uint len;

  if (! p)
    p= definer;		// Weird...
  len= p-definer;
  m_definer_user.str= strmake_root(mem_root, definer, len);
  m_definer_user.length= len;
  len= definerlen-len-1;
  m_definer_host.str= strmake_root(mem_root, p+1, len);
  m_definer_host.length= len;
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
sp_head::reset_thd_mem_root(THD *thd)
{
  DBUG_ENTER("sp_head::reset_thd_mem_root");
  m_thd_root= thd->mem_root;
  thd->mem_root= &main_mem_root;
  DBUG_PRINT("info", ("mem_root 0x%lx moved to thd mem root 0x%lx",
                      (ulong) &mem_root, (ulong) &thd->mem_root));
  free_list= thd->free_list; // Keep the old list
  thd->free_list= NULL;	// Start a new one
  /* Copy the db, since substatements will point to it */
  m_thd_db= thd->db;
  thd->db= thd->strmake(thd->db, thd->db_length);
  m_thd= thd;
  DBUG_VOID_RETURN;
}

void
sp_head::restore_thd_mem_root(THD *thd)
{
  DBUG_ENTER("sp_head::restore_thd_mem_root");
  Item *flist= free_list;	// The old list
  set_item_arena(thd);          // Get new free_list and mem_root
  state= INITIALIZED;

  DBUG_PRINT("info", ("mem_root 0x%lx returned from thd mem root 0x%lx",
                      (ulong) &mem_root, (ulong) &thd->mem_root));
  thd->free_list= flist;	// Restore the old one
  thd->db= m_thd_db;		// Restore the original db pointer
  thd->mem_root= m_thd_root;
  m_thd= NULL;
  DBUG_VOID_RETURN;
}


/*
  Check if a user has access right to a routine

  SYNOPSIS
    check_show_routine_access()
    thd			Thread handler
    sp			SP
    full_access		Set to 1 if the user has SELECT right to the
			'mysql.proc' able or is the owner of the routine
  RETURN
    0  ok
    1  error
*/

bool check_show_routine_access(THD *thd, sp_head *sp, bool *full_access)
{
  TABLE_LIST tables;
  bzero((char*) &tables,sizeof(tables));
  tables.db= (char*) "mysql";
  tables.table_name= tables.alias= (char*) "proc";
  *full_access= (!check_table_access(thd, SELECT_ACL, &tables, 1) ||
                 (!strcmp(sp->m_definer_user.str, thd->priv_user) &&
                  !strcmp(sp->m_definer_host.str, thd->priv_host)));
  if (!*full_access)
    return check_some_routine_access(thd, sp->m_db.str, sp->m_name.str,
                                     sp->m_type == TYPE_ENUM_PROCEDURE);
  return 0;
}


int
sp_head::show_create_procedure(THD *thd)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  int res;
  List<Item> field_list;
  ulong old_sql_mode;
  sys_var *sql_mode_var;
  byte *sql_mode_str;
  ulong sql_mode_len;
  bool full_access;

  DBUG_ENTER("sp_head::show_create_procedure");
  DBUG_PRINT("info", ("procedure %s", m_name.str));
  LINT_INIT(sql_mode_str);
  LINT_INIT(sql_mode_len);

  if (check_show_routine_access(thd, this, &full_access))
    return 1;
  
  old_sql_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= m_sql_mode;
  sql_mode_var= find_sys_var("SQL_MODE", 8);
  if (sql_mode_var)
  {
    sql_mode_str= sql_mode_var->value_ptr(thd, OPT_SESSION, 0);
    sql_mode_len= strlen((char*) sql_mode_str);
  }

  field_list.push_back(new Item_empty_string("Procedure", NAME_LEN));
  if (sql_mode_var)
    field_list.push_back(new Item_empty_string("sql_mode", sql_mode_len));
  // 1024 is for not to confuse old clients
  field_list.push_back(new Item_empty_string("Create Procedure",
					     max(buffer.length(), 1024)));
  if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                         Protocol::SEND_EOF))
  {
    res= 1;
    goto done;
  }
  protocol->prepare_for_resend();
  protocol->store(m_name.str, m_name.length, system_charset_info);
  if (sql_mode_var)
    protocol->store((char*) sql_mode_str, sql_mode_len, system_charset_info);
  if (full_access)
    protocol->store(m_defstr.str, m_defstr.length, system_charset_info);
  res= protocol->write();
  send_eof(thd);

 done:
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(res);
}


/*
  Add instruction to SP

  SYNOPSIS
    sp_head::add_instr()
    instr   Instruction
*/

void sp_head::add_instr(sp_instr *instr)
{
  instr->free_list= m_thd->free_list;
  m_thd->free_list= 0;
  insert_dynamic(&m_instr, (gptr)&instr);
}


int
sp_head::show_create_function(THD *thd)
{
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  int res;
  List<Item> field_list;
  ulong old_sql_mode;
  sys_var *sql_mode_var;
  byte *sql_mode_str;
  ulong sql_mode_len;
  bool full_access;
  DBUG_ENTER("sp_head::show_create_function");
  DBUG_PRINT("info", ("procedure %s", m_name.str));
  LINT_INIT(sql_mode_str);
  LINT_INIT(sql_mode_len);

  if (check_show_routine_access(thd, this, &full_access))
    return 1;

  old_sql_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= m_sql_mode;
  sql_mode_var= find_sys_var("SQL_MODE", 8);
  if (sql_mode_var)
  {
    sql_mode_str= sql_mode_var->value_ptr(thd, OPT_SESSION, 0);
    sql_mode_len= strlen((char*) sql_mode_str);
  }

  field_list.push_back(new Item_empty_string("Function",NAME_LEN));
  if (sql_mode_var)
    field_list.push_back(new Item_empty_string("sql_mode", sql_mode_len));
  field_list.push_back(new Item_empty_string("Create Function",
					     max(buffer.length(),1024)));
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    res= 1;
    goto done;
  }
  protocol->prepare_for_resend();
  protocol->store(m_name.str, m_name.length, system_charset_info);
  if (sql_mode_var)
    protocol->store((char*) sql_mode_str, sql_mode_len, system_charset_info);
  if (full_access)
    protocol->store(m_defstr.str, m_defstr.length, system_charset_info);
  res= protocol->write();
  send_eof(thd);

 done:
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(res);
}

void
sp_head::optimize()
{
  List<sp_instr> bp;
  sp_instr *i;
  uint src, dst;

  opt_mark(0);

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
      {
	sp_instr *ibp;
	List_iterator_fast<sp_instr> li(bp);

	set_dynamic(&m_instr, (gptr)&i, dst);
	while ((ibp= li++))
	{
	  sp_instr_jump *ji= static_cast<sp_instr_jump *>(ibp);
	  if (ji->m_dest == src)
	    ji->m_dest= dst;
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

void
sp_head::opt_mark(uint ip)
{
  sp_instr *i;

  while ((i= get_instr(ip)) && !i->marked)
    ip= i->opt_mark(this);
}

// ------------------------------------------------------------------


/*
  Prepare LEX and thread for execution of instruction, if requested open
  and lock LEX's tables, execute instruction's core function, perform
  cleanup afterwards.

  SYNOPSIS
    reset_lex_and_exec_core()
      thd         - thread context
      nextp       - out - next instruction
      open_tables - if TRUE then check read access to tables in LEX's table
                    list and open and lock them (used in instructions which
                    need to calculate some expression and don't execute
                    complete statement).
      sp_instr    - instruction for which we prepare context, and which core
                    function execute by calling its exec_core() method.

  NOTE
    We are not saving/restoring some parts of THD which may need this because
    we do this once for whole routine execution in sp_head::execute().

  RETURN VALUE
    0/non-0 - Success/Failure
*/

int
sp_lex_keeper::reset_lex_and_exec_core(THD *thd, uint *nextp,
                                       bool open_tables, sp_instr* instr)
{
  int res= 0;

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

  /*
    FIXME. Resetting statement (and using it) is not reentrant, thus recursive
           functions which try to use the same LEX twice will crash server.
           We should prevent such situations by tracking if LEX is already
           in use and throwing error about unallowed recursion if needed.
           OTOH it is nice to allow recursion in cases when LEX is not really
           used (e.g. in mathematical functions), so such tracking should be
           implemented at the same time as ability not to store LEX for
           instruction if it is not really used.
  */
  reset_stmt_for_execute(thd, m_lex);

  /*
    If requested check whenever we have access to tables in LEX's table list
    and open and lock them before executing instructtions core function.
  */
  if (open_tables &&
      (check_table_access(thd, SELECT_ACL, m_lex->query_tables, 0) ||
       open_and_lock_tables(thd, m_lex->query_tables)))
      res= -1;

  if (!res)
    res= instr->exec_core(thd, nextp);

  m_lex->unit.cleanup();

  thd->proc_info="closing tables";
  close_thread_tables(thd);

  thd->rollback_item_tree_changes();

  /*
    Unlike for PS we should not call Item's destructors for newly created
    items after execution of each instruction in stored routine. This is
    because SP often create Item (like Item_int, Item_string etc...) when
    they want to store some value in local variable, pass return value and
    etc... So their life time should be longer than one instruction.

    Probably we can call destructors for most of them then we are leaving
    routine. But this won't help much as they are allocated in main query
    MEM_ROOT anyway. So they all go to global thd->free_list.

    May be we can use some other MEM_ROOT for this purprose ???

    What else should we do for cleanup ?
    cleanup_items() is called in sp_head::execute()
  */
  return res;
}


//
// sp_instr
//
int sp_instr::exec_core(THD *thd, uint *nextp)
{
  DBUG_ASSERT(0);
  return 0;
}


//
// sp_instr_stmt
//
int
sp_instr_stmt::execute(THD *thd, uint *nextp)
{
  char *query;
  uint32 query_length;
  DBUG_ENTER("sp_instr_stmt::execute");
  DBUG_PRINT("info", ("command: %d", m_lex_keeper.sql_command()));
  int res;

  query= thd->query;
  query_length= thd->query_length;
  if (!(res= alloc_query(thd, m_query.str, m_query.length+1)))
  {
    if (query_cache_send_result_to_client(thd,
					  thd->query, thd->query_length) <= 0)
    {
      res= m_lex_keeper.reset_lex_and_exec_core(thd, nextp, FALSE, this);
      query_cache_end_of_result(thd);
    }
    else
      *nextp= m_ip+1;
    thd->query= query;
    thd->query_length= query_length;
  }
  DBUG_RETURN(res);
}

void
sp_instr_stmt::print(String *str)
{
  str->reserve(12);
  str->append("stmt ");
  str->qs_append((uint)m_lex_keeper.sql_command());
}


int
sp_instr_stmt::exec_core(THD *thd, uint *nextp)
{
  int res= mysql_execute_command(thd);
  *nextp= m_ip+1;
  return res;
}

//
// sp_instr_set
//
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
  int res= thd->spcont->set_item_eval(thd, m_offset, &m_value, m_type);

  *nextp = m_ip+1;
  return res;
}

void
sp_instr_set::print(String *str)
{
  str->reserve(12);
  str->append("set ");
  str->qs_append(m_offset);
  str->append(' ');
  m_value->print(str);
}


//
// sp_instr_set_trigger_field
//
int
sp_instr_set_trigger_field::execute(THD *thd, uint *nextp)
{
  int res= 0;

  DBUG_ENTER("sp_instr_set_trigger_field::execute");
  /* QQ: Still unsure what should we return in case of error 1 or -1 ? */
  if (!value->fixed && value->fix_fields(thd, 0, &value) ||
      trigger_field.fix_fields(thd, 0, 0) ||
      (value->save_in_field(trigger_field.field, 0) < 0))
    res= -1;
  *nextp= m_ip + 1;
  DBUG_RETURN(res);
}

void
sp_instr_set_trigger_field::print(String *str)
{
  str->append("set ", 4);
  trigger_field.print(str);
  str->append(":=", 2);
  value->print(str);
}

//
// sp_instr_jump
//
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
  str->reserve(12);
  str->append("jump ");
  str->qs_append(m_dest);
}

uint
sp_instr_jump::opt_mark(sp_head *sp)
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

//
// sp_instr_jump_if
//

int
sp_instr_jump_if::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_jump_if::execute");
  DBUG_PRINT("info", ("destination: %u", m_dest));
  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}

int
sp_instr_jump_if::exec_core(THD *thd, uint *nextp)
{
  Item *it;
  int res;

  it= sp_prepare_func_item(thd, &m_expr);
  if (!it)
    res= -1;
  else
  {
    res= 0;
    if (it->val_bool())
      *nextp = m_dest;
    else
      *nextp = m_ip+1;
  }

  return res;
}

void
sp_instr_jump_if::print(String *str)
{
  str->reserve(12);
  str->append("jump_if ");
  str->qs_append(m_dest);
  str->append(' ');
  m_expr->print(str);
}

uint
sp_instr_jump_if::opt_mark(sp_head *sp)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_dest)))
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }
  sp->opt_mark(m_dest);
  return m_ip+1;
}

//
// sp_instr_jump_if_not
//
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
    res= -1;
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
  str->reserve(16);
  str->append("jump_if_not ");
  str->qs_append(m_dest);
  str->append(' ');
  m_expr->print(str);
}

uint
sp_instr_jump_if_not::opt_mark(sp_head *sp)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_dest)))
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }
  sp->opt_mark(m_dest);
  return m_ip+1;
}

//
// sp_instr_freturn
//

int
sp_instr_freturn::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_freturn::execute");
  DBUG_RETURN(m_lex_keeper.reset_lex_and_exec_core(thd, nextp, TRUE, this));
}


int
sp_instr_freturn::exec_core(THD *thd, uint *nextp)
{
  Item *it;
  int res;

  it= sp_eval_func_item(thd, &m_value, m_type, NULL);
  if (! it)
    res= -1;
  else
  {
    res= 0;
    thd->spcont->set_result(it);
  }
  *nextp= UINT_MAX;

  return res;
}

void
sp_instr_freturn::print(String *str)
{
  str->reserve(12);
  str->append("freturn ");
  str->qs_append((uint)m_type);
  str->append(' ');
  m_value->print(str);
}

//
// sp_instr_hpush_jump
//
int
sp_instr_hpush_jump::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_hpush_jump::execute");
  List_iterator_fast<sp_cond_type_t> li(m_cond);
  sp_cond_type_t *p;

  while ((p= li++))
    thd->spcont->push_handler(p, m_handler, m_type, m_frame);

  *nextp= m_dest;
  DBUG_RETURN(0);
}

void
sp_instr_hpush_jump::print(String *str)
{
  str->reserve(32);
  str->append("hpush_jump ");
  str->qs_append(m_dest);
  str->append(" t=");
  str->qs_append(m_type);
  str->append(" f=");
  str->qs_append(m_frame);
  str->append(" h=");
  str->qs_append(m_handler);
}

uint
sp_instr_hpush_jump::opt_mark(sp_head *sp)
{
  sp_instr *i;

  marked= 1;
  if ((i= sp->get_instr(m_dest)))
  {
    m_dest= i->opt_shortcut_jump(sp, this);
    m_optdest= sp->get_instr(m_dest);
  }
  sp->opt_mark(m_dest);
  return m_ip+1;
}

//
// sp_instr_hpop
//
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
  str->reserve(12);
  str->append("hpop ");
  str->qs_append(m_count);
}

void
sp_instr_hpop::backpatch(uint dest, sp_pcontext *dst_ctx)
{
  m_count= m_ctx->diff_handlers(dst_ctx);
}


//
// sp_instr_hreturn
//
int
sp_instr_hreturn::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_hreturn::execute");
  if (m_dest)
    *nextp= m_dest;
  else
  {
    thd->spcont->restore_variables(m_frame);
    *nextp= thd->spcont->pop_hstack();
  }
  thd->spcont->in_handler= FALSE;
  DBUG_RETURN(0);
}

void
sp_instr_hreturn::print(String *str)
{
  str->reserve(16);
  str->append("hreturn ");
  str->qs_append(m_frame);
  if (m_dest)
    str->qs_append(m_dest);
}

uint
sp_instr_hreturn::opt_mark(sp_head *sp)
{
  if (m_dest)
    return sp_instr_jump::opt_mark(sp);
  else
  {
    marked= 1;
    return UINT_MAX;
  }
}


//
// sp_instr_cpush
//
int
sp_instr_cpush::execute(THD *thd, uint *nextp)
{
  DBUG_ENTER("sp_instr_cpush::execute");
  thd->spcont->push_cursor(&m_lex_keeper);
  *nextp= m_ip+1;
  DBUG_RETURN(0);
}

void
sp_instr_cpush::print(String *str)
{
  str->append("cpush");
}

//
// sp_instr_cpop
//
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
  str->reserve(12);
  str->append("cpop ");
  str->qs_append(m_count);
}

void
sp_instr_cpop::backpatch(uint dest, sp_pcontext *dst_ctx)
{
  m_count= m_ctx->diff_cursors(dst_ctx);
}

//
// sp_instr_copen
//
int
sp_instr_copen::execute(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res;
  DBUG_ENTER("sp_instr_copen::execute");

  if (! c)
    res= -1;
  else
  {
    sp_lex_keeper *lex_keeper= c->pre_open(thd);

    if (!lex_keeper)
    {
      res= -1;
      *nextp= m_ip+1;
    }
    else
      res= lex_keeper->reset_lex_and_exec_core(thd, nextp, FALSE, this);

    c->post_open(thd, (lex_keeper ? TRUE : FALSE));
  }

  DBUG_RETURN(res);
}

int
sp_instr_copen::exec_core(THD *thd, uint *nextp)
{
  int res= mysql_execute_command(thd);
  *nextp= m_ip+1;
  return res;
}

void
sp_instr_copen::print(String *str)
{
  str->reserve(12);
  str->append("copen ");
  str->qs_append(m_cursor);
}

//
// sp_instr_cclose
//
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
  str->reserve(12);
  str->append("cclose ");
  str->qs_append(m_cursor);
}

//
// sp_instr_cfetch
//
int
sp_instr_cfetch::execute(THD *thd, uint *nextp)
{
  sp_cursor *c= thd->spcont->get_cursor(m_cursor);
  int res;
  DBUG_ENTER("sp_instr_cfetch::execute");

  if (! c)
    res= -1;
  else
    res= c->fetch(thd, &m_varlist);
  *nextp= m_ip+1;
  DBUG_RETURN(res);
}

void
sp_instr_cfetch::print(String *str)
{
  List_iterator_fast<struct sp_pvar> li(m_varlist);
  sp_pvar_t *pv;

  str->reserve(12);
  str->append("cfetch ");
  str->qs_append(m_cursor);
  while ((pv= li++))
  {
    str->reserve(8);
    str->append(' ');
    str->qs_append(pv->offset);
  }
}

//
// sp_instr_error
//
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
  str->reserve(12);
  str->append("error ");
  str->qs_append(m_errcode);
}

/* ------------------------------------------------------------------ */


//
// Security context swapping
//

#ifndef NO_EMBEDDED_ACCESS_CHECKS
void
sp_change_security_context(THD *thd, sp_head *sp, st_sp_security_context *ctxp)
{
  ctxp->changed= (sp->m_chistics->suid != SP_IS_NOT_SUID &&
		   (strcmp(sp->m_definer_user.str, thd->priv_user) ||
		    strcmp(sp->m_definer_host.str, thd->priv_host)));

  if (ctxp->changed)
  {
    ctxp->master_access= thd->master_access;
    ctxp->db_access= thd->db_access;
    ctxp->priv_user= thd->priv_user;
    strncpy(ctxp->priv_host, thd->priv_host, sizeof(ctxp->priv_host));
    ctxp->user= thd->user;
    ctxp->host= thd->host;
    ctxp->ip= thd->ip;

    /* Change thise just to do the acl_getroot_no_password */
    thd->user= sp->m_definer_user.str;
    thd->host= thd->ip = sp->m_definer_host.str;

    if (acl_getroot_no_password(thd))
    {			// Failed, run as invoker for now
      ctxp->changed= FALSE;
      thd->master_access= ctxp->master_access;
      thd->db_access= ctxp->db_access;
      thd->priv_user= ctxp->priv_user;
      strncpy(thd->priv_host, ctxp->priv_host, sizeof(thd->priv_host));
    }

    /* Restore these immiediately */
    thd->user= ctxp->user;
    thd->host= ctxp->host;
    thd->ip= ctxp->ip;
  }
}

void
sp_restore_security_context(THD *thd, sp_head *sp, st_sp_security_context *ctxp)
{
  if (ctxp->changed)
  {
    ctxp->changed= FALSE;
    thd->master_access= ctxp->master_access;
    thd->db_access= ctxp->db_access;
    thd->priv_user= ctxp->priv_user;
    strncpy(thd->priv_host, ctxp->priv_host, sizeof(thd->priv_host));
  }
}

#endif /* NO_EMBEDDED_ACCESS_CHECKS */

/*
  Structure that represent all instances of one table
  in optimized multi-set of tables used by routine.
*/

typedef struct st_sp_table
{
  LEX_STRING qname;     /* Multi-set key: db_name\0table_name\0alias\0 */
  uint db_length, table_name_length;
  bool temp;               /* true if corresponds to a temporary table */
  thr_lock_type lock_type; /* lock type used for prelocking */
  uint lock_count;
  uint query_lock_count;
} SP_TABLE;

byte *
sp_table_key(const byte *ptr, uint *plen, my_bool first)
{
  SP_TABLE *tab= (SP_TABLE *)ptr;
  *plen= tab->qname.length;
  return (byte *)tab->qname.str;
}


/*
  Merge the list of tables used by some query into the multi-set of
  tables used by routine.

  SYNOPSIS
    merge_table_list()
      thd               - thread context
      table             - table list
      lex_for_tmp_check - LEX of the query for which we are merging
                          table list.

  NOTE
    This method will use LEX provided to check whenever we are creating
    temporary table and mark it as such in target multi-set.

  RETURN VALUE
    TRUE  - Success
    FALSE - Error
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
        It is safe to store pointer to table list elements in hash,
        since they are supposed to have the same lifetime.
      */
      if ((tab= (SP_TABLE *)hash_search(&m_sptabs, (byte *)tname, tlen)))
      {
        if (tab->lock_type < table->lock_type)
          tab->lock_type= table->lock_type; // Use the table with the highest lock type
        tab->query_lock_count++;
        if (tab->query_lock_count > tab->lock_count)
          tab->lock_count++;
      }
      else
      {
	if (!(tab= (SP_TABLE *)thd->calloc(sizeof(SP_TABLE))))
	  return FALSE;
	tab->qname.length= tlen;
	tab->qname.str= (char*) thd->memdup(tname, tab->qname.length + 1);
	if (!tab->qname.str)
	  return FALSE;
	if (lex_for_tmp_check->sql_command == SQLCOM_CREATE_TABLE &&
	    lex_for_tmp_check->query_tables == table &&
	    lex_for_tmp_check->create_info.options & HA_LEX_CREATE_TMP_TABLE)
	  tab->temp= TRUE;
        tab->table_name_length= table->table_name_length;
        tab->db_length= table->db_length;
        tab->lock_type= table->lock_type;
        tab->lock_count= tab->query_lock_count= 1;
	my_hash_insert(&m_sptabs, (byte *)tab);
      }
    }
  return TRUE;
}


/*
  Add tables used by routine to the table list.

  SYNOPSIS
    add_used_tables_to_table_list()
      thd                   - thread context
      query_tables_last_ptr - (in/out) pointer the next_global member of last
                              element of the list where tables will be added
                              (or to its root).

  DESCRIPTION
    Converts multi-set of tables used by this routine to table list and adds
    this list to the end of table list specified by 'query_tables_last_ptr'.

    Elements of list will be allocated in PS memroot, so this list will be
    persistent between PS executions.

  RETURN VALUE
    TRUE - if some elements were added, FALSE - otherwise.
*/

bool
sp_head::add_used_tables_to_table_list(THD *thd,
                                       TABLE_LIST ***query_tables_last_ptr)
{
  uint i;
  Item_arena *arena, backup;
  bool result= FALSE;
  DBUG_ENTER("sp_head::add_used_tables_to_table_list");

  /*
    Use persistent arena for table list allocation to be PS friendly.
  */
  arena= thd->change_arena_if_needed(&backup);

  for (i=0 ; i < m_sptabs.records ; i++)
  {
    char *tab_buff;
    TABLE_LIST *table;
    SP_TABLE *stab= (SP_TABLE *)hash_element(&m_sptabs, i);
    if (stab->temp)
      continue;

    if (!(tab_buff= (char *)thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST)) *
                                        stab->lock_count)))
      DBUG_RETURN(FALSE);

    for (uint j= 0; j < stab->lock_count; j++)
    {
      table= (TABLE_LIST *)tab_buff;

      /*
        It's enough to just copy the pointers as the data will not change
        during the lifetime of the SP. If the SP is used by PS, we assume
        that the PS will be invalidated if the functions is deleted or
        changed.
      */
      table->db= stab->qname.str;
      table->db_length= stab->db_length;
      table->table_name= table->db + table->db_length + 1;
      table->table_name_length= stab->table_name_length;
      table->alias= table->table_name + table->table_name_length + 1;
      table->lock_type= stab->lock_type;
      table->cacheable_table= 1;
      table->prelocking_placeholder= 1;

      /* Everyting else should be zeroed */

      **query_tables_last_ptr= table;
      table->prev_global= *query_tables_last_ptr;
      *query_tables_last_ptr= &table->next_global;

      tab_buff+= ALIGN_SIZE(sizeof(TABLE_LIST));
      result= TRUE;
    }
  }

  if (arena)
    thd->restore_backup_item_arena(arena, &backup);

  DBUG_RETURN(result);
}

/*
 * Simple function for adding an explicetly named (systems) table to
 * the global table list, e.g. "mysql", "proc".
 *
 */
TABLE_LIST *
sp_add_to_query_tables(THD *thd, LEX *lex,
		       const char *db, const char *name,
		       thr_lock_type locktype)
{
  TABLE_LIST *table;

  if (!(table= (TABLE_LIST *)thd->calloc(sizeof(TABLE_LIST))))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(TABLE_LIST));
    return NULL;
  }
  table->db_length= strlen(db);
  table->db= thd->strmake(db, table->db_length);
  table->table_name_length= strlen(name);
  table->table_name= thd->strmake(name, table->table_name_length);
  table->alias= thd->strdup(name);
  table->lock_type= locktype;
  table->select_lex= lex->current_select; // QQ?
  table->cacheable_table= 1;
  
  lex->add_to_query_tables(table);
  return table;
}


/*
  Auxilary function for adding tables used by routines used in query
  to table lists.

  SYNOPSIS
    sp_add_sp_tables_to_table_list_aux()
      thd       - thread context
      lex       - LEX to which table list tables will be added
      func_hash - routines for which tables should be added
      func_cache- SP cache in which this routines should be looked up

  NOTE
    See sp_add_sp_tables_to_table_list() for more info.

  RETURN VALUE
    TRUE  - some tables were added
    FALSE - no tables were added.
*/

static bool
sp_add_sp_tables_to_table_list_aux(THD *thd, LEX *lex, HASH *func_hash,
                                   sp_cache **func_cache)
{
  uint i;
  bool result= FALSE;

  for (i= 0 ; i < func_hash->records ; i++)
  {
    sp_head *sp;
    LEX_STRING *ls= (LEX_STRING *)hash_element(func_hash, i);
    sp_name name(*ls);

    name.m_qname= *ls;
    if ((sp= sp_cache_lookup(func_cache, &name)))
      result|= sp->add_used_tables_to_table_list(thd, &lex->query_tables_last);
  }

  return result;
}


/*
  Add tables used by routines used in query to table list.

  SYNOPSIS
    sp_add_sp_tables_to_table_list()
      thd      - thread context
      lex      - LEX to which table list tables will be added
      func_lex - LEX for which functions we get tables
                 (useful for adding tables used by view routines)

  NOTE
    Elements of list will be allocated in PS memroot, so this
    list will be persistent between PS execetutions.

  RETURN VALUE
    TRUE  - some tables were added
    FALSE - no tables were added.
*/

bool
sp_add_sp_tables_to_table_list(THD *thd, LEX *lex, LEX *func_lex)
{
  return (sp_add_sp_tables_to_table_list_aux(thd, lex, &func_lex->spfuns,
                                             &thd->sp_func_cache) |
          sp_add_sp_tables_to_table_list_aux(thd, lex, &func_lex->spprocs,
                                             &thd->sp_proc_cache));
}
