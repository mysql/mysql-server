/*
   Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "disabled_option.h"

#include <stddef.h>

using std::string;
using namespace Mysql::Tools::Base::Options;

Disabled_option::Disabled_option(string name, string description)
  : Abstract_option<Disabled_option>(NULL, GET_DISABLED, name, description, 0)
{
  this->m_option_structure.arg_type= OPT_ARG;
}
