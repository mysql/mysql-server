/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <gtest/gtest.h>

#include "include/template_utils.h"

namespace template_utils_unittest {

class Base
{
public:
  int id() const { return 1; }

  // Needed to make compiler understand that it's a polymorphic class.
  virtual ~Base() {}
};


class Descendent : public Base
{
public:
  int id() const { return 2; }
};


TEST(TemplateUtilsTest, DownCastReference)
{
  Descendent descendent;
  Base &baseref= descendent;
  auto descendentref= down_cast<Descendent&>(baseref);

  EXPECT_EQ(1, baseref.id());
  EXPECT_EQ(2, descendentref.id());
}


TEST(TemplateUtilsTest, DownCastRvalueReference)
{
  Descendent descendent;
  Base &&baseref= Descendent();
  auto descendentref= down_cast<Descendent&&>(baseref);

  EXPECT_EQ(1, baseref.id());
  EXPECT_EQ(2, descendentref.id());
}


TEST(TemplateUtilsTest, DownCastPointer)
{
  Descendent descendent;
  Base *baseref= &descendent;
  auto descendentref= down_cast<Descendent*>(baseref);

  EXPECT_EQ(1, baseref->id());
  EXPECT_EQ(2, descendentref->id());
}

}  // namespace
