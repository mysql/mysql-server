/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/dd/impl/system_views/keywords.h"

#include <string>

#include "sql/stateless_allocator.h"
#include "sql/keyword_list.h"

namespace dd {
namespace system_views {

const Keywords &Keywords::instance()
{
  static Keywords *s_instance= new Keywords();
  return *s_instance;
}

Keywords::Keywords()
{
  size_t max_word_size= 0;
  for (auto x : keyword_list)
    max_word_size= std::max(max_word_size, strlen(x.word));

  Stringstream_type ss;
  ss << "JSON_TABLE('[";
  for (auto x : keyword_list)
    ss << "[\"" << x.word << "\"," << x.reserved << "],";
  ss.seekp(ss.tellp() - static_cast<std::streamoff>(1)); // remove last ','
  ss << "]', '$[*]' COLUMNS(word VARCHAR(" << max_word_size << ") PATH '$[0]',"
     << "reserved INT PATH '$[1]')) AS j";

  m_target_def.set_view_name(view_name());
  m_target_def.add_field(FIELD_WORD, "WORD", "j.word");
  m_target_def.add_field(FIELD_RESERVED, "RESERVED", "j.reserved");
  m_target_def.add_from(ss.str());
}

}
}
