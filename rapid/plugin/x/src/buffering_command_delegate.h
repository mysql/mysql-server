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

#ifndef _XPL_BUFFERING_COMMAND_DELEGATE_H_
#define _XPL_BUFFERING_COMMAND_DELEGATE_H_

#include "callback_command_delegate.h"


namespace xpl
{
  class Buffering_command_delegate : public Callback_command_delegate
  {
  public:
    Buffering_command_delegate();

    // When vector is going to be reallocated then the Field pointers are copied
    // but are release by destructor of Row_data
    typedef std::list<Row_data> Resultset;


    Resultset &resultset() { return m_resultset; }
    virtual void reset();

  private:
    Resultset m_resultset;

    Row_data *begin_row_cb();
    bool end_row_cb(Row_data *row);
  };
}

#endif //  _XPL_BUFFERING_COMMAND_DELEGATE_H_
