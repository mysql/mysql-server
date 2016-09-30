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

#include "buffering_command_delegate.h"
#include "xpl_log.h"
#include "ngs_common/bind.h"

using namespace xpl;


Buffering_command_delegate::Buffering_command_delegate()
: Callback_command_delegate(ngs::bind(&Buffering_command_delegate::begin_row_cb, this),
                            ngs::bind(&Buffering_command_delegate::end_row_cb, this, ngs::placeholders::_1))
{
}


void Buffering_command_delegate::reset()
{
  m_resultset.clear();
  Command_delegate::reset();
}


Callback_command_delegate::Row_data *Buffering_command_delegate::begin_row_cb()
{
  m_resultset.push_back(Row_data());
  return &m_resultset.back();
}


bool Buffering_command_delegate::end_row_cb(Row_data *row)
{
  return true;
}

