/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <errmsg.h>
#include <my_byteorder.h>
#include <my_dbug.h>
#include <my_sys.h>
#include <my_time.h>
#include <cstdint>
#include "m_string.h"
#include "my_config.h"
#include "mysql.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "sql_common.h"

/*
  Reallocate the NET package to have at least length bytes available.

  SYNPOSIS
    my_realloc_str()
    net                 The NET structure to modify.
    length              Ensure that net->buff has space for at least
                        this number of bytes.

  RETURN VALUES
    0   Success.
    1   Error, i.e. out of memory or requested packet size is bigger
        than max_allowed_packet. The error code is stored in net->last_errno.
*/

static bool my_realloc_str(NET *net, ulong length) {
  ulong buf_length = (ulong)(net->write_pos - net->buff);
  bool res = false;
  DBUG_TRACE;
  if (buf_length + length > net->max_packet) {
    res = net_realloc(net, buf_length + length);
    if (res) {
      if (net->last_errno == ER_OUT_OF_RESOURCES)
        net->last_errno = CR_OUT_OF_MEMORY;
      else if (net->last_errno == ER_NET_PACKET_TOO_LARGE)
        net->last_errno = CR_NET_PACKET_TOO_LARGE;

      my_stpcpy(net->sqlstate, unknown_sqlstate);
      my_stpcpy(net->last_error, ER_CLIENT(net->last_errno));
    }
    net->write_pos = net->buff + buf_length;
  }
  return res;
}

/*
  Maximum sizes of MYSQL_TYPE_DATE, MYSQL_TYPE_TIME, MYSQL_TYPE_DATETIME
  values stored in network buffer.
*/

/* 1 (length) + 2 (year) + 1 (month) + 1 (day) */
#define MAX_DATE_REP_LENGTH 5

/*
  1 (length) + 1 (is negative) + 4 (day count) + 1 (hour)
  + 1 (minute) + 1 (seconds) + 4 (microseconds)
*/
#define MAX_TIME_REP_LENGTH 13

constexpr int MAX_DATETIME_REP_LENGTH =
    1 /* length */ + 2 /* year */ + 1 /* month */ + 1 /* day */ + 1 /* hour */ +
    1 /* minute */ + 1 /* second */ + 4 /* microseconds */ +
    2 /* time zone displacement (signed) */;

/* Store type of parameter in network buffer. */

static void store_param_type(unsigned char **pos, MYSQL_BIND *param) {
  uint typecode = param->buffer_type | (param->is_unsigned ? 32768 : 0);
  int2store(*pos, typecode);
  *pos += 2;
}

/*
  Functions to store parameter data in network packet.

  SYNOPSIS
    store_param_xxx()
    net			MySQL NET connection
    param		MySQL bind param

  DESCRIPTION
    These functions are invoked from mysql_stmt_execute() by
    MYSQL_BIND::store_param_func pointer. This pointer is set once per
    many executions in mysql_stmt_bind_param(). The caller must ensure
    that network buffer have enough capacity to store parameter
    (MYSQL_BIND::buffer_length contains needed number of bytes).
*/

static void store_param_tinyint(NET *net, MYSQL_BIND *param) {
  *(net->write_pos++) = *(uchar *)param->buffer;
}

static void store_param_short(NET *net, MYSQL_BIND *param) {
  short value = *(short *)param->buffer;
  int2store(net->write_pos, value);
  net->write_pos += 2;
}

static void store_param_int32(NET *net, MYSQL_BIND *param) {
  int32 value = *(int32 *)param->buffer;
  int4store(net->write_pos, value);
  net->write_pos += 4;
}

static void store_param_int64(NET *net, MYSQL_BIND *param) {
  longlong value = *(longlong *)param->buffer;
  int8store(net->write_pos, value);
  net->write_pos += 8;
}

static void store_param_float(NET *net, MYSQL_BIND *param) {
  float value = *(float *)param->buffer;
  float4store(net->write_pos, value);
  net->write_pos += 4;
}

static void store_param_double(NET *net, MYSQL_BIND *param) {
  double value = *(double *)param->buffer;
  float8store(net->write_pos, value);
  net->write_pos += 8;
}

static void store_param_time(NET *net, MYSQL_BIND *param) {
  MYSQL_TIME *tm = (MYSQL_TIME *)param->buffer;
  uchar buff[MAX_TIME_REP_LENGTH], *pos;
  uint length;

  pos = buff + 1;
  pos[0] = tm->neg ? 1 : 0;
  int4store(pos + 1, tm->day);
  pos[5] = (uchar)tm->hour;
  pos[6] = (uchar)tm->minute;
  pos[7] = (uchar)tm->second;
  int4store(pos + 8, tm->second_part);
  if (tm->second_part)
    length = 12;
  else if (tm->hour || tm->minute || tm->second || tm->day)
    length = 8;
  else
    length = 0;
  buff[0] = (char)length++;
  memcpy((char *)net->write_pos, buff, length);
  net->write_pos += length;
}

static void net_store_datetime(NET *net, MYSQL_TIME *tm) {
  uchar buff[MAX_DATETIME_REP_LENGTH], *pos;
  // The content of the buffer's length byte.
  uchar length_byte;

  pos = buff + 1;

  int2store(pos, static_cast<std::uint16_t>(tm->year));
  pos[2] = static_cast<std::uint8_t>(tm->month);
  pos[3] = static_cast<std::uint8_t>(tm->day);
  pos[4] = static_cast<std::uint8_t>(tm->hour);
  pos[5] = static_cast<std::uint8_t>(tm->minute);
  pos[6] = static_cast<std::uint8_t>(tm->second);
  int4store(pos + 7, static_cast<std::uint32_t>(tm->second_part));
  if (tm->time_type == MYSQL_TIMESTAMP_DATETIME_TZ) {
    int tzd = tm->time_zone_displacement;
    assert(tzd % SECS_PER_MIN == 0);
    assert(std::abs(tzd) <= MAX_TIME_ZONE_HOURS * SECS_PER_HOUR);
    int2store(pos + 11, static_cast<std::uint16_t>(tzd / SECS_PER_MIN));
    length_byte = 13;
  } else if (tm->second_part)
    length_byte = 11;
  else if (tm->hour || tm->minute || tm->second)
    length_byte = 7;
  else if (tm->year || tm->month || tm->day)
    length_byte = 4;
  else
    length_byte = 0;

  buff[0] = length_byte;

  size_t buffer_length = length_byte + 1;
  memcpy(net->write_pos, buff, buffer_length);
  net->write_pos += buffer_length;
}

static void store_param_date(NET *net, MYSQL_BIND *param) {
  MYSQL_TIME tm = *((MYSQL_TIME *)param->buffer);
  tm.hour = tm.minute = tm.second = tm.second_part = 0;
  net_store_datetime(net, &tm);
}

static void store_param_datetime(NET *net, MYSQL_BIND *param) {
  MYSQL_TIME *tm = (MYSQL_TIME *)param->buffer;
  net_store_datetime(net, tm);
}

static void store_param_str(NET *net, MYSQL_BIND *param) {
  /* param->length is always set in mysql_stmt_bind_param */
  ulong length = *param->length;
  uchar *to = net_store_length(net->write_pos, length);
  memcpy(to, param->buffer, length);
  net->write_pos = to + length;
}

/**
  Mark the parameter as NULL.

  @param net   MySQL NET connection
  @param param MySQL bind param
  @param null_pos_ofs the offset from the start of the buffer to
                      the first byte of the null mask

  A data package starts with a string of bits where we set a bit
  if a parameter is NULL. Unlike bit string in result set row, here
  we don't have reserved bits for OK/error packet.
*/

static void store_param_null(NET *net, MYSQL_BIND *param,
                             my_off_t null_pos_ofs) {
  uint pos = param->param_number;
  net->buff[pos / 8 + null_pos_ofs] |= (uchar)(1 << (pos & 7));
}

/**
  Store one parameter in network packet: data is read from
  client buffer and saved in network packet by means of one
  of store_param_xxxx functions.

  @param net   MySQL NET connection
  @param param MySQL bind param
  @param null_pos_ofs the offset from the start of the buffer to
                      the first byte of the null mask
  @retval true failure
  @retval false success
*/

static bool store_param(NET *net, MYSQL_BIND *param, my_off_t null_pos_ofs) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("type: %d  buffer: %p  length: %lu  is_null: %d",
              param->buffer_type, (param->buffer ? param->buffer : NullS),
              *param->length, *param->is_null));

  if (*param->is_null)
    store_param_null(net, param, null_pos_ofs);
  else {
    /*
      Param->length should ALWAYS point to the correct length for the type
      Either to the length pointer given by the user or param->buffer_length
    */
    if ((my_realloc_str(net, *param->length))) {
      return true;
    }
    (*param->store_param_func)(net, param);
  }
  return false;
}

/**
  Serialize the query parameters.

  Used by @ref mysql_real_query, @ref mysql_real_query_nonblocking and @ref
  mysql_stmt_execute()

  Must be called on connected sessions only.

  @param net the NET to use as a string buffer serializing the params. It's
  cleared at start.
  @param param_count the number of parameters to send
  @param params the filled in MYSQL_BIND structure to retrieve the values from
  @param names the names of the parameters in the params argument
  @param n_param_sets the number of sets of values to set
  @param[out] ret_data the buffer to the serialized parameter representation
  @param[out] ret_length the number of bytes stored into the buffer
  @param send_types_to_server : whether to send the parameter types to the
  server or not
  @param send_named_params : whether the names of the parameters should be sent
  @param send_parameter_set_count : whether to send 1 as parameter count or not
  @param send_parameter_count_when_zero ON to send the param count even when
     it's zero
  @retval true execution failed. Error in NET
  @retval false execution succeeded
*/
bool mysql_int_serialize_param_data(
    NET *net, unsigned int param_count, MYSQL_BIND *params, const char **names,
    unsigned long n_param_sets, uchar **ret_data, ulong *ret_length,
    uchar send_types_to_server, bool send_named_params,
    bool send_parameter_set_count, bool send_parameter_count_when_zero) {
  uint null_count;
  MYSQL_BIND *param, *param_end;
  const char **names_ptr = names;
  my_off_t null_pos_ofs;
  DBUG_TRACE;

  assert(net->vio);
  net_clear(net, true); /* Sets net->write_pos */

  if (send_named_params) {
    uchar *to;
    if (param_count > 0 || send_parameter_count_when_zero) {
      DBUG_PRINT("prep_stmt_exec", ("Sending param_count=%u", param_count));
      /* send the number of params */
      my_realloc_str(net, net_length_size(param_count));
      to = net_store_length(net->write_pos, param_count);
      net->write_pos = to;
    }

    /* also send the number of parameter data sets */
    assert(n_param_sets == 1);  // reserved for now
    if (send_parameter_set_count) {
      my_realloc_str(net, net_length_size(n_param_sets));
      to = net_store_length(net->write_pos, n_param_sets);
      net->write_pos = to;
    }
  }
  /* only send the null bits etc if there are params to send */
  if (param_count > 0 && n_param_sets > 0) {
    /* this is where the null bitmask starts */
    null_pos_ofs = net->write_pos - net->buff;

    /* Reserve place for null-marker bytes */
    null_count = (param_count + 7) / 8;
    if (my_realloc_str(net, null_count + 1)) {
      return true;
    }
    memset(net->write_pos, 0, null_count);
    net->write_pos += null_count;
    param_end = params + param_count;

    /* In case if buffers (type) altered, indicate to server */
    *(net->write_pos)++ = send_types_to_server;

    if (send_types_to_server) {
      if (my_realloc_str(net, 2 * param_count)) {
        return true;
      }
      /*
        Store types of parameters in first in first package
        that is sent to the server.
      */
      for (param = params; param < param_end; param++) {
        store_param_type(&net->write_pos, param);
        if (send_named_params) {
          const char *name = nullptr;
          size_t len = 0;
          if (names) {
            name = *names_ptr++;
            len = name ? strlen(name) : 0;
          }
          my_realloc_str(net, len + net_length_size(len));
          uchar *to = net_store_length(net->write_pos, len);
          if (len) memcpy(to, name, len);
          net->write_pos = to + len;
        }
      }
    }

    for (param = params; param < param_end; param++) {
      /* check if mysql_stmt_send_long_data() was used */
      if (param->long_data_used)
        param->long_data_used = false; /* Clear for next execute call */
      else if (store_param(net, param, null_pos_ofs))
        return true;
    }
  }
  *ret_length = (ulong)(net->write_pos - net->buff);
  /* TODO: Look into avoiding the following memdup */
  if (!(*ret_data = pointer_cast<uchar *>(
            my_memdup(PSI_NOT_INSTRUMENTED, net->buff, *ret_length, MYF(0))))) {
    net->last_errno = CR_OUT_OF_MEMORY;
    my_stpcpy(net->sqlstate, unknown_sqlstate);
    my_stpcpy(net->last_error, ER_CLIENT(net->last_errno));
    return true;
  }
  return false;
}

static bool int_is_null_true = true; /* Used for MYSQL_TYPE_NULL */
static bool int_is_null_false = false;

bool fix_param_bind(MYSQL_BIND *param, uint idx) {
  param->long_data_used = false;
  param->param_number = idx;

  /* If param->is_null is not set, then the value can never be NULL */
  if (!param->is_null) param->is_null = &int_is_null_false;

  /* Setup data copy functions for the different supported types */
  switch (param->buffer_type) {
    case MYSQL_TYPE_NULL:
      param->is_null = &int_is_null_true;
      break;
    case MYSQL_TYPE_TINY:
      /* Force param->length as this is fixed for this type */
      param->length = &param->buffer_length;
      param->buffer_length = 1;
      param->store_param_func = store_param_tinyint;
      break;
    case MYSQL_TYPE_SHORT:
      param->length = &param->buffer_length;
      param->buffer_length = 2;
      param->store_param_func = store_param_short;
      break;
    case MYSQL_TYPE_LONG:
      param->length = &param->buffer_length;
      param->buffer_length = 4;
      param->store_param_func = store_param_int32;
      break;
    case MYSQL_TYPE_LONGLONG:
      param->length = &param->buffer_length;
      param->buffer_length = 8;
      param->store_param_func = store_param_int64;
      break;
    case MYSQL_TYPE_FLOAT:
      param->length = &param->buffer_length;
      param->buffer_length = 4;
      param->store_param_func = store_param_float;
      break;
    case MYSQL_TYPE_DOUBLE:
      param->length = &param->buffer_length;
      param->buffer_length = 8;
      param->store_param_func = store_param_double;
      break;
    case MYSQL_TYPE_TIME:
      param->store_param_func = store_param_time;
      param->buffer_length = MAX_TIME_REP_LENGTH;
      break;
    case MYSQL_TYPE_DATE:
      param->store_param_func = store_param_date;
      param->buffer_length = MAX_DATE_REP_LENGTH;
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      param->store_param_func = store_param_datetime;
      param->buffer_length = MAX_DATETIME_REP_LENGTH;
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_JSON:
      param->store_param_func = store_param_str;
      /*
        For variable length types user must set either length or
        buffer_length.
      */
      break;
    default:
      return true;
  }
  /*
    If param->length is not given, change it to point to buffer_length.
    This way we can always use *param->length to get the length of data
  */
  if (!param->length) param->length = &param->buffer_length;
  return false;
}
