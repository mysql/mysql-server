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
    (if any )
     
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
    [type_spec_exists][type][length][data]
  - Checks if the type is specified by client, and if yes reads the type, 
    and stores the data in that format.
  - If length == MYSQL_END_OF_DATA, then server sets up the data read ended.
***********************************************************************/

#include "mysql_priv.h"
#include "sql_acl.h"
#include <assert.h> // for DEBUG_ASSERT()
#include <ctype.h>  // for isspace()

/**************************************************************************/
extern int yyparse(void);
static ulong get_param_length(uchar **packet);
static uint get_buffer_type(uchar **packet);
static bool param_is_null(uchar **packet);
static bool setup_param_fields(THD *thd,List<Item> &params);
static uchar* setup_param_field(Item_param *item_param, uchar *pos, uint buffer_type);
static void setup_longdata_field(Item_param *item_param, uchar *pos);
static bool setup_longdata(THD *thd,List<Item> &params);
static void send_prepare_results(THD *thd);
static void mysql_parse_prepare_query(THD *thd,char *packet,uint length);
static bool mysql_send_insert_fields(THD *thd,TABLE_LIST *table_list, 
				     List<Item> &fields,
				     List<List_item> &values_list,thr_lock_type lock_type);
static bool mysql_test_insert_fields(THD *thd,TABLE_LIST *table_list, 
				     List<Item> &fields,
				     List<List_item> &values_list,thr_lock_type lock_type);
static bool mysql_test_upd_fields(THD *thd,TABLE_LIST *table_list,
				  List<Item> &fields, List<Item> &values,
				  COND *conds,thr_lock_type lock_type);
static bool mysql_test_select_fields(THD *thd, TABLE_LIST *tables,
			      List<Item> &fields, List<Item> &values,
			      COND *conds, ORDER *order, ORDER *group,
			      Item *having,thr_lock_type lock_type);
extern const char *any_db;	
/**************************************************************************/

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
  switch (buffer_type)
  {    
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
    item_param->set_double(data);
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

static bool setup_param_fields(THD *thd, List<Item> &params)
{  
  reg2 Item_param *item_param;
  List_iterator<Item> it(params);  
  NET *net = &thd->net;
  DBUG_ENTER("setup_param_fields");  

  ulong param_count=0;
  uchar *pos=(uchar*)net->read_pos+1;// skip command type
  
  if(*pos++) // No types supplied, read only param data
  {
    while ((item_param=(Item_param *)it++) && 
	   (param_count++ < thd->param_count))
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
				  item_param->buffer_type=(enum_field_types)get_buffer_type(&pos))))
        DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

/*
  Buffer the long data and update the flags                                                                
*/

static void setup_longdata_field(Item_param *item_param, uchar *pos)
{
  ulong len;

  if (!*pos++)
    item_param->buffer_type=(enum_field_types)get_buffer_type(&pos);
 
  if (*pos == MYSQL_LONG_DATA_END)
    item_param->set_long_end();

  else
  {    
    len = get_param_length(&pos);    
    item_param->set_longdata((const char *)pos, len);
  }
}

/*
  Store the long data from client in pieces                                                              
*/

static bool setup_longdata(THD *thd, List<Item> &params)
{  
  NET *net=&thd->net;  
  List_iterator<Item> it(params);  
  DBUG_ENTER("setup_longdata");  

  uchar *pos=(uchar*)net->read_pos+1;// skip command type at first position
  ulong param_number = get_param_length(&pos);
  Item_param *item_param = thd->current_param;
  
  if (thd->current_param_number != param_number)
  {
    thd->current_param_number = param_number;
    while (param_number--) /* TODO: 
                            Change this loop by either having operator '+' 
                            overloaded to point to desired 'item' or 
                            add another memeber in list as 'goto' with 
                            location count as parameter number, but what 
                            is the best way to traverse ? 
			   */
    {
      it++;
    }
    thd->current_param = item_param = (Item_param *)it++;  
  }
  setup_longdata_field(item_param,pos);  
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
    table_list.name=table->table_name;
    table_list.table=table;
    table_list.grant=table->grant;

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
           
  If there is no column list spec exists, then update the field_list 
  with all columns from the table, and send fields info back to client                        
*/

static bool mysql_test_insert_fields(THD *thd, TABLE_LIST *table_list,
				     List<Item> &fields, 
				     List<List_item> &values_list,
				     thr_lock_type lock_type)                                       
{
  TABLE *table;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  DBUG_ENTER("mysql_test_insert_fields");

  if (!(table = open_ltable(thd,table_list,lock_type)))
    DBUG_RETURN(1);

  if ((values= its++))
  {
    uint value_count;
    ulong counter=0;
    
    if (check_insert_fields(thd,table,fields,*values,1))
      DBUG_RETURN(1);

    value_count= values->elements;
    its.rewind();
   
    while ((values = its++))
    {
      counter++;
      if (values->elements != value_count)
      {
        my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
			ER(ER_WRONG_VALUE_COUNT_ON_ROW),
			MYF(0),counter);
        DBUG_RETURN(1);
      }
    }
    if (fields.elements == 0)
    {
      /* No field listing, so setup all fields  */
      List<Item> all_fields;
      Field **ptr,*field;
      for (ptr=table->field; (field= *ptr) ; ptr++)
      {
        all_fields.push_back(new Item_field(table->table_cache_key,
                                            table->real_name,
                                            field->field_name));
      }
      if ((setup_fields(thd,table_list,all_fields,1,0,0) || 
	   send_fields(thd,all_fields,1)))
	DBUG_RETURN(1);
    }
    else if (send_fields(thd,fields,1))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Validate the following information                         
    UPDATE - set and where clause    DELETE - where clause                                             
                                                             
  And send update-set cluase column list fields info 
  back to client. For DELETE, just validate where cluase 
  and return no fields information back to client.
*/

static bool mysql_test_upd_fields(THD *thd, TABLE_LIST *table_list,
				  List<Item> &fields, List<Item> &values,
				  COND *conds, thr_lock_type lock_type)
{
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
  if (fields.elements && send_fields(thd,fields,1))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

/*
  Validate the following information:                         

    SELECT - column list 
           - where clause
           - orderr clause
           - having clause
           - group by clause
           - if no column spec i.e. '*', then setup all fields
                                                           
  And send column list fields info back to client. 
*/

static bool mysql_test_select_fields(THD *thd, TABLE_LIST *tables,
				     List<Item> &fields, List<Item> &values,
				     COND *conds, ORDER *order, ORDER *group,
				     Item *having,thr_lock_type lock_type)
{
  TABLE *table;
  bool hidden_group_fields;
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
    if (having->fix_fields(thd,tables) || thd->fatal_error)
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
  if (fields.elements && send_fields(thd,fields,1))
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
                     
static void send_prepare_results(THD *thd)     
{   
  DBUG_ENTER("send_prepare_results");
  enum enum_sql_command sql_command = thd->lex.sql_command;

  DBUG_PRINT("enter",("command :%d, param_count :%ld",
                      sql_command,thd->param_count));
  
  LEX *lex=&thd->lex;
  SELECT_LEX *select_lex = lex->select;
  TABLE_LIST *tables=(TABLE_LIST*) select_lex->table_list.first;
  
  switch(sql_command) {

  case SQLCOM_INSERT:
    if (mysql_test_insert_fields(thd,tables, lex->field_list,
				 lex->many_values, lex->lock_option))
      goto abort;    
    break;

  case SQLCOM_UPDATE:
    if (mysql_test_upd_fields(thd,tables, select_lex->item_list,
			      lex->value_list, select_lex->where,
			      lex->lock_option))
      goto abort;
    break;

  case SQLCOM_DELETE:
    if (mysql_test_upd_fields(thd,tables, select_lex->item_list,
			      lex->value_list, select_lex->where,
			      lex->lock_option))
      goto abort;
    break;

  case SQLCOM_SELECT:
    if (mysql_test_select_fields(thd,tables, select_lex->item_list,
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
  send_ok(&thd->net,thd->param_count,0);
  DBUG_VOID_RETURN;

abort:
  send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN : 0);
  DBUG_VOID_RETURN;
}

/*
  Parse the prepare query                                    
*/

static void mysql_parse_prepare_query(THD *thd, char *packet, uint length)
{
  DBUG_ENTER("mysql_parse_prepare_query");

  mysql_log.write(thd,COM_PREPARE,"%s",packet);       
  mysql_init_query(thd);   
  thd->prepare_command=true; 

  if (query_cache.send_result_to_client(thd, packet, length) <= 0)
  {   
    LEX *lex=lex_start(thd, (uchar*)packet, length);
     
    if (!yyparse() && !thd->fatal_error) 
    {
      send_prepare_results(thd);
      query_cache_end_of_result(&thd->net);
    }
    else
      query_cache_abort(&thd->net);   
    lex_end(lex);
  } 
  DBUG_VOID_RETURN;
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

void mysql_com_prepare(THD *thd, char *packet, uint packet_length)
{
  MEM_ROOT thd_root = thd->mem_root;
  DBUG_ENTER("mysql_com_prepare");
  
  packet_length--; 
    
  while (isspace(packet[0]) && packet_length > 0)
  {
    packet++;
    packet_length--;
  }
  char *pos=packet+packet_length;   
  while (packet_length > 0 && (pos[-1] == ';' || isspace(pos[-1])))
  {
    pos--;
    packet_length--;
  }
  /*
    Have the prepare items to have a connection level scope or 
    till next prepare statement by doing all allocations using 
    connection level memory allocator 'con_root' from THD.
  */
  free_root(&thd->con_root,MYF(0));  
  init_sql_alloc(&thd->con_root,8192,8192); 
  thd->mem_root = thd->con_root;
  
  if (!(thd->query= (char*) thd->memdup_w_gap((gptr) (packet),
                                              packet_length,
                                              thd->db_length+2)))
    DBUG_VOID_RETURN;
  thd->query[packet_length]=0;    
  thd->packet.shrink(net_buffer_length);
  thd->query_length = packet_length;  

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
 
  mysql_parse_prepare_query(thd,thd->query,packet_length);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);   
       
  thd->mem_root = thd_root; // restore main mem_root
  DBUG_PRINT("exit",("prepare query ready"));
  DBUG_VOID_RETURN;
}


/*
  Executes previously prepared query

  If there is any parameters(thd->param_count), then replace 
  markers with the data supplied from client, and then       
  execute the query                                            
*/

void mysql_com_execute(THD *thd)
{
  MEM_ROOT thd_root=thd->mem_root;
  DBUG_ENTER("mysql_com_execute");
  DBUG_PRINT("enter", ("parameters : %ld", thd->param_count));

  thd->mem_root = thd->con_root;
  if (thd->param_count && setup_param_fields(thd, thd->lex.param_list))
    DBUG_VOID_RETURN;
               
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);  
 
  /* TODO:
    Also, have checks on basic executions such as mysql_insert(), 
    mysql_delete(), mysql_update() and mysql_select() to not to 
    have re-check on setup_* and other things ..
  */  
  mysql_execute_command();      

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);
  
  thd->mem_root = (MEM_ROOT )thd_root;
  DBUG_PRINT("exit",("prepare-execute done!"));
  DBUG_VOID_RETURN;
}

/*
  Long data in pieces from client                            
*/

void mysql_com_longdata(THD *thd)
{
  DBUG_ENTER("mysql_com_execute");

  if(thd->param_count && setup_longdata(thd,thd->lex.param_list))
    DBUG_VOID_RETURN;    
  
  send_ok(&thd->net,0,0);// ok status to client 
  DBUG_PRINT("exit",("longdata-buffering done!"));
  DBUG_VOID_RETURN;
}

