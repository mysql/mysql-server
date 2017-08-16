/*
* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of the
* License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301  USA
*/

#ifndef _NGS_METADATA_BUILDER_H_
#define _NGS_METADATA_BUILDER_H_

#include <set>
#include <string>

#include "m_ctype.h"
#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/protocol/message_builder.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

namespace ngs
{

  class Output_buffer;

  class Metadata_builder: public Message_builder
  {
  public:

    void encode_metadata(Output_buffer* out_buffer,
      const std::string &catalog, const std::string &db_name,
      const std::string &table_name, const std::string &org_table_name,
      const std::string &col_name, const std::string &org_col_name,
      uint64 collation, int type, int decimals,
      uint32 flags, uint32 length, uint32 content_type = 0);

    void encode_metadata(Output_buffer* out_buffer,
      uint64 collation, int type, int decimals,
      uint32 flags, uint32 length, uint32 content_type = 0);

  };
}


#endif //  _NGS_METADATA_BUILDER_H_
