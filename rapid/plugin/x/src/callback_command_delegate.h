/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_CALLBACK_COMMAND_DELEGATE_H_
#define _XPL_CALLBACK_COMMAND_DELEGATE_H_

#include "command_delegate.h"

#include "ngs/protocol_encoder.h"

namespace xpl
{
  class Callback_command_delegate : public Command_delegate
  {
  public:
    struct Field_value
    {
      Field_value();
      Field_value(const Field_value& other);
      Field_value(const longlong &num, bool unsign = false);
      Field_value(const decimal_t &decimal);
      Field_value(const double num);
      Field_value(const MYSQL_TIME &time);
      Field_value(const char *str, size_t length);
      Field_value& operator=(const Field_value& other);

      ~Field_value();
      union
      {
        longlong v_long;
        double v_double;
        decimal_t v_decimal;
        MYSQL_TIME v_time;
        std::string *v_string;
      } value;
      bool is_unsigned;
      bool is_string;
    };

    struct Row_data
    {
      Row_data() {}
      Row_data(const Row_data& other);
      Row_data& operator=(const Row_data& other);
      ~Row_data();
      void clear();
      std::vector<Field_value*> fields;

    private:
      void clone_fields(const Row_data& other);
    };

    typedef ngs::function<Row_data *()> Start_row_callback;
    typedef ngs::function<bool (Row_data*)> End_row_callback;

    Callback_command_delegate();
    Callback_command_delegate(Start_row_callback start_row, End_row_callback end_row);

    void set_callbacks(Start_row_callback start_row, End_row_callback end_row);

    virtual void reset();

    virtual enum cs_text_or_binary representation() const { return CS_TEXT_REPRESENTATION; }
  private:
    Start_row_callback m_start_row;
    End_row_callback m_end_row;
    Row_data *m_current_row;

  private:
    virtual int start_row();

    virtual int end_row();

    virtual void abort_row();

    virtual ulong get_client_capabilities();

    /****** Getting data ******/
    virtual int get_null();

    virtual int get_integer(longlong value);

    virtual int get_longlong(longlong value, uint unsigned_flag);

    virtual int get_decimal(const decimal_t * value);

    virtual int get_double(double value, uint32 decimals);

    virtual int get_date(const MYSQL_TIME * value);

    virtual int get_time(const MYSQL_TIME * value, uint decimals);

    virtual int get_datetime(const MYSQL_TIME * value, uint decimals);

    virtual int get_string(const char * const value, size_t length,
                               const CHARSET_INFO * const valuecs);
  };
}

#endif //  _XPL_CALLBACK_COMMAND_DELEGATE_H_
