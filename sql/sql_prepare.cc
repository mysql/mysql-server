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

#define IS_PARAM_NULL(pos, param_no) (pos[param_no/8] & (1 << (param_no & 7)))

#define STMT_QUERY_LOG_LENGTH 8192

#ifdef EMBEDDED_LIBRARY
#define SETUP_PARAM_FUNCTION(fn_name) \
static void fn_name(Item_param *param, uchar **pos, ulong data_len)
#else
#define SETUP_PARAM_FUNCTION(fn_name) \
static void fn_name(Item_param *param, uchar **pos)
#endif

String my_null_string("NULL", 4, default_charset_info);

/*
  Find prepared statement in thd

  SYNOPSIS
    find_prepared_statement()
    thd		Thread handler
    stmt_id	Statement id server specified to the client on prepare

  RETURN VALUES
    0		error.  In this case the error is sent with my_error()
    ptr 	Pointer to statement
*/

static PREP_STMT *find_prepared_statement(THD *thd, ulong stmt_id,
					  const char *when)
{
  PREP_STMT *stmt;
  DBUG_ENTER("find_prepared_statement");
  DBUG_PRINT("enter",("stmt_id: %d", stmt_id));

  if (thd->last_prepared_stmt && thd->last_prepared_stmt->stmt_id == stmt_id)
    DBUG_RETURN(thd->last_prepared_stmt);
  if ((stmt= (PREP_STMT*) tree_search(&thd->prepared_statements, &stmt_id,
				      (void*) 0)))
    DBUG_RETURN (thd->last_prepared_stmt= stmt);
  my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), stmt_id, when);
  DBUG_RETURN(0);
}

/*
  Compare two prepared statements;  Used to find a prepared statement
*/

int compare_prep_stmt(void *not_used, PREP_STMT *stmt, ulong *key)
{
  return (stmt->stmt_id == *key) ? 0 : (stmt->stmt_id < *key) ? -1 : 1;
}


/*
  Free prepared statement.

  SYNOPSIS
    standard tree_element_free function.

  DESCRIPTION
    We don't have to free the stmt itself as this was stored in the tree
    and will be freed when the node is deleted
*/

void free_prep_stmt(PREP_STMT *stmt, TREE_FREE mode, void *not_used)
{     
  my_free((char *)stmt->param, MYF(MY_ALLOW_ZERO_PTR));
  if (stmt->query)
    stmt->query->free();
  free_items(stmt->free_list);
  free_root(&stmt->mem_root, MYF(0));
}

/*
  Send prepared stmt info to client after prepare
*/

#ifndef EMBEDDED_LIBRARY
static bool send_prep_stmt(PREP_STMT *stmt, uint columns)
{
  NET  *net=&stmt->thd->net;
  char buff[9];
  buff[0]= 0;
  int4store(buff+1, stmt->stmt_id);
  int2store(buff+5, columns);
  int2store(buff+7, stmt->param_count);
  /* This should be fixed to work with prepared statements
   */
  return (my_net_write(net, buff, sizeof(buff)) || net_flush(net));
}
#else
static bool send_prep_stmt(PREP_STMT *stmt, uint columns __attribute__((unused)))
{
  THD *thd= stmt->thd;

  thd->client_stmt_id= stmt->stmt_id;
  thd->client_param_count= stmt->param_count;
  thd->net.last_errno= 0;

  return 0;
}
#endif /*!EMBEDDED_LIBRAYR*/

/*
  Send information about all item parameters

  TODO: Not yet ready
*/

static bool send_item_params(PREP_STMT *stmt)
{
#if 0
  char buff[1];
  buff[0]=0;
  if (my_net_write(&stmt->thd->net, buff, sizeof(buff))) 
    return 1;
  send_eof(stmt->thd);
#endif
  return 0;
}

/*
  Read the length of the parameter data and retun back to   
  caller by positing the pointer to param data              
*/

#ifndef EMBEDDED_LIBRARY
static ulong get_param_length(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (ulong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (ulong) uint3korr(pos+1);
  }
  (*packet)+=9; // Must be 254 when here 
  return (ulong) uint4korr(pos+1);
}
#else
#define get_param_length(A) data_len
#endif /*!EMBEDDED_LIBRARY*/

 /*
  Setup param conversion routines

  setup_param_xx()
  param   Parameter Item
  pos     Input data buffer

  All these functions reads the data from pos and sets up that data
  through 'param' and advances the buffer position to predifined
  length position.

  Make a note that the NULL handling is examined at first execution
  (i.e. when input types altered) and for all subsequent executions
  we don't read any values for this.

  RETURN VALUES
    
*/

SETUP_PARAM_FUNCTION(setup_param_tiny)
{
  param->set_int((longlong)(**pos));
  *pos+= 1;
}

SETUP_PARAM_FUNCTION(setup_param_short)
{
  param->set_int((longlong)sint2korr(*pos));
  *pos+= 2;
}

SETUP_PARAM_FUNCTION(setup_param_int32)
{
  param->set_int((longlong)sint4korr(*pos));
  *pos+= 4;
}

SETUP_PARAM_FUNCTION(setup_param_int64)
{
  param->set_int((longlong)sint8korr(*pos));
  *pos+= 8;
}

SETUP_PARAM_FUNCTION(setup_param_float)
{
  float data;
  float4get(data,*pos);
  param->set_double((double) data);
  *pos+= 4;
}

SETUP_PARAM_FUNCTION(setup_param_double)
{
  double data;
  float8get(data,*pos);
  param->set_double((double) data);
  *pos+= 8;
}

SETUP_PARAM_FUNCTION(setup_param_time)
{
  ulong length;

  if ((length= get_param_length(pos)))
  {
    uchar *to= *pos;
    TIME  tm;   
    
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

SETUP_PARAM_FUNCTION(setup_param_datetime)
{
  uint length= get_param_length(pos);
 
  if (length)
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

SETUP_PARAM_FUNCTION(setup_param_date)
{
  ulong length;
 
  if ((length= get_param_length(pos)))
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

SETUP_PARAM_FUNCTION(setup_param_str)
{
  ulong len= get_param_length(pos);
  param->set_value((const char *)*pos, len);
  *pos+= len;        
}

void setup_param_functions(Item_param *param, uchar param_type)
{
  switch (param_type) {
  case FIELD_TYPE_TINY:
    param->setup_param_func= setup_param_tiny;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_SHORT:
    param->setup_param_func= setup_param_short;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_LONG:
    param->setup_param_func= setup_param_int32;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_LONGLONG:
    param->setup_param_func= setup_param_int64;
    param->item_result_type= INT_RESULT;
    break;
  case FIELD_TYPE_FLOAT:
    param->setup_param_func= setup_param_float;
    param->item_result_type= REAL_RESULT;
    break;
  case FIELD_TYPE_DOUBLE:
    param->setup_param_func= setup_param_double;
    param->item_result_type= REAL_RESULT;
    break;
  case FIELD_TYPE_TIME:
    param->setup_param_func= setup_param_time;
    param->item_result_type= STRING_RESULT;
    break;
  case FIELD_TYPE_DATE:
    param->setup_param_func= setup_param_date;
    param->item_result_type= STRING_RESULT;
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    param->setup_param_func= setup_param_datetime;
    param->item_result_type= STRING_RESULT;
    break;
  default:
    param->setup_param_func= setup_param_str;
    param->item_result_type= STRING_RESULT;
  }
}

#ifndef EMBEDDED_LIBRARY
/*
  Update the parameter markers by reading data from client packet 
  and if binary/update log is set, generate the valid query.
*/

static bool insert_params_withlog(PREP_STMT *stmt, uchar *pos, uchar *read_pos)
{
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  DBUG_ENTER("insert_params_withlog"); 
  
  String str, query, *res;

  if (query.copy(*stmt->query))
    DBUG_RETURN(1);
  
  ulong param_no= 0;  
  uint32 length= 0;
  
  while ((param= (Item_param *)param_iterator++))
  {
    if (param->long_data_supplied)
      res= param->query_val_str(&str);       
    
    else
    {
      if (IS_PARAM_NULL(pos,param_no))
      {
        param->maybe_null= param->null_value= 1;
        res= &my_null_string;
      }
      else
      {
        param->maybe_null= param->null_value= 0;
        param->setup_param_func(param,&read_pos);
        res= param->query_val_str(&str);
      }
    }
    if (query.replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);
    
    length+= res->length()-1;
    param_no++;
  }
  if (alloc_query(thd, (char *)query.ptr(), query.length()+1))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

static bool insert_params(PREP_STMT *stmt, uchar *pos, uchar *read_pos)
{
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  DBUG_ENTER("insert_params"); 
  
  ulong param_no= 0;  
  while ((param= (Item_param *)param_iterator++))
  {
    if (!param->long_data_supplied)   
    {
      if (IS_PARAM_NULL(pos,param_no))
        param->maybe_null= param->null_value= 1;
      else
      {
        param->maybe_null= param->null_value= 0;
        param->setup_param_func(param,&read_pos);
      }
    }
    param_no++;
  }
  DBUG_RETURN(0);
}

static bool setup_params_data(PREP_STMT *stmt)
{                                       
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  DBUG_ENTER("setup_params_data");

  uchar *pos=(uchar*) thd->net.read_pos+1+MYSQL_STMT_HEADER; //skip header
  uchar *read_pos= pos+(stmt->param_count+7) / 8; //skip null bits   

  if (*read_pos++) //types supplied / first execute
  {              
    /*
      First execute or types altered by the client, setup the 
      conversion routines for all parameters (one time)
    */
    while ((param= (Item_param *)param_iterator++))
    {       
      setup_param_functions(param,*read_pos);
      read_pos+= 2;
    }
    param_iterator.rewind();
  }    
  stmt->setup_params(stmt,pos,read_pos);
  DBUG_RETURN(0);
}

#endif /*!EMBEDDED_LIBRARY*/

/*
  Validate the following information for INSERT statement:                         
    - field existance           
    - fields count                          
*/

static bool mysql_test_insert_fields(PREP_STMT *stmt,
				     TABLE_LIST *table_list,
				     List<Item> &fields, 
				     List<List_item> &values_list)
{
  THD *thd= stmt->thd;
  TABLE *table;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  DBUG_ENTER("mysql_test_insert_fields");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  my_bool update=(thd->lex.value_list.elements ? UPDATE_ACL : 0);
  ulong privilege= (thd->lex.duplicates == DUP_REPLACE ?
                    INSERT_ACL | DELETE_ACL : INSERT_ACL | update);

  if (check_access(thd,privilege,table_list->db,
                   &table_list->grant.privilege,0,0) || 
      (grant_option && check_grant(thd,privilege,table_list)))
    DBUG_RETURN(1); 
#endif  
  if (open_and_lock_tables(thd, table_list))
    DBUG_RETURN(1); 
  table= table_list->table;

  if ((values= its++))
  {
    uint value_count;
    ulong counter= 0;
    
    if (check_insert_fields(thd,table,fields,*values,1))
      DBUG_RETURN(1);

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
        DBUG_RETURN(1);
      }
    }
  }
  if (send_prep_stmt(stmt, 0) || send_item_params(stmt))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Validate the following information                         
    UPDATE - set and where clause    DELETE - where clause                                             
                                                             
  And send update-set clause column list fields info 
  back to client. For DELETE, just validate where clause 
  and return no fields information back to client.
*/

static bool mysql_test_upd_fields(PREP_STMT *stmt, TABLE_LIST *table_list,
				  List<Item> &fields, List<Item> &values,
				  COND *conds)
{
  THD *thd= stmt->thd;
  DBUG_ENTER("mysql_test_upd_fields");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_access(thd,UPDATE_ACL,table_list->db,
                   &table_list->grant.privilege,0,0) || 
      (grant_option && check_grant(thd,UPDATE_ACL,table_list)))
    DBUG_RETURN(1);
#endif
  if (open_and_lock_tables(thd, table_list))
    DBUG_RETURN(1);
  if (setup_tables(table_list) ||
      setup_fields(thd, 0, table_list, fields, 1, 0, 0) || 
      setup_conds(thd, table_list, &conds) || thd->net.report_error)      
    DBUG_RETURN(1);

  /* 
     Currently return only column list info only, and we are not
     sending any info on where clause.
  */
  if (send_prep_stmt(stmt, 0) || send_item_params(stmt))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

/*
  Validate the following information:                         

    SELECT - column list 
           - where clause
           - order clause
           - having clause
           - group by clause
           - if no column spec i.e. '*', then setup all fields
                                                           
  And send column list fields info back to client. 
*/
static bool mysql_test_select_fields(PREP_STMT *stmt, TABLE_LIST *tables,
				     uint wild_num,
                                     List<Item> &fields, COND *conds, 
                                     uint og_num, ORDER *order, ORDER *group,
                                     Item *having, ORDER *proc,
                                     ulong select_options, 
                                     SELECT_LEX_UNIT *unit,
                                     SELECT_LEX *select_lex)
{
  THD *thd= stmt->thd;
  LEX *lex= &thd->lex;
  select_result *result= thd->lex.result;
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
  if ((&lex->select_lex != lex->all_selects_list &&
       lex->unit.create_total_list(thd, lex, &tables, 0)))
   DBUG_RETURN(1);
    
  if (open_and_lock_tables(thd, tables))
    DBUG_RETURN(1);

  if (lex->describe)
  {
    if (send_prep_stmt(stmt, 0) ||  send_item_params(stmt))
      DBUG_RETURN(1);      
  }   
  else 
  {
    fix_tables_pointers(thd->lex.all_selects_list);
    if (!result && !(result= new select_send()))
    {
      delete select_lex->having;
      delete select_lex->where;
      send_error(thd, ER_OUT_OF_RESOURCES);
      DBUG_RETURN(1);
    }

    JOIN *join= new JOIN(thd, fields, select_options, result);
    thd->used_tables= 0;	// Updated by setup_fields  

  if (join->prepare(&select_lex->ref_pointer_array, tables, 
		    wild_num, conds, og_num, order, group, having, proc, 
                    select_lex, unit))
    DBUG_RETURN(1);
    if (send_prep_stmt(stmt, fields.elements) ||
        thd->protocol_simple.send_fields(&fields, 0) ||
#ifndef EMBEDDED_LIBRARY
        net_flush(&thd->net) ||
#endif
        send_item_params(stmt))
      DBUG_RETURN(1);
    join->cleanup();
  }
  DBUG_RETURN(0);  
}


/*
  Send the prepare query results back to client              
*/
                     
static bool send_prepare_results(PREP_STMT *stmt)     
{   
  THD *thd= stmt->thd;
  LEX *lex= &thd->lex;
  enum enum_sql_command sql_command= thd->lex.sql_command;
  DBUG_ENTER("send_prepare_results");
  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, lex->param_count));
  
  /* Setup prepared stmt */
  stmt->param_count= lex->param_count;
  stmt->free_list= thd->free_list;		// Save items used in stmt
  thd->free_list= 0;

  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *tables=(TABLE_LIST*) select_lex->table_list.first;
  
  switch (sql_command) {

  case SQLCOM_INSERT:
    if (mysql_test_insert_fields(stmt, tables, lex->field_list,
				 lex->many_values))
      goto abort;    
    break;

  case SQLCOM_UPDATE:
    if (mysql_test_upd_fields(stmt, tables, select_lex->item_list,
			      lex->value_list, select_lex->where))
      goto abort;
    break;

  case SQLCOM_DELETE:
    if (mysql_test_upd_fields(stmt, tables, select_lex->item_list,
			      lex->value_list, select_lex->where))
      goto abort;
    break;

  case SQLCOM_SELECT:
    if (mysql_test_select_fields(stmt, tables, select_lex->with_wild,
                                 select_lex->item_list,
                                 select_lex->where,
				 select_lex->order_list.elements +
				 select_lex->group_list.elements,
                                 (ORDER*) select_lex->order_list.first,
                                 (ORDER*) select_lex->group_list.first, 
                                 select_lex->having,
                                 (ORDER*)lex->proc_list.first,
                                 select_lex->options | thd->options,
                                 &(lex->unit), select_lex))
      goto abort;
    break;

  default:
    {
      /* 
         Rest fall through to default category, no parsing 
         for non-DML statements 
      */
      if (send_prep_stmt(stmt, 0))
        goto abort;
    }
  }
  DBUG_RETURN(0);

abort:
  send_error(thd,thd->killed ? ER_SERVER_SHUTDOWN : 0);
  DBUG_RETURN(1);
}

/*
  Parse the prepare query                                    
*/

static bool parse_prepare_query(PREP_STMT *stmt,
		char *packet, uint length)
{
  bool error= 1;
  THD *thd= stmt->thd;
  DBUG_ENTER("parse_prepare_query");

  mysql_log.write(thd,COM_PREPARE,"%s",packet);       
  mysql_init_query(thd);   
  LEX *lex=lex_start(thd, (uchar*) packet, length);
  lex->safe_to_cache_query= 0;
  thd->lex.param_count= 0;
  if (!yyparse((void *)thd) && !thd->is_fatal_error) 
    error= send_prepare_results(stmt);
  lex_end(lex);
  DBUG_RETURN(error);
}

/*
  Initialize parameter items in statement
*/

static bool init_param_items(PREP_STMT *stmt)
{
  THD *thd= stmt->thd;
  List<Item> &params= thd->lex.param_list;
  Item_param **to;
  uint32 length= thd->query_length;
 
  stmt->lex=  thd->lex;

  if (mysql_bin_log.is_open() || mysql_update_log.is_open())
  {
    stmt->log_full_query= 1;
#ifndef EMBEDDED_LIBRARY
    stmt->setup_params= insert_params_withlog;
#else
    stmt->setup_params_data= setup_params_data_withlog;
#endif
  }
  else
#ifndef EMBEDDED_LIBRARY
    stmt->setup_params= insert_params; // not fully qualified query
#else
    stmt->setup_params_data= setup_params_data;
#endif
   
  if (!stmt->param_count)
    stmt->param= (Item_param **)0;
  else
  {    
    if (!(stmt->param= to= (Item_param **)
          my_malloc(sizeof(Item_param *)*(stmt->param_count+1), 
                    MYF(MY_WME))))
      return 1;

    if (stmt->log_full_query)
    {
      length= thd->query_length+(stmt->param_count*2)+1;
 
      if ( length < STMT_QUERY_LOG_LENGTH ) 
        length= STMT_QUERY_LOG_LENGTH;
    }
    List_iterator<Item> param_iterator(params);
    while ((*(to++)= (Item_param *)param_iterator++));
  }  
  stmt->query= new String(length);
  stmt->query->copy(thd->query, thd->query_length, default_charset_info);
  return 0;
}

/*
  Initialize stmt execution
*/

static void init_stmt_execute(PREP_STMT *stmt)
{
  THD *thd= stmt->thd;
  TABLE_LIST *tables= (TABLE_LIST*) thd->lex.select_lex.table_list.first;
  
  /*
  TODO: When the new table structure is ready, then have a status bit 
        to indicate the table is altered, and re-do the setup_* 
        and open the tables back.
  */  
  for (; tables ; tables= tables->next)
    tables->table= 0; //safety - nasty init
  
  if (!(stmt->log_full_query && stmt->param_count))
  {
    thd->query= stmt->query->c_ptr();
    thd->query_length= stmt->query->length();
  }
}

/*
  Parse the query and send the total number of parameters 
  and resultset metadata information back to client (if any), 
  without executing the query i.e. with out any log/disk 
  writes. This will allow the queries to be re-executed 
  without re-parsing during execute.          
                                                              
  If parameter markers are found in the query, then store    
  the information using Item_param along with maintaining a  
  list in lex->param_list, so that a fast and direct         
  retrieveal can be made without going through all field     
  items.                                                     
*/

bool mysql_stmt_prepare(THD *thd, char *packet, uint packet_length)
{
  MEM_ROOT thd_root= thd->mem_root;
  PREP_STMT stmt;
  SELECT_LEX *sl;
  DBUG_ENTER("mysql_stmt_prepare");

  bzero((char*) &stmt, sizeof(stmt));
  
  stmt.stmt_id= ++thd->current_stmt_id;
  init_sql_alloc(&stmt.mem_root,
		 thd->variables.query_alloc_block_size,
		 thd->variables.query_prealloc_size);
  
  stmt.thd= thd;
  stmt.thd->mem_root= stmt.mem_root;

  if (alloc_query(stmt.thd, packet, packet_length))
    goto err;

  if (parse_prepare_query(&stmt, thd->query, thd->query_length))
    goto err;

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);

  // save WHERE clause pointers to avoid damaging they by optimisation
  for (sl= thd->lex.all_selects_list;
       sl;
       sl= sl->next_select_in_list())
  {
    sl->prep_where= sl->where;
  }

  
  if (init_param_items(&stmt))
    goto err;

  
  stmt.mem_root= stmt.thd->mem_root;
  tree_insert(&thd->prepared_statements, (void *)&stmt, 0, (void *)0);
  thd->mem_root= thd_root; // restore main mem_root
  DBUG_RETURN(0);

err:
  stmt.mem_root= stmt.thd->mem_root;  
  free_prep_stmt(&stmt, free_free, (void*) 0);
  thd->mem_root= thd_root;	// restore main mem_root
  DBUG_RETURN(1);
}


/*
  Executes previously prepared query

  If there is any parameters(thd->param_count), then replace 
  markers with the data supplied from client, and then       
  execute the query                                            
*/

void mysql_stmt_execute(THD *thd, char *packet)
{
  ulong stmt_id= uint4korr(packet);
  PREP_STMT *stmt;
  SELECT_LEX *sl;
  DBUG_ENTER("mysql_stmt_execute");

  if (!(stmt=find_prepared_statement(thd, stmt_id, "execute")))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  /* Check if we got an error when sending long data */
  if (stmt->error_in_prepare)
  {
    send_error(thd, stmt->last_errno, stmt->last_error);
    DBUG_VOID_RETURN;
  }

  LEX thd_lex= thd->lex;
  thd->lex= stmt->lex;
  
  for (sl= stmt->lex.all_selects_list;
       sl;
       sl= sl->next_select_in_list())
  {
    /*
      Copy WHERE clause pointers to avoid damaging they by optimisation
    */
    if (sl->prep_where)
      sl->where= sl->prep_where->copy_andor_structure(thd);
    DBUG_ASSERT(sl->join == 0);
  }
  init_stmt_execute(stmt);

#ifndef EMBEDDED_LIBRARY
  if (stmt->param_count && setup_params_data(stmt))
    DBUG_VOID_RETURN;
#else
  if (stmt->param_count && (*stmt->setup_params_data)(stmt))
    DBUG_VOID_RETURN;
#endif

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);  
 
  /*
    TODO:
    Also, have checks on basic executions such as mysql_insert(), 
    mysql_delete(), mysql_update() and mysql_select() to not to 
    have re-check on setup_* and other things ..
  */  
  thd->protocol= &thd->protocol_prep;		// Switch to binary protocol
  mysql_execute_command(thd);
  thd->protocol= &thd->protocol_simple;	// Use normal protocol

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);

  thd->lex= thd_lex;
  DBUG_VOID_RETURN;
}


/*
  Reset a prepared statement
  
  SYNOPSIS
    mysql_stmt_reset()
    thd		Thread handle
    packet	Packet with stmt handle

  DESCRIPTION
    This function is useful when one gets an error after calling
    mysql_stmt_getlongdata() and one wants to reset the handle
    so that one can call execute again.
*/

void mysql_stmt_reset(THD *thd, char *packet)
{
  ulong stmt_id= uint4korr(packet);
  PREP_STMT *stmt;
  DBUG_ENTER("mysql_stmt_reset");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "reset")))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  stmt->error_in_prepare= 0;
  Item_param *item= *stmt->param, *end= item + stmt->param_count;

  /* Free long data if used */
  if (stmt->long_data_used)
  {
    stmt->long_data_used= 0;
    for (; item < end ; item++)
      item->reset();
  }
  DBUG_VOID_RETURN;
}


/*
  Delete a prepared statement from memory
*/

void mysql_stmt_free(THD *thd, char *packet)
{
  ulong stmt_id= uint4korr(packet);
  DBUG_ENTER("mysql_stmt_free");

  if (!find_prepared_statement(thd, stmt_id, "close"))
  {
    send_error(thd); // Not seen by the client
    DBUG_VOID_RETURN;
  }
  tree_delete(&thd->prepared_statements, (void*) &stmt_id, (void *)0);
  thd->last_prepared_stmt= (PREP_STMT *)0;
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
  PREP_STMT *stmt;
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

  if (!(stmt=find_prepared_statement(thd, stmt_id, "get_longdata")))
  {
    /*
      There is a chance that the client will never see this as
      it doesn't expect an answer from this call...
    */
    send_error(thd);
    DBUG_VOID_RETURN;
  }

#ifndef EMBEDDED_LIBRARY
  if (param_number >= stmt->param_count)
  {
    /* Error will be sent in execute call */
    stmt->error_in_prepare= 1;
    stmt->last_errno= ER_WRONG_ARGUMENTS;
    sprintf(stmt->last_error, ER(ER_WRONG_ARGUMENTS), "get_longdata");
    DBUG_VOID_RETURN;
  }
  pos+= MYSQL_LONG_DATA_HEADER;	// Point to data
#endif

  Item_param *param= *(stmt->param+param_number);
#ifndef EMBEDDED_LIBRARY
  param->set_longdata(pos, packet_length-MYSQL_LONG_DATA_HEADER-1);
#else
  param->set_longdata(thd->extra_data, thd->extra_length);
#endif
  stmt->long_data_used= 1;
  DBUG_VOID_RETURN;
}

