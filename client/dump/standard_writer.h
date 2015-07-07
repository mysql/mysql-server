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

#ifndef STANDARD_WRITER_INCLUDED
#define STANDARD_WRITER_INCLUDED

#include "i_output_writer.h"
#include "abstract_chain_element.h"
#include <string>

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Writes formatted data to standard output or error stream.
 */
class Standard_writer : public I_output_writer, public Abstract_chain_element
{
public:
  Standard_writer(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator);

  void append(const std::string& data_to_append);
};

}
}
}

#endif
