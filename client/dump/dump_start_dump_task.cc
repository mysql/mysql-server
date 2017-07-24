/*
  Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dump_start_dump_task.h"

using namespace Mysql::Tools::Dump;

Dump_start_dump_task::Dump_start_dump_task(std::string gtid_mode,
  std::string gtid_executed)
  : Abstract_dump_task(NULL),
    m_gtid_mode(gtid_mode),
    m_gtid_executed(gtid_executed)
{}
