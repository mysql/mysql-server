/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FILE_WRITER_INCLUDED
#define FILE_WRITER_INCLUDED

#include <functional>
#include <string>

#include "client/dump/abstract_chain_element.h"
#include "client/dump/i_output_writer.h"
#include "my_inttypes.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Writes formatted data to specified file.
 */
class File_writer : public I_output_writer, public Abstract_chain_element
{
public:
  File_writer(
    std::function<bool(const Mysql::Tools::Base::Message_data&)>*
    message_handler, Simple_id_generator* object_id_generator,
    const std::string& file_name);
  ~File_writer();

  void append(const std::string& data_to_append);

  // Fix "inherits ... via dominance" warnings
  void register_progress_watcher(I_progress_watcher* new_progress_watcher)
  { Abstract_chain_element::register_progress_watcher(new_progress_watcher); }

  // Fix "inherits ... via dominance" warnings
  uint64 get_id() const
  { return Abstract_chain_element::get_id(); }

protected:
  // Fix "inherits ... via dominance" warnings
  void item_completion_in_child_callback(Item_processing_data* item_processed)
  { Abstract_chain_element::item_completion_in_child_callback(item_processed); }

private:
  FILE* m_file;
};

}
}
}

#endif
