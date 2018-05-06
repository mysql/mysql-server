/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _XPL_PROTOUFF_MESSAGE_H_
#define _XPL_PROTOUFF_MESSAGE_H_

namespace xpl {

namespace test {

template <class Msg>
Msg *message_from_buffer(ngs::Buffer *buffer) {
  ngs::Buffer::Page_list &obuffer_pages = buffer->pages();

  std::string str_buff;
  ngs::Buffer::Page_list::const_iterator it = obuffer_pages.begin();
  for (; it != obuffer_pages.end(); ++it) {
    // skip the header (size+type) from the first page
    size_t offset = (it == obuffer_pages.begin()) ? 5 : 0;

    str_buff.append((*it)->data + offset, (*it)->length - offset);
  };
  Msg *result = new Msg();

  result->ParseFromString(str_buff);

  return result;
}

}  // namespace test

}  // namespace xpl

#endif  // _XPL_PROTOUFF_MESSAGE_H_
