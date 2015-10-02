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

#ifndef MYSQL_OBJECT_READER_OPTIONS_INCLUDED
#define MYSQL_OBJECT_READER_OPTIONS_INCLUDED

#include "base/abstract_options_provider.h"
#include "mysql_chain_element_options.h"

namespace Mysql{
namespace Tools{
namespace Dump{

#define MAX_EXTENDED_INSERT 0x100000

class Mysql_object_reader_options
  : public Mysql::Tools::Base::Options::Abstract_options_provider
{
public:
  Mysql_object_reader_options(
    const Mysql_chain_element_options* mysql_chain_element_options);

  void create_options();

  uint64 m_row_group_size;
  const Mysql_chain_element_options* m_mysql_chain_element_options;
};

}
}
}

#endif
