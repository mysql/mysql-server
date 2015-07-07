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

#ifndef MYSQL_CHAIN_ELEMENT_OPTIONS_INCLUDED
#define MYSQL_CHAIN_ELEMENT_OPTIONS_INCLUDED

#include "base/abstract_options_provider.h"
#include "base/abstract_connection_program.h"

namespace Mysql{
namespace Tools{
namespace Dump{


class Mysql_chain_element_options :
  public Mysql::Tools::Base::Options::Abstract_options_provider
{
public:
  Mysql_chain_element_options(
    Mysql::Tools::Base::Abstract_connection_program* program);

  void create_options();

  Mysql::Tools::Base::Abstract_connection_program* get_program() const;

private:

  Mysql::Tools::Base::Abstract_connection_program* m_program;
};

}
}
}

#endif
