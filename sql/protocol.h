#ifndef PROTOCOL_INCLUDED
#define PROTOCOL_INCLUDED

/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_error.h"
#include "my_decimal.h"                         /* my_decimal */

class i_string;
class Field;
class THD;
class Item_param;
typedef struct st_mysql_field MYSQL_FIELD;
typedef struct st_mysql_rows MYSQL_ROWS;

class Protocol
{
protected:
  THD	 *thd;
  String *packet;
  String *convert;
  uint field_pos;
#ifndef DBUG_OFF
  enum enum_field_types *field_types;
#endif
  uint field_count;
#ifndef EMBEDDED_LIBRARY
  bool net_store_data(const uchar *from, size_t length);
#else
  virtual bool net_store_data(const uchar *from, size_t length);
  char **next_field;
  MYSQL_FIELD *next_mysql_field;
  MEM_ROOT *alloc;
#endif
  bool net_store_data(const uchar *from, size_t length,
                      const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);
  bool store_string_aux(const char *from, size_t length,
                        const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);

  virtual bool send_ok(uint server_status, uint statement_warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message);

  virtual bool send_eof(uint server_status, uint statement_warn_count);

  virtual bool send_error(uint sql_errno, const char *err_msg,
                          const char *sql_state);

public:
  Protocol() {}
  Protocol(THD *thd_arg) { init(thd_arg); }
  virtual ~Protocol() {}
  void init(THD* thd_arg);

  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual bool send_result_set_metadata(List<Item> *list, uint flags);
  bool send_result_set_row(List<Item> *row_items);

  bool store(I_List<i_string> *str_list);
  bool store(const char *from, const CHARSET_INFO *cs);
  String *storage_packet() { return packet; }
  inline void free() { packet->free(); }
  virtual bool write();
  inline  bool store(int from)
  { return store_long((longlong) from); }
  inline  bool store(uint32 from)
  { return store_long((longlong) from); }
  inline  bool store(longlong from)
  { return store_longlong((longlong) from, 0); }
  inline  bool store(ulonglong from)
  { return store_longlong((longlong) from, 1); }
  inline bool store(String *str)
  { return store((char*) str->ptr(), str->length(), str->charset()); }

  virtual bool prepare_for_send(uint num_columns)
  {
    field_count= num_columns;
    return 0;
  }
  virtual bool flush();
  virtual void end_partial_result_set(THD *thd);
  virtual void prepare_for_resend()=0;

  virtual bool store_null()=0;
  virtual bool store_tiny(longlong from)=0;
  virtual bool store_short(longlong from)=0;
  virtual bool store_long(longlong from)=0;
  virtual bool store_longlong(longlong from, bool unsigned_flag)=0;
  virtual bool store_decimal(const my_decimal *)=0;
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *cs)=0;
  virtual bool store(const char *from, size_t length, 
                     const CHARSET_INFO *fromcs,
                     const CHARSET_INFO *tocs)=0;
  virtual bool store(float from, uint32 decimals, String *buffer)=0;
  virtual bool store(double from, uint32 decimals, String *buffer)=0;
  virtual bool store(MYSQL_TIME *time, uint precision)=0;
  virtual bool store_date(MYSQL_TIME *time)=0;
  virtual bool store_time(MYSQL_TIME *time, uint precision)=0;
  virtual bool store(Field *field)=0;

  virtual bool send_out_parameters(List<Item_param> *sp_params)=0;
#ifdef EMBEDDED_LIBRARY
  int begin_dataset();
  virtual void remove_last_row() {}
#else
  void remove_last_row() {}
#endif
  enum enum_protocol_type
  {
    /*
      Before adding a new type, please make sure
      there is enough storage for it in Query_cache_query_flags.
    */
    PROTOCOL_TEXT= 0, PROTOCOL_BINARY= 1, PROTOCOL_LOCAL= 2
  };
  virtual enum enum_protocol_type type()= 0;

  void end_statement();
};


/** Class used for the old (MySQL 4.0 protocol). */

class Protocol_text :public Protocol
{
public:
  Protocol_text() {}
  Protocol_text(THD *thd_arg) :Protocol(thd_arg) {}
  virtual void prepare_for_resend();
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO *cs);
  virtual bool store(const char *from, size_t length,
  		     const CHARSET_INFO *fromcs,
                     const CHARSET_INFO *tocs);
  virtual bool store(MYSQL_TIME *time, uint precision);
  virtual bool store_date(MYSQL_TIME *time);
  virtual bool store_time(MYSQL_TIME *time, uint precision);
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(Field *field);

  virtual bool send_out_parameters(List<Item_param> *sp_params);
#ifdef EMBEDDED_LIBRARY
  void remove_last_row();
#endif
  virtual enum enum_protocol_type type() { return PROTOCOL_TEXT; };
};


class Protocol_binary :public Protocol
{
private:
  uint bit_fields;
public:
  Protocol_binary() {}
  Protocol_binary(THD *thd_arg) :Protocol(thd_arg) {}
  virtual bool prepare_for_send(uint num_columns);
  virtual void prepare_for_resend();
#ifdef EMBEDDED_LIBRARY
  virtual bool write();
  bool net_store_data(const uchar *from, size_t length);
#endif
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO *cs);
  virtual bool store(const char *from, size_t length,
  		     const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);
  virtual bool store(MYSQL_TIME *time, uint precision);
  virtual bool store_date(MYSQL_TIME *time);
  virtual bool store_time(MYSQL_TIME *time, uint precision);
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(Field *field);

  virtual bool send_out_parameters(List<Item_param> *sp_params);

  virtual enum enum_protocol_type type() { return PROTOCOL_BINARY; };
};

void send_warning(THD *thd, uint sql_errno, const char *err=0);
bool net_send_error(THD *thd, uint sql_errno, const char *err,
                    const char* sqlstate);
uchar *net_store_data(uchar *to,const uchar *from, size_t length);
uchar *net_store_data(uchar *to,int32 from);
uchar *net_store_data(uchar *to,longlong from);

#endif /* PROTOCOL_INCLUDED */
