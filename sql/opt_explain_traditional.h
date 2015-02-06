/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
