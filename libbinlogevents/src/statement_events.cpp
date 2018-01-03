/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "statement_events.h"
#include <algorithm>
#include <string>

namespace binary_log
{

/******************************************************************************
                     Query_event methods
******************************************************************************/
/**
  The simplest constructor that could possibly work.  This is used for
  creating static objects that have a special meaning and are invisible
  to the log.
*/
Query_event::Query_event(Log_event_type type_arg)
: Binary_log_event(type_arg),
  query(0), db(0), user(0), user_len(0), host(0), host_len(0),
  db_len(0), q_len(0)
{
}

/**
  The constructor used by MySQL master to create a query event, to be
  written to the binary log.
*/
Query_event::Query_event(const char* query_arg, const char* catalog_arg,
                         const char* db_arg, uint32_t query_length,
                         unsigned long thread_id_arg,
                         unsigned long long sql_mode_arg,
                         unsigned long auto_increment_increment_arg,
                         unsigned long auto_increment_offset_arg,
                         unsigned int number,
                         unsigned long long table_map_for_update_arg,
                         int errcode)
: Binary_log_event(QUERY_EVENT),
  query(query_arg), db(db_arg), catalog(catalog_arg),
  user(0), user_len(0), host(0), host_len(0),
  thread_id(thread_id_arg), db_len(0), error_code(errcode),
  status_vars_len(0), q_len(query_length),
  flags2_inited(1), sql_mode_inited(1), charset_inited(1),
  sql_mode(sql_mode_arg),
  auto_increment_increment(static_cast<uint16_t>(auto_increment_increment_arg)),
  auto_increment_offset(static_cast<uint16_t>(auto_increment_offset_arg)),
  time_zone_len(0), lc_time_names_number(number),
  charset_database_number(0),
  table_map_for_update(table_map_for_update_arg),
  explicit_defaults_ts(TERNARY_UNSET),
  mts_accessed_dbs(0), ddl_xid(INVALID_XID)
{
}

/**
  Utility function for the Query_event constructor.
  The function copies n bytes from the source string and moves the
  destination pointer by the number of bytes copied.

  @param dst Pointer to the buffer into which the string is to be copied
  @param src Source string
  @param len The number of bytes to be copied
*/
static void copy_str_and_move(Log_event_header::Byte **dst,
                              const char** src,
                              size_t len)
{
  memcpy(*dst, *src, len);
  *src= reinterpret_cast<const char*>(*dst);
  (*dst)+= len;
  *(*dst)++= 0;
}


/**
   Macro to check that there is enough space to read from memory.

   @param PTR Pointer to memory
   @param END End of memory
   @param CNT Number of bytes that should be read.
*/
#define CHECK_SPACE(PTR,END,CNT)                      \
  do {                                                \
    BAPI_ASSERT((PTR) + (CNT) <= (END));              \
    if ((PTR) + (CNT) > (END)) {                      \
      query= 0;                                       \
      return;                               \
    }                                                 \
  } while (0)


/**
  The event occurs when an updating statement is done.
*/
Query_event::Query_event(const char* buf, unsigned int event_len,
                         const Format_description_event *description_event,
                         Log_event_type event_type)
: Binary_log_event(&buf, description_event->binlog_version),
  query(0), db(0), catalog(0), time_zone_str(0),
  user(0), user_len(0), host(0), host_len(0),
  db_len(0), status_vars_len(0), q_len(0),
  flags2_inited(0), sql_mode_inited(0), charset_inited(0),
  auto_increment_increment(1), auto_increment_offset(1),
  time_zone_len(0), catalog_len(0), lc_time_names_number(0),
  charset_database_number(0), table_map_for_update(0),
  explicit_defaults_ts(TERNARY_UNSET),
  mts_accessed_dbs(OVER_MAX_DBS_IN_EVENT_MTS), ddl_xid(INVALID_XID)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  uint32_t tmp;
  uint8_t common_header_len, post_header_len;
  Log_event_header::Byte *start;
  const Log_event_header::Byte *end;

  query_data_written= 0;

  common_header_len= description_event->common_header_len;
  post_header_len= description_event->post_header_len[event_type - 1];

  /*
    We test if the event's length is sensible, and if so we compute data_len.
    We cannot rely on QUERY_HEADER_LEN here as it would not be format-tolerant.
    We use QUERY_HEADER_MINIMAL_LEN which is the same for 3.23, 4.0 & 5.0.
  */
  if (event_len < (unsigned int)(common_header_len + post_header_len))
    return;
  data_len= event_len - (common_header_len + post_header_len);

  memcpy(&thread_id, buf + Q_THREAD_ID_OFFSET, sizeof(thread_id));
  thread_id= le32toh(thread_id);
  memcpy(&query_exec_time, buf + Q_EXEC_TIME_OFFSET, sizeof(query_exec_time));
  query_exec_time= le32toh(query_exec_time);

  db_len= (unsigned char)buf[Q_DB_LEN_OFFSET];
   // TODO: add a check of all *_len vars
  memcpy(&error_code, buf + Q_ERR_CODE_OFFSET, sizeof(error_code));
  error_code= le16toh(error_code);

  /*
    5.0 format starts here.
    Depending on the format, we may or not have affected/warnings etc
    The remnent post-header to be parsed has length:
  */
  tmp= post_header_len - QUERY_HEADER_MINIMAL_LEN;
  if (tmp)
  {
    memcpy(&status_vars_len, buf + Q_STATUS_VARS_LEN_OFFSET,
           sizeof(status_vars_len));
    status_vars_len= le16toh(status_vars_len);
    /*
      Check if status variable length is corrupt and will lead to very
      wrong data. We could be even more strict and require data_len to
      be even bigger, but this will suffice to catch most corruption
      errors that can lead to a crash.
    */
    if (status_vars_len >
        std::min<unsigned long>(data_len, MAX_SIZE_LOG_EVENT_STATUS))
    {
      query= 0;
      return;
    }
    data_len-= status_vars_len;
    tmp-= 2;
  }
  else
  {
    /* formats before 5.0 are not supported anymore */
    BAPI_ASSERT(0);
    return;
  }
  /*
    We have parsed everything we know in the post header for QUERY_EVENT,
    the rest of post header is either comes from older version MySQL or
    dedicated to derived events (e.g. Execute_load_query...)
  */

  /* variable-part: the status vars; only in MySQL 5.0  */
  start= (Log_event_header::Byte*) (buf + post_header_len);
  end= (const Log_event_header::Byte*) (start + status_vars_len);
  for (const Log_event_header::Byte* pos= start; pos < end;)
  {
    switch (*pos++) {
    case Q_FLAGS2_CODE:
      CHECK_SPACE(pos, end, 4);
      flags2_inited= 1;
      memcpy(&flags2, pos, sizeof(flags2));
      flags2= le32toh(flags2);
      pos+= 4;
      break;
    case Q_SQL_MODE_CODE:
    {
      CHECK_SPACE(pos, end, 8);
      sql_mode_inited= 1;
      memcpy(&sql_mode, pos, sizeof(sql_mode));
      sql_mode= le64toh(sql_mode);
      pos+= 8;
      break;
    }
    case Q_CATALOG_NZ_CODE:
      if ((catalog_len= *pos))
        catalog= (const char*) (pos + 1);
      CHECK_SPACE(pos, end, catalog_len + 1);
      pos+= catalog_len + 1;
      break;
    case Q_AUTO_INCREMENT:
      CHECK_SPACE(pos, end, 4);
      memcpy(&auto_increment_increment, pos, sizeof(auto_increment_increment));
      auto_increment_increment= le16toh(auto_increment_increment);
      memcpy(&auto_increment_offset, pos + 2, sizeof(auto_increment_offset));
      auto_increment_offset= le16toh(auto_increment_offset);
      pos+= 4;
      break;
    case Q_CHARSET_CODE:
    {
      CHECK_SPACE(pos, end, 6);
      charset_inited= 1;
      memcpy(charset, pos, 6);
      pos+= 6;
      break;
    }
    case Q_TIME_ZONE_CODE:
    {
      if ((time_zone_len= *pos))
        time_zone_str= (const char*)(pos + 1);
      pos+= time_zone_len + 1;
      break;
    }
    case Q_CATALOG_CODE: /* for 5.0.x where 0<=x<=3 masters */
      CHECK_SPACE(pos, end, 1);
      if ((catalog_len= *pos))
        catalog= (const char*) (pos+1);
      CHECK_SPACE(pos, end, catalog_len + 2);
      pos+= catalog_len + 2; // leap over end 0
      break;
    case Q_LC_TIME_NAMES_CODE:
      CHECK_SPACE(pos, end, 2);
      memcpy(&lc_time_names_number, pos, sizeof(lc_time_names_number));
      lc_time_names_number= le16toh(lc_time_names_number);
      pos+= 2;
      break;
    case Q_CHARSET_DATABASE_CODE:
      CHECK_SPACE(pos, end, 2);
      memcpy(&charset_database_number, pos, sizeof(lc_time_names_number));
      charset_database_number= le16toh(charset_database_number);
      pos+= 2;
      break;
    case Q_TABLE_MAP_FOR_UPDATE_CODE:
      CHECK_SPACE(pos, end, 8);
      memcpy(&table_map_for_update, pos, sizeof(table_map_for_update));
      table_map_for_update= le64toh(table_map_for_update);
      pos+= 8;
      break;
    case Q_MICROSECONDS:
    {
      CHECK_SPACE(pos, end, 3);
      uint32_t temp_usec= 0;
      memcpy(&temp_usec, pos, 3);
      header()->when.tv_usec= le32toh(temp_usec);
      pos+= 3;
break;
    }
    case Q_INVOKER:
    {
      CHECK_SPACE(pos, end, 1);
      user_len= *pos++;
      CHECK_SPACE(pos, end, user_len);
      user= (const char*)pos;
      if (user_len == 0)
        user= (const char *)"";
      pos+= user_len;

      CHECK_SPACE(pos, end, 1);
      host_len= *pos++;
      CHECK_SPACE(pos, end, host_len);
      host= (const char*)pos;
      if (host_len == 0)
        host= (const char *)"";
      pos+= host_len;
      break;
    }
    case Q_UPDATED_DB_NAMES:
    {
      unsigned char i= 0;
#ifndef DBUG_OFF
      bool is_corruption_injected= false;
#endif

      CHECK_SPACE(pos, end, 1);
      mts_accessed_dbs= *pos++;
      /*
         Notice, the following check is positive also in case of
         the master's MAX_DBS_IN_EVENT_MTS > the slave's one and the event
         contains e.g the master's MAX_DBS_IN_EVENT_MTS db:s.
      */
      if (mts_accessed_dbs > MAX_DBS_IN_EVENT_MTS)
      {
        mts_accessed_dbs= OVER_MAX_DBS_IN_EVENT_MTS;
        break;
      }

      BAPI_ASSERT(mts_accessed_dbs != 0);

      for (i= 0; i < mts_accessed_dbs && pos < start + status_vars_len; i++)
      {
        #ifndef DBUG_OFF
        /*
          This is specific to mysql test run on the server
          for the keyword "query_log_event_mts_corrupt_db_names"
        */
        if (binary_log_debug::debug_query_mts_corrupt_db_names)
        {
          if (mts_accessed_dbs == 2)
          {
            BAPI_ASSERT(pos[sizeof("d?") - 1] == 0);
            ((char*) pos)[sizeof("d?") - 1]= 'a';
            is_corruption_injected= true;
          }
        }
        #endif
        strncpy(mts_accessed_db_names[i], (char*) pos,
                std::min<unsigned long>(NAME_LEN, start + status_vars_len - pos));
        mts_accessed_db_names[i][NAME_LEN - 1]= 0;
        pos+= 1 + strlen((const char*) pos);
      }
      if (i != mts_accessed_dbs
#ifndef DBUG_OFF
          || is_corruption_injected
#endif
          )
        return;
      break;
    }
    case Q_EXPLICIT_DEFAULTS_FOR_TIMESTAMP:
    {
      CHECK_SPACE(pos, end, 1);
      explicit_defaults_ts= *pos++ == 0 ? TERNARY_OFF : TERNARY_ON;
      break;
    }
    case Q_DDL_LOGGED_WITH_XID:
      CHECK_SPACE(pos, end, 8);
      /*
        Like in Xid_log_event case, the xid value is not used on the slave
        so the number does not really need to respect endiness.
      */
      memcpy((char*) &ddl_xid, pos, 8);
      ddl_xid= le64toh(ddl_xid);
      pos+= 8;
      break;
    default:
      /* That's why you must write status vars in growing order of code */
      pos= (const unsigned char*) end;         // Break loop
    }
  }
  if (catalog_len)                             // If catalog is given
    query_data_written+= catalog_len + 1;
  if (time_zone_len)
    query_data_written+= time_zone_len + 1;
  if (user_len > 0)
    query_data_written+= user_len + 1;
  if (host_len > 0)
    query_data_written+= host_len + 1;

  /*
    if time_zone_len or catalog_len are 0, then time_zone and catalog
    are uninitialized at this point.  shouldn't they point to the
    zero-length null-terminated strings we allocated space for in the
    my_alloc call above? /sven
  */

  /* A 2nd variable part; this is common to all versions */
  query_data_written+= data_len + 1;
  db= (const char* )end;
  q_len= data_len - db_len -1;
  query= (const char *)(end + db_len + 1);
  return;
}

/**
  Layout for the data buffer is as follows
  <pre>
  +--------+-----------+------+------+---------+----+-------+----+
  | catlog | time_zone | user | host | db name | \0 | Query | \0 |
  +--------+-----------+------+------+---------+----+-------+----+
  </pre>
*/
int Query_event::fill_data_buf(Log_event_header::Byte* buf,
                               unsigned long buf_len)
{
  if (!buf)
    return 0;
  /* We need to check the buffer size */
  if (buf_len < catalog_len + 1 + time_zone_len +
                1 + user_len+ 1 + host_len+ 1 + data_len )
    return 0;
  unsigned char* start= buf;
  /*
    Note: catalog_len is one less than "catalog.length()"
    if Q_CATALOG flag is set
   */
  if (catalog_len)                                  // If catalog is given
    /*
      This covers both the cases, where the catalog_nz flag is set of unset.
      The end 0 will be a part of the string catalog in the second case,
      hence using catalog.length() instead of catalog_len makes the flags
      catalog_nz redundant.
     */
    copy_str_and_move(&start, &catalog, catalog_len);
  if (time_zone_len > 0)
    copy_str_and_move(&start, &time_zone_str, time_zone_len);
  if (user_len > 0)
    copy_str_and_move(&start, &user, user_len);
  if (host_len > 0)
    copy_str_and_move(&start, &host, host_len);
  if (data_len)
  {
    if (db_len >0 && db)
      copy_str_and_move(&start, &db, db_len);
    if (q_len > 0 && query)
      copy_str_and_move(&start, &query, q_len);
  }
  return 1;
}

/**
  The constructor for User_var_event.
*/
User_var_event::
User_var_event(const char* buf, unsigned int event_len,
               const Format_description_event* description_event)
  :Binary_log_event(&buf, description_event->binlog_version)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  bool error= false;
  const char* buf_start= buf - description_event->common_header_len;
  /* The Post-Header is empty. The Variable Data part begins immediately. */
  const char *start= buf_start;
  buf+= description_event->post_header_len[USER_VAR_EVENT-1];

  memcpy(&name_len, buf, 4);
  name_len= le32toh(name_len);
  name= (char *) buf + UV_NAME_LEN_SIZE;
  /*
    We don't know yet is_null value, so we must assume that name_len
    may have the bigger value possible, is_null= True and there is no
    payload for val, or even that name_len is 0.
  */
  if (!valid_buffer_range<unsigned int>(name_len, buf_start, name,
                                        event_len - UV_VAL_IS_NULL))
  {
    error= true;
    goto err;
  }

  buf+= UV_NAME_LEN_SIZE + name_len;
  is_null= (bool) *buf;
  flags= User_var_event::UNDEF_F;    // defaults to UNDEF_F
  if (is_null)
  {
    type= STRING_TYPE;
    /*
    *my_charset_bin.number= 63, and my_charset_bin is defined in server
    *so replacing it with its value.
    */
    charset_number= 63;
    val_len= 0;
    val= 0;
  }

  else
  {
    if (!valid_buffer_range<unsigned int>(UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE
                                          + UV_CHARSET_NUMBER_SIZE +
                                          UV_VAL_LEN_SIZE, buf_start, buf,
                                          event_len))
    {
      error= true;
      goto err;
    }

    type= (Value_type) buf[UV_VAL_IS_NULL];
     memcpy(&charset_number, buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE,
            sizeof(charset_number));
    charset_number= le32toh(charset_number);
    memcpy(&val_len, (buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
           UV_CHARSET_NUMBER_SIZE), sizeof(val_len));
    val_len= le32toh(val_len);
    val= (char *) (buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
                   UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE);

    if (!valid_buffer_range<unsigned int>(val_len, buf_start, val, event_len))
    {
      error= true;
      goto err;
    }

    /**
      We need to check if this is from an old server
      that did not pack information for flags.
      We do this by checking if there are extra bytes
      after the packed value. If there are we take the
      extra byte and it's value is assumed to contain
      the flags value.

      Old events will not have this extra byte, thence,
      we keep the flags set to UNDEF_F.
    */
  size_t bytes_read= ((val + val_len) - start);
#ifndef DBUG_OFF
  bool old_pre_checksum_fd= description_event->is_version_before_checksum();
  bool checksum_verify= (old_pre_checksum_fd ||
                         (description_event->footer()->checksum_alg ==
                          BINLOG_CHECKSUM_ALG_OFF));
  size_t data_written= (header()->data_written- checksum_verify);
  BAPI_ASSERT(((bytes_read == data_written) ? false : true) ||
              ((bytes_read == data_written - 1) ? false : true));
#endif
    if ((header()->data_written - bytes_read) > 0)
    {
      flags= (unsigned int) *(buf + UV_VAL_IS_NULL + UV_VAL_TYPE_SIZE +
                              UV_CHARSET_NUMBER_SIZE + UV_VAL_LEN_SIZE +
                              val_len);
    }
  }
err:
  if (error)
    name= 0;
}

/**
  Constructor receives a packet from the MySQL master or the binary
  log and decodes it to create an Intvar_event.
  Written every time a statement uses an AUTO_INCREMENT column or the
  LAST_INSERT_ID() function; precedes other events for the statement.
  This is written only before a QUERY_EVENT and is not used with row-based
  logging. An INTVAR_EVENT is written with a "subtype" in the event data part:

  * INSERT_ID_EVENT indicates the value to use for an AUTO_INCREMENT column in
    the next statement.

  * LAST_INSERT_ID_EVENT indicates the value to use for the LAST_INSERT_ID()
    function in the next statement.
*/
Intvar_event::Intvar_event(const char* buf,
                           const Format_description_event* description_event)
: Binary_log_event(&buf, description_event->binlog_version)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  /* The Post-Header is empty. The Varible Data part begins immediately. */
  buf+= description_event->post_header_len[INTVAR_EVENT - 1];
  type= buf[I_TYPE_OFFSET];
  memcpy(&val, buf + I_VAL_OFFSET, 8);
  val= le64toh(val);
}

Rand_event::Rand_event(const char* buf,
                       const Format_description_event* description_event)
  :Binary_log_event(&buf, description_event->binlog_version)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  /*
   We step to the post-header despite it being empty because it could later be
   filled with something and we have to support that case.
   The Variable Data part begins immediately.
  */
  buf+= description_event->post_header_len[RAND_EVENT - 1];
  memcpy(&seed1, buf + RAND_SEED1_OFFSET, 8);
  seed1= le64toh(seed1);
  memcpy(&seed2, buf + RAND_SEED2_OFFSET, 8);
  seed2= le64toh(seed2);
}

#ifndef HAVE_MYSYS
void Query_event::print_event_info(std::ostream& info)
{
  if (memcmp(query, "BEGIN", 5) != 0 &&
      memcmp(query, "COMMIT", 6) != 0)
  {
    info << "use `" << db << "`; ";
  }
  info << query;
}

void Query_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\tThread id: " << (int)thread_id;
  info << "\tExec time: " << (int)query_exec_time;
  info << "\nDatabase: " << db;
  info << "\tQuery: ";
  this->print_event_info(info);
}

void User_var_event::print_event_info(std::ostream& info)
{
  info << "@`" << name << "`=";
  if(type == STRING_TYPE)
    info  << val;
  else
    info << "<Binary encoded value>";
  //TODO: value is binary encoded, requires decoding
}

void User_var_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\tType: "
       << get_value_type_string(static_cast<Value_type>(type));
  info << "\n";
  this->print_event_info(info);
}

void Intvar_event::print_event_info(std::ostream& info)
{
  info << get_var_type_string();
  info << "\tValue: " << val;
}

void Intvar_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\t";
  this->print_event_info(info);
}

void Rand_event::print_event_info(std::ostream& info)
{
  info << " SEED1 is " << seed1;
  info << " SEED2 is " << seed2;
}
void Rand_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\t";
  this->print_event_info(info);
}

#endif
}//end namespace

