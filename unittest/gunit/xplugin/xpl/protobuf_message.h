/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_PROTOBUF_MESSAGE_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_PROTOBUF_MESSAGE_H_

namespace xpl {

namespace test {

template <typename T>
class Push_back_visitor : public ngs::Page_visitor {
 public:
  Push_back_visitor(T *t) : m_t(t) {}

  bool visit(const char *p, ssize_t s) override {
    m_t->push_back(std::make_pair(p, s));
    return true;
  }

 private:
  T *m_t;
};

using Page = std::pair<const char *, ssize_t>;
using Pages = std::vector<Page>;

static Pages get_pages_from_stream(ngs::Page_output_stream *stream) {
  Pages result;
  Push_back_visitor<Pages> visitor(&result);

  stream->visit_buffers(&visitor);

  return result;
}

template <class Msg>
Msg *message_from_buffer(ngs::Page_output_stream *stream) {
  Pages pages = get_pages_from_stream(stream);

  std::string str_buff;
  bool first = true;
  for (const auto &p : pages) {
    // skip the header (size+type) from the first page
    size_t offset = (first) ? 5 : 0;
    first = false;

    str_buff.append(p.first + offset, p.second - offset);
  }
  Msg *result = new Msg();

  result->ParseFromString(str_buff);

  return result;
}

template <class Msg>
Msg *message_from_buffer(const std::string &buffer) {
  Msg *result = new Msg();

  result->ParseFromString(buffer.substr(5));

  return result;
}

}  // namespace test
}  // namespace xpl

#endif  // UNITTEST_GUNIT_XPLUGIN_XPL_PROTOBUF_MESSAGE_H_
