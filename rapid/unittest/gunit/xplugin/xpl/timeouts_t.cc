/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <gmock/gmock.h>

#include "plugin/x/src/global_timeouts.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_server.h"
#include "unittest/gunit/xplugin/xpl/mock/session.h"

namespace ngs {

namespace test {

using ::testing::StrictMock;
using ::testing::_;
using ::testing::Return;
using ::testing::Expectation;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::SetErrnoAndReturn;
using ::testing::ReturnPointee;

class Timers_test_suite : public ::testing::Test
{
public:
  Timers_test_suite()
  : mock_vio(new Mock_vio()), //mock_connection will take ownership
    mock_connection(ngs::make_shared<StrictMock<Mock_connection>>(mock_vio)),
    timeouts({Global_timeouts::Default::k_interactive_timeout,
              Global_timeouts::Default::k_wait_timeout,
              Global_timeouts::Default::k_read_timeout,
              Global_timeouts::Default::k_write_timeout}),
    sut(mock_connection, mock_server, /* id */ 1, mock_protocol_monitor,
        timeouts)
  {
  }

  void TearDown() {
    EXPECT_CALL(*mock_connection->m_mock_vio, shutdown());
  }

  Mock_vio *mock_vio;
  ngs::shared_ptr<StrictMock<Mock_connection>> mock_connection;
  StrictMock<Mock_server> mock_server;
  StrictMock<Mock_protocol_monitor> mock_protocol_monitor;
  Global_timeouts timeouts;

  xpl::test::Mock_ngs_client sut;
};

TEST_F(Timers_test_suite,
    read_one_message_non_interactive_client_default_wait_timeout)
{
  Expectation set_timeout_exp = EXPECT_CALL(*mock_connection->m_mock_vio,
     set_timeout(Vio_interface::Direction::k_read,
                 Global_timeouts::Default::k_interactive_timeout));

  EXPECT_CALL(*mock_connection->m_mock_vio, read(_, _)).After(set_timeout_exp);

  EXPECT_CALL(*mock_connection->m_mock_vio, set_state(_)).Times(2);
  Error_code error;
  sut.read_one_message(error);
}

TEST_F(Timers_test_suite,
    read_one_message_interactive_client_default_interactive_timeout)
{
  Expectation set_timeout_exp = EXPECT_CALL(*mock_connection->m_mock_vio,
      set_timeout(Vio_interface::Direction::k_read,
                  Global_timeouts::Default::k_interactive_timeout));

  EXPECT_CALL(*mock_connection->m_mock_vio, read(_, _)).After(
      set_timeout_exp);

  EXPECT_CALL(*mock_connection->m_mock_vio, set_state(_)).Times(2);
  Error_code error;
  sut.read_one_message(error);
}

TEST_F(Timers_test_suite,
    read_one_message_interactive_client_custom_interactive_timer)
{
  timeouts.interactive_timeout = 11;
  Mock_vio *temp_vio(new Mock_vio()); //temp_connection will take ownership
  ngs::shared_ptr<StrictMock<Mock_connection>> temp_connection(
      ngs::make_shared<StrictMock<Mock_connection>>(temp_vio));
  xpl::test::Mock_ngs_client client(temp_connection, mock_server, /* id */ 1,
      mock_protocol_monitor, timeouts);

  client.set_wait_timeout(timeouts.interactive_timeout);

  EXPECT_CALL(*temp_connection->m_mock_vio,
              set_timeout(Vio_interface::Direction::k_read, 11));
  EXPECT_CALL(*temp_connection->m_mock_vio, read(_, _));
  EXPECT_CALL(*temp_connection->m_mock_vio, set_state(_)).Times(2);
  Error_code error;
  client.read_one_message(error);
  EXPECT_CALL(*temp_connection->m_mock_vio, shutdown());
}

TEST_F(Timers_test_suite,
    read_one_message_non_interactive_client_custom_wait_timer)
{
  timeouts.wait_timeout = 22;
  Mock_vio *temp_vio(new Mock_vio()); //temp_connection will take ownership
  ngs::shared_ptr<StrictMock<Mock_connection>> temp_connection(
      ngs::make_shared<StrictMock<Mock_connection>>(temp_vio));
  xpl::test::Mock_ngs_client client(temp_connection, mock_server, /* id */ 1,
      mock_protocol_monitor, timeouts);

  EXPECT_CALL(*temp_connection->m_mock_vio,
              set_timeout(Vio_interface::Direction::k_read, 22));
  EXPECT_CALL(*temp_connection->m_mock_vio, read(_, _));
  EXPECT_CALL(*temp_connection->m_mock_vio, set_state(_)).Times(2);
  Error_code error;
  client.read_one_message(error);
  EXPECT_CALL(*temp_connection->m_mock_vio, shutdown());
}

TEST_F(Timers_test_suite, read_one_message_default_read_timeout)
{
  EXPECT_CALL(*mock_connection->m_mock_vio, set_timeout(
      Vio_interface::Direction::k_read,
      Global_timeouts::Default::k_interactive_timeout));

  const std::size_t buff_size = 4;
  union {
    char buffer[5];
    longlong dummy;
  };
  for (int i = 1; i < 4; i++)
    buffer[i] = 0;
  buffer[0] = 1; // Paload size
  buffer[4] = 1; // Message id (CapGet)

  //Expected to be called twice - once for header and once for payload
  EXPECT_CALL(*mock_connection->m_mock_vio, read(_, _)).Times(2).
      WillOnce(DoAll(SetArrayArgument<0>(buffer, buffer + buff_size),
                     Return(buff_size))).
      WillOnce(DoAll(SetArrayArgument<0>(
                       buffer + buff_size,
                       buffer + buff_size + 1),
                     Return(1)));
  EXPECT_CALL(*mock_connection->m_mock_vio, set_state(_)).Times(2);
  EXPECT_CALL(mock_protocol_monitor, on_receive(_)).Times(2);

  std::shared_ptr<Protocol_config> conf = std::make_shared<Protocol_config>();
  EXPECT_CALL(mock_server, get_config()).WillRepeatedly(ReturnPointee(&conf));

  EXPECT_CALL(*mock_connection->m_mock_vio, set_timeout(
      Vio_interface::Direction::k_read,
      Global_timeouts::Default::k_read_timeout));

  Error_code error;
  sut.read_one_message(error);
}

TEST_F(Timers_test_suite, read_one_message_custom_read_timeout)
{
  timeouts.read_timeout = 33;
  Mock_vio *temp_vio(new Mock_vio()); //temp_connection will take ownership
  ngs::shared_ptr<StrictMock<Mock_connection>> temp_connection(
      ngs::make_shared<StrictMock<Mock_connection>>(temp_vio));
  xpl::test::Mock_ngs_client client(temp_connection, mock_server, /* id */ 1,
      mock_protocol_monitor, timeouts);

  EXPECT_CALL(*temp_connection->m_mock_vio, set_timeout(
      Vio_interface::Direction::k_read,
      Global_timeouts::Default::k_interactive_timeout));
  EXPECT_CALL(*temp_connection->m_mock_vio,
              set_timeout(Vio_interface::Direction::k_read, 33));

  const std::size_t buff_size = 4;
  union {
    char buffer[5];
    longlong dummy;
  };

  for (int i = 1; i < 5; i++)
    buffer[i] = 0;
  buffer[0] = 1; // Paload size
  buffer[4] = 1; // Message id (CapGet)

  //Expected to be called twice - once for header and once for payload
  EXPECT_CALL(*temp_connection->m_mock_vio, read(_, _)).Times(2).
      WillOnce(DoAll(SetArrayArgument<0>(buffer, buffer + buff_size),
                     Return(buff_size))).
      WillOnce(DoAll(SetArrayArgument<0>(
                       buffer + buff_size,
                       buffer + buff_size + 1),
                     Return(1)));
  EXPECT_CALL(*temp_connection->m_mock_vio, set_state(_)).Times(2);
  EXPECT_CALL(mock_protocol_monitor, on_receive(_)).Times(2);

  std::shared_ptr<Protocol_config> conf = std::make_shared<Protocol_config>();
  EXPECT_CALL(mock_server, get_config()).WillRepeatedly(ReturnPointee(&conf));

  Error_code error;
  client.read_one_message(error);
  EXPECT_CALL(*temp_connection->m_mock_vio, shutdown());
}

TEST_F(Timers_test_suite, read_one_message_failed_read)
{
  EXPECT_CALL(*mock_connection->m_mock_vio, set_timeout(
      Vio_interface::Direction::k_read,
      Global_timeouts::Default::k_interactive_timeout));

  EXPECT_CALL(*mock_connection->m_mock_vio, read(_, _)).WillRepeatedly(
      SetErrnoAndReturn(SOCKET_ETIMEDOUT, -1));
  EXPECT_CALL(*mock_connection->m_mock_vio, set_state(_)).Times(2);

  EXPECT_CALL(mock_protocol_monitor, on_receive(_)).Times(0);

  auto encoder = ngs::allocate_object<Mock_protocol_encoder>();
  EXPECT_CALL(*encoder,
              set_write_timeout(Global_timeouts::Default::k_write_timeout));

  sut.set_encoder(encoder);
#ifndef _WIN32
  EXPECT_CALL(*encoder, send_notice(Frame_type::WARNING,
        Frame_scope::GLOBAL,_,_));
#endif

  Error_code error;
  sut.read_one_message(error);
}

TEST_F(Timers_test_suite, send_message_default_write_timeout)
{
  EXPECT_CALL(*mock_connection->m_mock_vio, get_fd());
  Expectation set_timeout_exp = EXPECT_CALL(*mock_connection->m_mock_vio,
      set_timeout(Vio_interface::Direction::k_write,
                  Global_timeouts::Default::k_write_timeout));

  EXPECT_CALL(*mock_connection->m_mock_vio, write(_,_)).After(set_timeout_exp);

  auto stub_error_handler = [](int){};
  auto encoder = ngs::allocate_object<Protocol_encoder>(mock_connection,
      stub_error_handler, std::ref(mock_protocol_monitor));
  sut.set_encoder(encoder);
  encoder->send_message(Mysqlx::ServerMessages::OK, Mysqlx::Ok(), false);
}

TEST_F(Timers_test_suite, send_message_custom_write_timeout)
{
  timeouts.write_timeout = 44;
  Mock_vio *temp_vio(new Mock_vio()); //temp_connection will take ownership
  ngs::shared_ptr<StrictMock<Mock_connection>> temp_connection(
      ngs::make_shared<StrictMock<Mock_connection>>(temp_vio));
  xpl::test::Mock_ngs_client client(temp_connection, mock_server, /* id */ 1,
      mock_protocol_monitor, timeouts);

  EXPECT_CALL(*temp_connection->m_mock_vio, get_fd());
  Expectation set_timeout_exp = EXPECT_CALL(*temp_connection->m_mock_vio,
      set_timeout(Vio_interface::Direction::k_write, 44));

  EXPECT_CALL(*temp_connection->m_mock_vio, write(_,_)).After(set_timeout_exp);

  auto stub_error_handler = [](int){};
  auto encoder = ngs::allocate_object<Protocol_encoder>(temp_connection,
      stub_error_handler, std::ref(mock_protocol_monitor));
  client.set_encoder(encoder);
  encoder->send_message(Mysqlx::ServerMessages::OK, Mysqlx::Ok(), false);

  EXPECT_CALL(*temp_connection->m_mock_vio, shutdown());
}

TEST_F(Timers_test_suite, send_message_failed_write)
{
  EXPECT_CALL(*mock_connection->m_mock_vio, get_fd());
  EXPECT_CALL(*mock_connection->m_mock_vio, set_timeout(
      Vio_interface::Direction::k_write,
      Global_timeouts::Default::k_write_timeout));

  ON_CALL(*mock_connection->m_mock_vio, write(_,_)).WillByDefault(Return(-1));
  EXPECT_CALL(*mock_connection->m_mock_vio, write(_,_));

  struct CustomExpection {};
  auto stub_error_handler = [](int){ throw CustomExpection(); };
  auto encoder = ngs::allocate_object<Protocol_encoder>(mock_connection,
      stub_error_handler, std::ref(mock_protocol_monitor));
  sut.set_encoder(encoder);

  //write failed so error_handler should be used
  EXPECT_THROW(encoder->send_message(
        Mysqlx::ServerMessages::OK, Mysqlx::Ok(), false),
      CustomExpection);
}

} // namespace test

} // namespace ngs
