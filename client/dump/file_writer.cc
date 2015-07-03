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

#include "file_writer.h"

using namespace Mysql::Tools::Dump;

void File_writer::append(const std::string& data_to_append)
{
  fwrite(data_to_append.c_str(), 1, data_to_append.size(), m_file);
  // Check for I/O errors.
  if (ferror(m_file) != 0)
  {
    this->pass_message(Mysql::Tools::Base::Message_data(ferror(m_file),
      "Error occurred while writing to output.",
      Mysql::Tools::Base::Message_type_error));
  }
}

File_writer::~File_writer()
{
  // Check for I/O errors and close file.
  if (ferror(m_file) != 0 || fclose(m_file) != 0)
  {
    this->pass_message(Mysql::Tools::Base::Message_data(ferror(m_file),
      "Error occurred while finishing writing to output.",
      Mysql::Tools::Base::Message_type_error));
  }
}

File_writer::File_writer(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator,
  const std::string& file_name)
  : Abstract_chain_element(message_handler, object_id_generator),
  m_file(fopen(file_name.c_str(), "wb"))
{}
