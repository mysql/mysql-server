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
    [COM_LONG_DATA:1][STMT_ID:4][parameter_number:2][data]
  - data from the packet is appended to long data value buffer for this
    placeholder.
  - It's up to the client to check for read data ended. The server doesn't
    care; and also server doesn't notify to the client that it got the 
    data or not; if there is any error; then during execute; the error 
    will be returned

***********************************************************************/

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h" // for JOIN
#include <m_ctype.h>  // for isspace()
#include "sp_head.h"
#ifdef EMBEDDED_LIBRARY
/* include MYSQL_BIND headers */
#include <mysql.h>
#else
#include <mysql_com.h>
#endif

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
#ifndef EMBEDDED_LIBRARY
  bool (*set_params)(Prepared_statement *st, uchar *data, uchar *data_end,
                     uchar *read_pos, String *expanded_query);
#else
  bool (*set_params_data)(Prepared_statement *st, String *expanded_query);
#endif
  bool (*set_params_from_vars)(Prepared_statement *stmt, 
                               List<LEX_STRING>& varnames,
                               String *expanded_query);
public:
  Prepared_statement(THD *thd_arg);
  virtual ~Prepared_statement();
  void setup_set_params();
  virtual Item_arena::Type type() const;
};

static void execute_stmt(THD *thd, Prepared_statement *stmt,
                         String *expanded_query);

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

  if (stmt == 0 || stmt->type() != Item_arena::PREPARED_STATEMENT)
  {
    char llbuf[22];
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), 22, llstr(id, llbuf), where);
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
                                                 &stmt->lex->param_list,
                                                 Protocol::SEND_EOF)) ||
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
  /*
    In our client-server protocol all numbers bigger than 2^24
    stored as 8 bytes with uint8korr. Here we always know that
    parameter length is less than 2^4 so don't look at the second
    4 bytes. But still we need to obey the protocol hence 9 in the
    assignment above.
  */
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

static void set_param_tiny(Item_param *param, uchar **pos, ulong len)
{
#ifndef EMBEDDED_LIBRARY
  if (len < 1)
    return;
#endif
  int8 value= (int8) **pos;
  param->set_int(param->unsigned_flag ? (longlong) ((uint8) value) : 
                                        (longlong) value, 4);
  *pos+= 1;
}

static void set_param_short(Item_param *param, uchar **pos, ulong len)
{
  int16 value;
#ifndef EMBEDDED_LIBRARY
  if (len < 2)
    return;
  value= sint2korr(*pos);
#else
  shortget(value, *pos);
#endif
  param->set_int(param->unsigned_flag ? (longlong) ((uint16) value) :
                                        (longlong) value, 6);
  *pos+= 2;
}

static void set_param_int32(Item_param *param, uchar **pos, ulong len)
{
  int32 value;
#ifndef EMBEDDED_LIBRARY
  if (len < 4)
    return;
  value= sint4korr(*pos);
#else
  longget(value, *pos);
#endif
  param->set_int(param->unsigned_flag ? (longlong) ((uint32) value) :
                                        (longlong) value, 11);
  *pos+= 4;
}

static void set_param_int64(Item_param *param, uchar **pos, ulong len)
{
  longlong value;
#ifndef EMBEDDED_LIBRARY
  if (len < 8)
    return;
  value= (longlong) sint8korr(*pos);
#else
  longlongget(value, *pos);
#endif
  param->set_int(value, 21);
  *pos+= 8;
}

static void set_param_float(Item_param *param, uchar **pos, ulong len)
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

static void set_param_double(Item_param *param, uchar **pos, ulong len)
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

#ifndef EMBEDDED_LIBRARY

/*
  Read date/time/datetime parameter values from network (binary
  protocol). See writing counterparts of these functions in
  libmysql.c (store_param_{time,date,datetime}).
*/

static void set_param_time(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME tm;
  ulong length= get_param_length(pos, len);

  if (length >= 8)
  {
    uchar *to= *pos;
    uint day;

    tm.neg= (bool) to[0];
    day= (uint) sint4korr(to+1);
    /*
      Note, that though ranges of hour, minute and second are not checked
      here we rely on them being < 256: otherwise
      we'll get buffer overflow in make_{date,time} functions,
      which are called when time value is converted to string.
    */
    tm.hour=   (uint) to[5] + day * 24;
    tm.minute= (uint) to[6];
    tm.second= (uint) to[7];
    tm.second_part= (length > 8) ? (ulong) sint4korr(to+8) : 0;
    if (tm.hour > 838)
    {
      /* TODO: add warning 'Data truncated' here */
      tm.hour= 838;
      tm.minute= 59;
      tm.second= 59;
    }
    tm.day= tm.year= tm.month= 0;
  }
  else
    set_zero_time(&tm);
  param->set_time(&tm, MYSQL_TIMESTAMP_TIME,
                  MAX_TIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
  *pos+= length;
}

static void set_param_datetime(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME tm;
  ulong length= get_param_length(pos, len);

  if (length >= 4)
  {
    uchar *to= *pos;

    tm.neg=    0;
    tm.year=   (uint) sint2korr(to);
    tm.month=  (uint) to[2];
    tm.day=    (uint) to[3];
    /*
      Note, that though ranges of hour, minute and second are not checked
      here we rely on them being < 256: otherwise
      we'll get buffer overflow in make_{date,time} functions.
    */
    if (length > 4)
    {
      tm.hour=   (uint) to[4];
      tm.minute= (uint) to[5];
      tm.second= (uint) to[6];
    }
    else
      tm.hour= tm.minute= tm.second= 0;

    tm.second_part= (length > 7) ? (ulong) sint4korr(to+7) : 0;
  }
  else
    set_zero_time(&tm);
  param->set_time(&tm, MYSQL_TIMESTAMP_DATETIME,
                  MAX_DATETIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
  *pos+= length;
}


static void set_param_date(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME tm;
  ulong length= get_param_length(pos, len);

  if (length >= 4)
  {
    uchar *to= *pos;
    /*
      Note, that though ranges of hour, minute and second are not checked
      here we rely on them being < 256: otherwise
      we'll get buffer overflow in make_{date,time} functions.
    */
    tm.year=  (uint) sint2korr(to);
    tm.month=  (uint) to[2];
    tm.day= (uint) to[3];

    tm.hour= tm.minute= tm.second= 0;
    tm.second_part= 0;
    tm.neg= 0;
  }
  else
    set_zero_time(&tm);
  param->set_time(&tm, MYSQL_TIMESTAMP_DATE,
                  MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
  *pos+= length;
}

#else/*!EMBEDDED_LIBRARY*/
void set_param_time(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME *to= (MYSQL_TIME*)*pos;
  param->set_time(to, MYSQL_TIMESTAMP_TIME,
                  MAX_TIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);

}

void set_param_datetime(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME *to= (MYSQL_TIME*)*pos;

  param->set_time(to, MYSQL_TIMESTAMP_DATETIME,
                  MAX_DATETIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
}

void set_param_date(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME *to= (MYSQL_TIME*)*pos;

  param->set_time(to, MYSQL_TIMESTAMP_DATE,
                  MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
}
#endif /*!EMBEDDED_LIBRARY*/


static void set_param_str(Item_param *param, uchar **pos, ulong len)
{
  ulong length= get_param_length(pos, len);
  param->set_str((const char *)*pos, length);
  *pos+= length;
}


#undef get_param_length 

static void setup_one_conversion_function(THD *thd, Item_param *param,
                                          uchar param_type)
{
  switch (param_type) {
  case MYSQL_TYPE_TINY:
    param->set_param_func= set_param_tiny;
    param->item_type= Item::INT_ITEM;
    param->item_result_type= INT_RESULT;
    break;
  case MYSQL_TYPE_SHORT:
    param->set_param_func= set_param_short;
    param->item_type= Item::INT_ITEM;
    param->item_result_type= INT_RESULT;
    break;
  case MYSQL_TYPE_LONG:
    param->set_param_func= set_param_int32;
    param->item_type= Item::INT_ITEM;
    param->item_result_type= INT_RESULT;
    break;
  case MYSQL_TYPE_LONGLONG:
    param->set_param_func= set_param_int64;
    param->item_type= Item::INT_ITEM;
    param->item_result_type= INT_RESULT;
    break;
  case MYSQL_TYPE_FLOAT:
    param->set_param_func= set_param_float;
    param->item_type= Item::REAL_ITEM;
    param->item_result_type= REAL_RESULT;
    break;
  case MYSQL_TYPE_DOUBLE:
    param->set_param_func= set_param_double;
    param->item_type= Item::REAL_ITEM;
    param->item_result_type= REAL_RESULT;
    break;
  case MYSQL_TYPE_TIME:
    param->set_param_func= set_param_time;
    param->item_type= Item::STRING_ITEM;
    param->item_result_type= STRING_RESULT;
    break;
  case MYSQL_TYPE_DATE:
    param->set_param_func= set_param_date;
    param->item_type= Item::STRING_ITEM;
    param->item_result_type= STRING_RESULT;
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    param->set_param_func= set_param_datetime;
    param->item_type= Item::STRING_ITEM;
    param->item_result_type= STRING_RESULT;
    break;
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
    param->set_param_func= set_param_str;
    param->value.cs_info.character_set_client= &my_charset_bin;
    param->value.cs_info.final_character_set_of_str_value= &my_charset_bin;
    param->item_type= Item::STRING_ITEM;
    param->item_result_type= STRING_RESULT;
    break;
  default:
    /*
      The client library ensures that we won't get any other typecodes
      except typecodes above and typecodes for string types. Marking
      label as 'default' lets us to handle malformed packets as well.
    */
    {
      CHARSET_INFO *fromcs= thd->variables.character_set_client;
      CHARSET_INFO *tocs= thd->variables.collation_connection;
      uint32 dummy_offset;

      param->value.cs_info.character_set_client= fromcs;

      /*
        Setup source and destination character sets so that they
        are different only if conversion is necessary: this will
        make later checks easier.
      */
      param->value.cs_info.final_character_set_of_str_value=
        String::needs_conversion(0, fromcs, tocs, &dummy_offset) ?
        tocs : fromcs;
      param->set_param_func= set_param_str;
      /*
        Exact value of max_length is not known unless data is converted to
        charset of connection, so we have to set it later.
      */
      param->item_type= Item::STRING_ITEM;
      param->item_result_type= STRING_RESULT;
    }
  }
  param->param_type= (enum enum_field_types) param_type;
}

#ifndef EMBEDDED_LIBRARY
/*
  Update the parameter markers by reading data from client packet 
  and if binary/update log is set, generate the valid query.
*/

static bool insert_params_withlog(Prepared_statement *stmt, uchar *null_array,
                                  uchar *read_pos, uchar *data_end, 
                                  String *query)
{
  THD  *thd= stmt->thd;
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;
  uint32 length= 0;

  String str; 
  const String *res;

  DBUG_ENTER("insert_params_withlog"); 

  if (query->copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    if (param->state != Item_param::LONG_DATA_VALUE)
    {
      if (is_param_null(null_array, it - begin))
        param->set_null();
      else
      {
        if (read_pos >= data_end)
          DBUG_RETURN(1);
        param->set_param_func(param, &read_pos, data_end - read_pos);
      }
    }
    res= param->query_val_str(&str);
    if (param->convert_str_value(thd))
      DBUG_RETURN(1);                           /* out of memory */

    if (query->replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);
    
    length+= res->length()-1;
  }
  DBUG_RETURN(0);
}


static bool insert_params(Prepared_statement *stmt, uchar *null_array,
                          uchar *read_pos, uchar *data_end, 
                          String *expanded_query)
{
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;

  DBUG_ENTER("insert_params"); 

  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    if (param->state != Item_param::LONG_DATA_VALUE)
    {
      if (is_param_null(null_array, it - begin))
        param->set_null();
      else
      {
        if (read_pos >= data_end)
          DBUG_RETURN(1);
        param->set_param_func(param, &read_pos, data_end - read_pos);
      }
    }
    if (param->convert_str_value(stmt->thd))
      DBUG_RETURN(1);                           /* out of memory */
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
    THD *thd= stmt->thd;
    for (; it < end; ++it)
    {
      ushort typecode;
      const uint signed_bit= 1 << 15;

      if (read_pos >= data_end)
        DBUG_RETURN(1);

      typecode= sint2korr(read_pos);
      read_pos+= 2;
      (**it).unsigned_flag= test(typecode & signed_bit);
      setup_one_conversion_function(thd, *it, (uchar) (typecode & ~signed_bit));
    }
  }
  *data= read_pos;
  DBUG_RETURN(0);
}

#else

static bool emb_insert_params(Prepared_statement *stmt, String *expanded_query)
{
  THD *thd= stmt->thd;
  Item_param **it= stmt->param_array;
  Item_param **end= it + stmt->param_count;
  MYSQL_BIND *client_param= stmt->thd->client_params;

  DBUG_ENTER("emb_insert_params");

  for (; it < end; ++it, ++client_param)
  {
    Item_param *param= *it;
    setup_one_conversion_function(thd, param, client_param->buffer_type);
    if (param->state != Item_param::LONG_DATA_VALUE)
    {
      if (*client_param->is_null)
        param->set_null();
      else
      {
        uchar *buff= (uchar*) client_param->buffer;
        param->unsigned_flag= client_param->is_unsigned;
        param->set_param_func(param, &buff,
                              client_param->length ? 
                              *client_param->length : 
                              client_param->buffer_length);
      }
    }
    if (param->convert_str_value(thd))
      DBUG_RETURN(1);                           /* out of memory */
  }
  DBUG_RETURN(0);
}


static bool emb_insert_params_withlog(Prepared_statement *stmt, String *query)
{
  THD *thd= stmt->thd;
  Item_param **it= stmt->param_array;
  Item_param **end= it + stmt->param_count;
  MYSQL_BIND *client_param= thd->client_params;

  String str;
  const String *res;
  uint32 length= 0;

  DBUG_ENTER("emb_insert_params_withlog");

  if (query->copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);
  
  for (; it < end; ++it, ++client_param)
  {
    Item_param *param= *it;
    setup_one_conversion_function(thd, param, client_param->buffer_type);
    if (param->state != Item_param::LONG_DATA_VALUE)
    {
      if (*client_param->is_null)
        param->set_null();
      else
      {
        uchar *buff= (uchar*)client_param->buffer;
	param->unsigned_flag= client_param->is_unsigned;
        param->set_param_func(param, &buff,
                              client_param->length ? 
                              *client_param->length : 
                              client_param->buffer_length);
      }
    }
    res= param->query_val_str(&str);
    if (param->convert_str_value(thd))
      DBUG_RETURN(1);                           /* out of memory */

    if (query->replace(param->pos_in_query+length, 1, *res))
      DBUG_RETURN(1);

    length+= res->length()-1;
  }
  DBUG_RETURN(0);
}

#endif /*!EMBEDDED_LIBRARY*/


/*
  Set prepared statement parameters from user variables.
  SYNOPSIS
    insert_params_from_vars()
      stmt      Statement
      varnames  List of variables. Caller must ensure that number of variables
                in the list is equal to number of statement parameters
      query     Ignored
*/

static bool insert_params_from_vars(Prepared_statement *stmt,
                                    List<LEX_STRING>& varnames,
                                    String *query __attribute__((unused)))
{
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;
  user_var_entry *entry;
  LEX_STRING *varname;
  List_iterator<LEX_STRING> var_it(varnames);
  DBUG_ENTER("insert_params_from_vars");

  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    varname= var_it++;
    entry= (user_var_entry*)hash_search(&stmt->thd->user_vars,
                                        (byte*) varname->str,
                                         varname->length);
    if (param->set_from_user_var(stmt->thd, entry) ||
        param->convert_str_value(stmt->thd))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Do the same as insert_params_from_vars but also construct query text for
  binary log.
  SYNOPSIS
    insert_params_from_vars()
      stmt      Statement
      varnames  List of variables. Caller must ensure that number of variables
                in the list is equal to number of statement parameters
      query     The query with parameter markers replaced with their values
*/

static bool insert_params_from_vars_with_log(Prepared_statement *stmt,
                                             List<LEX_STRING>& varnames,
                                             String *query)
{
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;
  user_var_entry *entry;
  LEX_STRING *varname;
  DBUG_ENTER("insert_params_from_vars");

  List_iterator<LEX_STRING> var_it(varnames);
  String str;
  uint32 length= 0;
  if (query->copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);

  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    varname= var_it++;
    if (get_var_with_binlog(stmt->thd, *varname, &entry))
        DBUG_RETURN(1);
    DBUG_ASSERT(entry);

    if (param->set_from_user_var(stmt->thd, entry))
      DBUG_RETURN(1);
    /* Insert @'escaped-varname' instead of parameter in the query */
    char *buf, *ptr;
    str.length(0);
    if (str.reserve(entry->name.length*2+3))
      DBUG_RETURN(1);

    buf= str.c_ptr_quick();
    ptr= buf;
    *ptr++= '@';
    *ptr++= '\'';
    ptr+=
      escape_string_for_mysql(&my_charset_utf8_general_ci,
                              ptr, entry->name.str, entry->name.length);
    *ptr++= '\'';
    str.length(ptr - buf);

    if (param->convert_str_value(stmt->thd))
      DBUG_RETURN(1);                           /* out of memory */

    if (query->replace(param->pos_in_query+length, 1, str))
      DBUG_RETURN(1);
    length+= str.length()-1;
  }
  DBUG_RETURN(0);
}

/*
  Validate INSERT statement: 

  SYNOPSIS
    mysql_test_insert()
    stmt	prepared statemen handler
    tables	global/local table list  

  RETURN VALUE
    0   ok
    1   error, sent to the client
   -1   error, not sent to client
*/

static int mysql_test_insert(Prepared_statement *stmt,
			     TABLE_LIST *table_list,
			     List<Item> &fields, 
			     List<List_item> &values_list,
			     List<Item> &update_fields,
			     List<Item> &update_values,
			     enum_duplicates duplic)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  int res;
  my_bool update= (lex->value_list.elements ? UPDATE_ACL : 0);
  DBUG_ENTER("mysql_test_insert");

  if ((res= insert_precheck(thd, table_list, update)))
    DBUG_RETURN(res);

  /*
     open temporary memory pool for temporary data allocated by derived
     tables & preparation procedure
  */
  if ((res= open_and_lock_tables(thd, table_list)))
  {
    DBUG_RETURN(res);
  }

  if ((values= its++))
  {
    uint value_count;
    ulong counter= 0;

    if ((res= mysql_prepare_insert(thd, table_list, table_list->table, 
				   fields, values, update_fields,
				   update_values, duplic)))
      goto error;
    
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
      if (setup_fields(thd, 0, table_list, *values, 0, 0, 0))
	goto error;
    }
  }

  res= 0;
error:
  lex->unit.cleanup();
  DBUG_RETURN(res);
}


/*
  Validate UPDATE statement

  SYNOPSIS
    mysql_test_update()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_update(Prepared_statement *stmt,
			     TABLE_LIST *table_list)
{
  int res;
  THD *thd= stmt->thd;
  SELECT_LEX *select= &stmt->lex->select_lex;
  DBUG_ENTER("mysql_test_update");

  if ((res= update_precheck(thd, table_list)))
    DBUG_RETURN(res);

  if (!(res=open_and_lock_tables(thd, table_list)))
  {
    if (!(res= mysql_prepare_update(thd, table_list,
				    &select->where,
				    select->order_list.elements,
				    (ORDER *) select->order_list.first)))
    {
      thd->lex->select_lex.no_wrap_view_item= 1;
      if (setup_fields(thd, 0, table_list, select->item_list, 1, 0, 0))
      {
        res= -1;
        thd->lex->select_lex.no_wrap_view_item= 0;
      }
      else
      {
        thd->lex->select_lex.no_wrap_view_item= 0;
        if (setup_fields(thd, 0, table_list,
                         stmt->lex->value_list, 0, 0, 0))
          res= -1;
      }
    }
    stmt->lex->unit.cleanup();
  }
  /* TODO: here we should send types of placeholders to the client. */ 
  DBUG_RETURN(res);
}


/*
  Validate DELETE statement

  SYNOPSIS
    mysql_test_delete()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int mysql_test_delete(Prepared_statement *stmt,
			     TABLE_LIST *table_list)
{
  int res;
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  DBUG_ENTER("mysql_test_delete");

  if ((res= delete_precheck(thd, table_list)))
    DBUG_RETURN(res);

  if (!(res=open_and_lock_tables(thd, table_list)))
  {
    res= mysql_prepare_delete(thd, table_list, &lex->select_lex.where);
    lex->unit.cleanup();
  }
  /* TODO: here we should send types of placeholders to the client. */ 
  DBUG_RETURN(res);
}


/*
  Validate SELECT statement.
  In case of success, if this query is not EXPLAIN, send column list info
  back to client. 

  SYNOPSIS
    mysql_test_select()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/

static int mysql_test_select(Prepared_statement *stmt,
			     TABLE_LIST *tables, bool text_protocol)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX_UNIT *unit= &lex->unit;
  int result;
  DBUG_ENTER("mysql_test_select");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  ulong privilege= lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL;
  if (tables)
  {
    if (check_table_access(thd, privilege, tables,0))
      DBUG_RETURN(1);
  }
  else if (check_access(thd, privilege, any_db,0,0,0))
    DBUG_RETURN(1);
#endif

  if ((result= open_and_lock_tables(thd, tables)))
  {
    result= 1;                                  // Error sent
    send_error(thd);
    goto err;
  }
  result= 1;

  thd->used_tables= 0;                        // Updated by setup_fields

  // JOIN::prepare calls
  if (unit->prepare(thd, 0, 0))
  {
    send_error(thd);
    goto err_prep;
  }
  if (!text_protocol)
  {
    if (lex->describe)
    {
      if (send_prep_stmt(stmt, 0))
        goto err_prep;
    }
    else
    {
      if (send_prep_stmt(stmt, lex->select_lex.item_list.elements) ||
        thd->protocol_simple.send_fields(&lex->select_lex.item_list,
                                         Protocol::SEND_EOF)
#ifndef EMBEDDED_LIBRARY
          || net_flush(&thd->net)
#endif
         )
        goto err_prep;
    }
  }
  result= 0;                                    // ok

err_prep:
  unit->cleanup();
err:
  DBUG_RETURN(result);
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

  if (tables && (res= open_and_lock_tables(thd, tables)))
  {
    DBUG_RETURN(res);
  }
  res= setup_fields(thd, 0, 0, *values, 0, 0, 0);
  stmt->lex->unit.cleanup();
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
error:
  stmt->lex->unit.cleanup();
  DBUG_RETURN(res);
}


/*
  Check internal SELECT of the prepared command

  SYNOPSIS
    select_like_statement_test()
      stmt              - prepared table handler
      tables            - global list of tables
      specific_prepare  - function of command specific prepare

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/
static int select_like_statement_test(Prepared_statement *stmt,
				      TABLE_LIST *tables,
                                      int (*specific_prepare)(THD *thd))
{
  DBUG_ENTER("select_like_statement_test");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  int res= 0;

  if (tables && (res= open_and_lock_tables(thd, tables)))
    goto end;

  if (specific_prepare && (res= (*specific_prepare)(thd)))
    goto end;

  thd->used_tables= 0;                        // Updated by setup_fields

  // JOIN::prepare calls
  if (lex->unit.prepare(thd, 0, 0))
  {
    res= thd->net.report_error ? -1 : 1;
  }
end:
  lex->unit.cleanup();
  DBUG_RETURN(res);
}


/*
  Validate and prepare for execution CREATE TABLE statement

  SYNOPSIS
    mysql_test_create_table()
    stmt	prepared statemen handler
    tables	list of tables queries

  RETURN VALUE
    0   success
    1   error, sent to client
   -1   error, not sent to client
*/

static int mysql_test_create_table(Prepared_statement *stmt)
{
  DBUG_ENTER("mysql_test_create_table");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  int res= 0;
  /* Skip first table, which is the table we are creating */
  bool link_to_local;
  TABLE_LIST *create_table= lex->unlink_first_table(&link_to_local);
  TABLE_LIST *tables= lex->query_tables;

  if (!(res= create_table_precheck(thd, tables, create_table)) &&
      select_lex->item_list.elements)
  {
    select_lex->resolve_mode= SELECT_LEX::SELECT_MODE;
    res= select_like_statement_test(stmt, tables, 0);
    select_lex->resolve_mode= SELECT_LEX::NOMATTER_MODE;
  }

  /* put tables back for PS rexecuting */
  lex->link_first_table_back(create_table, link_to_local);
  DBUG_RETURN(res);
}


/*
  Validate and prepare for execution multi update statement

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
  return select_like_statement_test(stmt, tables, &mysql_multi_update_prepare);
}


/*
  Validate and prepare for execution multi delete statement

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

  uint fake_counter;
  if ((res= multi_delete_precheck(stmt->thd, tables, &fake_counter)))
    return res;
  return select_like_statement_test(stmt, tables, &mysql_multi_delete_prepare);
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
  DBUG_ASSERT(first_local_table != 0);
  /* Skip first table, which is the table we are inserting in */
  lex->select_lex.table_list.first= (byte*) first_local_table->next_local;
  /*
    insert/replace from SELECT give its SELECT_LEX for SELECT,
    and item_list belong to SELECT
  */
  lex->select_lex.resolve_mode= SELECT_LEX::SELECT_MODE;
  res= select_like_statement_test(stmt, tables, &mysql_insert_select_prepare);
  /* revert changes*/
  lex->select_lex.table_list.first= (byte*) first_local_table;
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
static int send_prepare_results(Prepared_statement *stmt, bool text_protocol)
{   
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *tables;
  enum enum_sql_command sql_command= lex->sql_command;
  int res= 0;
  DBUG_ENTER("send_prepare_results");

  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, stmt->param_count));

  lex->first_lists_tables_same();
  tables= lex->query_tables;

  switch (sql_command) {
  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
    res= mysql_test_insert(stmt, tables, lex->field_list,
			   lex->many_values,
			   select_lex->item_list, lex->value_list,
			   (lex->value_list.elements ?
			    DUP_UPDATE : lex->duplicates));
    break;

  case SQLCOM_UPDATE:
    res= mysql_test_update(stmt, tables);
    break;

  case SQLCOM_DELETE:
    res= mysql_test_delete(stmt, tables);
    break;

  case SQLCOM_SELECT:
    if ((res= mysql_test_select(stmt, tables, text_protocol)))
      goto error;
    /* Statement and field info has already been sent */
    DBUG_RETURN(0);

  case SQLCOM_CREATE_TABLE:
    res= mysql_test_create_table(stmt);
    break;
  
  case SQLCOM_DO:
    res= mysql_test_do_fields(stmt, tables, lex->insert_list);
    break;

  case SQLCOM_SET_OPTION:
    res= mysql_test_set_fields(stmt, tables, &lex->var_list);
    break;

  case SQLCOM_DELETE_MULTI:
    res= mysql_test_multidelete(stmt, tables);
    break;
  
  case SQLCOM_UPDATE_MULTI:
    res= mysql_test_multiupdate(stmt, tables);
    break;

  case SQLCOM_INSERT_SELECT:
    res= mysql_test_insert_select(stmt, tables);
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
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_COMMIT:
  case SQLCOM_CREATE_INDEX:
  case SQLCOM_DROP_INDEX:
  case SQLCOM_ROLLBACK:
  case SQLCOM_TRUNCATE:
    break;

  default:
    /*
      All other is not supported yet
    */
    res= -1;
    my_error(ER_UNSUPPORTED_PS, MYF(0));
    goto error;
  }
  if (res == 0)
    DBUG_RETURN(text_protocol? 0 : send_prep_stmt(stmt, 0));
error:
  if (res < 0)
    send_error(thd,thd->killed_errno());
  DBUG_RETURN(1);
}

/*
  Initialize array of parameters in statement from LEX.
  (We need to have quick access to items by number in mysql_stmt_get_longdata).
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
  Given a query string with parameter markers, create a Prepared Statement
  from it and send PS info back to the client.
  
  SYNOPSIS
    mysql_stmt_prepare()
      packet         query to be prepared 
      packet_length  query string length, including ignored trailing NULL or 
                     quote char.
      name           NULL or statement name. For unnamed statements binary PS
                     protocol is used, for named statements text protocol is 
                     used.
  RETURN 
    0      OK, statement prepared successfully
    other  Error
  
  NOTES
    This function parses the query and sends the total number of parameters 
    and resultset metadata information back to client (if any), without 
    executing the query i.e. without any log/disk writes. This allows the 
    queries to be re-executed without re-parsing during execute. 

    If parameter markers are found in the query, then store the information
    using Item_param along with maintaining a list in lex->param_array, so 
    that a fast and direct retrieval can be made without going through all 
    field items.
   
*/

int mysql_stmt_prepare(THD *thd, char *packet, uint packet_length,
                       LEX_STRING *name)
{
  LEX *lex;
  Prepared_statement *stmt= new Prepared_statement(thd);
  int error;
  DBUG_ENTER("mysql_stmt_prepare");

  DBUG_PRINT("prep_query", ("%s", packet));

  if (stmt == 0)
  {
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_RETURN(1);
  }

  if (name)
  {
    stmt->name.length= name->length;
    if (!(stmt->name.str= memdup_root(&stmt->mem_root, (char*)name->str,
                                      name->length)))
    {
      delete stmt;
      send_error(thd, ER_OUT_OF_RESOURCES);
      DBUG_RETURN(1);
    }
  }

  if (thd->stmt_map.insert(stmt))
  {
    delete stmt;
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_RETURN(1);
  }

  thd->set_n_backup_statement(stmt, &thd->stmt_backup);
  thd->set_n_backup_item_arena(stmt, &thd->stmt_backup);

  if (alloc_query(thd, packet, packet_length))
  {
    thd->restore_backup_statement(stmt, &thd->stmt_backup);
    thd->restore_backup_item_arena(stmt, &thd->stmt_backup);
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
    send_error(thd, ER_OUT_OF_RESOURCES);
    DBUG_RETURN(1);
  }

  mysql_log.write(thd, COM_PREPARE, "%s", packet);

  thd->current_arena= stmt;
  mysql_init_query(thd, (uchar *) thd->query, thd->query_length);
  lex= thd->lex;
  lex->safe_to_cache_query= 0;

  error= yyparse((void *)thd) || thd->is_fatal_error ||
         init_param_array(stmt);
  /*
    While doing context analysis of the query (in send_prepare_results) we
    allocate a lot of additional memory: for open tables, JOINs, derived
    tables, etc.  Let's save a snapshot of current parse tree to the
    statement and restore original THD. In cases when some tree
    transformation can be reused on execute, we set again thd->mem_root from
    stmt->mem_root (see setup_wild for one place where we do that).
  */
  thd->restore_backup_item_arena(stmt, &thd->stmt_backup);

  if (!error)
    error= send_prepare_results(stmt, test(name));

  /* restore to WAIT_PRIOR: QUERY_PRIOR is set inside alloc_query */
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);
  if (error && thd->lex->sphead)
  {
    if (lex != thd->lex)
      thd->lex->sphead->restore_lex(thd);
    delete thd->lex->sphead;
    thd->lex->sphead= NULL;
  }
  lex_end(lex);
  thd->restore_backup_statement(stmt, &thd->stmt_backup);
  cleanup_items(stmt->free_list);
  close_thread_tables(thd);
  thd->cleanup_after_query();
  thd->current_arena= thd;

  if (error)
  {
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
    stmt= NULL;
    /* error is sent inside yyparse/send_prepare_results */
  }
  else
  {
    stmt->setup_set_params();
    SELECT_LEX *sl= stmt->lex->all_selects_list;
    /*
      Save WHERE clause pointers, because they may be changed during query
      optimisation.
    */
    for (; sl; sl= sl->next_select_in_list())
      sl->prep_where= sl->where;
    stmt->state= Item_arena::PREPARED;
  }
  DBUG_RETURN(!stmt);
}

/* Reinit statement before execution */

void reset_stmt_for_execute(THD *thd, LEX *lex)
{
  SELECT_LEX *sl= lex->all_selects_list;

  if (lex->empty_field_list_on_rset)
  {
    lex->empty_field_list_on_rset= 0;
    lex->field_list.empty();
  }
  for (; sl; sl= sl->next_select_in_list())
  {
    if (!sl->first_execution)
    {
      /* remove option which was put by mysql_explain_union() */
      sl->options&= ~SELECT_DESCRIBE;

      /*
        Copy WHERE clause pointers to avoid damaging they by optimisation
      */
      if (sl->prep_where)
      {
        sl->where= sl->prep_where->copy_andor_structure(thd);
        sl->where->cleanup();
      }
      DBUG_ASSERT(sl->join == 0);
      ORDER *order;
      /* Fix GROUP list */
      for (order= (ORDER *)sl->group_list.first; order; order= order->next)
        order->item= &order->item_ptr;
      /* Fix ORDER list */
      for (order= (ORDER *)sl->order_list.first; order; order= order->next)
        order->item= &order->item_ptr;
    }
    {
      TABLE_LIST *tables= (TABLE_LIST *)sl->table_list.first;
      if (tables)
        tables->setup_is_done= 0;
    }
    {
      SELECT_LEX_UNIT *unit= sl->master_unit();
      unit->unclean();
      unit->types.empty();
      /* for derived tables & PS (which can't be reset by Item_subquery) */
      unit->reinit_exec_mechanism();
    }
  }

  /*
    TODO: When the new table structure is ready, then have a status bit 
    to indicate the table is altered, and re-do the setup_* 
    and open the tables back.
  */
  for (TABLE_LIST *tables= lex->query_tables;
	 tables;
	 tables= tables->next_global)
  {
    /*
      Reset old pointers to TABLEs: they are not valid since the tables
      were closed in the end of previous prepare or execute call.
    */
    tables->table= 0;
  }
  lex->current_select= &lex->select_lex;
  if (lex->result)
    lex->result->cleanup();
}


/* 
    Clears parameters from data left from previous execution or long data
    
  SYNOPSIS
    reset_stmt_params()
      stmt - prepared statement for which parameters should be reset
*/

static void reset_stmt_params(Prepared_statement *stmt)
{
  Item_param **item= stmt->param_array;
  Item_param **end= item + stmt->param_count;
  for (;item < end ; ++item)
    (**item).reset();
}


/*
  Executes previously prepared query.
  If there is any parameters, then replace markers with the data supplied
  from client, and then execute the query.
  SYNOPSIS
    mysql_stmt_execute()
      thd            Current thread
      packet         Query string
      packet_length  Query string length, including terminator character.
*/

void mysql_stmt_execute(THD *thd, char *packet, uint packet_length)
{
  ulong stmt_id= uint4korr(packet);
  ulong flags= (ulong) ((uchar) packet[4]);
  /*
    Query text for binary log, or empty string if the query is not put into
    binary log.
  */
  String expanded_query;
#ifndef EMBEDDED_LIBRARY
  uchar *packet_end= (uchar *) packet + packet_length - 1;
#endif
  Prepared_statement *stmt;
  DBUG_ENTER("mysql_stmt_execute");

  packet+= 9;                               /* stmt_id + 5 bytes of flags */

  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_execute",
                                      SEND_ERROR)))
    DBUG_VOID_RETURN;

  DBUG_PRINT("exec_query:", ("%s", stmt->query));

  /* Check if we got an error when sending long data */
  if (stmt->state == Item_arena::ERROR)
  {
    send_error(thd, stmt->last_errno, stmt->last_error);
    DBUG_VOID_RETURN;
  }

  if (flags & (ulong) CURSOR_TYPE_READ_ONLY)
  {
    if (stmt->lex->result)
    {
      /*
        If lex->result is set in the parser, this is not a SELECT
        statement: we can't open a cursor for it.
      */
      flags= 0;
    }
    else
    {
      if (!stmt->cursor &&
          !(stmt->cursor= new (&stmt->mem_root) Cursor()))
      {
        send_error(thd, ER_OUT_OF_RESOURCES);
        DBUG_VOID_RETURN;
      }
      /* If lex->result is set, mysql_execute_command will use it */
      stmt->lex->result= &stmt->cursor->result;
    }
  }
#ifndef EMBEDDED_LIBRARY
  if (stmt->param_count)
  {
    uchar *null_array= (uchar *) packet;
    if (setup_conversion_functions(stmt, (uchar **) &packet, packet_end) ||
        stmt->set_params(stmt, null_array, (uchar *) packet, packet_end,
                         &expanded_query))
      goto set_params_data_err;
  }
#else
  /*
    In embedded library we re-install conversion routines each time 
    we set params, and also we don't need to parse packet. 
    So we do it in one function.
  */
  if (stmt->param_count && stmt->set_params_data(stmt, &expanded_query))
    goto set_params_data_err;
#endif
  DBUG_ASSERT(thd->free_list == NULL);
  thd->stmt_backup.set_statement(thd);
  thd->set_statement(stmt);
  thd->current_arena= stmt;
  reset_stmt_for_execute(thd, stmt->lex);
  /* From now cursors assume that thd->mem_root is clean */
  if (expanded_query.length() &&
      alloc_query(thd, (char *)expanded_query.ptr(),
                  expanded_query.length()+1))
  {
    my_error(ER_OUTOFMEMORY, 0, expanded_query.length());
    goto err;
  }

  thd->protocol= &thd->protocol_prep;           // Switch to binary protocol
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
  mysql_execute_command(thd);
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);
  thd->protocol= &thd->protocol_simple;         // Use normal protocol

  if (flags & (ulong) CURSOR_TYPE_READ_ONLY)
  {
    if (stmt->cursor->is_open())
      stmt->cursor->init_from_thd(thd);
    thd->set_item_arena(&thd->stmt_backup);
  }
  else
  {
    thd->lex->unit.cleanup();
    cleanup_items(stmt->free_list);
    reset_stmt_params(stmt);
    close_thread_tables(thd);                   /* to close derived tables */
    thd->cleanup_after_query();
  }

  thd->set_statement(&thd->stmt_backup);
  thd->current_arena= thd;
  DBUG_VOID_RETURN;

set_params_data_err:
  reset_stmt_params(stmt);
  my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysql_stmt_execute");
err:
  send_error(thd);
  DBUG_VOID_RETURN;
}


/*
  Execute prepared statement using parameter values from
  lex->prepared_stmt_params and send result to the client using text protocol.
*/

void mysql_sql_stmt_execute(THD *thd, LEX_STRING *stmt_name)
{
  Prepared_statement *stmt;
  /*
    Query text for binary log, or empty string if the query is not put into
    binary log.
  */
  String expanded_query;
  DBUG_ENTER("mysql_sql_stmt_execute");

  DBUG_ASSERT(thd->free_list == NULL);

  if (!(stmt= (Prepared_statement*)thd->stmt_map.find_by_name(stmt_name)))
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), stmt_name->length,
             stmt_name->str, "EXECUTE");
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  if (stmt->param_count != thd->lex->prepared_stmt_params.elements)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  thd->set_n_backup_statement(stmt, &thd->stmt_backup);
  thd->set_statement(stmt);
  if (stmt->set_params_from_vars(stmt,
                                 thd->stmt_backup.lex->prepared_stmt_params,
                                 &expanded_query))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    send_error(thd);
  }
  execute_stmt(thd, stmt, &expanded_query);
  DBUG_VOID_RETURN;
}


/*
  Execute prepared statement.
  SYNOPSIS
    execute_stmt()
      thd            Current thread
      stmt           Statement to execute
      expanded_query If binary log is enabled, query string with parameter
                     placeholders replaced with actual values. Otherwise empty
                     string.
  NOTES
    Caller must set parameter values and thd::protocol.
*/

static void execute_stmt(THD *thd, Prepared_statement *stmt,
                         String *expanded_query)
{
  DBUG_ENTER("execute_stmt");

  reset_stmt_for_execute(thd, stmt->lex);

  if (expanded_query->length() &&
      alloc_query(thd, (char *)expanded_query->ptr(),
                  expanded_query->length()+1))
  {
    my_error(ER_OUTOFMEMORY, 0, expanded_query->length());
    DBUG_VOID_RETURN;
  }
  /*
    At first execution of prepared statement we will perform logical
    transformations of the query tree (i.e. negations elimination).
    This should be done permanently on the parse tree of this statement.
  */
  if (stmt->state == Item_arena::PREPARED)
    thd->current_arena= stmt;

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
  mysql_execute_command(thd);
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);

  thd->lex->unit.cleanup();
  cleanup_items(stmt->free_list);
  reset_stmt_params(stmt);
  close_thread_tables(thd);                    // to close derived tables
  thd->set_statement(&thd->stmt_backup);
  thd->cleanup_after_query();

  if (stmt->state == Item_arena::PREPARED)
  {
    thd->current_arena= thd;
    stmt->state= Item_arena::EXECUTED;
  }
  DBUG_VOID_RETURN;
}


/*
  COM_FETCH handler: fetches requested amount of rows from cursor
  SYNOPSIS
*/

void mysql_stmt_fetch(THD *thd, char *packet, uint packet_length)
{
  /* assume there is always place for 8-16 bytes */
  ulong stmt_id= uint4korr(packet);
  ulong num_rows= uint4korr(packet+=4);
  Statement *stmt;
  int error;

  DBUG_ENTER("mysql_stmt_fetch");

  if (!(stmt= thd->stmt_map.find(stmt_id)) ||
      !stmt->cursor ||
      !stmt->cursor->is_open())
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), stmt_id, "fetch");
    send_error(thd);
    DBUG_VOID_RETURN;
  }

  thd->stmt_backup.set_statement(thd);
  thd->stmt_backup.set_item_arena(thd);
  thd->set_statement(stmt);
  stmt->cursor->init_thd(thd);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), QUERY_PRIOR);

  thd->protocol= &thd->protocol_prep;		// Switch to binary protocol
  error= stmt->cursor->fetch(num_rows);
  thd->protocol= &thd->protocol_simple;         // Use normal protocol

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);

  /* Restore THD state */
  stmt->cursor->reset_thd(thd);
  thd->set_statement(&thd->stmt_backup);
  thd->set_item_arena(&thd->stmt_backup);

  if (error && error != -4)
    send_error(thd, ER_OUT_OF_RESOURCES);

  DBUG_VOID_RETURN;
}


/*
  Reset a prepared statement in case there was a recoverable error.
  SYNOPSIS
    mysql_stmt_reset()
      thd       Thread handle
      packet	Packet with stmt id 

  DESCRIPTION
    This function resets statement to the state it was right after prepare.
    It can be used to:
     - clear an error happened during mysql_stmt_send_long_data
     - cancel long data stream for all placeholders without
       having to call mysql_stmt_execute.
    Sends 'OK' packet in case of success (statement was reset)
    or 'ERROR' packet (unrecoverable error/statement not found/etc).
*/

void mysql_stmt_reset(THD *thd, char *packet)
{
  /* There is always space for 4 bytes in buffer */
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;
  
  DBUG_ENTER("mysql_stmt_reset");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_reset",
                                      SEND_ERROR)))
    DBUG_VOID_RETURN;

  stmt->state= Item_arena::PREPARED;

  /* 
    Clear parameters from data which could be set by 
    mysql_stmt_send_long_data() call.
  */
  reset_stmt_params(stmt);

  send_ok(thd);
  
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

  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_close",
                                      DONT_SEND_ERROR)))
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

void mysql_stmt_get_longdata(THD *thd, char *packet, ulong packet_length)
{
  ulong stmt_id;
  uint param_number;
  Prepared_statement *stmt;
  Item_param *param;
  char *packet_end= packet + packet_length - 1;
  
  DBUG_ENTER("mysql_stmt_get_longdata");

#ifndef EMBEDDED_LIBRARY
  /* Minimal size of long data packet is 6 bytes */
  if ((ulong) (packet_end - packet) < MYSQL_LONG_DATA_HEADER)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysql_stmt_send_long_data");
    DBUG_VOID_RETURN;
  }
#endif

  stmt_id= uint4korr(packet);
  packet+= 4;

  if (!(stmt=find_prepared_statement(thd, stmt_id, "mysql_stmt_send_long_data",
                                     DONT_SEND_ERROR)))
    DBUG_VOID_RETURN;

  param_number= uint2korr(packet);
  packet+= 2;
#ifndef EMBEDDED_LIBRARY
  if (param_number >= stmt->param_count)
  {
    /* Error will be sent in execute call */
    stmt->state= Item_arena::ERROR;
    stmt->last_errno= ER_WRONG_ARGUMENTS;
    sprintf(stmt->last_error, ER(ER_WRONG_ARGUMENTS),
            "mysql_stmt_send_long_data");
    DBUG_VOID_RETURN;
  }
#endif

  param= stmt->param_array[param_number];

#ifndef EMBEDDED_LIBRARY
  if (param->set_longdata(packet, (ulong) (packet_end - packet)))
#else
  if (param->set_longdata(thd->extra_data, thd->extra_length))
#endif
  {
    stmt->state= Item_arena::ERROR;
    stmt->last_errno= ER_OUTOFMEMORY;
    sprintf(stmt->last_error, ER(ER_OUTOFMEMORY), 0);
  }
  DBUG_VOID_RETURN;
}


Prepared_statement::Prepared_statement(THD *thd_arg)
  :Statement(thd_arg),
  thd(thd_arg),
  param_array(0),
  param_count(0),
  last_errno(0)
{
  *last_error= '\0';
}

void Prepared_statement::setup_set_params()
{
  /* Setup binary logging */
  if (mysql_bin_log.is_open() && is_update_query(lex->sql_command))
  {
    set_params_from_vars= insert_params_from_vars_with_log;
#ifndef EMBEDDED_LIBRARY
    set_params= insert_params_withlog;
#else
    set_params_data= emb_insert_params_withlog;
#endif
  }
  else
  {
    set_params_from_vars= insert_params_from_vars;
#ifndef EMBEDDED_LIBRARY
    set_params= insert_params;
#else
    set_params_data= emb_insert_params;
#endif
  }
}


Prepared_statement::~Prepared_statement()
{
  if (cursor)
    cursor->Cursor::~Cursor();
  free_items(free_list);
  delete lex->result;
}


Item_arena::Type Prepared_statement::type() const
{
  return PREPARED_STATEMENT;
}
