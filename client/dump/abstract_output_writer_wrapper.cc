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

#include "abstract_output_writer_wrapper.h"

using namespace Mysql::Tools::Dump;

void Abstract_output_writer_wrapper::append_output(
  const std::string& data_to_append)
{
  for (std::vector<I_output_writer*>::iterator it= m_output_writers.begin();
    it != m_output_writers.end(); ++it)
  {
    (*it)->append(data_to_append);
  }
}

Abstract_output_writer_wrapper::Abstract_output_writer_wrapper(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator)
  : Abstract_chain_element(message_handler, object_id_generator)
{}

void Abstract_output_writer_wrapper::register_output_writer(
  I_output_writer* new_output_writter)
{
  m_output_writers.push_back(new_output_writter);
}
