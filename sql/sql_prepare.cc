/* Copyright (C) 1995-2002 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

/**********************************************************************
This file contains the implementation of prepare and executes. 

Prepare:

  - Server gets the query from client with command 'COM_PREPARE'; 
    in the following format:
    [COM_PREPARE:1] [query]
  - Parse the query and recognize any parameter markers '?' and 
    store its information list in lex->param_list
  - Allocate a new statement for this prepare; and keep this in 
    'thd->prepared_statements' pool.
  - Without executing the query, return back to client the total 
    number of parameters along with result-set metadata information
    (if any) in the following format:
    [STMT_ID:4]
    [Column_count:2]
    [Param_count:2]
    [Columns meta info] (if Column_count > 0)
    [Params meta info]  (if Param_count > 0 ) (TODO : 4.1.1)
     
Prepare-execute:

  - Server gets the command 'COM_EXECUTE' to execute the 
    previously prepared query. If there is any param markers; then client
    will send the data in the following format:
    [COM_EXECUTE:1]
    [STMT_ID:4]
    [NULL_BITS:(param_count+7)/8)]
    [TYPES_SUPPLIED_BY_CLIENT(0/1):1]
    [[length]data]
    [[length]data] .. [[length]data]. 
    (Note: Except for string/binary types; all other types will not be 
    supplied with length field)
  - Replace the param items with this new data. If it is a first execute 
    or types altered by client; then setup the conversion routines.
  - Execute the query without re-parsing and send back the results 
    to client

Long data handling:

  - Server gets the long data in pieces with command type 'COM_LONG_DATA'.
  - The packet recieved will have the format as:
    [COM_LONG_DATA:1][STMT_ID:4][parameter_number:2][type:2][data]
  - Checks if the type is specified by client, and if yes reads the type, 
    and stores the data in that format.
  - It's up to the client to check for read data ended. The server doesn't
    care; and also server doesn't notify to the client that it got the 
    data or not; if there is any error; then during execute; the error 
    will be returned

***********************************************************************/

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h" // for JOIN
#include <m_ctype.h>  // for isspace()
#ifdef EMBEDDED_LIBRARY
/* include MYSQL_BIND headers */
#include <mysql.h>
#endif

const String my_null_string("NULL", 4, default_charset_info);

/******************************************************************************
  Prepared_statement: statement which can contain placeholders
******************************************************************************/

class Prepared_statement: public Statement
{
public:
  THD *thd;
  Item_param **param_array;
  uint param_count;
  uint last_errno;
  char last_error[MYSQL_ERRMSG_SIZE];
  bool get_longdata_error;
  bool long_data_used;
  bool log_full_query;
#ifndef EMBEDDED_LIBRARY
  bool (*set_params)(Prepared_statement *st, uchar *data, uchar *data_end,
                     uchar *read_pos);
#else
  bool (*set_params_data)(Prepared_statement *st);
#endif
public:
  Prepared_statement(THD *thd_arg);
  virtual ~Prepared_statement();
  virtual Statement::Type type() const;
};


/******************************************************************************
  Implementation
******************************************************************************/


inline bool is_param_null(const uchar *pos, ulong param_no)
{
  return pos[param_no/8] & (1 << (param_no & 7));
}

enum { STMT_QUERY_LOG_LENGTH= 8192 };

enum enum_send_error { DONT_SEND_ERROR= 0, SEND_ERROR };

/*
  Seek prepared statement in statement map by id: returns zero if statement
  was not found, pointer otherwise.
*/

static Prepared_statement *
find_prepared_statement(THD *thd, ulong id, const char *where,
                        enum enum_send_error se)
{
  Statement *stmt= thd->stmt_map.find(id);

  if (stmt == 0 || stmt->type() != Statement::PREPARED_STATEMENT)
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), id, where);
    if (se == SEND_ERROR)
      send_error(thd);
    return 0;
  }
  return (Prepared_statement *) stmt;
}


/*
  Send prepared stmt info to client after prepare
*/

#ifndef EMBEDDED_LIBRARY
static bool send_prep_stmt(Prepared_statement *stmt, uint columns)
{
  NET *net= &stmt->thd->net;
  char buff[9];
  buff[0]= 0;                                   /* OK packet indicator */
  int4store(buff+1, stmt->id);
  int2store(buff+5, columns);
  int2store(buff+7, stmt->param_count);
  /*
    Send types and names of placeholders to the client
    XXX: fix this nasty upcast from List<Item_param> to List<Item>
  */
  return my_net_write(net, buff, sizeof(buff)) || 
         (stmt->param_count &&
          stmt->thd->protocol_simple.send_fields((List<Item> *)
                                                 &stmt->lex->param_list, 0)) ||
         net_flush(net);
  return 0;
}
#else
static bool send_prep_stmt(Prepared_statement *stmt,
                           uint columns __attribute__((unused)))
{
  THD *thd= stmt->thd;

  thd->client_stmt_id= stmt->id;
  thd->client_param_count= stmt->param_count;
  thd->net.last_errno= 0;

  return 0;
}
#endif /*!EMBEDDED_LIBRARY*/


/*
  Read the length of the parameter data and return back to
  caller by positing the pointer to param data.
*/

#ifndef EMBEDDED_LIBRARY
static ulong get_param_length(uchar **packet, ulong len)
{
  reg1 uchar *pos= *packet;
  if (len < 1)
    return 0;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (len < 3)
    return 0;
  if (*pos == 252)
  {
    (*packet)+=3;
    return (ulong) uint2korr(pos+1);
  }
  if (len < 4)
    return 0;
  if (*pos == 253)
  {
    (*packet)+=4;
    return (ulong) uint3korr(pos+1);
  }
  if (len < 5)
    return 0;
  (*packet)+=9; // Must be 254 when here 
  /* TODO: why uint4korr here? (should be uint8korr) */
  return (ulong) uint4korr(pos+1);
}
#else
#define get_param_length(packet, len) len
#endif /*!EMBEDDED_LIBRARY*/

 /*
   Data conversion routines
   SYNOPSIS
   set_param_xx()
    param   parameter item
    pos     input data buffer
    len     length of data in the buffer

  All these functions read the data from pos, convert it to requested type 
  and assign to param; pos is advanced to predefined length.

  Make a note that the NULL handling is examined at first execution
  (i.e. when input types altered) and for all subsequent executions
  we don't read any values for this.

  RETURN VALUE
    none
*/

void set_param_tiny(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 1)
    return;
#endif
  param->set_int((longlong)(**pos));
  *pos+= 1;
}

void set_param_short(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 2)
    return;
#endif
  param->set_int((longlong)sint2korr(*pos));
  *pos+= 2;
}

void set_param_int32(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 4)
    return;
#endif
  param->set_int((longlong)sint4korr(*pos));
  *pos+= 4;
}

void set_param_int64(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 8)
    return;
#endif
  param->set_int((longlong)sint8korr(*pos));
  *pos+= 8;
}

void set_param_float(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 4)
    return;
#endif
  float data;
  float4get(data,*pos);
  param->set_double((double) data);
  *pos+= 4;
}

void set_param_double(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 8)
    return;
#endif
  double data;
  float8get(data,*pos);
  param->set_double((double) data);
  *pos+= 8;
}

void set_param_time(Item_param *param, uchar **pos, ulong len)
{
  ulong length;

  if ((length= get_param_length(pos, len)) >= 8)
  {
    uchar *to= *pos;
    TIME  tm;
    
    /* TODO: why length is compared with 8 here? */
    tm.second_part= (length > 8 ) ? (ulong) sint4korr(to+7): 0;

    tm.day=    (ulong) sint4korr(to+1);
    tm.hour=   (uint) to[5];
    tm.minute= (uint) to[6];
    tm.second= (uint) to[7];

    tm.year= tm.month= 0;
    tm.neg= (bool)to[0];

    param->set_time(&tm, TIMESTAMP_TIME);
  }
  *pos+= length;
}

void set_param_datetime(Item_param *param, uchar **pos, ulong len)
{
  uint length;
 
  if ((length= get_param_length(pos, len)) >= 4)
  {
    uchar *to= *pos;
    TIME  tm;
    
    tm.second_part= (length > 7 ) ? (ulong) sint4korr(to+7): 0;
    
    if (length > 4)
    {
      tm.hour=   (uint) to[4];
      tm.minute= (uint) to[5];
      tm.second= (uint) to[6];
    }
    else
      tm.hour= tm.minute= tm.second= 0;
    
    tm.year=   (uint) sint2korr(to);
    tm.month=  (uint) to[2];
    tm.day=    (uint) to[3];
    tm.neg=    0;

    param->set_time(&tm, TIMESTAMP_DATETIME);
  }
  *pos+= length;
}

void set_param_date(Item_param *param, uchar **pos, ulong len)
{
  ulong length;
 
  if ((length= get_param_length(pos, len)) >= 4)
  {
    uchar *to= *pos;
    TIME tm;

    tm.year=  (uint) sint2korr(to);
    tm.month=  (uint) to[2];
    tm.day= (uint) to[3];

    tm.hour= tm.minute= tm.second= 0;
    tm.second_part= 0;
    tm.neg= 0;

    param->set_time(&tm, TIMESTAMP_DATE);
  }
  *pos+= length;
}

void set_param_str(Item_param *param, uchar **pos, ulong len)
{
  ulong length= get_param_length(pos, len);
  param->set_value((const char *)*pos, length);
  *pos+= length;
}

static void setup_one_conversion_function(Item_param *param, uchar param_type)
{
  switch (param_type) {
  case FIELD_TYPE_TINY:
    param->set_param_func= set_param_tiny;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_SHORT:
    param->set_param_func= set_param_short;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_LONG:
    param->set_param_func= set_param_int32;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_LONGLONG:
    param->set_param_func= set_param_int64;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_FLOAT:
    param->set_param_func= set_param_float;
    param->item_result_type= REAL_RESULT;
    break;
  case FIELD_TYPE_DOUBLE:
    param->set_param_func= set_param_double;
    param->item_result_type= REAL_RESULT;
    break;
  case FIELD_TYPE_TIME:
    param->set_param_func= set_param_time;
    param->item_result_type= STRING_RESULT;
    break;
  case FIELD_TYPE_DATE:
    param->set_param_func= set_param_date;
    param->item_result_type= STRING_RESULT;
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    param->set_param_func= set_param_datetime;
    param->item_result_type= STRING_RESULT;
    break;
  default:
    param->set_param_func= set_param_str;
    param->item_result_type= STRING_RESULT;
  }
}

#ifndef EMBEDDED_LIBRARY
/*
  Update the parameter markers by reading data from client packet 
  and if binary/update log is set, generate the valid query.
*/

static bool insert_params_withlog(Prepared_statement *stmt, uchar *null_array,
                                  uchar *read_pos, uchar *data_end)
{
  THD  *thd= stmt->thd;
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;
  uint32 length= 0;

  String str, query;
  const String *res;

  DBUG_ENTER("insert_params_withlog"); 

  if (query.copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    if (param->long_data_supplied)
      res= param->query_val_str(&str);
    else
    {
      if (is_param_null(null_array, it - begin))
      {
        param->maybe_null= param->null_value= 1;
        res= &my_null_string;
      }
      else
      {
        param->maybe_null= param->null_value= 0;
        if (read_pos >= data_end)
          DBUG_RETURN(1);
        param->set_param_func(param, &read_pos, data_end - read_pos);
        res= param->query_val_str(&str);
      }
    }
    if (query.replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);
    
    length+= res->length()-1;
  }
  if (alloc_query(thd, (char *)query.ptr(), query.length()+1))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


static bool insert_params(Prepared_statement *stmt, uchar *null_array,
                          uchar *read_pos, uchar *data_end)
{
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;

  DBUG_ENTER("insert_params"); 

  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    if (!param->long_data_supplied)
    {
      if (is_param_null(null_array, it - begin))
        param->maybe_null= param->null_value= 1;
      else
      {
        param->maybe_null= param->null_value= 0;
        if (read_pos >= data_end)
          DBUG_RETURN(1);
        param->set_param_func(param, &read_pos, data_end - read_pos);
      }
    }
  }
  DBUG_RETURN(0);
}


static bool setup_conversion_functions(Prepared_statement *stmt,
                                       uchar **data, uchar *data_end)
{
  /* skip null bits */
  uchar *read_pos= *data + (stmt->param_count+7) / 8;

  DBUG_ENTER("setup_conversion_functions");

  if (*read_pos++) //types supplied / first execute
  {
    /*
      First execute or types altered by the client, setup the 
      conversion routines for all parameters (one time)
    */
    Item_param **it= stmt->param_array;
    Item_param **end= it + stmt->param_count;
    for (; it < end; ++it)
    {
      if (read_pos >= data_end)
        DBUG_RETURN(1);
      setup_one_conversion_function(*it, *read_pos);
      read_pos+= 2;
    }
  }
  *data= read_pos;
  DBUG_RETURN(0);
}

#else

static bool emb_insert_params(Prepared_statement *stmt)
{
  Item_param **it= stmt->param_array;
  Item_param **end= it + stmt->param_count;
  MYSQL_BIND *client_param= stmt->thd->client_params;

  DBUG_ENTER("emb_insert_params");

  for (; it < end; ++it, ++client_param)
  {
    Item_param *param= *it;
    setup_one_conversion_function(param, client_param->buffer_type);
    if (!param->long_data_supplied)
    {
      if (*client_param->is_null)
        param->maybe_null= param->null_value= 1;
      else
      {
	uchar *buff= (uchar*)client_param->buffer;
        param->maybe_null= param->null_value= 0;
        param->set_param_func(param, &buff,
                              client_param->length ? 
                              *client_param->length : 
                              client_param->buffer_length);
      }
    }
  }
  DBUG_RETURN(0);
}


static bool emb_insert_params_withlog(Prepared_statement *stmt)
{
  THD *thd= stmt->thd;
  Item_param **it= stmt->param_array;
  Item_param **end= it + stmt->param_count;
  MYSQL_BIND *client_param= thd->client_params;

  String str, query;
  const String *res;
  uint32 length= 0;

  DBUG_ENTER("emb_insert_params_withlog");

  if (query.copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  for (; it < end; ++it, ++client_param)
  {
    Item_param *param= *it;
    setup_one_conversion_function(param, client_param->buffer_type);
    if (param->long_data_supplied)
      res= param->query_val_str(&str);
    else
    {
      if (*client_param->is_null)
      {
        param->maybe_null= param->null_value= 1;
        res= &my_null_string;
      }
      else
      {
	uchar *buff= (uchar*)client_param->buffer;
        param->maybe_null= param->null_value= 0;
        param->set_param_func(param, &buff,
                              client_param->length ? 
                              *client_param->length : 
                              client_param->buffer_length);
        res= param->query_val_str(&str);
      }
    }
    if (query.replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);
    length+= res->length()-1;
  }
  
  if (alloc_query(thd, (char *) query.ptr(), query.length()+1))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

#endif /*!EMBEDDED_LIBRARY*/

/*
  Validate the following information for INSERT statement: 
    - field existence
    - fields count
  SYNOPSIS
    mysql_test_insert_fields()
  RETURN VALUE
    0   ok
    1   error, sent to the client
   -1   error, not sent to client
*/

static int mysql_test_insert_fields(Prepared_statement *stmt,
                                    TABLE_LIST *table_list,
                                    List<Item> &fields, 
                                    List<List_item> &values_list,
				    List<Item> &update_fields,
				    List<Item> &update_values,
				    enum_duplicates duplic)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  TABLE *table;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  TABLE_LIST *insert_table_list=
    (TABLE_LIST*) lex->select_lex.table_list.first;

  DBUG_ENTER("mysql_test_insert_fields");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  ulong privilege= (lex->duplicates == DUP_REPLACE ?
		    INSERT_ACL | DELETE_ACL : INSERT_ACL);
  if (check_one_table_access(thd, privilege, table_list, 0))
    DBUG_RETURN(1);
#endif

  /*
     open temporary memory pool for temporary data allocated by derived
     tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();
  if (open_and_lock_tables(thd, table_list))
  {
    thd->free_temporary_memory_pool_for_ps_preparing();
    DBUG_RETURN(-1);
  }

  table= table_list->table;

  if ((values= its++))
  {
    uint value_count;
    ulong counter= 0;
    
    if (check_insert_fields(thd, table, fields, *values, 1) ||
	setup_tables(insert_table_list) ||
	setup_fields(thd, 0, insert_table_list, *values, 0, 0, 0) ||
	(duplic == DUP_UPDATE &&
	 (setup_fields(thd, 0, insert_table_list, update_fields, 0, 0, 0) ||
	  setup_fields(thd, 0, insert_table_list, update_values, 0, 0, 0))))
      goto error;
    if (find_real_table_in_list(table_list->next, 
				table_list->db, table_list->real_name))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->real_name);
      goto error;
    }

    value_count= values->elements;
    its.rewind();
   
    while ((values= its++))
    {
      counter++;
      if (values->elements != value_count)
      {
        my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
			ER(ER_WRONG_VALUE_COUNT_ON_ROW),
			MYF(0), counter);
        goto error;
      }
      if (setup_fields(thd, 0, insert_table_list, *values, 0, 0, 0))
	goto error;
    }
  }
  lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(0);

error:
  lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(-1);
}


/*
  Validate the following information:
    UPDATE - set and where clause
    DELETE - where clause
  SYNOPSIS
    mysql_test_upd_fields()
  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/

static int mysql_test_upd_fields(Prepared_statement *stmt,
                                 TABLE_LIST *table_list,
                                 List<Item> &fields, List<Item> &values,
                                 COND *conds, ulong privelege)
{
  THD *thd= stmt->thd;
  SELECT_LEX *select_lex= &stmt->lex->select_lex;
  TABLE_LIST *upd_table_list=
    (TABLE_LIST*) select_lex->table_list.first;
  List<Item> all_fields;
  uint order_num= select_lex->order_list.elements;
  ORDER *order= (ORDER *) select_lex->order_list.first;

  DBUG_ENTER("mysql_test_upd_fields");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_one_table_access(thd, privelege, table_list, 0))
    DBUG_RETURN(1);
  // Set privilege for the WHERE clause
  table_list->grant.want_privilege= (SELECT_ACL &
				     ~table_list->grant.privilege);
#endif

  /*
     open temporary memory pool for temporary data allocated by derived
     tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();

  if (open_and_lock_tables(thd, table_list))
    goto err;
  if (setup_tables(upd_table_list) ||
      setup_conds(thd, upd_table_list, &conds) ||
      thd->lex->select_lex.setup_ref_array(thd, order_num) ||
      setup_fields(thd, 0, upd_table_list, fields, 1, 0, 0) ||
      setup_order(thd, thd->lex->select_lex.ref_pointer_array,
		  upd_table_list, all_fields, all_fields, order) ||
      thd->net.report_error)
  {
    stmt->lex->unit.cleanup();
    goto err;
  }

  stmt->lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();

  /* TODO: here we should send types of placeholders to the client. */
  DBUG_RETURN(0);
err:
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(-1);
}

/*
  Validate the following information:
    SELECT - column list 
           - where clause
           - order clause
           - having clause
           - group by clause
           - if no column spec i.e. '*', then setup all fields
  In case of success, if this query is not EXPLAIN, send column list info
  back to client. 
  SYNOPSIS
    mysql_test_select_fields()
  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/

static int mysql_test_select_fields(Prepared_statement *stmt,
                                    TABLE_LIST *tables,
                                    List<Item> &fields)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX_UNIT *unit= &lex->unit;
  

  DBUG_ENTER("mysql_test_select_fields");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  ulong privilege= lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL;
  if (tables)
  {
    if (check_table_access(thd, privilege, tables,0))
      DBUG_RETURN(1);
  }
  else if (check_access(thd, privilege, "*any*",0,0,0))
    DBUG_RETURN(1);
#endif

  /*
     open temporary memory pool for temporary data allocated by derived
     tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();
  if (open_and_lock_tables(thd, tables))
  {
    send_error(thd);
    goto err;
  }

  if (lex->describe)
  {
    if (send_prep_stmt(stmt, 0))
      goto err;
  }
  else
  {
    thd->used_tables= 0;                        // Updated by setup_fields

    if (unit->prepare(thd, 0, 0))
    {
      send_error(thd);
      goto err_prep;
    }

    if (send_prep_stmt(stmt, fields.elements) ||
        thd->protocol_simple.send_fields(&fields, 0)
#ifndef EMBEDDED_LIBRARY
        || net_flush(&thd->net)
#endif
       )
      goto err_prep;

    unit->cleanup();
  }
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(0);

err_prep:
  unit->cleanup();
err:
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(1);
}


/*
  Validate and prepare for execution DO statement expressions

  SYNOPSIS
    mysql_test_do_fields()
    stmt	prepared statemen handler
    tables	list of tables queries
    values	list of expressions

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/

static int mysql_test_do_fields(Prepared_statement *stmt,
				TABLE_LIST *tables,
				List<Item> *values)
{
  DBUG_ENTER("mysql_test_do_fields");
  THD *thd= stmt->thd;
  int res= 0;
  if (tables && (res= check_table_access(thd, SELECT_ACL, tables, 0)))
    DBUG_RETURN(res);
  /*
    open temporary memory pool for temporary data allocated by derived
    tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();
  if (tables && (res= open_and_lock_tables(thd, tables)))
  {
    thd->free_temporary_memory_pool_for_ps_preparing();
    DBUG_RETURN(res);
  }
  res= setup_fields(thd, 0, 0, *values, 0, 0, 0);
  stmt->lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();
  if (res)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}


/*
  Validate and prepare for execution SET statement expressions

  SYNOPSIS
    mysql_test_set_fields()
    stmt	prepared statemen handler
    tables	list of tables queries
    values	list of expressions

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_set_fields(Prepared_statement *stmt,
				TABLE_LIST *tables,
				List<set_var_base> *var_list)
{
  DBUG_ENTER("mysql_test_set_fields");
  List_iterator_fast<set_var_base> it(*var_list);
  THD *thd= stmt->thd;
  set_var_base *var;
  int res= 0;

  if (tables && (res= check_table_access(thd, SELECT_ACL, tables, 0)))
    DBUG_RETURN(res);
  /*
    open temporary memory pool for temporary data allocated by derived
    tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();
  if (tables && (res= open_and_lock_tables(thd, tables)))
    goto error;
  while ((var= it++))
  {
    if (var->light_check(thd))
    {
      stmt->lex->unit.cleanup();
      res= -1;
      goto error;
    }
  }
  stmt->lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(0);

error:
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(res);
}


/*
  Check internal SELECT of the prepared command

  SYNOPSIS
    select_like_statement_test()
      stmt	- prepared table handler
      tables	- global list of tables

  RETURN
    0   success
    1   error, sent to client
   -1   error, not sent to client    
*/
static int select_like_statement_test(Prepared_statement *stmt,
				      TABLE_LIST *tables)
{
  DBUG_ENTER("select_like_statement_test");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  int res= 0;
  /*
    open temporary memory pool for temporary data allocated by derived
    tables & preparation procedure
  */
  thd->allocate_temporary_memory_pool_for_ps_preparing();
  if (tables && (res= open_and_lock_tables(thd, tables)))
    goto end;

  thd->used_tables= 0;                        // Updated by setup_fields

  if (lex->unit.prepare(thd, 0, 0))
  {
    res= thd->net.report_error ? -1 : 1;
  }
end:
  lex->unit.cleanup();
  thd->free_temporary_memory_pool_for_ps_preparing();
  DBUG_RETURN(res);
}


/*
  Validate and prepare for execution CRETE TABLE statement

  SYNOPSIS
    mysql_test_create_table()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_create_table(Prepared_statement *stmt,
				   TABLE_LIST *tables)
{
  DBUG_ENTER("mysql_test_create_table");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  int res= 0;
  
  ulong want_priv= ((lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) ?
		    CREATE_TMP_ACL : CREATE_ACL);
  if (check_one_table_access(thd, want_priv, tables, 0) ||
      check_merge_table_access(thd, tables->db,
			       (TABLE_LIST *)
			       lex->create_info.merge_list.first))
    DBUG_RETURN(1);

  /* Skip first table, which is the table we are creating */
  TABLE_LIST *create_table, *create_table_local;
  tables= lex->unlink_first_table(tables, &create_table,
				    &create_table_local);

  if (grant_option && want_priv != CREATE_TMP_ACL &&
	check_grant(thd, want_priv, create_table,0,0))
  {
    res= 1;
    goto end;
  }

  if (tables)
    res= select_like_statement_test(stmt, tables);

end:
  // put tables back for PS rexecuting
  tables= lex->link_first_table_back(tables, create_table,
				     create_table_local);
  DBUG_RETURN(res);
}

/*
  Validate and prepare for execution multy update statement

  SYNOPSIS
    mysql_test_multiupdate()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_multiupdate(Prepared_statement *stmt,
				  TABLE_LIST *tables)
{
  int res;
  if ((res= multi_update_precheck(stmt->thd, tables)))
    return res;
  return select_like_statement_test(stmt, tables);
}


/*
  Validate and prepare for execution multy delete statement

  SYNOPSIS
    mysql_test_multidelete()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_multidelete(Prepared_statement *stmt,
				  TABLE_LIST *tables)
{
  int res;
  stmt->thd->lex->current_select= &stmt->thd->lex->select_lex;
  if (add_item_to_list(stmt->thd, new Item_null()))
    return -1;

  uint fake_counter= 0;
  if ((res= multi_delete_precheck(stmt->thd, tables, &fake_counter)))
    return res;
  return select_like_statement_test(stmt, tables);
}


/*
  Validate and prepare for execution INSERT ... SELECT statement

  SYNOPSIS
    mysql_test_insert_select()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_insert_select(Prepared_statement *stmt,
				    TABLE_LIST *tables)
{
  int res;
  LEX *lex= stmt->lex;
  if ((res= insert_select_precheck(stmt->thd, tables)))
    return res;
  TABLE_LIST *first_local_table=
    (TABLE_LIST *)lex->select_lex.table_list.first;
  /* Skip first table, which is the table we are inserting in */
  lex->select_lex.table_list.first= (gptr) first_local_table->next;
  lex->select_lex.resolve_mode= SELECT_LEX::NOMATTER_MODE;
  res= select_like_statement_test(stmt, tables);
  /* revert changes*/
  lex->select_lex.table_list.first= (gptr) first_local_table;
  lex->select_lex.resolve_mode= SELECT_LEX::INSERT_MODE;
  return res;
}


/*
  Send the prepare query results back to client
  SYNOPSIS
  send_prepare_results()
    stmt prepared statement
  RETURN VALUE
    0   success
    1   error, sent to client
*/
static int send_prepare_results(Prepared_statement *stmt)
{   
  DBUG_ENTER("send_prepare_results");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *tables=(TABLE_LIST*) select_lex->table_list.first;
  enum enum_sql_command sql_command= lex->sql_command;
  int res;
  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, stmt->param_count));

  if (&lex->select_lex != lex->all_selects_list &&
      lex->unit.create_total_list(thd, lex, &tables))
    DBUG_RETURN(1);

  
  switch (sql_command) {

  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
    if ((res=
	 mysql_test_insert_fields(stmt, tables, lex->field_list,
				  lex->many_values,
				  select_lex->item_list, lex->value_list,
				  (lex->value_list.elements ?
				   DUP_UPDATE : lex->duplicates))))
      goto error;
    break;

  case SQLCOM_UPDATE:
    /* XXX: fallthrough */
  case SQLCOM_DELETE:
    if ((res= mysql_test_upd_fields(stmt, tables, select_lex->item_list,
                                    lex->value_list, select_lex->where,
				    ((sql_command == SQLCOM_DELETE)?
				     DELETE_ACL : UPDATE_ACL))))
      goto error;
    break;

  case SQLCOM_SELECT:
    if ((res= mysql_test_select_fields(stmt, tables, select_lex->item_list)))
      goto error;
    /* Statement and field info has already been sent */
    DBUG_RETURN(0);

  case SQLCOM_CREATE_TABLE:
    if ((res= mysql_test_create_table(stmt, tables)))
      goto error;
    break;
  
  case SQLCOM_DO:
    if ((res= mysql_test_do_fields(stmt, tables, lex->insert_list)))
      goto error;
     break;

  case SQLCOM_SET_OPTION:
    if ((res= mysql_test_set_fields(stmt, tables, &lex->var_list)))
      goto error;
    break;

  case SQLCOM_DELETE_MULTI:
    if ((res= mysql_test_multidelete(stmt, tables)))
      goto error;
    break;
  
  case SQLCOM_UPDATE_MULTI:
    if ((res= mysql_test_multiupdate(stmt, tables)))
      goto error;
    break;

  case SQLCOM_INSERT_SELECT:
    if ((res= mysql_test_insert_select(stmt, tables)))
      goto error;
    break;

  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_PROCESSLIST:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_PRIVILEGES:
  case SQLCOM_SHOW_COLUMN_TYPES:
  case SQLCOM_SHOW_STATUS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_LOGS:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_CREATE_DB:
  case SQLCOM_SHOW_GRANTS:
  case SQLCOM_DROP_TABLE:
  case SQLCOM_RENAME_TABLE:
    break;

  default:
    /*
      All other is not supported yet
    */
    res= -1;
    my_error(ER_UNSUPPORTED_PS, MYF(0));
    goto error;
  }
  DBUG_RETURN(send_prep_stmt(stmt, 0));

error:
  if (res < 0)
    send_error(thd, thd->killed ? ER_SERVER_SHUTDOWN : 0);
  DBUG_RETURN(1);
}

/*
  Initialize array of parametes in statement from LEX.
  (We need to have quick access to items by number in mysql_send_longdata).
  This is to avoid using malloc/realloc in the parser.
*/

static bool init_param_array(Prepared_statement *stmt)
{
  LEX *lex= stmt->lex;
  if ((stmt->param_count= lex->param_list.elements))
  {
    Item_param **to;
    List_iterator<Item_param> param_iterator(lex->param_list);
    /* Use thd->mem_root as it points at statement mem_root */
    stmt->param_array= (Item_param **)
                       alloc_root(&stmt->thd->mem_root,
                                  sizeof(Item_param*) * stmt->param_count);
    if (!stmt->param_array)
    {
      send_error(stmt->thd, ER_OUT_OF_RESOURCES);
      return 1;
    }
    for (to= stmt->param_array;
         to < stmt->param_array + stmt->param_count;
         ++to)
    {
      *to= param_iterator++;
    }
  }
  return 0;
}


/*
  Parse the query and send the total number of parameters 
  and resultset metadata information back to client (if any), 
  without executing the query i.e. without any log/disk 
  writes. This will allow the queries to be re-executed 
  without re-parsing during execute. 

  If parameter markers are found in the query, then store 
  the information using Item_param along with maintaining a
  list in lex->param_array, so that a fast and direct
  retrieval can be made without going through all field
  items.
*/

void mysql_stmt_prepare(THD *thd, char *packet, uint packet_length)
{
  LEX *lex;
  Prepared_statement *stmt= new Prepared_statement(thd);
  int error;
  DBUG_ENTER("mysql_stmt_prepare");

  DBUG_PRINT("pquery", ("%s", packet));

  if (stmt == 0)
  {
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_VOID_RETURN;
  }

  if (thd->stmt_map.insert(stmt))
  {
    delete stmt;
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_VOID_RETURN;
  }

  thd->stmt_backup.set_statement(thd);
  thd->stmt_backup.set_item_arena(thd);
  thd->set_statement(stmt);
  thd->set_item_arena(stmt);

  if (alloc_query(thd, packet, packet_length))
  {
    stmt->set_statement(thd);
    stmt->set_item_arena(thd);
    thd->set_statement(&thd->stmt_backup);
    thd->set_item_arena(&thd->stmt_backup);
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_VOID_RETURN;
  }

  mysql_log.write(thd, COM_PREPARE, "%s", packet);

  thd->current_statement= stmt;
  lex= lex_start(thd, (uchar *) thd->query, thd->query_length);
  mysql_init_query(thd);
  lex->safe_to_cache_query= 0;

  error= yyparse((void *)thd) || thd->is_fatal_error ||
         init_param_array(stmt) ||
         send_prepare_results(stmt);

  /* restore to WAIT_PRIOR: QUERY_PRIOR is set inside alloc_query */
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);
  lex_end(lex);
  stmt->set_statement(thd);
  stmt->set_item_arena(thd);
  thd->set_statement(&thd->stmt_backup);
  thd->set_item_arena(&thd->stmt_backup);
  thd->current_statement= 0;

  if (error)
  {
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
    /* error is sent inside yyparse/send_prepare_results */
  }
  else
  {
    SELECT_LEX *sl= stmt->lex->all_selects_list;
    /*
      Save WHERE clause pointers, because they may be changed during query
      optimisation.
    */
    for (; sl; sl= sl->next_select_in_list())
    {
      sl->prep_where= sl->where;
    }
  }
  DBUG_VOID_RETURN;
}

/* Reinit statement before execution */

static void reset_stmt_for_execute(Prepared_statement *stmt)
{
  THD *thd= stmt->thd;
  SELECT_LEX *sl= stmt->lex->all_selects_list;

  for (; sl; sl= sl->next_select_in_list())
  {
    /*
      Copy WHERE clause pointers to avoid damaging they by optimisation
    */
    if (sl->prep_where)
      sl->where= sl->prep_where->copy_andor_structure(thd);
    DBUG_ASSERT(sl->join == 0);
    ORDER *order;
    /* Fix GROUP list */
    for (order= (ORDER *)sl->group_list.first; order; order= order->next)
      order->item= &order->item_ptr;
    /* Fix ORDER list */
    for (order= (ORDER *)sl->order_list.first; order; order= order->next)
      order->item= &order->item_ptr;

    /*
      TODO: When the new table structure is ready, then have a status bit 
      to indicate the table is altered, and re-do the setup_* 
      and open the tables back.
    */
    for (TABLE_LIST *tables= (TABLE_LIST*) sl->table_list.first;
	 tables;
	 tables= tables->next)
    {
      /*
        Reset old pointers to TABLEs: they are not valid since the tables
        were closed in the end of previous prepare or execute call.
      */
      tables->table= 0;
      tables->table_list= 0;
    }
    
    {
      SELECT_LEX_UNIT *unit= sl->master_unit();
      unit->unclean();
      unit->types.empty();
      /* for derived tables & PS (which can't be reset by Item_subquery) */
      unit->reinit_exec_mechanism();
    }
  }
}

/*
  Executes previously prepared query.
  If there is any parameters, then replace markers with the data supplied
  from client, and then execute the query.
  SYNOPSYS
    mysql_stmt_execute()
*/


void mysql_stmt_execute(THD *thd, char *packet, uint packet_length)
{
  ulong stmt_id= uint4korr(packet);
  uchar *packet_end= (uchar *) packet + packet_length - 1;
  Prepared_statement *stmt;

  DBUG_ENTER("mysql_stmt_execute");

  packet+= 9;                               /* stmt_id + 5 bytes of flags */
  
  if (!(stmt= find_prepared_statement(thd, stmt_id, "execute", SEND_ERROR)))
    DBUG_VOID_RETURN;

  DBUG_PRINT("equery:", ("%s", stmt->query));

  /* Check if we got an error when sending long data */
  if (stmt->get_longdata_error)
  {
    send_error(thd, stmt->last_errno, stmt->last_error);
    DBUG_VOID_RETURN;
  }

  thd->stmt_backup.set_statement(thd);
  thd->set_statement(stmt);

  reset_stmt_for_execute(stmt);

#ifndef EMBEDDED_LIBRARY
  if (stmt->param_count)
  {
    uchar *null_array= (uchar *) packet;
    if (setup_conversion_functions(stmt, (uchar **) &packet, packet_end) ||
        stmt->set_params(stmt, null_array, (uchar *) packet, packet_end)) 
      goto set_params_data_err;
  }
#else
  /*
    In embedded library we re-install conversion routines each time 
    we set params, and also we don't need to parse packet. 
    So we do it in one function.
  */
  if (stmt->param_count && stmt->set_params_data(stmt))
    goto set_params_data_err;
#endif

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
 
  /*
    TODO:
    Also, have checks on basic executions such as mysql_insert(), 
    mysql_delete(), mysql_update() and mysql_select() to not to 
    have re-check on setup_* and other things ..
  */
  thd->protocol= &thd->protocol_prep;           // Switch to binary protocol
  mysql_execute_command(thd);
  thd->lex->unit.cleanup();
  thd->protocol= &thd->protocol_simple;         // Use normal protocol

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);

  cleanup_items(stmt->free_list);
  close_thread_tables(thd); // to close derived tables
  thd->set_statement(&thd->stmt_backup);
  DBUG_VOID_RETURN;

set_params_data_err:
  thd->set_statement(&thd->stmt_backup);
  my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysql_execute");
  send_error(thd);
  DBUG_VOID_RETURN;
}


/*
    Reset a prepared statement, in case there was an error in send_longdata.
    Note: we don't send any reply to that command.
  SYNOPSIS
    mysql_stmt_reset()
    thd		Thread handle
    packet	Packet with stmt id 

  DESCRIPTION
    This function is useful when one gets an error after calling
    mysql_stmt_getlongdata() and wants to reset the handle
    so that one can call execute again.
    See also bug #1664
*/

void mysql_stmt_reset(THD *thd, char *packet)
{
  /* There is always space for 4 bytes in buffer */
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;
  
  DBUG_ENTER("mysql_stmt_reset");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "reset", DONT_SEND_ERROR)))
    DBUG_VOID_RETURN;

  stmt->get_longdata_error= 0;

  /* Free long data if used */
  if (stmt->long_data_used)
  {
    Item_param **item= stmt->param_array;
    Item_param **end= item + stmt->param_count;
    stmt->long_data_used= 0;
    for (; item < end ; item++)
      (**item).reset();
  }
  DBUG_VOID_RETURN;
}


/*
  Delete a prepared statement from memory.
  Note: we don't send any reply to that command. 
*/

void mysql_stmt_free(THD *thd, char *packet)
{
  /* There is always space for 4 bytes in packet buffer */
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;

  DBUG_ENTER("mysql_stmt_free");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "close", DONT_SEND_ERROR)))
    DBUG_VOID_RETURN;

  /* Statement map deletes statement on erase */
  thd->stmt_map.erase(stmt);
  DBUG_VOID_RETURN;
}


/*
  Long data in pieces from client

  SYNOPSIS
    mysql_stmt_get_longdata()
    thd			Thread handle
    pos			String to append
    packet_length	Length of string

  DESCRIPTION
    Get a part of a long data.
    To make the protocol efficient, we are not sending any return packages
    here.
    If something goes wrong, then we will send the error on 'execute'

    We assume that the client takes care of checking that all parts are sent
    to the server. (No checking that we get a 'end of column' in the server)
*/

void mysql_stmt_get_longdata(THD *thd, char *pos, ulong packet_length)
{
  Prepared_statement *stmt;
  
  DBUG_ENTER("mysql_stmt_get_longdata");

#ifndef EMBEDDED_LIBRARY
  /* The following should never happen */
  if (packet_length < MYSQL_LONG_DATA_HEADER+1)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "get_longdata");
    DBUG_VOID_RETURN;
  }
#endif

  ulong stmt_id=     uint4korr(pos);
  uint param_number= uint2korr(pos+4);

  if (!(stmt=find_prepared_statement(thd, stmt_id, "get_longdata",
                                     DONT_SEND_ERROR)))
    DBUG_VOID_RETURN;

#ifndef EMBEDDED_LIBRARY
  if (param_number >= stmt->param_count)
  {
    /* Error will be sent in execute call */
    stmt->get_longdata_error= 1;
    stmt->last_errno= ER_WRONG_ARGUMENTS;
    sprintf(stmt->last_error, ER(ER_WRONG_ARGUMENTS), "get_longdata");
    DBUG_VOID_RETURN;
  }
  pos+= MYSQL_LONG_DATA_HEADER;	// Point to data
#endif

  Item_param *param= stmt->param_array[param_number];
#ifndef EMBEDDED_LIBRARY
  param->set_longdata(pos, packet_length-MYSQL_LONG_DATA_HEADER-1);
#else
  param->set_longdata(thd->extra_data, thd->extra_length);
#endif
  stmt->long_data_used= 1;
  DBUG_VOID_RETURN;
}


Prepared_statement::Prepared_statement(THD *thd_arg)
  :Statement(thd_arg),
  thd(thd_arg),
  param_array(0),
  param_count(0),
  last_errno(0),
  get_longdata_error(0),
  long_data_used(0),
  log_full_query(0)
{
  *last_error= '\0';
  if (mysql_bin_log.is_open())
  {
    log_full_query= 1;
#ifndef EMBEDDED_LIBRARY
    set_params= insert_params_withlog;
#else
    set_params_data= emb_insert_params_withlog;
#endif
  }
  else
#ifndef EMBEDDED_LIBRARY
    set_params= insert_params;
#else
    set_params_data= emb_insert_params;
#endif
}


Prepared_statement::~Prepared_statement()
{
  free_items(free_list);
}


Statement::Type Prepared_statement::type() const
{
  return PREPARED_STATEMENT;
}

