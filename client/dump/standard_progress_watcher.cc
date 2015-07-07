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

#include "standard_progress_watcher.h"

using namespace Mysql::Tools::Dump;

void Standard_progress_watcher::process_progress_step(
  Abstract_progress_watcher::Progress_data& change)
{
  std::cerr << "Dump progress: " << m_last_progress.m_table_count << "/"
    << m_total.m_table_count << " tables, " << m_last_progress.m_row_count
    << "/" << m_total.m_row_count << " rows" << std::endl;
}

Standard_progress_watcher::Standard_progress_watcher(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator)
  : Abstract_progress_watcher(message_handler, object_id_generator)
{}
