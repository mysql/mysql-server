/*
* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_NOTICE_BUILDER_H_
#define _NGS_NOTICE_BUILDER_H_

#include <string>

#include "message_builder.h"

namespace ngs
{
  class Output_buffer;

  class Notice_builder: public Message_builder
  {
  public:
    void encode_frame(
      Output_buffer* out_buffer,
      uint32 type,
      const std::string &data,
      int scope);

    void encode_rows_affected(
      Output_buffer* out_buffer,
      uint64 value);
  };
}


#endif //  _NGS_NOTICE_BUILDER_H_
