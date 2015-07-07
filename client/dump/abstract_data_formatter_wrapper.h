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

#ifndef ABSTRACT_DATA_FORMATTER_WRAPPER_INCLUDED
#define ABSTRACT_DATA_FORMATTER_WRAPPER_INCLUDED

#include "i_data_formatter_wrapper.h"
#include "abstract_chain_element.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Implementation of common logic for classes that directs execution of
  dump tasks to Data Formatters.
 */
class Abstract_data_formatter_wrapper : public I_data_formatter_wrapper,
  public Abstract_chain_element
{
public:
  void register_data_formatter(I_data_formatter* new_data_formatter);

protected:
  Abstract_data_formatter_wrapper(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator);

  void format_object(Item_processing_data* current_processing_data);

private:
  std::vector<I_data_formatter*> m_formatters;
};

}
}
}

#endif
