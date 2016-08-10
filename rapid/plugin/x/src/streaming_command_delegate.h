/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_STREAMING_COMMAND_DELEGATE_H_
#define _XPL_STREAMING_COMMAND_DELEGATE_H_

#include "command_delegate.h"


namespace ngs
{

  class Protocol_encoder;

}  // namespace ngs

namespace xpl
{

  class Streaming_command_delegate : public Command_delegate
  {
  public:
    Streaming_command_delegate(ngs::Protocol_encoder *proto);
    virtual ~Streaming_command_delegate();

    void set_compact_metadata(bool flag) { m_compact_metadata = flag; }
    bool compact_metadata() const { return m_compact_metadata; }

    virtual void reset();

  private:
    virtual int start_result_metadata(uint num_cols, uint flags,
                                      const CHARSET_INFO *resultcs);
    virtual int field_metadata(struct st_send_field *field,
                               const CHARSET_INFO *charset);
    virtual int end_result_metadata(uint server_status,
                                    uint warn_count);

    virtual int start_row();
    virtual int end_row();
    virtual void abort_row();
    virtual ulong get_client_capabilities();
    virtual int get_null();
    virtual int get_integer(longlong value);
    virtual int get_longlong(longlong value, uint unsigned_flag);
    virtual int get_decimal(const decimal_t *value);
    virtual int get_double(double value, uint32 decimals);
    virtual int get_date(const MYSQL_TIME *value);
    virtual int get_time(const MYSQL_TIME *value, uint decimals);
    virtual int get_datetime(const MYSQL_TIME *value, uint decimals);
    virtual int get_string(const char * const value, size_t length,
                           const CHARSET_INFO *const valuecs);
    virtual void handle_ok(uint server_status, uint statement_warn_count,
                           ulonglong affected_rows, ulonglong last_insert_id,
                           const char * const message);

    virtual enum cs_text_or_binary representation() const { return CS_BINARY_REPRESENTATION; }

    bool send_column_metadata(uint64_t xcollation, const Mysqlx::Resultset::ColumnMetaData::FieldType &xtype,
                              uint32_t xflags, uint32_t ctype, const st_send_field *field);

    ngs::Protocol_encoder *m_proto;
    const CHARSET_INFO *m_resultcs;
    bool m_sent_result;
    bool m_compact_metadata;
  };
}

#endif //  _XPL_STREAMING_COMMAND_DELEGATE_H_
