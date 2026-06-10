/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtest/gtest.h>

#include "mrs/database/converters/column_datatype_converter.h"
#include "mrs/database/converters/column_mapping_converter.h"
#include "mrs/database/converters/id_generation_type_converter.h"
#include "mrs/database/converters/kind_converter.h"

using namespace mrs::database;
using namespace mrs::database::entry;

using testing::Test;
using testing::TestWithParam;
using testing::WithParamInterface;
using ColumnMapping = entry::ForeignKeyReference::ColumnMapping;

template <typename Type>
struct Data {
  std::string in_datatype;
  Type out_type;
};

template <typename Converter, typename Type>
class ConverterTests : public Test {
 public:
  Type convertValue(const char *value) {
    Type out;
    sut(&out, value);
    return out;
  }

  Converter sut;
};

template <typename Converter, typename Type>
class ConverterParamTests : public ConverterTests<Converter, Type>,
                            public WithParamInterface<Data<Type>> {
 public:
};

using KindParamTests = ConverterParamTests<KindTypeConverter, KindType>;
using KindTests = ConverterTests<KindTypeConverter, KindType>;
using KindData = Data<KindType>;

TEST_P(KindParamTests, valid_conversions) {
  auto &p = GetParam();
  ASSERT_EQ(p.out_type, convertValue(p.in_datatype.c_str()));
}

TEST_F(KindTests, invalid_conversions) {
  ASSERT_THROW(convertValue(nullptr), std::runtime_error);
  ASSERT_THROW(convertValue(""), std::runtime_error);
  ASSERT_THROW(convertValue("INVALID"), std::runtime_error);
  ASSERT_THROW(convertValue("RESULT(AAA)"), std::runtime_error);
  ASSERT_THROW(convertValue("  RESULT"), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    ValidParameters, KindParamTests,
    testing::Values(KindData{"PARAMETERS", KindType::PARAMETERS},
                    KindData{"RESULT", KindType::RESULT},
                    KindData{"parameters", KindType::PARAMETERS},
                    KindData{"result", KindType::RESULT}));

// IdGeneration

using IdGenParamTests =
    ConverterParamTests<IdGenerationTypeConverter, IdGenerationType>;
using IdGenTests = ConverterTests<IdGenerationTypeConverter, IdGenerationType>;
using IgGenData = Data<IdGenerationType>;

TEST_P(IdGenParamTests, valid_conversions) {
  auto &p = GetParam();
  ASSERT_EQ(p.out_type, convertValue(p.in_datatype.c_str()));
}

TEST_F(IdGenTests, valid_conversions) {
  ASSERT_EQ(IdGenerationType::NONE, convertValue(nullptr));
}

TEST_F(IdGenTests, invalid_conversions) {
  ASSERT_THROW(convertValue(""), std::runtime_error);
  ASSERT_THROW(convertValue("INVALID"), std::runtime_error);
  ASSERT_THROW(convertValue("rev_uuid(AAA)"), std::runtime_error);
  ASSERT_THROW(convertValue("rev_uuid   "), std::runtime_error);
  ASSERT_THROW(convertValue("  rev_uuid"), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    ValidParameters, IdGenParamTests,
    testing::Values(IgGenData{"auto_inc", IdGenerationType::AUTO_INCREMENT},
                    IgGenData{"rev_uuid", IdGenerationType::REVERSE_UUID},
                    IgGenData{"null", IdGenerationType::NONE},
                    IgGenData{"AUTO_INC", IdGenerationType::AUTO_INCREMENT},
                    IgGenData{"REV_UUID", IdGenerationType::REVERSE_UUID},
                    IgGenData{"NULL", IdGenerationType::NONE}));

// ColumnMapping

using ColumnMappingParamTests =
    ConverterParamTests<ColumnMappingConverter, ColumnMapping>;
using ColumnMappingTests =
    ConverterTests<ColumnMappingConverter, ColumnMapping>;
using ColumnMappingData = Data<ColumnMapping>;

TEST_P(ColumnMappingParamTests, valid_conversions) {
  auto &p = GetParam();
  ASSERT_EQ(p.out_type, convertValue(p.in_datatype.c_str()));
}

TEST_F(ColumnMappingTests, invalid_conversions) {
  ASSERT_THROW(convertValue(""), std::runtime_error);
  ASSERT_THROW(convertValue("sdsds"), std::runtime_error);
  ASSERT_THROW(convertValue("{}"), std::runtime_error);
  ASSERT_THROW(convertValue("{\"base\":\"aa\",\"ref\":\"aa\"}"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[{\"base\":1,\"ref\":\"aa\"}]"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[{\"base\":\"aa\",\"ref\":1}]"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[\"\"]"), std::runtime_error);
  ASSERT_THROW(convertValue("[1,2]"), std::runtime_error);
  ASSERT_THROW(convertValue("1"), std::runtime_error);
  ASSERT_THROW(convertValue("\"1\""), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    ValidParameters, ColumnMappingParamTests,
    testing::Values(ColumnMappingData{"[]", {}},
                    ColumnMappingData{"[{\"base\":\"a\", \"ref\":\"b\"}]",
                                      {{"a", "b"}}},
                    ColumnMappingData{"[{\"base\":\"1\", \"ref\":\"2\"},"
                                      "{\"base\":\"3\", \"ref\":\"4\"}]",
                                      {{"1", "2"}, {"3", "4"}}}));

// Column Datatype

using ColumnDatatypeParamTests =
    ConverterParamTests<ColumnDatatypeConverter, ColumnType>;
using ColumnDatatypeTests = ConverterTests<ColumnDatatypeConverter, ColumnType>;
using ColumnDatatypeData = Data<ColumnType>;

TEST_P(ColumnDatatypeParamTests, valid_conversions) {
  auto &p = GetParam();
  ASSERT_EQ(p.out_type, convertValue(p.in_datatype.c_str()));
}

TEST_F(ColumnDatatypeTests, invalid_conversions) {
  ASSERT_THROW(convertValue(""), std::runtime_error);
  ASSERT_THROW(convertValue("sdsds"), std::runtime_error);
  ASSERT_THROW(convertValue("{}"), std::runtime_error);
  ASSERT_THROW(convertValue("{\"base\":\"aa\",\"ref\":\"aa\"}"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[{\"base\":1,\"ref\":\"aa\"}]"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[{\"base\":\"aa\",\"ref\":1}]"),
               std::runtime_error);
  ASSERT_THROW(convertValue("[\"\"]"), std::runtime_error);
  ASSERT_THROW(convertValue("[1,2]"), std::runtime_error);
  ASSERT_THROW(convertValue("1"), std::runtime_error);
  ASSERT_THROW(convertValue("\"1\""), std::runtime_error);
  ASSERT_THROW(convertValue("TEST TINYINT"), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(
    ValidParameters, ColumnDatatypeParamTests,
    testing::Values(
        ColumnDatatypeData{"TINYINT", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"SMALLINT", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"MEDIUMINT", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"INT", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"BIGINT", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"FLOAT", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"REAL", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"DOUBLE", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"DECIMAL", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"CHAR", entry::ColumnType::STRING},
        ColumnDatatypeData{"NCHAR", entry::ColumnType::STRING},
        ColumnDatatypeData{"VARCHAR", entry::ColumnType::STRING},
        ColumnDatatypeData{"NVARCHAR", entry::ColumnType::STRING},
        ColumnDatatypeData{"BINARY", entry::ColumnType::BINARY},
        ColumnDatatypeData{"VARBINARY", entry::ColumnType::BINARY},
        ColumnDatatypeData{"TINYTEXT", entry::ColumnType::STRING},
        ColumnDatatypeData{"TEXT", entry::ColumnType::STRING},
        ColumnDatatypeData{"MEDIUMTEXT", entry::ColumnType::STRING},
        ColumnDatatypeData{"LONGTEXT", entry::ColumnType::STRING},
        ColumnDatatypeData{"TINYBLOB", entry::ColumnType::BINARY},
        ColumnDatatypeData{"BLOB", entry::ColumnType::BINARY},
        ColumnDatatypeData{"MEDIUMBLOB", entry::ColumnType::BINARY},
        ColumnDatatypeData{"LONGBLOB", entry::ColumnType::BINARY},
        ColumnDatatypeData{"JSON", entry::ColumnType::JSON},
        ColumnDatatypeData{"DATETIME", entry::ColumnType::STRING},
        ColumnDatatypeData{"DATE", entry::ColumnType::STRING},
        ColumnDatatypeData{"TIME", entry::ColumnType::STRING},
        ColumnDatatypeData{"YEAR", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"TIMESTAMP", entry::ColumnType::STRING},
        ColumnDatatypeData{"GEOMETRY", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POINT", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"LINESTRING", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POLYGON", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMCOLLECTION", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMETRYCOLLECTION", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOINT", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTILINESTRING", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOLYGON", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"BIT", entry::ColumnType::BINARY},
        ColumnDatatypeData{"BOOLEAN", entry::ColumnType::BOOLEAN},
        ColumnDatatypeData{"ENUM", entry::ColumnType::STRING},
        ColumnDatatypeData{"SET", entry::ColumnType::STRING},

        ColumnDatatypeData{"TINYINT(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"SMALLINT(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"MEDIUMINT(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"INT(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"BIGINT(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"FLOAT(10)", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"REAL(10)", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"DOUBLE(10)", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"DECIMAL(10)", entry::ColumnType::DOUBLE},
        ColumnDatatypeData{"CHAR(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"NCHAR(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"VARCHAR(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"NVARCHAR(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"BINARY(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"VARBINARY(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"TINYTEXT(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"TEXT(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"MEDIUMTEXT(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"LONGTEXT(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"TINYBLOB(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"BLOB(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"MEDIUMBLOB(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"LONGBLOB(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"JSON(10)", entry::ColumnType::JSON},
        ColumnDatatypeData{"DATETIME(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"DATE(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"TIME(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"YEAR(10)", entry::ColumnType::INTEGER},
        ColumnDatatypeData{"TIMESTAMP(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"GEOMETRY(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POINT(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"LINESTRING(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POLYGON(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMCOLLECTION(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMETRYCOLLECTION(10)",
                           entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOINT(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTILINESTRING(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOLYGON(10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"BIT(10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"BOOLEAN(10)", entry::ColumnType::BOOLEAN},
        ColumnDatatypeData{"ENUM(10)", entry::ColumnType::STRING},
        ColumnDatatypeData{"SET(10)", entry::ColumnType::STRING},

        ColumnDatatypeData{"POINT  ", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POINT  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"LINESTRING  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"POLYGON  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMCOLLECTION  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"GEOMETRYCOLLECTION  (10)",
                           entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOINT  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTILINESTRING  (10)",
                           entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"MULTIPOLYGON  (10)", entry::ColumnType::GEOMETRY},
        ColumnDatatypeData{"BIT  (10)", entry::ColumnType::BINARY},
        ColumnDatatypeData{"BIT(1)", entry::ColumnType::BOOLEAN}));
