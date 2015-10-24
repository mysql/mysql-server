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

#ifndef ABSTRACT_OUTPUT_WRITER_WRAPPER_INCLUDED
#define ABSTRACT_OUTPUT_WRITER_WRAPPER_INCLUDED

#include "i_output_writer_wrapper.h"
#include "abstract_chain_element.h"
#include "i_output_writer.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Implementation of common logic for classes that directs execution of
  dump tasks to Data Formatters.
 */
class Abstract_output_writer_wrapper : public Abstract_chain_element,
  public virtual I_output_writer_wrapper
{
public:
  void register_output_writer(I_output_writer* new_output_writter);

protected:
  Abstract_output_writer_wrapper(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator);

  void append_output(const std::string& data_to_append);

private:
  std::vector<I_output_writer*> m_output_writers;
};

}
}
}

#endif
