/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _XPL_BUFFERING_COMMAND_DELEGATE_H_
#define _XPL_BUFFERING_COMMAND_DELEGATE_H_

#include <list>

#include "plugin/x/src/callback_command_delegate.h"

namespace xpl {
class Buffering_command_delegate : public Callback_command_delegate {
 public:
  Buffering_command_delegate();

  // When vector is going to be reallocated then the Field pointers are copied
  // but are release by destructor of Row_data
  typedef std::list<Row_data> Resultset;

  void set_resultset(const Resultset &resultset) { m_resultset = resultset; }
  const Resultset &get_resultset() const { return m_resultset; }
  virtual void reset();

 private:
  Resultset m_resultset;

  Row_data *begin_row_cb();
  bool end_row_cb(Row_data *row);
};
}

#endif  // _XPL_BUFFERING_COMMAND_DELEGATE_H_
