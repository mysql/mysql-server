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

#ifndef STORED_PROCEDURE_INCLUDED
#define STORED_PROCEDURE_INCLUDED

#include "abstract_plain_sql_object_dump_task.h"
#include <string>

namespace Mysql{
namespace Tools{
namespace Dump{

class Stored_procedure : public Abstract_plain_sql_object_dump_task
{
public:
  Stored_procedure(uint64 id, const std::string& name,
    const std::string& schema,
    const std::string& sql_formatted_definition);
};

}
}
}

#endif
