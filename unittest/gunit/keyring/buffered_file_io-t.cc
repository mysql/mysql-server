/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_global.h>
#include <gtest/gtest.h>
#include <mysql/plugin_keyring.h>
#include <sql_plugin_ref.h>
#include "keyring_key.h"
#include "buffered_file_io.h"

#if defined(HAVE_PSI_INTERFACE)
#if !defined(MERGE_UNITTESTS)
namespace keyring
{
  PSI_memory_key key_memory_KEYRING = PSI_NOT_INSTRUMENTED;
  PSI_memory_key key_LOCK_keyring = PSI_NOT_INSTRUMENTED;
}
#endif
namespace keyring
{
  extern PSI_file_key keyring_file_data_key;
  extern PSI_file_key keyring_backup_file_data_key;
}
#endif

namespace keyring_buffered_file_io_unittest
{
  using namespace keyring;

  class Buffered_file_io_test : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      keyring_file_data_key = PSI_NOT_INSTRUMENTED;
      keyring_backup_file_data_key = PSI_NOT_INSTRUMENTED;
      logger= new Logger(logger);
    }

    virtual void TearDown()
    {
      fake_mysql_plugin.name.str= const_cast<char*>("FakeKeyringPlugin");
      fake_mysql_plugin.name.length= strlen("FakeKeyringPlugin");
      delete logger;
    }

  protected:
    st_plugin_int fake_mysql_plugin;
    ILogger *logger;
  };

  TEST_F(Buffered_file_io_test, InitWithNotExisitingKeyringFile)
  {
    std::string file_name("./some_funny_name");
    Buffered_file_io buffered_io(logger);
    remove(file_name.c_str());
    my_bool retVal= 1;
    retVal= buffered_io.init(&file_name);
    EXPECT_EQ(retVal, 0);
    Key empty_key;
    //The keyring file is new so no keys should be available
    retVal= buffered_io >> &empty_key;
    EXPECT_EQ(retVal, 0);
    ASSERT_TRUE(empty_key.get_key_signature()->empty() == TRUE);
    remove(file_name.c_str());
  }

  TEST_F(Buffered_file_io_test, WriteAndFetchKey)
  {
    std::string file_name("./write_key");
    Buffered_file_io *buffered_io= new Buffered_file_io(logger);
    remove(file_name.c_str());
    my_bool retVal= 1;
    retVal= buffered_io->init(&file_name);
    EXPECT_EQ(retVal, 0);
    std::string sample_key_data;

    Key key_to_add("Robert_add_key", "AES", "Roberts_add_key_type", sample_key_data.c_str(), sample_key_data.length()+1);
    buffered_io->reserve_buffer(key_to_add.get_key_pod_size());
    retVal= *buffered_io << &key_to_add;
    EXPECT_EQ(retVal, 1);
    retVal= buffered_io->flush_to_keyring();
    EXPECT_EQ(retVal, 0);
    delete buffered_io;

    Buffered_file_io *buffered_io_2= new Buffered_file_io(logger);
    buffered_io_2->init(&file_name);
    Key empty_key;
    retVal= *buffered_io_2 >> &empty_key;
    EXPECT_EQ(retVal, 1);
    EXPECT_STREQ("Robert_add_keyRoberts_add_key_type",
                 empty_key.get_key_signature()->c_str());
    EXPECT_EQ(strlen("Robert_add_keyRoberts_add_key_type"),
              empty_key.get_key_signature()->length());

    uchar* empty_key_data= empty_key.get_key_data();
    size_t empty_key_data_size= empty_key.get_key_data_size();
    EXPECT_EQ(empty_key_data_size, sample_key_data.length()+1);
    retVal= memcmp(empty_key_data, sample_key_data.c_str(), empty_key_data_size);
    EXPECT_EQ(retVal, 0);
    buffered_io_2->close();

    delete buffered_io_2;
    remove(file_name.c_str());
  }

  TEST_F(Buffered_file_io_test, Write2KeysAndFetchKeys)
  {
    std::string file_name("./write_key");
    Buffered_file_io *buffered_io= new Buffered_file_io(logger);
    remove(file_name.c_str());
    my_bool retVal= 1;
    retVal= buffered_io->init(&file_name);
    EXPECT_EQ(retVal, 0);
    std::string sample_key_data1("Robi1");
    std::string sample_key_data2("Robi2");

    Key key_to_add1("Robert_add_key1", "AES", "Roberts_add_key1_type", sample_key_data1.c_str(), sample_key_data1.length()+1);
    Key key_to_add2("Robert_add_key2", "AES", "Roberts_add_key2_type", sample_key_data2.c_str(), sample_key_data2.length()+1);
    buffered_io->reserve_buffer(key_to_add1.get_key_pod_size() + key_to_add2.get_key_pod_size());
    retVal= *buffered_io << &key_to_add1;
    EXPECT_EQ(retVal, 1);
    retVal= *buffered_io << &key_to_add2;
    EXPECT_EQ(retVal, 1);
    retVal= buffered_io->flush_to_keyring();
    EXPECT_EQ(retVal, 0);
    delete buffered_io;

    Buffered_file_io *buffered_io_2= new Buffered_file_io(logger);
    buffered_io_2->init(&file_name);

    Key empty_key_1;
    retVal= *buffered_io_2 >> &empty_key_1;
    EXPECT_EQ(retVal, 1);
    EXPECT_STREQ("Robert_add_key1Roberts_add_key1_type",
                 empty_key_1.get_key_signature()->c_str());
    EXPECT_EQ(strlen("Robert_add_key1Roberts_add_key1_type"),
              empty_key_1.get_key_signature()->length());
    uchar* empty_key_1_data= empty_key_1.get_key_data();
    size_t empty_key_1_data_size= empty_key_1.get_key_data_size();
    EXPECT_EQ(empty_key_1_data_size, sample_key_data1.length()+1);
    retVal= memcmp(empty_key_1_data, sample_key_data1.c_str(), empty_key_1_data_size);
    EXPECT_EQ(retVal, 0);

    Key empty_key_2;
    retVal= *buffered_io_2 >> &empty_key_2;
    EXPECT_EQ(retVal, 1);
    EXPECT_STREQ("Robert_add_key2Roberts_add_key2_type",
                 empty_key_2.get_key_signature()->c_str());
    EXPECT_EQ(strlen("Robert_add_key2Roberts_add_key2_type"),
              empty_key_2.get_key_signature()->length());
    uchar* empty_key_2_data= empty_key_2.get_key_data();
    size_t empty_key_2_data_size= empty_key_2.get_key_data_size();
    EXPECT_EQ(empty_key_2_data_size, sample_key_data2.length()+1);
    retVal= memcmp(empty_key_2_data, sample_key_data2.c_str(), empty_key_2_data_size);
    EXPECT_EQ(retVal, 0);

    buffered_io_2->close();

    delete buffered_io_2;
    remove(file_name.c_str());
  }
}
