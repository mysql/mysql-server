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
This file contains the implementation of prepared statements.

When one prepares a statement:

  - Server gets the query from client with command 'COM_STMT_PREPARE';
    in the following format:
    [COM_STMT_PREPARE:1] [query]
  - Parse the query and recognize any parameter markers '?' and
    store its information list in lex->param_list
  - Allocate a new statement for this prepare; and keep this in
    'thd->stmt_map'.
  - Without executing the query, return back to client the total
    number of parameters along with result-set metadata information
    (if any) in the following format:
    [STMT_ID:4]
    [Column_count:2]
    [Param_count:2]
    [Params meta info (stubs only for now)]  (if Param_count > 0)
    [Columns meta info] (if Column_count > 0)

When one executes a statement:

  - Server gets the command 'COM_STMT_EXECUTE' to execute the
    previously prepared query. If there are any parameter markers, then the
    client will send the data in the following format:
    [COM_STMT_EXECUTE:1]
    [STMT_ID:4]
    [NULL_BITS:(param_count+7)/8)]
    [TYPES_SUPPLIED_BY_CLIENT(0/1):1]
    [[length]data]
    [[length]data] .. [[length]data].
    (Note: Except for string/binary types; all other types will not be
    supplied with length field)
  - If it is a first execute or types of parameters were altered by client,
    then setup the conversion routines.
  - Assign parameter items from the supplied data.
  - Execute the query without re-parsing and send back the results
    to client

When one supplies long data for a placeholder:

  - Server gets the long data in pieces with command type
    'COM_STMT_SEND_LONG_DATA'.
  - The packet recieved will have the format as:
    [COM_STMT_SEND_LONG_DATA:1][STMT_ID:4][parameter_number:2][data]
  - data from the packet is appended to the long data value buffer for this
    placeholder.
  - It's up to the client to stop supplying data chunks at any point. The
    server doesn't care; also, the server doesn't notify the client whether
    it got the data or not; if there is any error, then it will be returned
    at statement execute.

***********************************************************************/

#include "mysql_priv.h"
#include "sql_select.h" // for JOIN
#include "sp_head.h"
#include "sp.h"
#include "sp_cache.h"
#ifdef EMBEDDED_LIBRARY
/* include MYSQL_BIND headers */
#include <mysql.h>
#else
#include <mysql_com.h>
#endif

/******************************************************************************
  Prepared_statement: a statement that can contain placeholders
******************************************************************************/

class Prepared_statement: public Statement
{
public:
  THD *thd;
  Protocol *protocol;
  Item_param **param_array;
  uint param_count;
  uint last_errno;
  uint flags;
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
  Prepared_statement(THD *thd_arg, Protocol *protocol_arg);
  virtual ~Prepared_statement();
  void setup_set_params();
  virtual Query_arena::Type type() const;
  virtual void close_cursor();
  bool set_name(LEX_STRING *name);

  bool prepare(const char *packet, uint packet_length);
  bool execute(String *expanded_query, bool open_cursor);
  /* Destroy this statement */
  bool deallocate();

  /* Possible values of flags */
#if defined(_MSC_VER) && _MSC_VER < 1300
  static const int IS_IN_USE;
#else
  static const int IS_IN_USE= 1;
#endif
};

/* VC6 can't handle initializing in declaration */
#if defined(_MSC_VER) && _MSC_VER < 1300
const int Prepared_statement::IS_IN_USE= 1;
#endif

/******************************************************************************
  Implementation
******************************************************************************/


inline bool is_param_null(const uchar *pos, ulong param_no)
{
  return pos[param_no/8] & (1 << (param_no & 7));
}

enum { STMT_QUERY_LOG_LENGTH= 8192 };

/*
  Find a prepared statement in the statement map by id.

  SYNOPSIS
    find_prepared_statement()
      thd                thread handle
      id                 statement id
      where              the place from which this function is called (for
                         error reporting).

  DESCRIPTION
    Try to find a prepared statement and set THD error if it's not found.

  RETURN VALUE
    0 if the statement was not found, a pointer otherwise.
*/

static Prepared_statement *
find_prepared_statement(THD *thd, ulong id, const char *where)
{
  /*
    To strictly separate namespaces of SQL prepared statements and C API
    prepared statements find() will return 0 if there is a named prepared
    statement with such id.
  */
  Statement *stmt= thd->stmt_map.find(id);

  if (stmt == 0 || stmt->type() != Query_arena::PREPARED_STATEMENT)
  {
    char llbuf[22];
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), sizeof(llbuf), llstr(id, llbuf),
             where);
    return 0;
  }
  return (Prepared_statement *) stmt;
}


/*
  Send prepared statement id and metadata to the client after prepare.

  SYNOPSIS
    send_prep_stmt()

  RETURN VALUE
    0 in case of success, 1 otherwise
*/

#ifndef EMBEDDED_LIBRARY
static bool send_prep_stmt(Prepared_statement *stmt, uint columns)
{
  NET *net= &stmt->thd->net;
  char buff[12];
  uint tmp;
  DBUG_ENTER("send_prep_stmt");

  buff[0]= 0;                                   /* OK packet indicator */
  int4store(buff+1, stmt->id);
  int2store(buff+5, columns);
  int2store(buff+7, stmt->param_count);
  buff[9]= 0;                                   // Guard against a 4.1 client
  tmp= min(stmt->thd->total_warn_count, 65535);
  int2store(buff+10, tmp);

  /*
    Send types and names of placeholders to the client
    XXX: fix this nasty upcast from List<Item_param> to List<Item>
  */
  DBUG_RETURN(my_net_write(net, buff, sizeof(buff)) ||
              (stmt->param_count &&
               stmt->thd->protocol_simple.send_fields((List<Item> *)
                                                      &stmt->lex->param_list,
                                                      Protocol::SEND_EOF)));
}
#else
static bool send_prep_stmt(Prepared_statement *stmt,
                           uint columns __attribute__((unused)))
{
  THD *thd= stmt->thd;

  thd->client_stmt_id= stmt->id;
  thd->client_param_count= stmt->param_count;
  thd->clear_error();

  return 0;
}
#endif /*!EMBEDDED_LIBRARY*/


/*
  Read the length of the parameter data and return it back to
  the caller.

  SYNOPSIS
    get_param_length()
      packet             a pointer to the data
      len                remaining packet length

  DESCRIPTION
    Read data length, position the packet to the first byte after it,
    and return the length to the caller.

  RETURN VALUE
    Length of data piece.
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
   Data conversion routines.

   SYNOPSIS
     set_param_xx()
       param             parameter item
       pos               input data buffer
       len               length of data in the buffer

  DESCRIPTION
    All these functions read the data from pos, convert it to requested
    type and assign to param; pos is advanced to predefined length.

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
  float data;
#ifndef EMBEDDED_LIBRARY
  if (len < 4)
    return;
  float4get(data,*pos);
#else
  floatget(data, *pos);
#endif
  param->set_double((double) data);
  *pos+= 4;
}

static void set_param_double(Item_param *param, uchar **pos, ulong len)
{
  double data;
#ifndef EMBEDDED_LIBRARY
  if (len < 8)
    return;
  float8get(data,*pos);
#else
  doubleget(data, *pos);
#endif
  param->set_double((double) data);
  *pos+= 8;
}

static void set_param_decimal(Item_param *param, uchar **pos, ulong len)
{
  ulong length= get_param_length(pos, len);
  param->set_decimal((char*)*pos, length);
  *pos+= len;
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
    set_zero_time(&tm, MYSQL_TIMESTAMP_TIME);
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
    set_zero_time(&tm, MYSQL_TIMESTAMP_DATETIME);
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

    tm.year=  (uint) sint2korr(to);
    tm.month=  (uint) to[2];
    tm.day= (uint) to[3];

    tm.hour= tm.minute= tm.second= 0;
    tm.second_part= 0;
    tm.neg= 0;
  }
  else
    set_zero_time(&tm, MYSQL_TIMESTAMP_DATE);
  param->set_time(&tm, MYSQL_TIMESTAMP_DATE,
                  MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
  *pos+= length;
}

#else/*!EMBEDDED_LIBRARY*/
void set_param_time(Item_param *param, uchar **pos, ulong len)
{
  MYSQL_TIME tm= *((MYSQL_TIME*)*pos);
  tm.hour+= tm.day * 24;
  tm.day= tm.year= tm.month= 0;
  if (tm.hour > 838)
  {
    /* TODO: add warning 'Data truncated' here */
    tm.hour= 838;
    tm.minute= 59;
    tm.second= 59;
  }
  param->set_time(&tm, MYSQL_TIMESTAMP_TIME,
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
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_NEWDECIMAL:
    param->set_param_func= set_param_decimal;
    param->item_type= Item::DECIMAL_ITEM;
    param->item_result_type= DECIMAL_RESULT;
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
    param->value.cs_info.character_set_of_placeholder= &my_charset_bin;
    param->value.cs_info.character_set_client=
      thd->variables.character_set_client;
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

      param->value.cs_info.character_set_of_placeholder= fromcs;
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
  Routines to assign parameters from data supplied by the client.

  DESCRIPTION
    Update the parameter markers by reading data from the packet and
    and generate a valid query for logging.

  NOTES
    This function, along with other _withlog functions is called when one of
    binary, slow or general logs is open. Logging of prepared statements in
    all cases is performed by means of conventional queries: if parameter
    data was supplied from C API, each placeholder in the query is
    replaced with its actual value; if we're logging a [Dynamic] SQL
    prepared statement, parameter markers are replaced with variable names.
    Example:
     mysql_stmt_prepare("UPDATE t1 SET a=a*1.25 WHERE a=?")
       --> general logs gets [Prepare] UPDATE t1 SET a*1.25 WHERE a=?"
     mysql_stmt_execute(stmt);
       --> general and binary logs get
                             [Execute] UPDATE t1 SET a*1.25 WHERE a=1"
     If a statement has been prepared using SQL syntax:
     PREPARE stmt FROM "UPDATE t1 SET a=a*1.25 WHERE a=?"
       --> general log gets
                                 [Query]   PREPARE stmt FROM "UPDATE ..."
     EXECUTE stmt USING @a
       --> general log gets
                                 [Query]   EXECUTE stmt USING @a;

  RETURN VALUE
   0 if success, 1 otherwise
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

/*
  Embedded counterparts of parameter assignment routines.

  DESCRIPTION
    The main difference between the embedded library and the server is
    that in embedded case we don't serialize/deserialize parameters data.
    Additionally, for unknown reason, the client-side flag raised for
    changed types of placeholders is ignored and we simply setup conversion
    functions at each execute (TODO: fix).
*/

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
  Assign prepared statement parameters from user variables.

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
      stmt      Prepared statement
      varnames  List of variables. Caller must ensure that number of variables
                in the list is equal to number of statement parameters
      query     The query with parameter markers replaced with corresponding
                user variables that were used to execute the query.
*/

static bool insert_params_from_vars_with_log(Prepared_statement *stmt,
                                             List<LEX_STRING>& varnames,
                                             String *query)
{
  Item_param **begin= stmt->param_array;
  Item_param **end= begin + stmt->param_count;
  user_var_entry *entry;
  LEX_STRING *varname;
  List_iterator<LEX_STRING> var_it(varnames);
  String buf;
  const String *val;
  uint32 length= 0;

  DBUG_ENTER("insert_params_from_vars");

  if (query->copy(stmt->query, stmt->query_length, default_charset_info))
    DBUG_RETURN(1);

  for (Item_param **it= begin; it < end; ++it)
  {
    Item_param *param= *it;
    varname= var_it++;
    if (get_var_with_binlog(stmt->thd, stmt->lex->sql_command,
                            *varname, &entry))
        DBUG_RETURN(1);

    if (param->set_from_user_var(stmt->thd, entry))
      DBUG_RETURN(1);
    /* Insert @'escaped-varname' instead of parameter in the query */
    if (entry)
    {
      char *begin, *ptr;
      buf.length(0);
      if (buf.reserve(entry->name.length*2+3))
        DBUG_RETURN(1);

      begin= ptr= buf.c_ptr_quick();
      *ptr++= '@';
      *ptr++= '\'';
      ptr+= escape_string_for_mysql(&my_charset_utf8_general_ci,
                                    ptr, 0, entry->name.str,
                                    entry->name.length);
      *ptr++= '\'';
      buf.length(ptr - begin);
      val= &buf;
    }
    else
      val= &my_null_string;

    if (param->convert_str_value(stmt->thd))
      DBUG_RETURN(1);                           /* out of memory */

    if (query->replace(param->pos_in_query+length, 1, *val))
      DBUG_RETURN(1);
    length+= val->length()-1;
  }
  DBUG_RETURN(0);
}

/*
  Validate INSERT statement.

  SYNOPSIS
    mysql_test_insert()
      stmt               prepared statement
      tables             global/local table list

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_insert(Prepared_statement *stmt,
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
  DBUG_ENTER("mysql_test_insert");

  if (insert_precheck(thd, table_list))
    goto error;

  /*
    open temporary memory pool for temporary data allocated by derived
    tables & preparation procedure
    Note that this is done without locks (should not be needed as we will not
    access any data here)
    If we would use locks, then we have to ensure we are not using
    TL_WRITE_DELAYED as having two such locks can cause table corruption.
  */
  if (open_normal_and_derived_tables(thd, table_list, 0))
    goto error;

  if ((values= its++))
  {
    uint value_count;
    ulong counter= 0;
    Item *unused_conds= 0;

    if (table_list->table)
    {
      // don't allocate insert_values
      table_list->table->insert_values=(byte *)1;
    }

    if (mysql_prepare_insert(thd, table_list, table_list->table,
                             fields, values, update_fields, update_values,
                             duplic, &unused_conds, FALSE))
      goto error;

    value_count= values->elements;
    its.rewind();

    if (table_list->lock_type == TL_WRITE_DELAYED &&
        !(table_list->table->file->table_flags() & HA_CAN_INSERT_DELAYED))
    {
      my_error(ER_ILLEGAL_HA, MYF(0), (table_list->view ?
                                       table_list->view_name.str :
                                       table_list->table_name));
      goto error;
    }
    while ((values= its++))
    {
      counter++;
      if (values->elements != value_count)
      {
        my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);
        goto error;
      }
      if (setup_fields(thd, 0, *values, 0, 0, 0))
        goto error;
    }
  }
  DBUG_RETURN(FALSE);

error:
  /* insert_values is cleared in open_table */
  DBUG_RETURN(TRUE);
}


/*
  Validate UPDATE statement

  SYNOPSIS
    mysql_test_update()
      stmt               prepared statement
      tables             list of tables used in this query

  RETURN VALUE
    0                    success
    1                    error, error message is set in THD
    2                    convert to multi_update
*/

static int mysql_test_update(Prepared_statement *stmt,
                              TABLE_LIST *table_list)
{
  int res;
  THD *thd= stmt->thd;
  uint table_count= 0;
  SELECT_LEX *select= &stmt->lex->select_lex;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint          want_privilege;
#endif
  DBUG_ENTER("mysql_test_update");

  if (update_precheck(thd, table_list))
    goto error;

  if (open_tables(thd, &table_list, &table_count, 0))
    goto error;

  if (table_list->multitable_view)
  {
    DBUG_ASSERT(table_list->view != 0);
    DBUG_PRINT("info", ("Switch to multi-update"));
    /* pass counter value */
    thd->lex->table_count= table_count;
    /* convert to multiupdate */
    DBUG_RETURN(2);
  }

  /*
    thd->fill_derived_tables() is false here for sure (because it is
    preparation of PS, so we even do not check it).
  */
  if (lock_tables(thd, table_list, table_count) ||
      mysql_handle_derived(thd->lex, &mysql_derived_prepare))
    goto error;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* TABLE_LIST contain right privilages request */
  want_privilege= table_list->grant.want_privilege;
#endif

  if (mysql_prepare_update(thd, table_list, &select->where,
                           select->order_list.elements,
                           (ORDER *) select->order_list.first))
    goto error;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table_list->grant.want_privilege= want_privilege;
  table_list->table->grant.want_privilege= want_privilege;
#endif
  thd->lex->select_lex.no_wrap_view_item= TRUE;
  res= setup_fields(thd, 0, select->item_list, 1, 0, 0);
  thd->lex->select_lex.no_wrap_view_item= FALSE;
  if (res)
    goto error;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Check values */
  table_list->grant.want_privilege=
  table_list->table->grant.want_privilege=
    (SELECT_ACL & ~table_list->table->grant.privilege);
#endif
  if (setup_fields(thd, 0, stmt->lex->value_list, 0, 0, 0))
    goto error;
  /* TODO: here we should send types of placeholders to the client. */
  DBUG_RETURN(0);
error:
  DBUG_RETURN(1);
}


/*
  Validate DELETE statement.

  SYNOPSIS
    mysql_test_delete()
      stmt               prepared statement
      tables             list of tables used in this query

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_delete(Prepared_statement *stmt,
                              TABLE_LIST *table_list)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  DBUG_ENTER("mysql_test_delete");

  if (delete_precheck(thd, table_list) ||
      open_and_lock_tables(thd, table_list))
    goto error;

  if (!table_list->table)
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
             table_list->view_db.str, table_list->view_name.str);
    goto error;
  }

  DBUG_RETURN(mysql_prepare_delete(thd, table_list, &lex->select_lex.where));
error:
  DBUG_RETURN(TRUE);
}


/*
  Validate SELECT statement.

  SYNOPSIS
    mysql_test_select()
      stmt               prepared statement
      tables             list of tables used in the query

  DESCRIPTION
    In case of success, if this query is not EXPLAIN, send column list info
    back to the client.

  RETURN VALUE
    0                    success
    1                    error, error message is set in THD
    2                    success, and statement metadata has been sent
*/

static int mysql_test_select(Prepared_statement *stmt,
                             TABLE_LIST *tables, bool text_protocol)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX_UNIT *unit= &lex->unit;
  DBUG_ENTER("mysql_test_select");

  lex->select_lex.context.resolve_in_select_list= TRUE;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  ulong privilege= lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL;
  if (tables)
  {
    if (check_table_access(thd, privilege, tables,0))
      goto error;
  }
  else if (check_access(thd, privilege, any_db,0,0,0,0))
    goto error;
#endif

  if (!lex->result && !(lex->result= new (stmt->mem_root) select_send))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(select_send));
    goto error;
  }

  if (open_and_lock_tables(thd, tables))
    goto error;

  thd->used_tables= 0;                        // Updated by setup_fields

  /*
    JOIN::prepare calls
    It is not SELECT COMMAND for sure, so setup_tables will be called as
    usual, and we pass 0 as setup_tables_done_option
  */
  if (unit->prepare(thd, 0, 0, ""))
    goto error;
  if (!lex->describe && !text_protocol)
  {
    /* Make copy of item list, as change_columns may change it */
    List<Item> fields(lex->select_lex.item_list);

    /* Change columns if a procedure like analyse() */
    if (unit->last_procedure && unit->last_procedure->change_columns(fields))
      goto error;

    /*
      We can use lex->result as it should've been prepared in
      unit->prepare call above.
    */
    if (send_prep_stmt(stmt, lex->result->field_count(fields)) ||
        lex->result->send_fields(fields, Protocol::SEND_EOF) ||
        thd->protocol->flush())
      goto error;
    DBUG_RETURN(2);
  }
  DBUG_RETURN(0);
error:
  DBUG_RETURN(1);
}


/*
  Validate and prepare for execution DO statement expressions.

  SYNOPSIS
    mysql_test_do_fields()
      stmt               prepared statement
      tables             list of tables used in this query
      values             list of expressions

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_do_fields(Prepared_statement *stmt,
                                TABLE_LIST *tables,
                                List<Item> *values)
{
  THD *thd= stmt->thd;

  DBUG_ENTER("mysql_test_do_fields");
  if (tables && check_table_access(thd, SELECT_ACL, tables, 0))
    DBUG_RETURN(TRUE);

  if (open_and_lock_tables(thd, tables))
    DBUG_RETURN(TRUE);
  DBUG_RETURN(setup_fields(thd, 0, *values, 0, 0, 0));
}


/*
  Validate and prepare for execution SET statement expressions

  SYNOPSIS
    mysql_test_set_fields()
      stmt               prepared statement
      tables             list of tables used in this query
      values             list of expressions

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_set_fields(Prepared_statement *stmt,
                                  TABLE_LIST *tables,
                                  List<set_var_base> *var_list)
{
  DBUG_ENTER("mysql_test_set_fields");
  List_iterator_fast<set_var_base> it(*var_list);
  THD *thd= stmt->thd;
  set_var_base *var;

  if (tables && check_table_access(thd, SELECT_ACL, tables, 0) ||
      open_and_lock_tables(thd, tables))
    goto error;

  while ((var= it++))
  {
    if (var->light_check(thd))
      goto error;
  }
  DBUG_RETURN(FALSE);
error:
  DBUG_RETURN(TRUE);
}


/*
  Check internal SELECT of the prepared command

  SYNOPSIS
    select_like_stmt_test()
      stmt                      prepared statement
      specific_prepare          function of command specific prepare
      setup_tables_done_option  options to be passed to LEX::unit.prepare()

  NOTE
    This function won't directly open tables used in select. They should
    be opened either by calling function (and in this case you probably
    should use select_like_stmt_test_with_open_n_lock()) or by
    "specific_prepare" call (like this happens in case of multi-update).

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool select_like_stmt_test(Prepared_statement *stmt,
                                  bool (*specific_prepare)(THD *thd),
                                  ulong setup_tables_done_option)
{
  DBUG_ENTER("select_like_stmt_test");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;

  lex->select_lex.context.resolve_in_select_list= TRUE;

  if (specific_prepare && (*specific_prepare)(thd))
    DBUG_RETURN(TRUE);

  thd->used_tables= 0;                        // Updated by setup_fields

  /* Calls JOIN::prepare */
  DBUG_RETURN(lex->unit.prepare(thd, 0, setup_tables_done_option, ""));
}

/*
  Check internal SELECT of the prepared command (with opening and
  locking of used tables).

  SYNOPSIS
    select_like_stmt_test_with_open_n_lock()
      stmt                      prepared statement
      tables                    list of tables to be opened and locked
                                before calling specific_prepare function
      specific_prepare          function of command specific prepare
      setup_tables_done_option  options to be passed to LEX::unit.prepare()

  RETURN VALUE
    FALSE                success
    TRUE                 error
*/

static bool
select_like_stmt_test_with_open_n_lock(Prepared_statement *stmt,
                                       TABLE_LIST *tables,
                                       bool (*specific_prepare)(THD *thd),
                                       ulong setup_tables_done_option)
{
  DBUG_ENTER("select_like_stmt_test_with_open_n_lock");

  /*
    We should not call LEX::unit.cleanup() after this open_and_lock_tables()
    call because we don't allow prepared EXPLAIN yet so derived tables will
    clean up after themself.
  */
  if (open_and_lock_tables(stmt->thd, tables))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(select_like_stmt_test(stmt, specific_prepare,
                                    setup_tables_done_option));
}


/*
  Validate and prepare for execution CREATE TABLE statement

  SYNOPSIS
    mysql_test_create_table()
      stmt               prepared statement
      tables             list of tables used in this query

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_create_table(Prepared_statement *stmt)
{
  DBUG_ENTER("mysql_test_create_table");
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  bool res= FALSE;
  /* Skip first table, which is the table we are creating */
  bool link_to_local;
  TABLE_LIST *create_table= lex->unlink_first_table(&link_to_local);
  TABLE_LIST *tables= lex->query_tables;

  if (create_table_precheck(thd, tables, create_table))
    DBUG_RETURN(TRUE);

  if (select_lex->item_list.elements)
  {
    select_lex->context.resolve_in_select_list= TRUE;
    res= select_like_stmt_test_with_open_n_lock(stmt, tables, 0, 0);
  }

  /* put tables back for PS rexecuting */
  lex->link_first_table_back(create_table, link_to_local);
  DBUG_RETURN(res);
}


/*
  Validate and prepare for execution a multi update statement.

  SYNOPSIS
    mysql_test_multiupdate()
      stmt               prepared statement
      tables             list of tables used in this query
      converted          converted to multi-update from usual update

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_multiupdate(Prepared_statement *stmt,
                                  TABLE_LIST *tables,
                                  bool converted)
{
  /* if we switched from normal update, rights are checked */
  if (!converted && multi_update_precheck(stmt->thd, tables))
    return TRUE;

  return select_like_stmt_test(stmt, &mysql_multi_update_prepare,
                               OPTION_SETUP_TABLES_DONE);
}


/*
  Validate and prepare for execution a multi delete statement.

  SYNOPSIS
    mysql_test_multidelete()
      stmt               prepared statement
      tables             list of tables used in this query

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message in THD is set.
*/

static bool mysql_test_multidelete(Prepared_statement *stmt,
                                  TABLE_LIST *tables)
{
  stmt->thd->lex->current_select= &stmt->thd->lex->select_lex;
  if (add_item_to_list(stmt->thd, new Item_null()))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), 0);
    goto error;
  }

  if (multi_delete_precheck(stmt->thd, tables) ||
      select_like_stmt_test_with_open_n_lock(stmt, tables,
                                             &mysql_multi_delete_prepare,
                                             OPTION_SETUP_TABLES_DONE))
    goto error;
  if (!tables->table)
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
             tables->view_db.str, tables->view_name.str);
    goto error;
  }
  return FALSE;
error:
  return TRUE;
}


/*
  Wrapper for mysql_insert_select_prepare, to make change of local tables
  after open_and_lock_tables() call.

  SYNOPSIS
    mysql_insert_select_prepare_tester()
      thd                thread handle

  NOTE
    We need to remove the first local table after open_and_lock_tables,
    because mysql_handle_derived uses local tables lists.
*/

static bool mysql_insert_select_prepare_tester(THD *thd)
{
  TABLE_LIST *first;
  bool res;
  SELECT_LEX *first_select= &thd->lex->select_lex;
  /* Skip first table, which is the table we are inserting in */
  first_select->table_list.first= (byte*)(first=
                                          ((TABLE_LIST*)first_select->
                                           table_list.first)->next_local);
  res= mysql_insert_select_prepare(thd);
  /*
    insert/replace from SELECT give its SELECT_LEX for SELECT,
    and item_list belong to SELECT
  */
  thd->lex->select_lex.context.resolve_in_select_list= TRUE;
  thd->lex->select_lex.context.table_list= first;
  return res;
}


/*
  Validate and prepare for execution INSERT ... SELECT statement.

  SYNOPSIS
    mysql_test_insert_select()
      stmt               prepared statement
      tables             list of tables used in this query

  RETURN VALUE
    FALSE                success
    TRUE                 error, error message is set in THD
*/

static bool mysql_test_insert_select(Prepared_statement *stmt,
                                     TABLE_LIST *tables)
{
  int res;
  LEX *lex= stmt->lex;
  TABLE_LIST *first_local_table;

  if (tables->table)
  {
    // don't allocate insert_values
    tables->table->insert_values=(byte *)1;
  }

  if (insert_precheck(stmt->thd, tables))
    return 1;

  /* store it, because mysql_insert_select_prepare_tester change it */
  first_local_table= (TABLE_LIST *)lex->select_lex.table_list.first;
  DBUG_ASSERT(first_local_table != 0);

  res=
    select_like_stmt_test_with_open_n_lock(stmt, tables,
                                           &mysql_insert_select_prepare_tester,
                                           OPTION_SETUP_TABLES_DONE);
  /* revert changes  made by mysql_insert_select_prepare_tester */
  lex->select_lex.table_list.first= (byte*) first_local_table;
  return res;
}


/*
  Perform semantic analysis of the parsed tree and send a response packet
  to the client.

  SYNOPSIS
    check_prepared_statement()
      stmt               prepared statement

  DESCRIPTION
    This function
    - opens all tables and checks access rights
    - validates semantics of statement columns and SQL functions
      by calling fix_fields.

  RETURN VALUE
    FALSE                success, statement metadata is sent to client
    TRUE                 error, error message is set in THD (but not sent)
*/

static bool check_prepared_statement(Prepared_statement *stmt,
                                     bool text_protocol)
{
  THD *thd= stmt->thd;
  LEX *lex= stmt->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *tables;
  enum enum_sql_command sql_command= lex->sql_command;
  int res= 0;
  DBUG_ENTER("check_prepared_statement");
  DBUG_PRINT("enter",("command: %d, param_count: %ld",
                      sql_command, stmt->param_count));

  lex->first_lists_tables_same();
  tables= lex->query_tables;

  /* set context for commands which do not use setup_tables */
  lex->select_lex.context.resolve_in_table_list_only(select_lex->
                                                     get_table_list());

  switch (sql_command) {
  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
    res= mysql_test_insert(stmt, tables, lex->field_list,
                           lex->many_values,
                           select_lex->item_list, lex->value_list,
                           lex->duplicates);
    break;

  case SQLCOM_UPDATE:
    res= mysql_test_update(stmt, tables);
    /* mysql_test_update returns 2 if we need to switch to multi-update */
    if (res != 2)
      break;

  case SQLCOM_UPDATE_MULTI:
    res= mysql_test_multiupdate(stmt, tables, res == 2);
    break;

  case SQLCOM_DELETE:
    res= mysql_test_delete(stmt, tables);
    break;

  case SQLCOM_SELECT:
    res= mysql_test_select(stmt, tables, text_protocol);
    if (res == 2)
    {
      /* Statement and field info has already been sent */
      DBUG_RETURN(FALSE);
    }
    break;
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

  case SQLCOM_INSERT_SELECT:
  case SQLCOM_REPLACE_SELECT:
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
  case SQLCOM_CALL:
    break;

  default:
    /* All other statements are not supported yet. */
    my_message(ER_UNSUPPORTED_PS, ER(ER_UNSUPPORTED_PS), MYF(0));
    goto error;
  }
  if (res == 0)
    DBUG_RETURN(text_protocol? FALSE : (send_prep_stmt(stmt, 0) ||
                                        thd->protocol->flush()));
error:
  DBUG_RETURN(TRUE);
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
    if (stmt->param_count > (uint) UINT_MAX16)
    {
      /* Error code to be defined in 5.0 */
      my_message(ER_PS_MANY_PARAM, ER(ER_PS_MANY_PARAM), MYF(0));
      return TRUE;
    }
    Item_param **to;
    List_iterator<Item_param> param_iterator(lex->param_list);
    /* Use thd->mem_root as it points at statement mem_root */
    stmt->param_array= (Item_param **)
                       alloc_root(stmt->thd->mem_root,
                                  sizeof(Item_param*) * stmt->param_count);
    if (!stmt->param_array)
      return TRUE;
    for (to= stmt->param_array;
         to < stmt->param_array + stmt->param_count;
         ++to)
    {
      *to= param_iterator++;
    }
  }
  return FALSE;
}


/* Cleanup PS after execute/prepare and restore THD state */

static void cleanup_stmt_and_thd_after_use(Statement *stmt, THD *thd)
{
  DBUG_ENTER("cleanup_stmt_and_thd_after_use");
  stmt->lex->unit.cleanup();
  cleanup_items(stmt->free_list);
  thd->rollback_item_tree_changes();
  thd->cleanup_after_query();
  DBUG_VOID_RETURN;
}


/*
  COM_STMT_PREPARE handler.

  SYNOPSIS
    mysql_stmt_prepare()
      packet             query to be prepared
      packet_length      query string length, including ignored
                         trailing NULL or quote char.

  DESCRIPTION
    Given a query string with parameter markers, create a prepared
    statement from it and send PS info back to the client.

  NOTES
    This function parses the query and sends the total number of parameters
    and resultset metadata information back to client (if any), without
    executing the query i.e. without any log/disk writes. This allows the
    queries to be re-executed without re-parsing during execute.

    If parameter markers are found in the query, then store the information
    using Item_param along with maintaining a list in lex->param_array, so
    that a fast and direct retrieval can be made without going through all
    field items.

  RETURN VALUE
    none: in case of success a new statement id and metadata is sent
    to the client, otherwise an error message is set in THD.
*/

void mysql_stmt_prepare(THD *thd, const char *packet, uint packet_length)
{
  Prepared_statement *stmt= new Prepared_statement(thd, &thd->protocol_prep);
  bool rc;
  DBUG_ENTER("mysql_stmt_prepare");

  DBUG_PRINT("prep_query", ("%s", packet));

  if (stmt == 0)
    DBUG_VOID_RETURN; /* out of memory: error is set in Sql_alloc */

  if (thd->stmt_map.insert(stmt))
  {
    delete stmt;
    DBUG_VOID_RETURN;                           /* out of memory */
  }

  mysql_reset_thd_for_next_command(thd);
  /* Reset warnings from previous command */
  mysql_reset_errors(thd, 0);
  sp_cache_flush_obsolete(&thd->sp_proc_cache);
  sp_cache_flush_obsolete(&thd->sp_func_cache);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);

  rc= stmt->prepare(packet, packet_length);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),WAIT_PRIOR);

  if (rc)
  {
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
  }
  else
    mysql_log.write(thd, COM_STMT_PREPARE, "[%lu] %s", stmt->id, packet);

  /* check_prepared_statemnt sends the metadata packet in case of success */
  DBUG_VOID_RETURN;
}

/*
  SYNOPSIS
    get_dynamic_sql_string()
      lex       in      main lex
      query_len out     length of the SQL statement (is set only
                        in case of success)

  DESCRIPTION
    Get an SQL statement text from a user variable or from plain
    text. If the statement is plain text, just assign the
    pointers, otherwise allocate memory in thd->mem_root and copy
    the contents of the variable, possibly with character
    set conversion.

  RETURN VALUE
    non-zero success, 0 in case of error (out of memory)
*/

static const char *get_dynamic_sql_string(LEX *lex, uint *query_len)
{
  THD *thd= lex->thd;
  char *query_str= 0;

  if (lex->prepared_stmt_code_is_varref)
  {
    /* This is PREPARE stmt FROM or EXECUTE IMMEDIATE @var. */
    String str;
    CHARSET_INFO *to_cs= thd->variables.collation_connection;
    bool needs_conversion;
    user_var_entry *entry;
    String *pstr= &str;
    uint32 unused, len;
    /*
      Convert @var contents to string in connection character set. Although
      it is known that int/real/NULL value cannot be a valid query we still
      convert it for error messages to be uniform.
    */
    if ((entry=
         (user_var_entry*)hash_search(&thd->user_vars,
                                      (byte*)lex->prepared_stmt_code.str,
                                      lex->prepared_stmt_code.length))
        && entry->value)
    {
      my_bool is_var_null;
      pstr= entry->val_str(&is_var_null, &str, NOT_FIXED_DEC);
      /*
        NULL value of variable checked early as entry->value so here
        we can't get NULL in normal conditions
      */
      DBUG_ASSERT(!is_var_null);
      if (!pstr)
        goto end;
    }
    else
    {
      /*
        variable absent or equal to NULL, so we need to set variable to
        something reasonable to get a readable error message during parsing
      */
      str.set("NULL", 4, &my_charset_latin1);
    }

    needs_conversion= String::needs_conversion(pstr->length(),
                                              pstr->charset(), to_cs, &unused);

    len= needs_conversion ? pstr->length() * to_cs->mbmaxlen : pstr->length();
    if (!(query_str= alloc_root(thd->mem_root, len+1)))
      goto end;

    if (needs_conversion)
    {
      uint dummy_errors;
      len= copy_and_convert(query_str, len, to_cs, pstr->ptr(), pstr->length(),
                            pstr->charset(), &dummy_errors);
    }
    else
      memcpy(query_str, pstr->ptr(), pstr->length());
    query_str[len]= '\0';
    *query_len= len;
  }
  else
  {
    query_str= lex->prepared_stmt_code.str;
    *query_len= lex->prepared_stmt_code.length;
  }
end:
  return query_str;
}


/* Init PS/SP specific parse tree members.  */

static void init_stmt_after_parse(LEX *lex)
{
  SELECT_LEX *sl= lex->all_selects_list;
  /*
    Switch off a temporary flag that prevents evaluation of
    subqueries in statement prepare.
  */
  for (; sl; sl= sl->next_select_in_list())
   sl->uncacheable&= ~UNCACHEABLE_PREPARE;
}

/*
  SQLCOM_PREPARE implementation.

  SYNOPSIS
    mysql_sql_stmt_prepare()
      thd     thread handle

  DESCRIPTION
    Prepare an SQL prepared statement. This is called from
    mysql_execute_command and should therefore behave like an
    ordinary query (e.g. should not reset any global THD data).

  RETURN VALUE
    none: in case of success, OK packet is sent to the client,
    otherwise an error message is set in THD
*/

void mysql_sql_stmt_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  LEX_STRING *name= &lex->prepared_stmt_name;
  Prepared_statement *stmt;
  const char *query;
  uint query_len;

  DBUG_ENTER("mysql_sql_stmt_prepare");

  DBUG_ASSERT(thd->protocol == &thd->protocol_simple);
  if ((stmt= (Prepared_statement*) thd->stmt_map.find_by_name(name)))
  {
    /*
      If there is a statement with the same name, remove it. It is ok to
      remove old and fail to insert a new one at the same time.
    */
    if (stmt->deallocate())
      DBUG_VOID_RETURN;
  }

  if (! (query= get_dynamic_sql_string(lex, &query_len)) ||
      ! (stmt= new Prepared_statement(thd, &thd->protocol_simple)))
  {
    DBUG_VOID_RETURN;                           /* out of memory */
  }

  if (stmt->set_name(name) || thd->stmt_map.insert(stmt))
  {
    delete stmt;
    DBUG_VOID_RETURN;
  }

  if (stmt->prepare(query, query_len+1))
  {
    /* Statement map deletes the statement on erase */
    thd->stmt_map.erase(stmt);
  }
  else
    send_ok(thd, 0L, 0L, "Statement prepared");

  DBUG_VOID_RETURN;
}

/* Reinit prepared statement/stored procedure before execution */

void reinit_stmt_before_use(THD *thd, LEX *lex)
{
  SELECT_LEX *sl= lex->all_selects_list;
  DBUG_ENTER("reinit_stmt_before_use");

  /*
    We have to update "thd" pointer in LEX, all its units and in LEX::result,
    since statements which belong to trigger body are associated with TABLE
    object and because of this can be used in different threads.
  */
  lex->thd= thd;

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

      /* see unique_table() */
      sl->exclude_from_table_unique_test= FALSE;

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
      SELECT_LEX_UNIT *unit= sl->master_unit();
      unit->unclean();
      unit->types.empty();
      /* for derived tables & PS (which can't be reset by Item_subquery) */
      unit->reinit_exec_mechanism();
      unit->set_thd(thd);
    }
  }

  /*
    TODO: When the new table structure is ready, then have a status bit
    to indicate the table is altered, and re-do the setup_*
    and open the tables back.
  */
  /*
    NOTE: We should reset whole table list here including all tables added
    by prelocking algorithm (it is not a problem for substatements since
    they have their own table list).
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
    if (tables->nested_join)
      tables->nested_join->counter= 0;

    if (tables->prep_on_expr)
    {
      tables->on_expr= tables->prep_on_expr->copy_andor_structure(thd);
      tables->on_expr->cleanup();
    }
  }
  lex->current_select= &lex->select_lex;

  /* restore original list used in INSERT ... SELECT */
  if (lex->leaf_tables_insert)
    lex->select_lex.leaf_tables= lex->leaf_tables_insert;

  if (lex->result)
  {
    lex->result->cleanup();
    lex->result->set_thd(thd);
  }

  DBUG_VOID_RETURN;
}


/*
  Clears parameters from data left from previous execution or long data

  SYNOPSIS
    reset_stmt_params()
      stmt               prepared statement for which parameters should
                         be reset
*/

static void reset_stmt_params(Prepared_statement *stmt)
{
  Item_param **item= stmt->param_array;
  Item_param **end= item + stmt->param_count;
  for (;item < end ; ++item)
    (**item).reset();
}


/*
  COM_STMT_EXECUTE handler: execute a previously prepared statement.

  SYNOPSIS
    mysql_stmt_execute()
      thd                current thread
      packet             parameter types and data, if any
      packet_length      packet length, including the terminator character.

  DESCRIPTION
    If there are any parameters, then replace parameter markers with the
    data supplied from the client, and then execute the statement.
    This function uses binary protocol to send a possible result set
    to the client.

  RETURN VALUE
    none: in case of success OK packet or a result set is sent to the
    client, otherwise an error message is set in THD.
*/

void mysql_stmt_execute(THD *thd, char *packet, uint packet_length)
{
  ulong stmt_id= uint4korr(packet);
  ulong flags= (ulong) ((uchar) packet[4]);
  /* Query text for binary, general or slow log, if any of them is open */
  String expanded_query;
#ifndef EMBEDDED_LIBRARY
  uchar *packet_end= (uchar *) packet + packet_length - 1;
#endif
  Prepared_statement *stmt;
  bool rc;
  DBUG_ENTER("mysql_stmt_execute");

  packet+= 9;                               /* stmt_id + 5 bytes of flags */

  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_execute")))
    DBUG_VOID_RETURN;

  DBUG_PRINT("exec_query", ("%s", stmt->query));
  DBUG_PRINT("info",("stmt: %p", stmt));

  mysql_reset_thd_for_next_command(thd);
  sp_cache_flush_obsolete(&thd->sp_proc_cache);
  sp_cache_flush_obsolete(&thd->sp_func_cache);

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
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
  rc= stmt->execute(&expanded_query,
                    test(flags & (ulong) CURSOR_TYPE_READ_ONLY));
  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);
  if (rc)
    goto err;

  mysql_log.write(thd, COM_STMT_EXECUTE, "[%lu] %s", stmt->id, thd->query);

  DBUG_VOID_RETURN;

set_params_data_err:
  my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysql_stmt_execute");
err:
  reset_stmt_params(stmt);
  DBUG_VOID_RETURN;
}


/*
  SQLCOM_EXECUTE implementation.

  SYNOPSIS
    mysql_sql_stmt_execute()
      thd                thread handle

  DESCRIPTION
    Execute prepared statement using parameter values from
    lex->prepared_stmt_params and send result to the client using
    text protocol. This is called from mysql_execute_command and
    therefore should behave like an ordinary query (e.g. not change
    global THD data, such as warning count, server status, etc).
    This function uses text protocol to send a possible result set.

  RETURN
    none: in case of success, OK (or result set) packet is sent to the
    client, otherwise an error is set in THD
*/

void mysql_sql_stmt_execute(THD *thd)
{
  LEX *lex= thd->lex;
  Prepared_statement *stmt;
  LEX_STRING *name= &lex->prepared_stmt_name;
  /* Query text for binary, general or slow log, if any of them is open */
  String expanded_query;

  DBUG_ENTER("mysql_sql_stmt_execute");

  DBUG_PRINT("info", ("EXECUTE: %.*s\n", name->length, name->str));

  if (!(stmt= (Prepared_statement*) thd->stmt_map.find_by_name(name)))
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0),
             name->length, name->str, "EXECUTE");
    DBUG_VOID_RETURN;
  }

  if (stmt->param_count != lex->prepared_stmt_params.elements)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("info",("stmt: %p", stmt));

  if (stmt->set_params_from_vars(stmt, lex->prepared_stmt_params,
                                 &expanded_query))
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    DBUG_VOID_RETURN;
  }

  (void) stmt->execute(&expanded_query, FALSE);

  DBUG_VOID_RETURN;
}


/*
  COM_STMT_FETCH handler: fetches requested amount of rows from cursor

  SYNOPSIS
    mysql_stmt_fetch()
      thd                Thread handle
      packet             Packet from client (with stmt_id & num_rows)
      packet_length      Length of packet
*/

void mysql_stmt_fetch(THD *thd, char *packet, uint packet_length)
{
  /* assume there is always place for 8-16 bytes */
  ulong stmt_id= uint4korr(packet);
  ulong num_rows= uint4korr(packet+4);
  Prepared_statement *stmt;
  Statement stmt_backup;
  Cursor *cursor;
  DBUG_ENTER("mysql_stmt_fetch");

  statistic_increment(thd->status_var.com_stmt_fetch, &LOCK_status);
  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_fetch")))
    DBUG_VOID_RETURN;

  cursor= stmt->cursor;
  if (!cursor || !cursor->is_open())
  {
    my_error(ER_STMT_HAS_NO_OPEN_CURSOR, MYF(0), stmt_id);
    DBUG_VOID_RETURN;
  }

  thd->stmt_arena= stmt;
  thd->set_n_backup_statement(stmt, &stmt_backup);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), QUERY_PRIOR);

  thd->protocol= stmt->protocol;                // Switch to binary protocol
  cursor->fetch(num_rows);
  thd->protocol= &thd->protocol_simple;         // Use normal protocol

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(), WAIT_PRIOR);

  if (!cursor->is_open())
  {
    /* We're done with the fetch: reset PS for next execution */
    cleanup_stmt_and_thd_after_use(stmt, thd);
    reset_stmt_params(stmt);
    /*
      Must be the last, as some memory is still needed for
      the previous calls.
    */
    free_root(cursor->mem_root, MYF(0));
    if (cursor->close_at_commit)
      thd->stmt_map.erase_transient_cursor(stmt);
  }

  thd->restore_backup_statement(stmt, &stmt_backup);
  thd->stmt_arena= thd;

  DBUG_VOID_RETURN;
}


/*
  Reset a prepared statement in case there was a recoverable error.
  SYNOPSIS
    mysql_stmt_reset()
      thd                Thread handle
      packet             Packet with stmt id

  DESCRIPTION
    This function resets statement to the state it was right after prepare.
    It can be used to:
     - clear an error happened during mysql_stmt_send_long_data
     - cancel long data stream for all placeholders without
       having to call mysql_stmt_execute.
     - close an open cursor
    Sends 'OK' packet in case of success (statement was reset)
    or 'ERROR' packet (unrecoverable error/statement not found/etc).
*/

void mysql_stmt_reset(THD *thd, char *packet)
{
  /* There is always space for 4 bytes in buffer */
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;
  Cursor *cursor;
  DBUG_ENTER("mysql_stmt_reset");

  statistic_increment(thd->status_var.com_stmt_reset, &LOCK_status);
  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_reset")))
    DBUG_VOID_RETURN;

  stmt->close_cursor();                    /* will reset statement params */

  stmt->state= Query_arena::PREPARED;

  mysql_reset_thd_for_next_command(thd);
  send_ok(thd);

  DBUG_VOID_RETURN;
}


/*
  Delete a prepared statement from memory.
  Note: we don't send any reply to this command.
*/

void mysql_stmt_close(THD *thd, char *packet)
{
  /* There is always space for 4 bytes in packet buffer */
  ulong stmt_id= uint4korr(packet);
  Prepared_statement *stmt;

  DBUG_ENTER("mysql_stmt_close");

  if (!(stmt= find_prepared_statement(thd, stmt_id, "mysql_stmt_close")))
    DBUG_VOID_RETURN;

  /*
    The only way currently a statement can be deallocated when it's
    in use is from within Dynamic SQL.
  */
  DBUG_ASSERT(! (stmt->flags & Prepared_statement::IS_IN_USE));
  (void) stmt->deallocate();

  DBUG_VOID_RETURN;
}


/*
  SQLCOM_DEALLOCATE implementation.

  DESCRIPTION
    Close an SQL prepared statement. As this can be called from Dynamic
    SQL, we should be careful to not close a statement that is currently
    being executed.

  RETURN VALUE
    none: OK packet is sent in case of success, otherwise an error
    message is set in THD
*/

void mysql_sql_stmt_close(THD *thd)
{
  Prepared_statement* stmt;
  LEX_STRING *name= &thd->lex->prepared_stmt_name;
  DBUG_PRINT("info", ("DEALLOCATE PREPARE: %.*s\n", name->length, name->str));

  if (! (stmt= (Prepared_statement*) thd->stmt_map.find_by_name(name)))
  {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0),
             name->length, name->str, "DEALLOCATE PREPARE");
    return;
  }

  if (stmt->deallocate() == 0)
    send_ok(thd);
}

/*
  Handle long data in pieces from client.

  SYNOPSIS
    mysql_stmt_get_longdata()
      thd                Thread handle
      packet             String to append
      packet_length      Length of string

  DESCRIPTION
    Get a part of a long data. To make the protocol efficient, we are
    not sending any return packets here. If something goes wrong, then
    we will send the error on 'execute' We assume that the client takes
    care of checking that all parts are sent to the server. (No checking
    that we get a 'end of column' in the server is performed).
*/

void mysql_stmt_get_longdata(THD *thd, char *packet, ulong packet_length)
{
  ulong stmt_id;
  uint param_number;
  Prepared_statement *stmt;
  Item_param *param;
  char *packet_end= packet + packet_length - 1;

  DBUG_ENTER("mysql_stmt_get_longdata");

  statistic_increment(thd->status_var.com_stmt_send_long_data, &LOCK_status);
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

  if (!(stmt=find_prepared_statement(thd, stmt_id,
                                     "mysql_stmt_send_long_data")))
    DBUG_VOID_RETURN;

  param_number= uint2korr(packet);
  packet+= 2;
#ifndef EMBEDDED_LIBRARY
  if (param_number >= stmt->param_count)
  {
    /* Error will be sent in execute call */
    stmt->state= Query_arena::ERROR;
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
    stmt->state= Query_arena::ERROR;
    stmt->last_errno= ER_OUTOFMEMORY;
    sprintf(stmt->last_error, ER(ER_OUTOFMEMORY), 0);
  }
  DBUG_VOID_RETURN;
}


Prepared_statement::Prepared_statement(THD *thd_arg, Protocol *protocol_arg)
  :Statement(INITIALIZED, ++thd_arg->statement_id_counter,
             thd_arg->variables.query_alloc_block_size,
             thd_arg->variables.query_prealloc_size),
  thd(thd_arg),
  protocol(protocol_arg),
  param_array(0),
  param_count(0),
  last_errno(0),
  flags(IS_IN_USE)
{
  *last_error= '\0';
}


void Prepared_statement::setup_set_params()
{
  /* Setup binary logging */
  if (mysql_bin_log.is_open() && is_update_query(lex->sql_command) ||
      mysql_log.is_open() || mysql_slow_log.is_open())
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


/*
  DESCRIPTION
    Destroy this prepared statement, cleaning up all used memory
    and resources. This is called from ::deallocate() to
    handle COM_STMT_CLOSE and DEALLOCATE PREPARE or when
    THD ends and all prepared statements are freed.
*/

Prepared_statement::~Prepared_statement()
{
  DBUG_ENTER("Prepared_statement::~Prepared_statement");
  DBUG_PRINT("enter",("stmt: %p  cursor: %p", this, cursor));
  if (cursor)
  {
    if (cursor->is_open())
    {
      cursor->close(FALSE);
      cleanup_items(free_list);
      thd->rollback_item_tree_changes();
      free_root(cursor->mem_root, MYF(0));
    }
    cursor->Cursor::~Cursor();
  }
  /*
    We have to call free on the items even if cleanup is called as some items,
    like Item_param, don't free everything until free_items()
  */
  free_items();
  delete lex->result;
  DBUG_VOID_RETURN;
}


Query_arena::Type Prepared_statement::type() const
{
  return PREPARED_STATEMENT;
}


void Prepared_statement::close_cursor()
{
  DBUG_ENTER("Prepared_statement::close_cursor");
  DBUG_PRINT("enter",("stmt: %p", this));

  if (cursor && cursor->is_open())
  {
    thd->change_list= cursor->change_list;
    cursor->close(FALSE);
    cleanup_stmt_and_thd_after_use(this, thd);
    free_root(cursor->mem_root, MYF(0));
    if (cursor->close_at_commit)
      thd->stmt_map.erase_transient_cursor(this);
  }
  /*
    Clear parameters from data which could be set by
    mysql_stmt_send_long_data() call.
  */
  reset_stmt_params(this);
  DBUG_VOID_RETURN;
}


bool Prepared_statement::set_name(LEX_STRING *name_arg)
{
  name.length= name_arg->length;
  name.str= memdup_root(mem_root, (char*) name_arg->str, name_arg->length);
  return name.str == 0;
}

/**************************************************************************
  Common parts of mysql_[sql]_stmt_prepare, mysql_[sql]_stmt_execute.
  Essentially, these functions do all the magic of preparing/executing
  a statement, leaving network communication, input data handling and
  global THD state management to the caller.
***************************************************************************/

/*
  Parse statement text, validate the statement, and prepare it for execution.

  SYNOPSIS
    Prepared_statement::prepare()
      packet             statement text
      packet_len

  DESCRIPTION
    You should not change global THD state in this function, if at all
    possible: it may be called from any context, e.g. when executing
    a COM_* command, and SQLCOM_* command, or a stored procedure.

  NOTES
      Precondition.
      -------------
    The caller must ensure that thd->change_list and thd->free_list
    is empty: this function will not back them up but will free
    in the end of its execution.

      Postcondition.
      --------------
    thd->mem_root contains unused memory allocated during validation.
*/

bool Prepared_statement::prepare(const char *packet, uint packet_len)
{
  bool rc;
  Statement stmt_backup;
  Query_arena *old_stmt_arena;
  DBUG_ENTER("Prepared_statement::prepare");
  /*
    If this is an SQLCOM_PREPARE, we also increase Com_prepare_sql.
    However, it seems handy if com_stmt_prepare is increased always,
    no matter what kind of prepare is processed.
  */
  statistic_increment(thd->status_var.com_stmt_prepare, &LOCK_status);

  /*
    alloc_query() uses thd->memroot && thd->query, so we should call
    both of backup_statement() and backup_query_arena() here.
  */
  thd->set_n_backup_statement(this, &stmt_backup);
  thd->set_n_backup_active_arena(this, &stmt_backup);

  if (alloc_query(thd, packet, packet_len))
  {
    thd->restore_backup_statement(this, &stmt_backup);
    thd->restore_active_arena(this, &stmt_backup);
    DBUG_RETURN(TRUE);
  }

  old_stmt_arena= thd->stmt_arena;
  thd->stmt_arena= this;
  lex_start(thd, (uchar*) thd->query, thd->query_length);
  lex->safe_to_cache_query= FALSE;
  lex->stmt_prepare_mode= TRUE;

  rc= yyparse((void *)thd) || thd->is_fatal_error ||
      thd->net.report_error || init_param_array(this);
  /*
    While doing context analysis of the query (in check_prepared_statement)
    we allocate a lot of additional memory: for open tables, JOINs, derived
    tables, etc.  Let's save a snapshot of current parse tree to the
    statement and restore original THD. In cases when some tree
    transformation can be reused on execute, we set again thd->mem_root from
    stmt->mem_root (see setup_wild for one place where we do that).
  */
  thd->restore_active_arena(this, &stmt_backup);

  /*
    If called from a stored procedure, ensure that we won't rollback
    external changes when cleaning up after validation.
  */
  DBUG_ASSERT(thd->change_list.is_empty());
  /*
    If the free_list is not empty, we'll wrongly free some externally
    allocated items when cleaning up after validation of the prepared
    statement.
  */
  DBUG_ASSERT(thd->free_list == NULL);

  if (rc == 0)
    rc= check_prepared_statement(this, name.str != 0);

  if (rc && thd->lex->sphead)
  {
    delete thd->lex->sphead;
    thd->lex->sphead= NULL;
  }
  lex_end(lex);
  close_thread_tables(thd);
  cleanup_stmt_and_thd_after_use(this, thd);
  thd->restore_backup_statement(this, &stmt_backup);
  thd->stmt_arena= old_stmt_arena;

  if (rc == 0)
  {
    setup_set_params();
    init_stmt_after_parse(lex);
    state= Query_arena::PREPARED;
    flags&= ~IS_IN_USE;
  }
  DBUG_RETURN(rc);
}

/*
  Execute a prepared statement.

  SYNOPSIS
    Prepared_statement::execute()
      expanded_query     A query for binlogging which has all parameter
                         markers ('?') replaced with their actual values.
      open_cursor        True if an attempt to open a cursor should be made.
                         Currenlty used only in the binary protocol.

  DESCRIPTION
    You should not change global THD state in this function, if at all
    possible: it may be called from any context, e.g. when executing
    a COM_* command, and SQLCOM_* command, or a stored procedure.

  NOTES
      Preconditions, postconditions.
      ------------------------------
      See the comment for Prepared_statement::prepare().
*/

bool Prepared_statement::execute(String *expanded_query, bool open_cursor)
{
  Statement stmt_backup;
  Query_arena *old_stmt_arena;
  Item *old_free_list;
  bool rc= 1;

  statistic_increment(thd->status_var.com_stmt_execute, &LOCK_status);

  /* Check if we got an error when sending long data */
  if (state == Query_arena::ERROR)
  {
    my_message(last_errno, last_error, MYF(0));
    return 1;
  }
  if (flags & IS_IN_USE)
  {
    my_error(ER_PS_NO_RECURSION, MYF(0));
    return 1;
  }
  /* In case the command has a call to SP which re-uses this statement name */
  flags|= IS_IN_USE;

  if (cursor && cursor->is_open())
    close_cursor();

  /*
    If the free_list is not empty, we'll wrongly free some externally
    allocated items when cleaning up after execution of this statement.
  */
  DBUG_ASSERT(thd->change_list.is_empty());
  DBUG_ASSERT(thd->free_list == NULL);
  if (open_cursor)
  {
    if (!lex->result || !lex->result->simple_select())
    {
      DBUG_PRINT("info",("Cursor asked for not SELECT stmt"));
      /*
        If lex->result is set in the parser, this is not a SELECT
        statement: we can't open a cursor for it.
      */
      my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
      goto error;
    }

    DBUG_PRINT("info",("Using READ_ONLY cursor"));
    if (!cursor && !(cursor= new (mem_root) Cursor(thd)))
      goto error;
    /* If lex->result is set, mysql_execute_command will use it */
    lex->result= &cursor->result;
    protocol= &cursor->protocol;
    thd->lock_id= &cursor->lock_id;
    /*
      Currently cursors can be used only from C API, so
      we don't have to create an own memory root for them:
      the one in THD is clean and can be used.
    */
  }
  thd->set_n_backup_statement(this, &stmt_backup);
  if (expanded_query->length() &&
      alloc_query(thd, (char*) expanded_query->ptr(),
                  expanded_query->length()+1))
  {
    my_error(ER_OUTOFMEMORY, 0, expanded_query->length());
    goto error;
  }
  /*
    Expanded query is needed for slow logging, so we want thd->query
    to point at it even after we restore from backup. This is ok, as
    expanded query was allocated in thd->mem_root.
  */
  stmt_backup.query= thd->query;
  stmt_backup.query_length= thd->query_length;

  /*
    At first execution of prepared statement we may perform logical
    transformations of the query tree. Such changes should be performed
    on the parse tree of current prepared statement and new items should
    be allocated in its memory root. Set the appropriate pointer in THD
    to the arena of the statement.
  */
  old_stmt_arena= thd->stmt_arena;
  thd->stmt_arena= this;
  reinit_stmt_before_use(thd, lex);

  thd->protocol= protocol;                      /* activate stmt protocol */
  mysql_execute_command(thd);
  thd->protocol= &thd->protocol_simple;         /* use normal protocol */

  if (cursor && cursor->is_open())
  {
    /*
      It's safer if we grab THD state after mysql_execute_command is
      finished and not in Cursor::open(), because currently the call to
      Cursor::open is buried deep in JOIN::exec of the top level join.
    */
    cursor->init_from_thd(thd);

    if (cursor->close_at_commit)
      thd->stmt_map.add_transient_cursor(this);
  }
  else
  {
    close_thread_tables(thd);
    cleanup_stmt_and_thd_after_use(this, thd);
    reset_stmt_params(this);
  }

  thd->set_statement(&stmt_backup);
  thd->lock_id= &thd->main_lock_id;
  thd->stmt_arena= old_stmt_arena;

  if (state == Query_arena::PREPARED)
    state= Query_arena::EXECUTED;

  rc= 0;
error:
  thd->lock_id= &thd->main_lock_id;
  flags&= ~IS_IN_USE;
  return rc;
}


/* Common part of DEALLOCATE PREPARE and mysql_stmt_close */

bool Prepared_statement::deallocate()
{
  /* We account deallocate in the same manner as mysql_stmt_close */
  statistic_increment(thd->status_var.com_stmt_close, &LOCK_status);
  if (flags & IS_IN_USE)
  {
    my_error(ER_PS_NO_RECURSION, MYF(0));
    return TRUE;
  }
  /* Statement map calls delete stmt on erase */
  thd->stmt_map.erase(this);
  return FALSE;
}
