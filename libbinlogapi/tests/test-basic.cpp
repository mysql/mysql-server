/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#include "binlog_api.h"
#include <gtest/gtest.h>
#include <iostream>
#include <stdlib.h>

class TestBinaryLog : public ::testing::Test {
 protected:
  TestBinaryLog() {
    // You can do set-up work for each test here.
  }

  virtual ~TestBinaryLog() {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

};


TEST_F(TestBinaryLog, ConnectTo_Bogus)
{
  using mysql::system::create_transport;
  mysql::Binary_log *binlog=
  new mysql::Binary_log(create_transport("bogus-url"));
  EXPECT_GT(binlog->connect(), 0);
  delete(binlog);
}

TEST_F(TestBinaryLog, ConnectTo_TcpIp)
{
  using mysql::system::create_transport;
  mysql::Binary_log *binlog=
  new mysql::Binary_log(create_transport("mysql://root@127.0.0.1:13000"));
  EXPECT_EQ(binlog->connect(),0);
  delete binlog;
}

TEST_F(TestBinaryLog, Connected_TcpIp)
{
  using mysql::system::create_transport;
  mysql::Binary_log *binlog=
  new mysql::Binary_log(create_transport("mysql://root@127.0.0.1:13000"));
  EXPECT_EQ(binlog->connect(),0);
  mysql::Binary_log_event *event;
  binlog->wait_for_next_event(&event);
  EXPECT_TRUE(event->get_event_type() == mysql::ROTATE_EVENT);
  delete event;
  binlog->wait_for_next_event(&event);
  EXPECT_TRUE(event->get_event_type() == mysql::FORMAT_DESCRIPTION_EVENT);
  delete event;
}

TEST_F(TestBinaryLog, SetPosition)
{
  using mysql::system::create_transport;
  mysql::Binary_log_event *event;
  mysql::Binary_log *binlog=
  new mysql::Binary_log(create_transport("mysql://root@127.0.0.1:13000"));
  EXPECT_EQ(binlog->connect(),0);
  std::string filename;
  unsigned long position= binlog->get_position(filename);
  int result= binlog->set_position(filename,4);
  EXPECT_EQ(result,ERR_OK);
  position= binlog->get_position();
  EXPECT_EQ(position, 4);

  binlog->wait_for_next_event(&event);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  // TODO require that the connection string is passed as an argument to the
  // test suite.
  std::cout << "Important: Make sure that the MySQL server is started using "
    "'mysql-test-run --mysqld=--log_bin=searchbin --mysqld=--binlog_format=row"
    " --start alias' and that the server is listening on IP 127.0.0.1 and port"
    " 13000." << std::endl;
  return RUN_ALL_TESTS();
}
