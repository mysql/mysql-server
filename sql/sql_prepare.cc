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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/**********************************************************************
This file contains the implementation of prepare and executes. 

Prepare:

  - Server gets the query from client with command 'COM_PREPARE'
  - Parse the query and recognize any parameter markers '?' and 
    store its information list lex->param_list
  - Without executing the query, return back to client the total 
    number of parameters along with result-set metadata information
    (if any)
     
Prepare-execute:

  - Server gets the command 'COM_EXECUTE' to execute the 
    previously prepared query.
  - If there is are any parameters, then replace the markers with the 
    data supplied by client with the following format:
    [types_specified(0/1)][type][length][data] .. [type][length]..
  - Execute the query without re-parsing and send back the results 
    to client

Long data handling:
  - Server gets the long data in pieces with command type 'COM_LONG_DATA'.
  - The packet recieved will have the format as:
    [COM_LONG_DATA:1][parameter_number:2][type:2][data]
  - Checks if the type is specified by client, and if yes reads the type, 
    and stores the data in that format.
  - It's up to the client to check for read data ended. The server doesn't
    care.

***********************************************************************/

#include "mysql_priv.h"
#include "sql_acl.h"
#include <assert.h> // for DEBUG_ASSERT()
#include <m_ctype.h>  // for isspace()

extern int yyparse(void);
static ulong get_param_length(uchar **packet);
static uint get_buffer_type(uchar **packet);
static bool param_is_null(uchar **packet);
static bool setup_param_fields(THD *thd,List<Item> &params);
static uchar* setup_param_field(Item_param *item_param, uchar *pos,
				uint buffer_type);
static void setup_longdata_field(Item_param *item_param, uchar *pos);
static bool setup_longdata(THD *thd,List<Item> &params);
static bool send_prepare_results(PREP_STMT *stmt);
static bool parse_prepare_query(PREP_STMT *stmt, char *packet, uint length);
static bool mysql_send_insert_fields(PREP_STMT *stmt, TABLE_LIST *table_list, 
				     List<Item> &fields,
				     List<List_item> &values_list,
				     thr_lock_type lock_type);
static bool mysql_test_insert_fields(PREP_STMT *stmt, TABLE_LIST *table_list, 
				     List<Item> &fields,
				     List<List_item> &values_list,
				     thr_lock_type lock_type);
static bool mysql_test_upd_fields(PREP_STMT *stmt, TABLE_LIST *table_list,
				  List<Item> &fields, List<Item> &values,
				  COND *conds,thr_lock_type lock_type);
static bool mysql_test_select_fields(PREP_STMT *stmt, TABLE_LIST *tables,
				     List<Item> &fields, List<Item> &values,
				     COND *conds, ORDER *order, ORDER *group,
				     Item *having,thr_lock_type lock_type);


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

int compare_prep_stmt(PREP_STMT *a, PREP_STMT *b, void *not_used)
{
  return (a->stmt_id < b->stmt_id) ? -1 : (a->stmt_id == b->stmt_id) ? 0 : 1;
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
  free_root(&stmt->mem_root, MYF(0));
  free_items(stmt->free_list);
}

/*
  Send prepared stmt info to client after prepare
*/

bool send_prep_stmt(PREP_STMT *stmt, uint columns)
{
  char buff[8];
  int4store(buff, stmt->stmt_id);
  int2store(buff+4, columns);
  int2store(buff+6, stmt->param_count);
  return my_net_write(&stmt->thd->net, buff, sizeof(buff));
}

/*
  Send information about all item parameters

  TODO: Not yet ready
*/

bool send_item_params(PREP_STMT *stmt)
{
  char buff[1];
  buff[0]=0;
  return my_net_write(&stmt->thd->net, buff, sizeof(buff));
}



/*
  Read the buffer type, this happens only first time        
*/

static uint get_buffer_type(uchar **packet)
{
  reg1 uchar *pos= *packet;
  (*packet)+= 2;
  return (uint) uint2korr(pos);
}


/*
  Check for NULL param data

  RETURN VALUES
    0	Value was not NULL
    1	Value was NULL
*/

static bool param_is_null(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos == 251)
  {
    (*packet)++;
    return 1;
  }
  return 0;
}

/*
  Read the length of the parameter data and retun back to   
  caller by positing the pointer to param data              
*/

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

/*
  Read and return the data for parameters supplied by client 
*/

static uchar* setup_param_field(Item_param *item_param, 
				uchar *pos, uint buffer_type)
{
  if (param_is_null(&pos))
  {
    item_param->set_null();
    return(pos);
  }
  switch (buffer_type) {    
  case FIELD_TYPE_TINY:
    item_param->set_int((longlong)(*pos));
    pos += 1;
    break;
  case FIELD_TYPE_SHORT:
    item_param->set_int((longlong)sint2korr(pos));
    pos += 2;
    break;
  case FIELD_TYPE_INT24:
    item_param->set_int((longlong)sint4korr(pos));
    pos += 3;
    break;
  case FIELD_TYPE_LONG:
    item_param->set_int((longlong)sint4korr(pos));
    pos += 4;
    break;
  case FIELD_TYPE_LONGLONG:
    item_param->set_int((longlong)sint8korr(pos));
    pos += 8;
    break;
  case FIELD_TYPE_FLOAT:
    float data;
    float4get(data,pos);
    item_param->set_double((double) data);
    pos += 4;
    break;
  case FIELD_TYPE_DOUBLE:
    double j;
    float8get(j,pos)
    item_param->set_double(j);
    pos += 8;
    break;
  default:
    {      
      ulong len=get_param_length(&pos);
      item_param->set_value((const char*)pos,len);     
      pos+=len;        
    }
  }
  return(pos);
}

/*
  Update the parameter markers by reading the data           
  from client ..                                             
*/

static bool setup_param_fields(THD *thd, PREP_STMT *stmt)
{  
  DBUG_ENTER("setup_param_fields");  
#ifdef READY_TO_BE_USED
  Item_param *item_param;
  ulong param_count=0;
  uchar *pos=(uchar*) thd->net.read_pos+1;// skip command type

 
  if (*pos++) // No types supplied, read only param data
  {
    while ((item_param=(Item_param *)it++) && 
	   (param_count++ < stmt->param_count))
    {
      if (item_param->long_data_supplied)
        continue;

      if (!(pos=setup_param_field(item_param,pos,item_param->buffer_type)))
        DBUG_RETURN(1);
    }
  }
  else // Types supplied, read and store it along with param data 
  {
    while ((item_param=(Item_param *)it++) && 
	   (param_count++ < thd->param_count))
    {
      if (item_param->long_data_supplied)
        continue;

      if (!(pos=setup_param_field(item_param,pos,
				  item_param->buffer_type=
				  (enum_field_types) get_buffer_type(&pos))))
        DBUG_RETURN(1);
    }
  }
#endif
  DBUG_RETURN(0);
}


/*
  Validates insert fields                                    
*/

static int check_prepare_fields(THD *thd,TABLE *table, List<Item> &fields,
				List<Item> &values, ulong counter)
{
  if (fields.elements == 0 && values.elements != 0)
  {
    if (values.elements != table->fields)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
  }
  else
  {           
    if (fields.elements != values.elements)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.db=  table->table_cache_key;
    table_list.real_name= table_list.alias= table->table_name;
    table_list.table= table;
    table_list.grant= table->grant;

    thd->dupp_field=0;
    if (setup_tables(&table_list) ||
	setup_fields(thd,&table_list,fields,1,0,0))
      return -1;
    if (thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE,MYF(0), thd->dupp_field->field_name);
      return -1;
    }
  }
  return 0;
}


/*
  Validate the following information for INSERT statement:                         
    - field existance           
    - fields count                          
*/

static bool mysql_test_insert_fields(PREP_STMT *stmt,
				     TABLE_LIST *table_list,
				     List<Item> &fields, 
				     List<List_item> &values_list,
				     thr_lock_type lock_type)                                       
{
  THD *thd= stmt->thd;
  TABLE *table;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  DBUG_ENTER("mysql_test_insert_fields");

  if (!(table = open_ltable(thd,table_list,lock_type)))
    DBUG_RETURN(1);

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
				  COND *conds, thr_lock_type lock_type)
{
  THD *thd= stmt->thd;
  TABLE *table;
  DBUG_ENTER("mysql_test_upd_fields");

  if (!(table = open_ltable(thd,table_list,lock_type)))
    DBUG_RETURN(1);

  if (setup_tables(table_list) || setup_fields(thd,table_list,fields,1,0,0) || 
      setup_conds(thd,table_list,&conds))      
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
				     List<Item> &fields, List<Item> &values,
				     COND *conds, ORDER *order, ORDER *group,
				     Item *having, thr_lock_type lock_type)
{
  TABLE *table;
  bool hidden_group_fields;
  THD *thd= stmt->thd;
  List<Item>  all_fields(fields);
  DBUG_ENTER("mysql_test_select_fields");

  if (!(table = open_ltable(thd,tables,lock_type)))
    DBUG_RETURN(1);
  
  thd->used_tables=0;	// Updated by setup_fields
  if (setup_tables(tables) ||
      setup_fields(thd,tables,fields,1,&all_fields,1) ||
      setup_conds(thd,tables,&conds) ||
      setup_order(thd,tables,fields,all_fields,order) ||
      setup_group(thd,tables,fields,all_fields,group,&hidden_group_fields))
    DBUG_RETURN(1);

  if (having)
  {
    thd->where="having clause";
    thd->allow_sum_func=1;
    if (having->fix_fields(thd, tables, &having) || thd->fatal_error)
      DBUG_RETURN(1);				
    if (having->with_sum_func)
      having->split_sum_func(all_fields);
  }
  if (setup_ftfuncs(thd)) 
    DBUG_RETURN(1);

  /* 
     Currently return only column list info only, and we are not
     sending any info on where clause.
  */
  if (send_prep_stmt(stmt, fields.elements) ||
      send_fields(thd,fields,0) || send_item_params(stmt))
    DBUG_RETURN(1);
  DBUG_RETURN(0);  
}


/*
  Check the access privileges
*/

static bool check_prepare_access(THD *thd, TABLE_LIST *tables,
                                 uint type)
{
  if (check_access(thd,type,tables->db,&tables->grant.privilege))
    return 1; 
  if (grant_option && check_grant(thd,type,tables))
    return 1;
  return 0;
}

/*
  Send the prepare query results back to client              
*/
                     
static bool send_prepare_results(PREP_STMT *stmt)     
{   
  THD *thd= stmt->thd;
  LEX *lex= &thd->lex;
  enum enum_sql_command sql_command = thd->lex.sql_command;
  DBUG_ENTER("send_prepare_results");
  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, lex->param_count));
  
  /* Setup prepared stmt */
  stmt->param_count= lex->param_count;
  stmt->free_list= thd->free_list;		// Save items used in stmt
  thd->free_list= 0;

  SELECT_LEX *select_lex = lex->select;
  TABLE_LIST *tables=(TABLE_LIST*) select_lex->table_list.first;
  
  switch (sql_command) {

  case SQLCOM_INSERT:
    if (mysql_test_insert_fields(stmt, tables, lex->field_list,
				 lex->many_values, lex->lock_option))
      goto abort;    
    break;

  case SQLCOM_UPDATE:
    if (mysql_test_upd_fields(stmt, tables, select_lex->item_list,
			      lex->value_list, select_lex->where,
			      lex->lock_option))
      goto abort;
    break;

  case SQLCOM_DELETE:
    if (mysql_test_upd_fields(stmt, tables, select_lex->item_list,
			      lex->value_list, select_lex->where,
			      lex->lock_option))
      goto abort;
    break;

  case SQLCOM_SELECT:
    if (mysql_test_select_fields(stmt, tables, select_lex->item_list,
				 lex->value_list, select_lex->where,
				 (ORDER*) select_lex->order_list.first,
				 (ORDER*) select_lex->group_list.first,
				 select_lex->having, lex->lock_option))
      goto abort;
    break;

  default:
    {
      /* 
         Rest fall through to default category, no parsing 
         for non-DML statements 
      */
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
  thd->prepare_command=true; 
  thd->safe_to_cache_query= 0;

  LEX *lex=lex_start(thd, (uchar*) packet, length);
  if (!yyparse() && !thd->fatal_error) 
    error= send_prepare_results(stmt);
  lex_end(lex);
  DBUG_RETURN(error);
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
  MEM_ROOT thd_root = thd->mem_root;
  PREP_STMT stmt;
  DBUG_ENTER("mysql_stmt_prepare");

  bzero((char*) &stmt, sizeof(stmt));
  stmt.thd= thd;
  stmt.stmt_id= ++thd->current_stmt_id;
  init_sql_alloc(&stmt.mem_root, 8192, 8192);

  thd->mem_root= stmt.mem_root;
  if (alloc_query(thd, packet, packet_length))
    goto err;
  if (parse_prepare_query(&stmt, thd->query, thd->query_length))
    goto err;

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);   
       
  stmt.mem_root= thd->mem_root;
  thd->mem_root= thd_root; // restore main mem_root
  DBUG_RETURN(0);

err:
  stmt.mem_root= thd->mem_root;
  free_prep_stmt(&stmt, free_free, (void*) 0);
  thd->mem_root = thd_root;	// restore main mem_root
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
  ulong stmt_id=     uint4korr(packet);
  PREP_STMT	*stmt;
  DBUG_ENTER("mysql_stmt_execute");

  if (!(stmt=find_prepared_statement(thd, stmt_id, "execute")))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  /* Check if we got an error when sending long data */
  if (stmt->error_in_prepare)
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  if (stmt->param_count && setup_param_fields(thd, stmt))
    DBUG_VOID_RETURN;

  MEM_ROOT thd_root= thd->mem_root;
  thd->mem_root = thd->con_root;
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);  
 
  /*
    TODO:
    Also, have checks on basic executions such as mysql_insert(), 
    mysql_delete(), mysql_update() and mysql_select() to not to 
    have re-check on setup_* and other things ..
  */  
  mysql_execute_command(thd);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);
  
  thd->mem_root= thd_root;
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

  if (!(stmt=find_prepared_statement(thd, stmt_id, "close")))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  stmt->error_in_prepare=0;
  Item_param *item= stmt->param, *end= item + stmt->param_count;

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

void mysql_stmt_close(THD *thd, char *packet)
{
  ulong stmt_id= uint4korr(packet);
  PREP_STMT *stmt;
  DBUG_ENTER("mysql_stmt_close");

  if (!(stmt=find_prepared_statement(thd, stmt_id, "close")))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }
  /* Will call free_prep_stmt() */
  tree_delete(&thd->prepared_statements, (void*) stmt, NULL);
  thd->last_prepared_stmt=0;
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

  /* The following should never happen */
  if (packet_length < 9)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "get_longdata");
    DBUG_VOID_RETURN;
  }

  pos++;				// skip command type at first position
  ulong stmt_id=     uint4korr(pos);
  uint param_number= uint2korr(pos+4);
  uint param_type=   uint2korr(pos+6);
  pos+=8;				// Point to data

  if (!(stmt=find_prepared_statement(thd, stmt_id, "get_longdata")))
  {
    /*
      There is a chance that the client will never see this as
      it doesn't expect an answer from this call...
    */
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  if (param_number >= stmt->param_count)
  {
    stmt->error_in_prepare=1;
    stmt->last_errno=ER_WRONG_ARGUMENTS;
    sprintf(stmt->last_error, ER(ER_WRONG_ARGUMENTS), "get_longdata");
    DBUG_VOID_RETURN;
  }
  stmt->param[param_number].set_longdata(pos, packet_length-9);
  stmt->long_data_used= 1;
  DBUG_VOID_RETURN;
}
