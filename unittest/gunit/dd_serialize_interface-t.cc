/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <gtest/gtest.h>
#include "../../sql/dd/dd.h"
#include "../../sql/dd/impl/types/weak_object_impl.h"
#include "../../sql/dd/types/column.h"
#include "../../sql/dd/types/column_type_element.h"
#include "../../sql/dd/types/foreign_key_element.h"
#include "../../sql/dd/types/foreign_key.h"
#include "../../sql/dd/types/index_element.h"
#include "../../sql/dd/types/index.h"
#include "../sql/dd/types/object_type.h"
#include "../../sql/dd/types/partition.h"
#include "../../sql/dd/types/partition_index.h"
#include "../../sql/dd/types/partition_value.h"
#include "../../sql/dd/types/schema.h"
#include "../../sql/dd/types/table.h"
#include "../../sql/dd/types/tablespace_file.h"
#include "../../sql/dd/types/tablespace.h"

#include <m_string.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>

class SerializeInterfaceTest: public ::testing::Test
{
protected:

  void SetUp()
  {
  }

  void TearDown()
  {
  }

  SerializeInterfaceTest() {}
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(SerializeInterfaceTest);
};

namespace {
    template <typename T>
    void simple_test() {
        T *src = dd::create_object<T>();
        dd::RJ_StringBuffer buf;
        dd::WriterVariant wv(buf);
        dynamic_cast<dd::Weak_object_impl*> (src)->impl()->serialize(&wv);

        T *dst = dd::create_object<T>();

        rapidjson::Document doc;
        doc.Parse<0>("{ \"value\": 42 }");
        EXPECT_EQ(42, doc["value"].GetInt());
        dynamic_cast<dd::Weak_object_impl*> (dst)->impl()->deserialize(&doc);

        delete src;
        delete dst;
    }
}

#define SIMPLE_TEST(t) TEST(SerializeInterfaceTest, t) { simple_test<dd:: t>(); }

SIMPLE_TEST(Column)
SIMPLE_TEST(Column_type_element)
SIMPLE_TEST(Foreign_key_element)
SIMPLE_TEST(Foreign_key)
SIMPLE_TEST(Index_element)
SIMPLE_TEST(Index)
SIMPLE_TEST(Partition)
SIMPLE_TEST(Partition_index)
SIMPLE_TEST(Partition_value)
SIMPLE_TEST(Schema)
SIMPLE_TEST(Table)
SIMPLE_TEST(Tablespace_file)
SIMPLE_TEST(Tablespace)
