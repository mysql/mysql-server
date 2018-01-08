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


#include <gtest/gtest.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/protocol/message_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/metadata_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/column_info_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/notice_builder.h"
#include "plugin/x/ngs/include/ngs/protocol/output_buffer.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "unittest/gunit/xplugin/xpl/protobuf_message.h"

namespace xpl
{

namespace test
{

typedef ngs::Output_buffer Output_buffer;
typedef ngs::Message_builder Message_builder;
typedef ngs::Metadata_builder Metadata_builder;
typedef ngs::Notice_builder Notice_builder;
typedef ngs::Page_pool Page_pool;

const ngs::Pool_config default_pool_config = { 0, 0, BUFFER_PAGE_SIZE };

TEST(message_builder, encode_resultset_fetch_done)
{
  Message_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  mb.encode_empty_message(obuffer.get(), Mysqlx::ServerMessages::RESULTSET_FETCH_DONE);
  ngs::unique_ptr<Mysqlx::Resultset::FetchDone> msg (message_from_buffer<Mysqlx::Resultset::FetchDone>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);
  ASSERT_TRUE(msg->IsInitialized());
}

TEST(message_builder, encode_resultset_fetch_done_more_resultsets)
{
  Message_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  mb.encode_empty_message(obuffer.get(), Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS);
  ngs::unique_ptr<Mysqlx::Resultset::FetchDoneMoreResultsets> msg(message_from_buffer<Mysqlx::Resultset::FetchDoneMoreResultsets>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);
  ASSERT_TRUE(msg->IsInitialized());
}

TEST(message_builder, encode_stmt_execute_ok)
{
  Message_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  mb.encode_empty_message(obuffer.get(), Mysqlx::ServerMessages::OK);
  ngs::unique_ptr<Mysqlx::Sql::StmtExecuteOk> msg(message_from_buffer<Mysqlx::Sql::StmtExecuteOk>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);
  ASSERT_TRUE(msg->IsInitialized());
}

TEST(message_builder, encode_compact_metadata)
{
  Metadata_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  const uint64 COLLATION = 1u;
  const auto TYPE = Mysqlx::Resultset::ColumnMetaData::SINT;
  const int DECIMALS = 3;
  const uint32 FLAGS = 0xabcdu;
  const uint32 LENGTH = 20u;
  const uint32 CONTENT_TYPE = 7u;

  ::ngs::Column_info_builder column_info;

  column_info.set_collation(COLLATION);
  column_info.set_decimals(DECIMALS);
  column_info.set_flags(FLAGS);
  column_info.set_length(LENGTH);
  column_info.set_type(TYPE);
  column_info.set_content_type(CONTENT_TYPE);
  mb.encode_metadata(obuffer.get(), &column_info.get());

  ngs::unique_ptr<Mysqlx::Resultset::ColumnMetaData> msg(message_from_buffer<Mysqlx::Resultset::ColumnMetaData>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);

  ASSERT_TRUE(msg->has_collation());
  ASSERT_EQ(COLLATION, msg->collation());
  ASSERT_TRUE(msg->has_type());
  ASSERT_EQ(TYPE, msg->type());
  ASSERT_TRUE(msg->has_fractional_digits());
  ASSERT_EQ(DECIMALS, msg->fractional_digits());
  ASSERT_TRUE(msg->has_flags());
  ASSERT_EQ(FLAGS, msg->flags());
  ASSERT_TRUE(msg->has_length());
  ASSERT_EQ(LENGTH, msg->length());
  ASSERT_TRUE(msg->has_content_type());
  ASSERT_EQ(CONTENT_TYPE, msg->content_type());

  ASSERT_FALSE(msg->has_catalog());
  ASSERT_FALSE(msg->has_name());
  ASSERT_FALSE(msg->has_original_name());
  ASSERT_FALSE(msg->has_original_table());
  ASSERT_FALSE(msg->has_schema());
  ASSERT_FALSE(msg->has_table());
}

TEST(message_builder, encode_full_metadata)
{
  Metadata_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  const uint64 COLLATION = 2u;
  const auto TYPE = Mysqlx::Resultset::ColumnMetaData::BYTES;
  const int DECIMALS = 4;
  const uint32 FLAGS = 0x89abu;
  const uint32 LENGTH = 0u;
  const uint32 CONTENT_TYPE = 1u;
  const std::string CATALOG = "CATALOG_NAME";
  const std::string TABLE_NAME = "TABLE_NAME";
  const std::string ORG_TABLE_NAME = "ORG_TABLE_NAME";
  const std::string SCHEMA = "SCHEMA_NAME";
  const std::string COLUM_NAME = "COLUMN_NAME";
  const std::string ORG_COLUM_NAME = "ORG_COLUMN_NAME";

  ::ngs::Column_info_builder column_info;

  column_info.set_non_compact_data(
      CATALOG.c_str(),
      COLUM_NAME.c_str(),
      TABLE_NAME.c_str(),
      SCHEMA.c_str(),
      ORG_COLUM_NAME.c_str(),
      ORG_TABLE_NAME.c_str());
  column_info.set_collation(COLLATION);
  column_info.set_decimals(DECIMALS);
  column_info.set_flags(FLAGS);
  column_info.set_length(LENGTH);
  column_info.set_type(TYPE);
  column_info.set_content_type(CONTENT_TYPE);

  mb.encode_metadata(obuffer.get(), &column_info.get());

  ngs::unique_ptr<Mysqlx::Resultset::ColumnMetaData> msg(message_from_buffer<Mysqlx::Resultset::ColumnMetaData>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);

  ASSERT_TRUE(msg->has_collation());
  ASSERT_EQ(COLLATION, msg->collation());
  ASSERT_TRUE(msg->has_type());
  ASSERT_EQ(TYPE, msg->type());
  ASSERT_TRUE(msg->has_fractional_digits());
  ASSERT_EQ(DECIMALS, msg->fractional_digits());
  ASSERT_TRUE(msg->has_flags());
  ASSERT_EQ(FLAGS, msg->flags());
  ASSERT_TRUE(msg->has_length());
  ASSERT_EQ(LENGTH, msg->length());
  ASSERT_TRUE(msg->has_content_type());
  ASSERT_EQ(CONTENT_TYPE, msg->content_type());
  ASSERT_TRUE(msg->has_catalog());
  ASSERT_EQ(CATALOG, msg->catalog());
  ASSERT_TRUE(msg->has_name());
  ASSERT_EQ(COLUM_NAME, msg->name());
  ASSERT_TRUE(msg->has_original_name());
  ASSERT_EQ(ORG_COLUM_NAME, msg->original_name());
  ASSERT_TRUE(msg->has_original_table());
  ASSERT_EQ(ORG_TABLE_NAME, msg->original_table());
  ASSERT_TRUE(msg->has_schema());
  ASSERT_EQ(SCHEMA, msg->schema());
  ASSERT_TRUE(msg->has_table());
  ASSERT_EQ(TABLE_NAME, msg->table());
}

TEST(message_builder, encode_notice_frame)
{
  Notice_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  const uint32 TYPE = 2;
  const int SCOPE = Mysqlx::Notice::Frame_Scope_GLOBAL;
  const std::string DATA = "\0\0\1\12\12aaa\0";

  mb.encode_frame(obuffer.get(), TYPE, DATA, SCOPE);

  ngs::unique_ptr<Mysqlx::Notice::Frame> msg(message_from_buffer<Mysqlx::Notice::Frame>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);

  ASSERT_TRUE(msg->has_type());
  ASSERT_EQ(TYPE, msg->type());
  ASSERT_TRUE(msg->has_scope());
  ASSERT_EQ(SCOPE, msg->scope());
  ASSERT_TRUE(msg->has_payload());
  ASSERT_EQ(DATA, msg->payload());
}

TEST(message_builder, encode_notice_rows_affected)
{
  Notice_builder mb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  const uint64 ROWS_AFFECTED = 10001u;

  mb.encode_rows_affected(obuffer.get(), ROWS_AFFECTED);

  ngs::unique_ptr<Mysqlx::Notice::Frame> msg(message_from_buffer<Mysqlx::Notice::Frame>(obuffer.get()));

  ASSERT_TRUE(NULL != msg);

  ASSERT_TRUE(msg->has_type());
  ASSERT_EQ(3, msg->type()); /*Mysqlx::Notice::SessionStateChanged*/
  ASSERT_TRUE(msg->has_scope());
  ASSERT_EQ(Mysqlx::Notice::Frame_Scope_LOCAL, msg->scope());
  ASSERT_TRUE(msg->has_payload());

  Mysqlx::Notice::SessionStateChanged change;
  change.ParseFromString(msg->payload());

  ASSERT_EQ(Mysqlx::Notice::SessionStateChanged::ROWS_AFFECTED, change.param());
  ASSERT_EQ(Mysqlx::Datatypes::Scalar::V_UINT, change.value().type());
  ASSERT_EQ(ROWS_AFFECTED, change.value().v_unsigned_int());
}

} // namespace test

} // namespace xpl
