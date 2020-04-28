/* Copyright (c) 2011, 2020, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
   */

#include "my_config.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <limits>

#include "sql/item_regexp_func.h"
#include "sql/parse_tree_items.h"
#include "unittest/gunit/item_utils.h"
#include "unittest/gunit/mock_parse_tree.h"
#include "unittest/gunit/test_utils.h"

namespace item_func_regexp_unittest {

using my_testing::Mock_pt_item_list;
using my_testing::Server_initializer;
using std::string;

class ItemFuncRegexpTest : public ::testing::Test {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;

  template <typename Item_type>
  void test_print(const char *expected,
                  std::initializer_list<const char *> args) {
    auto items = new (thd()->mem_root) Mock_pt_item_list(args);
    Item *item = new Item_type(POS(), items);
    Parse_context pc(thd(), thd()->lex->select_lex);
    item->itemize(&pc, &item);
    item->fix_fields(thd(), nullptr);
    String buf;
    item->print(thd(), &buf, QT_ORDINARY);
    EXPECT_STREQ(expected, buf.c_ptr_safe());
  }
};

TEST_F(ItemFuncRegexpTest, Print) {
  test_print<Item_func_regexp_instr>("regexp_instr('abc','def')",
                                     {"abc", "def"});
  test_print<Item_func_regexp_like>("regexp_like('abc','def')", {"abc", "def"});
  test_print<Item_func_regexp_replace>("regexp_replace('ab','c','d')",
                                       {"ab", "c", "d"});
  test_print<Item_func_regexp_substr>("regexp_substr('x','y')", {"x", "y"});
}

static bool is_control_character(char c) {
  // This set of control characters is by no means exhaustive. It's based solely
  // on experimentation of which characters can't be used in a single-character
  // pattern in this particular regexp library (ICU).
  static const std::vector<char> control_chars = {
      '.', '$', '|', '^', '(', ')', '*', '+', '?', '[', '\\', '{', '}'};
  return std::find(control_chars.begin(), control_chars.end(), c) !=
         control_chars.end();
}

Item *make_binary_literal(THD *thd, const string &s) {
  POS pos;  // We expect this object to be copied.
  LEX_STRING lex_string = {const_cast<char *>(s.c_str()), s.length()};
  return new (thd->mem_root) PTI_text_literal_underscore_charset(
      pos, false, &my_charset_bin, lex_string);
}

TEST_F(ItemFuncRegexpTest, BinaryCharset) {
  static const int max_char = std::numeric_limits<unsigned char>::max();
  for (int i = 0; i <= max_char; ++i) {
    string subject;
    subject = char(i);
    for (int j = 0; j <= max_char; ++j) {
      string pattern;
      if (is_control_character(char(j))) pattern = '\\';
      pattern += char(j);

      Item *subject_item = make_binary_literal(thd(), subject);
      Item *pattern_item = make_binary_literal(thd(), pattern);
      auto rlike = make_resolved<Item_func_regexp_like>(thd(), subject_item,
                                                        pattern_item);
      if (i == j)
        EXPECT_EQ(1, rlike->val_int())
            << "They're equal(" << i
            << "), they should match via regexp matching.";
      else
        EXPECT_EQ(0, rlike->val_int())
            << "They're not equal(" << i << " vs " << j
            << "), they should not match via regexp matching.";
    }
  }
}

}  // namespace item_func_regexp_unittest
