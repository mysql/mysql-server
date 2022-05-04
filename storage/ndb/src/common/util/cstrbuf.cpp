/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

#ifdef TEST_CSTRBUF

#include "util/require.h"
#include "unittest/mytap/tap.h"
#include "util/cstrbuf.h"
#include "util/span.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main()
{
  plan(37);

  char buf[30];

  constexpr size_t ptr_size = sizeof(void*);
  static_assert(sizeof(size_t) == ptr_size);

  // cstrbuf with static extent

  cstrbuf a(buf);
  ok1(sizeof(a) == 2 * ptr_size);

  cstrbuf b(ndb::span<char, 10>{buf + 10, 10});
  ok1(sizeof(b) == 2 * ptr_size);

  cstrbuf<10, false> c({buf + 10, 10});
  ok1(sizeof(c) == 2 * ptr_size);

  cstrbuf d{ndb::span(buf)};
  ok1(sizeof(d) == 2 * ptr_size);

  // cstrbuf with dynamic extent

  cstrbuf e({buf + 10, 10});
  ok1(sizeof(e) == 3 * ptr_size);

  cstrbuf f({buf + 20, buf + 25});
  ok1(sizeof(f) == 3 * ptr_size);

  // cstrbuf owning buffer

  cstrbuf<24> g;
  ok1(sizeof(g) == ptr_size + 24);

  a.append("Rumpnisse");
  if (a.appendf(" %s:", __func__) == -1)
    fprintf(stderr, "ERROR: %s %d\n", __func__, __LINE__);
  a.append(" fantasier usch usch!");
  ok1(a.untruncated_length() == 36);
  int a_was_truncated = a.replace_end_if_truncated("...");
  ok1(a.length() == 29);
  ok1(a.extent() == 30);
  ok1(!a.is_truncated());
  ok1(a_was_truncated == 1);
  ok1(std::strcmp(a.c_str(), "Rumpnisse main: fantasier ...") == 0);

  g.append(a);
  ok1(g.length() == 23);
  ok1(g.untruncated_length() == 29);
  ok1(g.extent() == 24);
  ok1(g.is_truncated());
  ok1(std::strcmp(g.c_str(), "Rumpnisse main: fantasi") == 0);

  cstrbuf<0> nullbuf;
  ok1(sizeof(nullbuf) == 2 * ptr_size);
  ok1(nullbuf.is_truncated());  // Not even room for null termination!
  ok1(nullbuf.appendf("Tjoho %2zu", sizeof(nullbuf)) == 1);
  ok1(nullbuf.length() == 0);
  ok1(nullbuf.untruncated_length() == 8);
  ok1(nullbuf.extent() == 0);

  ok1(cstrbuf_copy(buf, "Mugge vigge") == 0);
  ok1(cstrbuf_copy({buf + 3, 5}, "Mugge vigge") == 1);
  ok1(cstrbuf_format(ndb::span(buf + 19, buf + 27), "Mugge %zu", sizeof(buf)) ==
      1);

  std::vector<char> vec(10);
  cstrbuf h(vec);
  ok1(h.extent() == 10);
  ok1(h.length() == 0);

  const char buf4_1[4] = {'A', '\0', '3', 'B'};
  cstrbuf<5> cbuf5;
  cbuf5.append("Magnus");
  ok1(cbuf5.length() == 4);
  cbuf5.replace_end_if_truncated(buf4_1);
  ok1(cbuf5.length() == 4);
  require(cbuf5.length() == strlen(cbuf5.c_str()));

  constexpr char buf4_4[4] = {'A', 'A', '3', 'D'};
  cbuf5.clear();
  cbuf5.append("Magnus");
  ok1(cbuf5.length() == 4);
  cbuf5.replace_end_if_truncated(buf4_4);
  ok1(std::strcmp(cbuf5.c_str(), "AA3D") == 0);

  cstrbuf<6> cbuf6;
  cbuf6.append(3, 'A');
  cbuf6.append(10, 'A');
  ok1(std::strcmp(cbuf6.c_str(), "AAAAA") == 0);

  std::string mark("Much too big mark");
  cstrbuf<7> cbuf7;
  cbuf7.append("Too big, or?");
  cbuf7.replace_end_if_truncated(mark);
  ok1(std::strcmp(cbuf7.c_str(), "Much t") == 0);

  cbuf7.clear();
  const int trettisju = 37;
  ok1(cbuf7.append("XYZDFABC") == 1);
  ok1(cbuf7.appendf("name: %d", trettisju) == 1);

  return exit_status();
}

#endif
