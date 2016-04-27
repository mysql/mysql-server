/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef _XPL_PROTOUFF_MESSAGE_H_
#define _XPL_PROTOUFF_MESSAGE_H_


namespace xpl
{

namespace test
{

template <class Msg>
Msg* message_from_buffer(ngs::Buffer* buffer)
{
  ngs::Buffer::Page_list &obuffer_pages = buffer->pages();

  std::string str_buff;
  ngs::Buffer::Page_list::const_iterator it = obuffer_pages.begin();
  for (; it != obuffer_pages.end(); ++it)
  {
    // skip the header (size+type) from the first page
    size_t offset = (it == obuffer_pages.begin()) ? 5 : 0;

    str_buff.append((*it)->data + offset, (*it)->length - offset);
  };
  Msg* result = new Msg();

  result->ParseFromString(str_buff);

  return result;
}


} // namespace test

} // namespace xpl


#endif // _XPL_PROTOUFF_MESSAGE_H_
