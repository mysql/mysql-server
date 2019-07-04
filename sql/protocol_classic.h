#ifndef PROTOCOL_CLASSIC_INCLUDED
#define PROTOCOL_CLASSIC_INCLUDED

/* Copyright (c) 2002, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>
#include <sys/types.h>

#include "field_types.h"  // enum_field_types
#include "m_ctype.h"
#include "my_command.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "sql/protocol.h"  // Protocol
#include "violite.h"

class Item_param;
class Proto_field;
class Send_field;
class String;
class i_string;
class my_decimal;
template <class T>
class I_List;
template <class T>
class List;
union COM_DATA;
class THD;

class Protocol_classic : public Protocol {
 private:
  ulong m_client_capabilities;
  virtual bool parse_packet(union COM_DATA *data, enum_server_command cmd);

 protected:
  THD *m_thd;
  String *packet;
  String convert;
  uint field_pos;
  bool send_metadata;
#ifndef DBUG_OFF
  enum enum_field_types *field_types;
  uint count;
#endif
  uint field_count;
  uint sending_flags;
  ulong input_packet_length;
  uchar *input_raw_packet;
  CHARSET_INFO *result_cs;
  bool net_store_data(const uchar *from, size_t length);
  virtual bool net_store_data(const uchar *from, size_t length,
                              const CHARSET_INFO *fromcs,
                              const CHARSET_INFO *tocs);
  bool store_string_aux(const char *from, size_t length,
                        const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);

  virtual bool send_ok(uint server_status, uint statement_warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message);

  virtual bool send_eof(uint server_status, uint statement_warn_count);

  virtual bool send_error(uint sql_errno, const char *err_msg,
                          const char *sql_state);
  virtual bool store_ps_status(ulong stmt_id, uint column_count,
                               uint param_count, ulong cond_count);

 public:
  bool bad_packet;
  Protocol_classic()
      : send_metadata(false), input_packet_length(0), bad_packet(true) {}
  Protocol_classic(THD *thd)
      : send_metadata(false),
        input_packet_length(0),
        input_raw_packet(nullptr),
        bad_packet(true) {
    init(thd);
  }
  virtual ~Protocol_classic() {}
  void init(THD *thd_arg);
  virtual int read_packet();
  virtual int get_command(COM_DATA *com_data, enum_server_command *cmd);
  /**
    Parses the passed parameters and creates a command

    @param com_data  out parameter
    @param cmd       in  parameter
    @param pkt       packet to be parsed
    @param length    size of the packet

    @return
      @retval false   ok
      @retval true    error
  */
  virtual bool create_command(COM_DATA *com_data, enum_server_command cmd,
                              uchar *pkt, size_t length);
  virtual bool flush();
  virtual void end_partial_result_set();

  virtual void start_row() = 0;
  virtual bool end_row();
  virtual uint get_rw_status();
  virtual bool get_compression();

  virtual bool start_result_metadata(uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs);
  virtual bool end_result_metadata();
  virtual bool send_field_metadata(Send_field *field,
                                   const CHARSET_INFO *item_charset);
  virtual void abort_row() {}
  virtual enum enum_protocol_type type() const = 0;

  /**
    Returns the type of the connection

    @return
      enum enum_vio_type
  */
  virtual enum enum_vio_type connection_type() const {
    const Vio *v = get_vio();
    return v ? vio_type(v) : NO_VIO_TYPE;
  }

  virtual my_socket get_socket();
  // NET interaction functions
  /* Initialize NET */
  bool init_net(Vio *vio);
  void claim_memory_ownership();
  /* Deinitialize NET */
  void end_net();
  /* Write data to NET buffer */
  bool write(const uchar *ptr, size_t len);
  /* Return last error from NET */
  uchar get_error();
  /* Set max allowed packet size */
  void set_max_packet_size(ulong max_packet_size);
  /* Deinitialize VIO */
  virtual int shutdown(bool server_shutdown = false);
  /* Wipe NET with zeros */
  void wipe_net();
  /* Check whether VIO is healhty */
  virtual bool connection_alive() const;
  /* Returns the client capabilities */
  virtual ulong get_client_capabilities() { return m_client_capabilities; }
  /* Sets the client capabilities */
  void set_client_capabilities(ulong client_capabilities) {
    this->m_client_capabilities = client_capabilities;
  }
  /* Adds  client capability */
  void add_client_capability(ulong client_capability) {
    m_client_capabilities |= client_capability;
  }
  /* Removes a client capability*/
  void remove_client_capability(unsigned long capability) {
    m_client_capabilities &= ~capability;
  }
  /* Returns true if the client has the capability and false otherwise*/
  virtual bool has_client_capability(unsigned long client_capability) {
    return (bool)(m_client_capabilities & client_capability);
  }
  // TODO: temporary functions. Will be removed.
  // DON'T USE IN ANY NEW FEATURES.
  /* Return NET */
  NET *get_net();
  /* return VIO */
  Vio *get_vio();
  const Vio *get_vio() const;
  /* Set VIO */
  void set_vio(Vio *vio);
  /* Set packet number */
  void set_output_pkt_nr(uint pkt_nr);
  /* Return packet number */
  uint get_output_pkt_nr();
  /* Return packet string */
  String *get_output_packet();
  /* return packet length */
  uint get_packet_length() { return input_packet_length; }
  /* Return raw packet buffer */
  uchar *get_raw_packet() { return input_raw_packet; }
  /* Set read timeout */
  virtual void set_read_timeout(ulong read_timeout);
  /* Set write timeout */
  virtual void set_write_timeout(ulong write_timeout);
};

/** Class used for the old (MySQL 4.0 protocol). */

class Protocol_text : public Protocol_classic {
 public:
  Protocol_text() {}
  Protocol_text(THD *thd_arg) : Protocol_classic(thd_arg) {}
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *, uint, uint);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO *cs) {
    return store(from, length, cs, result_cs);
  }
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(MYSQL_TIME *time, uint precision);
  virtual bool store_date(MYSQL_TIME *time);
  virtual bool store_time(MYSQL_TIME *time, uint precision);
  virtual bool store(Proto_field *field);
  virtual void start_row();
  virtual bool send_parameters(List<Item_param> *parameters, bool);

  virtual enum enum_protocol_type type() const { return PROTOCOL_TEXT; }

 protected:
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);
};

class Protocol_binary : public Protocol_text {
 private:
  uint bit_fields;

 public:
  Protocol_binary() {}
  Protocol_binary(THD *thd_arg) : Protocol_text(thd_arg) {}
  virtual void start_row();
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *, uint, uint);
  virtual bool store(MYSQL_TIME *time, uint precision);
  virtual bool store_date(MYSQL_TIME *time);
  virtual bool store_time(MYSQL_TIME *time, uint precision);
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(Proto_field *field);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO *cs) {
    return store(from, length, cs, result_cs);
  }

  virtual bool start_result_metadata(uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs);
  virtual bool send_parameters(List<Item_param> *parameters,
                               bool is_sql_prepare);

  virtual enum enum_protocol_type type() const { return PROTOCOL_BINARY; }

 protected:
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);
};

bool net_send_error(THD *thd, uint sql_errno, const char *err);
bool net_send_error(NET *net, uint sql_errno, const char *err);
uchar *net_store_data(uchar *to, const uchar *from, size_t length);
uchar *net_store_data(uchar *to, int32 from);
uchar *net_store_data(uchar *to, longlong from);
bool store(Protocol *prot, I_List<i_string> *str_list);
bool net_send_ok(THD *, uint, uint, ulonglong, ulonglong, const char *, bool);
bool net_send_eof(THD *thd, uint server_status, uint statement_warn_count);
bool net_send_error_packet(THD *, uint, const char *, const char *);
#endif /* PROTOCOL_CLASSIC_INCLUDED */
