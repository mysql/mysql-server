/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/admin_cmd_index.h"
#include "plugin/x/src/xpl_error.h"
#include "unittest/gunit/xplugin/xpl/assert_error_code.h"
#include "unittest/gunit/xplugin/xpl/mock/session.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"
#include "unittest/gunit/xplugin/xpl/one_row_resultset.h"

namespace xpl {
namespace test {

using namespace ::testing;

namespace {
const char* const PATH = "$.path";
#define PATH_HASH "6EA549FAA434CCD150A7DB5FF9C0AEC77C4F5D25"
const Any::Object::Fld MEMBER{"member", PATH};
const Any::Object::Fld NOT_REQUIRED{"required", false};
const Any::Object::Fld REQUIRED{"required", true};
const Any::Object::Fld OPTIONS{"options", 42u};
const Any::Object::Fld SRID{"srid", 666u};
#define SHOW_COLUMNS(field)                 \
  "SHOW COLUMNS FROM `schema`.`collection`" \
  " WHERE Field = '" field "'"

using Index_field = Admin_command_index::Index_field;
}  // namespace

struct Param_index_field_create {
  int expect_error;
  Any::Object constraint;
};

class Index_field_create_test
    : public Test,
      public WithParamInterface<Param_index_field_create> {};

TEST_P(Index_field_create_test, fail_on_create) {
  const Param_index_field_create& param = GetParam();
  Admin_command_arguments_object args(param.constraint);

  ngs::Error_code error;
  std::unique_ptr<const Index_field> field(Index_field::create(
      Admin_command_handler::MYSQLX_NAMESPACE, true, "DEFAULT", &args, &error));
  ASSERT_ERROR_CODE(param.expect_error, error);
}

Param_index_field_create fail_on_create_param[] = {
    {ER_X_CMD_NUM_ARGUMENTS, {/*no path*/ {"type", "DECIMAL"}, NOT_REQUIRED}},
    {ER_X_CMD_NUM_ARGUMENTS, {MEMBER, {"type", "DECIMAL"} /*norequired*/}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, /*default type*/ NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "DECIMAL SIGNED"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "tinyint(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "tinyint"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "tinyint"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "smallint(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "smallint"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "smallint"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "mediumint(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "mediumint"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "mediumint"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "int(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "int"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "int"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "integer(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "integer"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "integer"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bigint(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "bigint"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bigint"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "real"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "real"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "float"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "float"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "double"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "double"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "numeric"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "numeric"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "date(10)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "date(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "date unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "date"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "date"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "time(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "time unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "time"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "time"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "timestamp(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "timestamp unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "timestamp"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "timestamp"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "datetime(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "datetime unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "datetime"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "datetime"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "year(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "year unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "year"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "year"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bit(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bit unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bit"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "bit"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "blob(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "blob unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "blob"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "blob"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "text(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "text unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "text"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "text"}, SRID, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "geojson(10)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "geojson(10,2)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "geojson unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE, {MEMBER, {"type", "fulltext(10)"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "fulltext unsigned"}, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "fulltext"}, OPTIONS, NOT_REQUIRED}},
    {ER_X_CMD_ARGUMENT_VALUE,
     {MEMBER, {"type", "fulltext"}, SRID, NOT_REQUIRED}}, };

INSTANTIATE_TEST_CASE_P(fail_on_create_field, Index_field_create_test,
                        ValuesIn(fail_on_create_param));

struct Param_index_field_add_field {
  std::string expect;
  Any::Object constraint;
};

class Index_field_add_field_test
    : public Test,
      public WithParamInterface<Param_index_field_add_field> {};

TEST_P(Index_field_add_field_test, add_field) {
  const Param_index_field_add_field& param = GetParam();

  Admin_command_arguments_object args(param.constraint);

  Query_string_builder qb;
  ngs::Error_code error;
  std::unique_ptr<const Index_field> field(Index_field::create(
      Admin_command_handler::MYSQLX_NAMESPACE, true, "TEXT", &args, &error));
  ASSERT_ERROR_CODE(ER_X_SUCCESS, error);
  field->add_field(&qb);
  ASSERT_STREQ(std::string("`" + param.expect + PATH_HASH "`").c_str(),
               qb.get().c_str());
}

Param_index_field_add_field add_field_param[] = {
    {"$ix_xd_", {MEMBER, {"type", "DECIMAL"}, NOT_REQUIRED}},
    {"$ix_xd_", {MEMBER, {"type", "decimal"}, NOT_REQUIRED}},
    {"$ix_xd_", {MEMBER, {"type", "DEcimAL"}, NOT_REQUIRED}},
    {"$ix_xd32_", {MEMBER, {"type", "DECIMAL(32)"}, NOT_REQUIRED}},
    {"$ix_xd32_16_", {MEMBER, {"type", "DECIMAL(32,16)"}, NOT_REQUIRED}},
    {"$ix_xd_16_", {MEMBER, {"type", "DECIMAL(0,16)"}, NOT_REQUIRED}},
    {"$ix_xd32_16_u_",
     {MEMBER, {"type", "DECIMAL(32,16) UNSIGNED"}, NOT_REQUIRED}},
    {"$ix_xd32_16_ur_",
     {MEMBER, {"type", "DECIMAL(32,16) UNSIGNED"}, REQUIRED}},
    {"$ix_xd32_16_r_", {MEMBER, {"type", "DECIMAL(32,16)"}, REQUIRED}},
    {"$ix_xd_ur_", {MEMBER, {"type", "DECIMAL UNSIGNED"}, REQUIRED}},
    {"$ix_xd_ur_", {MEMBER, {"type", "DECIMAL unsigned"}, REQUIRED}},
    {"$ix_xd_ur_", {MEMBER, {"type", "DECIMAL UNsignED"}, REQUIRED}},
    {"$ix_it_", {MEMBER, {"type", "tinyint"}, NOT_REQUIRED}},
    {"$ix_is_", {MEMBER, {"type", "smallint"}, NOT_REQUIRED}},
    {"$ix_im_", {MEMBER, {"type", "mediumint"}, NOT_REQUIRED}},
    {"$ix_i_", {MEMBER, {"type", "int"}, NOT_REQUIRED}},
    {"$ix_i_", {MEMBER, {"type", "integer"}, NOT_REQUIRED}},
    {"$ix_ib_", {MEMBER, {"type", "bigint"}, NOT_REQUIRED}},
    {"$ix_fr_", {MEMBER, {"type", "real"}, NOT_REQUIRED}},
    {"$ix_f_", {MEMBER, {"type", "float"}, NOT_REQUIRED}},
    {"$ix_fd_", {MEMBER, {"type", "double"}, NOT_REQUIRED}},
    {"$ix_xn_", {MEMBER, {"type", "numeric"}, NOT_REQUIRED}},
    {"$ix_d_", {MEMBER, {"type", "date"}, NOT_REQUIRED}},
    {"$ix_dt_", {MEMBER, {"type", "time"}, NOT_REQUIRED}},
    {"$ix_ds_", {MEMBER, {"type", "timestamp"}, NOT_REQUIRED}},
    {"$ix_dd_", {MEMBER, {"type", "datetime"}, NOT_REQUIRED}},
    {"$ix_dy_", {MEMBER, {"type", "year"}, NOT_REQUIRED}},
    {"$ix_t_", {MEMBER, {"type", "bit"}, NOT_REQUIRED}},
    {"$ix_bt_", {MEMBER, {"type", "blob"}, NOT_REQUIRED}},
    {"$ix_t_", {MEMBER, {"type", "text"}, NOT_REQUIRED}},
    {"$ix_gj_", {MEMBER, {"type", "geojson"}, NOT_REQUIRED}},
    {"$ix_ft_", {MEMBER, {"type", "fulltext"}, NOT_REQUIRED}},
    {"$ix_t_", {MEMBER, /*default type*/ NOT_REQUIRED}}};

INSTANTIATE_TEST_CASE_P(get_index_field_name, Index_field_add_field_test,
                        ValuesIn(add_field_param));

struct Param_index_field_add_column {
  std::string expect;
  bool virtual_supported;
  Any::Object constraint;
};

class Index_field_add_column_test
    : public Test,
      public WithParamInterface<Param_index_field_add_column> {};

TEST_P(Index_field_add_column_test, add_column) {
  const Param_index_field_add_column& param = GetParam();

  Admin_command_arguments_object args(param.constraint);

  Query_string_builder qb;
  ngs::Error_code error;
  std::unique_ptr<const Index_field> field(
      Index_field::create(Admin_command_handler::MYSQLX_NAMESPACE,
                          param.virtual_supported, "TEXT", &args, &error));
  ASSERT_ERROR_CODE(ER_X_SUCCESS, error);
  field->add_column(&qb);
  ASSERT_STREQ(param.expect.c_str(), qb.get().c_str());
}

Param_index_field_add_column add_column_param[] = {
    {" ADD COLUMN `$ix_xd_" PATH_HASH
     "` DECIMAL GENERATED ALWAYS AS (JSON_EXTRACT(doc, '$.path')) VIRTUAL",
     true, {MEMBER, {"type", "DECIMAL"}, NOT_REQUIRED}},
    {" ADD COLUMN `$ix_xd_" PATH_HASH
     "` DECIMAL GENERATED ALWAYS AS (JSON_EXTRACT(doc, '$.path')) STORED",
     false, {MEMBER, {"type", "DECIMAL"}, NOT_REQUIRED}},
    {" ADD COLUMN `$ix_t32_" PATH_HASH
     "` TEXT GENERATED ALWAYS AS (JSON_UNQUOTE(JSON_EXTRACT(doc, "
     "'$.path'))) VIRTUAL",
     true, {MEMBER, {"type", "TEXT(32)"}, NOT_REQUIRED}},
    {" ADD COLUMN `$ix_t32_r_" PATH_HASH
     "` TEXT GENERATED ALWAYS AS (JSON_UNQUOTE(JSON_EXTRACT(doc, "
     "'$.path'))) VIRTUAL NOT NULL",
     true, {MEMBER, {"type", "TEXT(32)"}, REQUIRED}},
    {" ADD COLUMN `$ix_gj_r_" PATH_HASH
     "` GEOMETRY GENERATED ALWAYS AS (ST_GEOMFROMGEOJSON"
     "(JSON_EXTRACT(doc, '$.path'),1,4326)) STORED NOT NULL",
     true, {MEMBER, {"type", "GEOJSON"}, REQUIRED}},
    {" ADD COLUMN `$ix_gj_" PATH_HASH
     "` GEOMETRY GENERATED ALWAYS AS (ST_GEOMFROMGEOJSON("
     "JSON_EXTRACT(doc, '$.path'),42,4326)) STORED",
     true, {MEMBER, {"type", "GEOJSON"}, OPTIONS, NOT_REQUIRED}},
    {" ADD COLUMN `$ix_gj_" PATH_HASH
     "` GEOMETRY GENERATED ALWAYS AS (ST_GEOMFROMGEOJSON("
     "JSON_EXTRACT(doc, '$.path'),1,666)) STORED",
     false, {MEMBER, {"type", "GEOJSON"}, SRID, NOT_REQUIRED}},
    {" ADD COLUMN `$ix_ft_" PATH_HASH
     "` TEXT GENERATED ALWAYS AS (JSON_UNQUOTE("
     "JSON_EXTRACT(doc, '$.path'))) STORED",
     false, {MEMBER, {"type", "FULLTEXT"}, NOT_REQUIRED}}};

INSTANTIATE_TEST_CASE_P(add_column, Index_field_add_column_test,
                        ValuesIn(add_column_param));

class Index_field_is_column_exists_test : public Test {
 public:
  void SetUp() {
    ngs::Error_code error;
    field.reset(Index_field::create(Admin_command_handler::MYSQLX_NAMESPACE,
                                    true, "TEXT", &args, &error));
    ASSERT_ERROR_CODE(ER_X_SUCCESS, error);
  }

  using Sql = ngs::PFS_string;
  using Fld = Any::Object::Fld;
  Any::Object constraint{MEMBER, Fld{"type", "int"}, REQUIRED};
  Admin_command_arguments_object args{constraint};
  StrictMock<ngs::test::Mock_sql_data_context> data_context;
  std::unique_ptr<const Index_field> field;
};

TEST_F(Index_field_is_column_exists_test, column_is_not_exist) {
  EXPECT_CALL(data_context, execute(Eq(Sql(SHOW_COLUMNS("$ix_i_r_" PATH_HASH))),
                                    _, _)).WillOnce(Return(ngs::Success()));
  ngs::Error_code error;
  ASSERT_FALSE(
      field->is_column_exists(&data_context, "schema", "collection", &error));
  ASSERT_ERROR_CODE(ER_X_SUCCESS, error);
}

TEST_F(Index_field_is_column_exists_test, column_is_not_exist_error) {
  EXPECT_CALL(data_context,
              execute(Eq(Sql(SHOW_COLUMNS("$ix_i_r_" PATH_HASH))), _, _))
      .WillOnce(Return(ngs::Error(ER_X_ARTIFICIAL1, "internal error")));
  ngs::Error_code error;
  ASSERT_FALSE(
      field->is_column_exists(&data_context, "schema", "collection", &error));
  ASSERT_ERROR_CODE(ER_X_ARTIFICIAL1, error);
}

TEST_F(Index_field_is_column_exists_test, column_is_exist) {
  One_row_resultset data{"anything"};
  EXPECT_CALL(data_context,
              execute(Eq(Sql(SHOW_COLUMNS("$ix_i_r_" PATH_HASH))), _, _))
      .WillOnce(DoAll(SetUpResultset(data), Return(ngs::Success())));
  ngs::Error_code error;
  ASSERT_TRUE(
      field->is_column_exists(&data_context, "schema", "collection", &error));
  ASSERT_ERROR_CODE(ER_X_SUCCESS, error);
}
}  // namespace test
}  // namespace xpl
