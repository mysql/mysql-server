/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <string>

#include <boost/scoped_ptr.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock/connection.h"

namespace ngs
{

  namespace tests {

    using ngs::test::Mock_socket_operations;
    using ngs::test::Mock_system_operations;
    using namespace ::testing;


    class Connection_vio_test : public ::testing::Test
    {
    public:
      Connection_vio_test()
      {
        m_connection_vio.reset(new Connection_vio(m_ssl_context, NULL));

        m_mock_socket_operations = new StrictMock<Mock_socket_operations>();
        m_connection_vio->set_socket_operations(m_mock_socket_operations);

        m_mock_system_operations = new StrictMock<Mock_system_operations>();
        m_connection_vio->set_system_operations(m_mock_system_operations);
      }

      ~Connection_vio_test()
      {
        m_connection_vio->set_socket_operations(NULL);
        m_connection_vio->set_system_operations(NULL);
      }

      boost::scoped_ptr<Connection_vio> m_connection_vio;
      Ssl_context m_ssl_context;
      Mock_socket_operations *m_mock_socket_operations;
      Mock_system_operations *m_mock_system_operations;

      static const unsigned short PORT = 3030;
      static const uint32 BACKLOG = 122;
      static const my_socket SOCKET_OK;
      static const int BIND_ERR = -1;
      static const int BIND_OK = 0;
      static const int LISTEN_ERR = -1;
      static const int LISTEN_OK = 0;
      static const int OPEN_ERR = -1;
      static const int OPEN_OK = 1;
      static const int READ_ERR = -1;
      static const int WRITE_ERR = -1;
      static const int UNLINK_ERR = -1;
      static const int UNLINK_OK = 0;
      static const int FSYNC_ERR = -1;
      static const int FSYNC_OK = 0;
      static const int CLOSE_ERR = -1;
      static const int CLOSE_OK = 0;
      static const int CURRENT_PID = 6;
      static const std::string UNIX_SOCKET_FILE;

      char m_buffer[8];
    };

    const std::string Connection_vio_test::UNIX_SOCKET_FILE = "/tmp/xplugin_test.sock";
    const my_socket Connection_vio_test::SOCKET_OK = 10;

    TEST_F(Connection_vio_test, accept_error)
    {
      std::string error_msg;
      my_socket sock_ok = SOCKET_OK;
      my_socket result_err = INVALID_SOCKET;
      struct sockaddr addr;
      socklen_t sock_len;
      int err = 0;

      EXPECT_CALL(*m_mock_socket_operations, accept(_, _, _)).WillOnce(Return(result_err))
                                                                .WillOnce(Return(result_err))
                                                                .WillOnce(Return(result_err));

      EXPECT_CALL(*m_mock_socket_operations, get_socket_errno()).WillOnce(Return(SOCKET_EINTR))
                                                                .WillOnce(Return(SOCKET_EAGAIN)).WillOnce(Return(SOCKET_EAGAIN))
                                                                .WillOnce(Return(-1)).WillOnce(Return(-1));

      my_socket result = Connection_vio::accept(sock_ok, &addr, sock_len, err, error_msg);

      ASSERT_EQ(INVALID_SOCKET, result);
    }

    TEST_F(Connection_vio_test, create_and_bind_socket_socket_error)
    {
      std::string error_msg;
      my_socket result_err = INVALID_SOCKET;

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_err));

      my_socket result = Connection_vio::create_and_bind_socket(PORT, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
    }

    TEST_F(Connection_vio_test, create_and_bind_socket_bind_error)
    {
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(PORT, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
    }

    TEST_F(Connection_vio_test, create_and_bind_socket_listen_error)
    {
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_OK));
      EXPECT_CALL(*m_mock_socket_operations, listen(_, BACKLOG)).WillOnce(Return(LISTEN_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(PORT, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
    }

    TEST_F(Connection_vio_test, create_and_bind_socket_ok)
    {
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_OK));
      EXPECT_CALL(*m_mock_socket_operations, listen(_, BACKLOG)).WillOnce(Return(LISTEN_OK));

      my_socket result = Connection_vio::create_and_bind_socket(PORT, error_msg, BACKLOG);

      ASSERT_EQ(SOCKET_OK, result);
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_empty_lock_filename)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result = Connection_vio::create_and_bind_socket("", error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_too_long)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;
      std::string long_filename(2000, 'a');

      my_socket result = Connection_vio::create_and_bind_socket(long_filename, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_cant_create)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(-1));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_cant_open_existing)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_ERR));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_cant_read_existing)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));
      EXPECT_CALL(*m_mock_system_operations, read(_, _, _)).WillOnce(Return(READ_ERR));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_empty)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));
      EXPECT_CALL(*m_mock_system_operations, read(_, _, _)).WillOnce(Return(0));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    ACTION_P(SetArg1ToChar, value) { *static_cast<char*>(arg1) = value; }
    ACTION_P(SetArg1ToChar2, value) { (static_cast<char*>(arg1)[1]) = value; }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_not_x_plugin)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));
      EXPECT_CALL(*m_mock_system_operations, read(_, _, _)).WillOnce(DoAll(SetArg1ToChar('Y'), Return(1))).WillOnce(Return(0));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_cant_kill)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));
      EXPECT_CALL(*m_mock_system_operations, read(_, _, _)).WillOnce(DoAll(SetArg1ToChar('X'), SetArg1ToChar2('5'), Return(2))).WillOnce(Return(0));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));
      EXPECT_CALL(*m_mock_system_operations, getppid()).WillOnce(Return(4));
      EXPECT_CALL(*m_mock_system_operations, kill(_,_)).WillOnce(Return(0));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_cant_unlink)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_ERR)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, get_errno()).WillOnce(Return(EEXIST));
      EXPECT_CALL(*m_mock_system_operations, read(_, _, _)).WillOnce(DoAll(SetArg1ToChar('X'), SetArg1ToChar2('6'), Return(2))).WillOnce(Return(0));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));
      EXPECT_CALL(*m_mock_system_operations, getppid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, unlink(_)).WillOnce(Return(UNLINK_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_write_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(WRITE_ERR));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_existing_sync_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_ERR));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_lock_filename_close_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_OK));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif
    }


    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_socket_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result_err = INVALID_SOCKET;
      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_OK));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_err));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif // defined(HAVE_SYS_UN_H)
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_bind_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;
      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_OK));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif // defined(HAVE_SYS_UN_H)
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_listen_error)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;
      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_OK));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_OK));
      EXPECT_CALL(*m_mock_socket_operations, listen(_, _)).WillOnce(Return(LISTEN_ERR));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif // defined(HAVE_SYS_UN_H)
    }

    TEST_F(Connection_vio_test, unix_socket_create_and_bind_socket_ok)
    {
#if defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result_ok = SOCKET_OK;
      snprintf(m_buffer, sizeof(m_buffer), "%c%d\n", 'X', static_cast<int>(CURRENT_PID));

      EXPECT_CALL(*m_mock_system_operations, getpid()).WillOnce(Return(CURRENT_PID));
      EXPECT_CALL(*m_mock_system_operations, open(_, _, _)).WillOnce(Return(OPEN_OK));
      EXPECT_CALL(*m_mock_system_operations, write(_, _, _)).WillOnce(Return(strlen(m_buffer)));
      EXPECT_CALL(*m_mock_system_operations, fsync(OPEN_OK)).WillOnce(Return(FSYNC_OK));
      EXPECT_CALL(*m_mock_system_operations, close(OPEN_OK)).WillOnce(Return(CLOSE_OK));

      EXPECT_CALL(*m_mock_socket_operations, socket(_, _, _)).WillOnce(Return(result_ok));
      EXPECT_CALL(*m_mock_socket_operations, bind(_, _, _)).WillOnce(Return(BIND_OK));
      EXPECT_CALL(*m_mock_socket_operations, listen(_, _)).WillOnce(Return(LISTEN_OK));

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(SOCKET_OK, result);
#endif // defined(HAVE_SYS_UN_H)
    }

    TEST_F(Connection_vio_test, unix_socket_unsupported)
    {
#if !defined(HAVE_SYS_UN_H)
      std::string error_msg;

      my_socket result = Connection_vio::create_and_bind_socket(UNIX_SOCKET_FILE, error_msg, BACKLOG);

      ASSERT_EQ(INVALID_SOCKET, result);
#endif // !defined(HAVE_SYS_UN_H)
    }

    TEST_F(Connection_vio_test, try_to_unlink_empty_string)
    {
      const std::string expected_unix_socket_file = "";

      // Does nothing
      Connection_vio::unlink_unix_socket_file(expected_unix_socket_file);
    }

    TEST_F(Connection_vio_test, try_to_unlink_when_system_interfaces_are_not_set)
    {
      const std::string expected_unix_socket_file = "existing file";

      m_connection_vio->set_system_operations(NULL);

      // Does nothing
      Connection_vio::unlink_unix_socket_file(expected_unix_socket_file);
    }

    TEST_F(Connection_vio_test, try_to_unlink_existing_unix_socket_file)
    {
      const std::string expected_unix_socket_file = "expected file";
      const std::string expected_lockfile = "expected file.lock";

      EXPECT_CALL(*m_mock_system_operations, unlink(StrEq(expected_unix_socket_file)));
      EXPECT_CALL(*m_mock_system_operations, unlink(StrEq(expected_lockfile)));

      Connection_vio::unlink_unix_socket_file(expected_unix_socket_file);
    }
  }
}
