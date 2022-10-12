/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#include <ctype.h>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>
#include "include/template_utils.h"

namespace template_utils_unittest {

class Base {
 public:
  int id() const { return 1; }

  // Needed to make compiler understand that it's a polymorphic class.
  virtual ~Base() = default;

  // To silence -Wdeprecated-copy.
  Base() = default;
  Base(const Base &) = default;
};

class Descendent : public Base {
 public:
  int id() const { return 2; }
};

TEST(TemplateUtilsTest, DownCastReference) {
  Descendent descendent;
  Base &baseref = descendent;
  auto descendentref = down_cast<Descendent &>(baseref);

  EXPECT_EQ(1, baseref.id());
  EXPECT_EQ(2, descendentref.id());
}

TEST(TemplateUtilsTest, DownCastRvalueReference) {
  Descendent descendent;
  Base &&baseref = Descendent();
  auto descendentref = down_cast<Descendent &&>(baseref);

  EXPECT_EQ(1, baseref.id());
  EXPECT_EQ(2, descendentref.id());
}

TEST(TemplateUtilsTest, DownCastPointer) {
  Descendent descendent;
  Base *baseref = &descendent;
  auto descendentref = down_cast<Descendent *>(baseref);

  EXPECT_EQ(1, baseref->id());
  EXPECT_EQ(2, descendentref->id());
}

TEST(TemplateUtilsTest, FindTrimmedEndCstr) {
  const char *s = "";
  EXPECT_EQ(s, myu::FindTrimmedEnd(s, s + strlen(s), myu::IsSpace));

  s = "foo";
  EXPECT_EQ(s + 3, myu::FindTrimmedEnd(s, s + strlen(s), myu::IsSpace));

  s = " ba  r   ";
  EXPECT_EQ(s + 6, myu::FindTrimmedEnd(s, s + strlen(s), myu::IsSpace));
}

TEST(TemplateUtilsTest, FindTrimmedEndString) {
  std::string s = "";
  EXPECT_EQ(s.end(), myu::FindTrimmedEnd(s.begin(), s.end(), myu::IsSpace));

  s = "foo";
  EXPECT_EQ(s.end(), myu::FindTrimmedEnd(s.begin(), s.end(), myu::IsSpace));

  s = " ba  r   ";
  EXPECT_EQ(s.begin() + 6,
            myu::FindTrimmedEnd(s.begin(), s.end(), myu::IsSpace));
}

TEST(TemplateUtilsTest, FindTrimmedRangeCstr) {
  const char *s = "";
  EXPECT_EQ(std::make_pair(s, s),
            myu::FindTrimmedRange(s, s + strlen(s), myu::IsSpace));

  s = "foo";
  EXPECT_EQ(std::make_pair(s, s + 3),
            myu::FindTrimmedRange(s, s + strlen(s), myu::IsSpace));

  s = " ba  r   ";
  EXPECT_EQ(std::make_pair(s + 1, s + 6),
            myu::FindTrimmedRange(s, s + strlen(s), myu::IsSpace));
}

TEST(TemplateUtilsTest, FindTrimmedRangeString) {
  std::string s = "";
  EXPECT_EQ(std::make_pair(s.begin(), s.end()),
            myu::FindTrimmedRange(s.begin(), s.end(), myu::IsSpace));

  s = "foo";
  EXPECT_EQ(std::make_pair(s.begin(), s.end()),
            myu::FindTrimmedRange(s.begin(), s.end(), myu::IsSpace));

  s = " ba  r   ";
  EXPECT_EQ(std::make_pair(s.begin() + 1, s.begin() + 6),
            myu::FindTrimmedRange(s.begin(), s.end(), myu::IsSpace));

  auto begin_end = myu::FindTrimmedRange(s.cbegin(), s.cend(), myu::IsSpace);
  EXPECT_NE(begin_end.first, begin_end.second);
  EXPECT_EQ(std::string("ba  r"),
            std::string(begin_end.first, begin_end.second));
}

using StrVec = std::vector<std::string>;
TEST(TemplateUtilsTest, SplitEmptyCstr) {
  const char *s = "";
  StrVec elts;
  myu::Split(s, s + strlen(s), myu::IsComma, [&](const char *f, const char *l) {
    elts.emplace_back(f, (l - f));
  });

  EXPECT_EQ(0u, elts.size());
}

TEST(TemplateUtilsTest, SplitEmptyString) {
  std::string s = "";
  StrVec elts;
  myu::Split(s.begin(), s.end(), myu::IsComma,
             [&](const auto &f, const auto &l) { elts.emplace_back(f, l); });

  EXPECT_EQ(0u, elts.size());
}

TEST(TemplateUtilsTest, SplitSingleRangeCstr) {
  const char *s = "foo";
  StrVec elts;
  myu::Split(s, s + strlen(s), myu::IsComma, [&](const char *f, const char *l) {
    elts.emplace_back(f, (l - f));
  });

  EXPECT_EQ(StrVec{"foo"}, elts);
}

TEST(TemplateUtilsTest, SplitSingleRangeString) {
  std::string s = "foo";
  StrVec elts;
  myu::Split(s.begin(), s.end(), myu::IsComma,
             [&](const auto &f, const auto &l) { elts.emplace_back(f, l); });

  EXPECT_EQ(StrVec{"foo"}, elts);
}

TEST(TemplateUtilsTest, SplitCstr) {
  const char *s = " , ,, some text   ,,,additional text,,,, ";
  StrVec elts;
  myu::Split(s, s + strlen(s), myu::IsComma, [&](const char *f, const char *l) {
    elts.emplace_back(f, (l - f));
  });

  StrVec expected{" ", " ", "", " some text   ", "", "", "additional text", "",
                  "",  "",  " "};
  EXPECT_EQ(expected, elts);
}

TEST(TemplateUtilsTest, SplitStringDiscardEmpty) {
  std::string s = " , ,, some text   ,,,additional text,,,, ";
  StrVec elts;
  myu::Split(std::begin(s), std::end(s), myu::IsComma,
             [&](std::string::const_iterator f, std::string::const_iterator l) {
               if (f != l) elts.emplace_back(f, l);
             });

  StrVec expected{" ", " ", " some text   ", "additional text", " "};
  EXPECT_EQ(expected, elts);
}

TEST(TemplateUtilsTest, SplitStringTrimDiscardEmpty) {
  std::string s = " , ,, some text   ,,,additional text,,,, ";
  StrVec elts;
  myu::Split(std::begin(s), std::end(s), myu::IsComma,
             [&](std::string::const_iterator f, std::string::const_iterator l) {
               auto rng = myu::FindTrimmedRange(f, l, myu::IsSpace);
               if (rng.first != rng.second)
                 elts.emplace_back(rng.first, rng.second);
             });

  StrVec expected{"some text", "additional text"};
  EXPECT_EQ(expected, elts);
}

TEST(TemplateUtilsTest, SplitVector) {
  std::vector<int> v = {0, 1, -1, 3, 4, -1, -1};
  std::vector<std::vector<int>> elts;
  myu::Split(
      std::begin(v), std::end(v), [](int i) { return i < 0; },
      [&](std::vector<int>::const_iterator f,
          std::vector<int>::const_iterator l) {
        if (f != l) elts.emplace_back(f, l);
      });

  std::vector<std::vector<int>> exp{{0, 1}, {3, 4}};
  EXPECT_EQ(exp, elts);
}
}  // namespace template_utils_unittest
