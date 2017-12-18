/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

#include <gtest/gtest.h>

#include "sql/mysqld.h"
#include "unittest/gunit/parsertest.h"

namespace character_set_deprecation_unittest {

/**
  Test testing the hypothetical situation that the National character
  set is not "utf8". There should not be any warning in that case, which
  parse(const char*) makes sure of. The set-up involves changing a global
  variable, so we can't test it in MTR.
*/
class CharacterSetDeprecationTest : public ParserTest
{
protected:
  void SetUp() override {
    ParserTest::SetUp();
    thd()->lex->will_contextualize= false;
    m_saved_cs = national_charset_info;
    national_charset_info= &my_charset_bin;
  }

  void TearDown() override {
    ParserTest::TearDown();
    national_charset_info = m_saved_cs;
  }

private:
  CHARSET_INFO *m_saved_cs;
};

TEST_F(CharacterSetDeprecationTest, National)
{
  parse("SELECT n'abc'");
  parse("CREATE TABLE t ( a NATIONAL CHAR(1) )");
  parse("CREATE TABLE t ( a NCHAR(1) )");
  parse("CREATE TABLE t ( a NVARCHAR(1) )");
}

} // namespace
