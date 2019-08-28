/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#ifndef OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED
#define OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED

#include "opt_explain_format.h"

/**
  Formatter for the traditional EXPLAIN output
*/

class Explain_format_traditional : public Explain_format
{
  class Item_null *nil;
  qep_row column_buffer; ///< buffer for the current output row

public:
  Explain_format_traditional() : nil(NULL) {}

  virtual bool is_hierarchical() const { return false; }
  virtual bool send_headers(Query_result *result);
  virtual bool begin_context(enum_parsing_context,
                             SELECT_LEX_UNIT *subquery,
                             const Explain_format_flags *flags)
  {
    return false;
  }
  virtual bool end_context(enum_parsing_context) { return false; }
  virtual bool flush_entry();
  virtual qep_row *entry() { return &column_buffer; }

private:
  bool push_select_type(List<Item> *items);
};

#endif//OPT_EXPLAIN_FORMAT_TRADITIONAL_INCLUDED
