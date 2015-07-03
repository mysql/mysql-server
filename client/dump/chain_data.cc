/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "chain_data.h"

using namespace Mysql::Tools::Dump;

Chain_data::Chain_data(uint64 chain_id)
  : m_chain_id(chain_id),
  m_is_aborted(false)
{}

void Chain_data::abort()
{
  m_is_aborted= true;
}

bool Chain_data::is_aborted()
{
  return m_is_aborted;
}
