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
  Item_param **param;                           /* array of all placeholders */
  uint param_count;
  uint last_errno;
  char last_error[MYSQL_ERRMSG_SIZE];
  bool error_in_prepare, long_data_used;
  bool log_full_query;
#ifndef EMBEDDED_LIBRARY
  bool (*setup_params)(Prepared_statement *st, uchar *pos, uchar *read_pos);
#else
  bool (*setup_params_data)(Prepared_statement *st);
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

#ifdef EMBEDDED_LIBRARY
#define SETUP_PARAM_FUNCTION(fn_name) \
static void fn_name(Item_param *param, uchar **pos, ulong data_len)
#else
#define SETUP_PARAM_FUNCTION(fn_name) \
static void fn_name(Item_param *param, uchar **pos)
#endif


/*
  Seek prepared statement in statement map by id: returns zero if statement
  was not found, pointer otherwise.
*/

static Prepared_statement *
find_prepared_statement(THD *thd, ulong id, const char *where)
{
  Statement *stmt= thd->stmt_map.find(id);

  if (stmt == 0 || stmt->type() != Statement::PREPARED_STATEMENT)
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), id, where);
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
  buff[0]= 0;
  int4store(buff+1, stmt->id);
  int2store(buff+5, columns);
  int2store(buff+7, stmt->param_count);
  /* This should be fixed to work with prepared statements */
  return (my_net_write(net, buff, sizeof(buff)) || net_flush(net));
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

static bool insert_params_withlog(Prepared_statement *stmt, uchar *pos,
                                  uchar *read_pos)
{
  THD *thd= stmt->thd;
  List<Item> &params= stmt->lex->param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  
  String str, query;
  const String *res;

  DBUG_ENTER("insert_params_withlog"); 

  if (query.copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  ulong param_no= 0;  
  uint32 length= 0;
  
  while ((param= (Item_param *)param_iterator++))
  {
    if (param->long_data_supplied)
      res= param->query_val_str(&str);       
    else
    {
      if (is_param_null(pos,param_no))
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


static bool insert_params(Prepared_statement *stmt, uchar *pos,
                          uchar *read_pos)
{
  List<Item> &params= stmt->lex->param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  ulong param_no= 0;  

  DBUG_ENTER("insert_params"); 

  while ((param= (Item_param *)param_iterator++))
  {
    if (!param->long_data_supplied)   
    {
      if (is_param_null(pos,param_no))
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

static bool setup_params_data(Prepared_statement *stmt)
{                                       
  List<Item> &params= stmt->lex->param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;

  uchar *pos= (uchar*) stmt->thd->net.read_pos + 1 +
              MYSQL_STMT_HEADER;                //skip header
  uchar *read_pos= pos+(stmt->param_count+7) / 8; //skip null bits   

  DBUG_ENTER("setup_params_data");

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

#else

bool setup_params_data(Prepared_statement *stmt)
{                                       
  List<Item> &params= stmt->lex->param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  MYSQL_BIND *client_param= stmt->thd->client_params;

  DBUG_ENTER("setup_params_data");

  for (;(param= (Item_param *)param_iterator++); client_param++)
  {       
    setup_param_functions(param, client_param->buffer_type);
    if (!param->long_data_supplied)
    {
      if (*client_param->is_null)
        param->maybe_null= param->null_value= 1;
      else
      {
	uchar *buff= (uchar*)client_param->buffer;
        param->maybe_null= param->null_value= 0;
        param->setup_param_func(param,&buff, 
				client_param->length ? 
				*client_param->length : 
				client_param->buffer_length);
      }
    }
  }
  DBUG_RETURN(0);
}

bool setup_params_data_withlog(Prepared_statement *stmt)
{                                       
  THD *thd= stmt->thd;
  List<Item> &params= stmt->lex->param_list;
  List_iterator<Item> param_iterator(params);
  Item_param *param;
  MYSQL_BIND *client_param= thd->client_params;

  String str, query;
  const String *res;

  DBUG_ENTER("setup_params_data_withlog");

  if (query.copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  uint32 length= 0;

  for (;(param= (Item_param *)param_iterator++); client_param++)
  {       
    setup_param_functions(param, client_param->buffer_type);
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
        param->setup_param_func(param,&buff,
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
    - field existance           
    - fields count                          
*/

static bool mysql_test_insert_fields(Prepared_statement *stmt,
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
  my_bool update=(stmt->lex->value_list.elements ? UPDATE_ACL : 0);
  ulong privilege= (stmt->lex->duplicates == DUP_REPLACE ?
                    INSERT_ACL | DELETE_ACL : INSERT_ACL | update);
  if (check_access(thd,privilege,table_list->db,
                   &table_list->grant.privilege,0,0) || 
      (grant_option && check_grant(thd,privilege,table_list,0,0)))
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
  if (send_prep_stmt(stmt, 0))
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

static bool mysql_test_upd_fields(Prepared_statement *stmt,
                                  TABLE_LIST *table_list,
				  List<Item> &fields, List<Item> &values,
				  COND *conds)
{
  THD *thd= stmt->thd;

  DBUG_ENTER("mysql_test_upd_fields");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_access(thd,UPDATE_ACL,table_list->db,
                   &table_list->grant.privilege,0,0) || 
      (grant_option && check_grant(thd,UPDATE_ACL,table_list,0,0)))
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
  if (send_prep_stmt(stmt, 0))
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

static bool mysql_test_select_fields(Prepared_statement *stmt,
                                     TABLE_LIST *tables,
				     uint wild_num,
                                     List<Item> &fields, COND *conds, 
                                     uint og_num, ORDER *order, ORDER *group,
                                     Item *having, ORDER *proc,
                                     ulong select_options, 
                                     SELECT_LEX_UNIT *unit,
                                     SELECT_LEX *select_lex)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  select_result *result= lex->result;

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
    if (send_prep_stmt(stmt, 0))
      DBUG_RETURN(1);      
  }
  else 
  {
    fix_tables_pointers(lex->all_selects_list);
    if (!result && !(result= new select_send()))
    {
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
        thd->protocol_simple.send_fields(&fields, 0)
#ifndef EMBEDDED_LIBRARY
         || net_flush(&thd->net)
#endif
       )
      DBUG_RETURN(1);
    join->cleanup();
  }
  DBUG_RETURN(0);  
}


/*
  Send the prepare query results back to client              
*/
                     
static bool send_prepare_results(Prepared_statement *stmt)     
{   
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  enum enum_sql_command sql_command= lex->sql_command;

  DBUG_ENTER("send_prepare_results");
  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, lex->param_count));
  
  /* Setup prepared stmt */
  stmt->param_count= lex->param_count;

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
  Initialize parameter items in statement
*/

static bool init_param_items(Prepared_statement *stmt)
{
  Item_param **to;
 
  if (!stmt->param_count)
    stmt->param= (Item_param **)0;
  else
  {    
    if (!(stmt->param= to= (Item_param **)
          my_malloc(sizeof(Item_param *)*(stmt->param_count+1), 
                    MYF(MY_WME))))
      return 1;

    List_iterator<Item> param_iterator(stmt->lex->param_list);
    while ((*(to++)= (Item_param *)param_iterator++));
  }  
  return 0;
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
  retrieval can be made without going through all field     
  items.                                                     
*/

bool mysql_stmt_prepare(THD *thd, char *packet, uint packet_length)
{
  LEX *lex;
  Prepared_statement *stmt= new Prepared_statement(thd);
  SELECT_LEX *sl;
  DBUG_ENTER("mysql_stmt_prepare");

  if (stmt == 0)
    DBUG_RETURN(0);

  if (thd->stmt_map.insert(stmt))
    goto insert_stmt_err;

  thd->stmt_backup.set_statement(thd);
  thd->set_statement(stmt);

  if (alloc_query(thd, packet, packet_length))
    goto alloc_query_err;

  mysql_log.write(thd, COM_PREPARE, "%s", packet);       

  lex= lex_start(thd, (uchar *) thd->query, thd->query_length);
  mysql_init_query(thd);
  lex->safe_to_cache_query= 0;
  lex->param_count= 0;

  if (yyparse((void *)thd) || thd->is_fatal_error || send_prepare_results(stmt))
    goto yyparse_err;

  lex_end(lex);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);

  // save WHERE clause pointers to avoid damaging they by optimisation
  for (sl= thd->lex->all_selects_list;
       sl;
       sl= sl->next_select_in_list())
  {
    sl->prep_where= sl->where;
  }

  stmt->set_statement(thd);
  thd->set_statement(&thd->stmt_backup);

  if (init_param_items(stmt))
    goto init_param_err;

  stmt->command= COM_EXECUTE;                   // set it only once here 

  DBUG_RETURN(0);

yyparse_err:
  lex_end(lex);
  stmt->set_statement(thd);
  thd->set_statement(&thd->stmt_backup);
init_param_err:
alloc_query_err:
  /* Statement map deletes statement on erase */
  thd->stmt_map.erase(stmt);
  DBUG_RETURN(1);
insert_stmt_err:
  delete stmt;
  DBUG_RETURN(1);
}


/*
  Executes previously prepared query

  If there is any parameters (stmt->param_count), then replace 
  markers with the data supplied from client, and then       
  execute the query                                            
*/

void mysql_stmt_execute(THD *thd, char *packet)
{
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;

  DBUG_ENTER("mysql_stmt_execute");
  
  if (!(stmt= find_prepared_statement(thd, stmt_id, "execute")))
    DBUG_VOID_RETURN;

  /* Check if we got an error when sending long data */
  if (stmt->error_in_prepare)
  {
    send_error(thd, stmt->last_errno, stmt->last_error);
    DBUG_VOID_RETURN;
  }

  /*
    XXX: while thd->query_id is incremented for each command, stmt->query_id
    holds query_id of prepare stage. Keeping old query_id seems to be more
    natural, but differs from the way prepared statements work in 4.1:
  */ 
  /* stmt->query_id= thd->query_id; */
  thd->stmt_backup.set_statement(thd);
  thd->set_statement(stmt);

  /*
    To make sure that all runtime data is stored in its own memory root and 
    does not interfere with data possibly present in thd->mem_root.
    This root is cleaned up in the end of execution.
    FIXME: to be replaced with more efficient approach, and verified why we
    can not use thd->mem_root safely.
  */
  init_sql_alloc(&thd->mem_root,
                 thd->variables.query_alloc_block_size,
                 thd->variables.query_prealloc_size);


  for (SELECT_LEX *sl= stmt->lex->all_selects_list;
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

  /*
    TODO: When the new table structure is ready, then have a status bit 
    to indicate the table is altered, and re-do the setup_* 
    and open the tables back.
  */
  for (TABLE_LIST *tables= (TABLE_LIST*) stmt->lex->select_lex.table_list.first;
       tables;
       tables= tables->next)
    tables->table= 0; // safety - nasty init

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

  free_root(&thd->mem_root, MYF(0));
  thd->set_statement(&thd->stmt_backup);
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
  Prepared_statement *stmt;
  
  DBUG_ENTER("mysql_stmt_reset");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "reset")))
    DBUG_VOID_RETURN;

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
  Prepared_statement *stmt;

  DBUG_ENTER("mysql_stmt_free");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "close")))
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

  if (!(stmt=find_prepared_statement(thd, stmt_id, "get_longdata")))
    DBUG_VOID_RETURN;

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


Prepared_statement::Prepared_statement(THD *thd_arg)
  :Statement(thd_arg),
  thd(thd_arg),
  param(0),
  param_count(0),
  last_errno(0),
  error_in_prepare(0),
  long_data_used(0),
  log_full_query(0)
{
  *last_error= '\0';
  if (mysql_bin_log.is_open())
  {
    log_full_query= 1;
#ifndef EMBEDDED_LIBRARY
    setup_params= insert_params_withlog;
#else
    setup_params_data= setup_params_data_withlog;
#endif
  }
  else
#ifndef EMBEDDED_LIBRARY
    setup_params= insert_params; // not fully qualified query
#else
    setup_params_data= setup_params_data;
#endif
}


Prepared_statement::~Prepared_statement()
{
  my_free((char *) param, MYF(MY_ALLOW_ZERO_PTR));
  free_items(free_list);
}


Statement::Type Prepared_statement::type() const
{
  return PREPARED_STATEMENT;
}


