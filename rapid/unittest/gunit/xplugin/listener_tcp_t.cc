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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock/ngs_general.h"

#include "io/xpl_listener_tcp.h"


namespace xpl {

namespace tests {

using namespace ::testing;

const std::string ADDRESS = "0.1.2.3";
const std::string ALL_INTERFACES_4 = "0.0.0.0";
const std::string ALL_INTERFACES_6 = "::";
const uint16 PORT = 3030;
const std::string PORT_STRING = "3030";
const uint32 PORT_TIMEOUT = 123;
const uint32 BACKLOG = 122;
const my_socket SOCKET_OK = 10;
const int POSIX_OK = 0;
const int POSIX_FAILURE = -1;

MATCHER(EqInvalidSocket, "") {
  return INVALID_SOCKET == mysql_socket_getfd(arg);
}

MATCHER_P(EqCastToCStr, expected, "") {
  std::string force_string = expected;
  return force_string == (char*)arg;
}


class Listener_tcp_testsuite : public Test {
public:
  void SetUp() {
    KEY_socket_x_tcpip = 1;

    m_mock_factory = ngs::make_shared<StrictMock<ngs::test::Mock_factory> >();
    m_mock_socket = ngs::make_shared<StrictMock<ngs::test::Mock_socket> >();
    m_mock_system = ngs::make_shared<StrictMock<ngs::test::Mock_system> >();
    m_mock_socket_invalid = ngs::make_shared<StrictMock<ngs::test::Mock_socket> >();

    ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  }

  void assert_verify_and_reinitailize_rules() {
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(m_mock_factory.get()));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(m_mock_socket_invalid.get()));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(m_mock_socket.get()));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(m_mock_system.get()));

    EXPECT_CALL(*m_mock_factory, create_system_interface()).WillRepeatedly(Return(m_mock_system));
    EXPECT_CALL(*m_mock_factory, create_socket(EqInvalidSocket())).WillRepeatedly(Return(m_mock_socket_invalid));
    EXPECT_CALL(*m_mock_socket_invalid, get_socket_fd()).WillRepeatedly(Return(INVALID_SOCKET));
    EXPECT_CALL(*m_mock_socket, get_socket_fd()).WillRepeatedly(Return(SOCKET_OK));
  }

  void make_sut(const std::string &interface, const uint32 port = PORT, const uint32 port_timeout = PORT_TIMEOUT) {
    m_resulting_bind_address = interface;
    sut = ngs::make_shared<Listener_tcp>(
        m_mock_factory,
        ngs::ref(m_resulting_bind_address),
        port,
        port_timeout,
        ngs::ref(m_mock_socket_events),
        BACKLOG);
  }

  void expect_create_socket(
      addrinfo &ai,
      const std::string &interface,
      const int family,
      const int result = SOCKET_OK) {

    make_sut(interface,
             PORT,
             PORT_TIMEOUT);

    EXPECT_CALL(*m_mock_system, getaddrinfo(
        StrEq(interface),
        StrEq(PORT_STRING),
        _,
        _)).WillOnce(DoAll(SetArgPointee<3>(&ai),Return(POSIX_OK)));

    EXPECT_CALL(*m_mock_socket, get_socket_fd())
      .WillOnce(Return(result));
    EXPECT_CALL(*m_mock_factory, create_socket(KEY_socket_x_tcpip, family, SOCK_STREAM, 0))
      .WillOnce(Return(m_mock_socket));

    #ifdef IPV6_V6ONLY
    EXPECT_CALL(*m_mock_socket, set_socket_opt(IPPROTO_IPV6, IPV6_V6ONLY, _, sizeof(int)))
      .WillRepeatedly(Return(POSIX_OK));
    #endif
  }

  void expect_listen_socket(
      ngs::shared_ptr<ngs::test::Mock_socket> mock_socket,
      addrinfo &ai,
      const bool socket_events_listen = true) {
    EXPECT_CALL(*mock_socket, set_socket_thread_owner());
    EXPECT_CALL(*mock_socket, bind(ai.ai_addr, ai.ai_addrlen))
      .WillOnce(Return(POSIX_OK));
    EXPECT_CALL(*mock_socket, listen(BACKLOG))
      .WillOnce(Return(POSIX_OK));
    ngs::Socket_interface::Shared_ptr socket_ptr = mock_socket;
    EXPECT_CALL(m_mock_socket_events, listen(socket_ptr, _))
      .WillOnce(Return(socket_events_listen));
  }


  struct addrinfo get_ai_ipv6()
  {
    struct addrinfo result;
    static struct sockaddr_in6 in6;

    in6.sin6_family = result.ai_family = AF_INET6;
    result.ai_socktype = 0;
    result.ai_protocol = 0;
    result.ai_addrlen = sizeof(in6);
    result.ai_addr = (sockaddr*)&in6;
    result.ai_next = NULL;

    return result;
  }

  struct addrinfo get_ai_ipv4()
  {
    struct addrinfo result;
    static struct sockaddr_in in4;

    in4.sin_family = result.ai_family = AF_INET;
    result.ai_socktype = 0;
    result.ai_protocol = 0;
    result.ai_addrlen = sizeof(in4);
    result.ai_addr = (sockaddr*)&in4;
    result.ai_next = NULL;

    return result;
  }
  std::string m_resulting_bind_address;

  ngs::shared_ptr<ngs::test::Mock_socket> m_mock_socket;
  ngs::shared_ptr<ngs::test::Mock_socket> m_mock_socket_invalid;
  ngs::shared_ptr<ngs::test::Mock_system> m_mock_system;
  StrictMock<ngs::test::Mock_socket_events> m_mock_socket_events;
  ngs::shared_ptr<ngs::test::Mock_factory> m_mock_factory;

  ngs::shared_ptr<Listener_tcp> sut;
};

TEST_F(Listener_tcp_testsuite, setup_listener_does_nothing_when_resolve_failes) {
  make_sut(ADDRESS);

  EXPECT_CALL(*m_mock_system, getaddrinfo(
      StrEq(ADDRESS),
      StrEq(PORT_STRING),
      _,
      _)).WillOnce(Return(POSIX_FAILURE));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

TEST_F(Listener_tcp_testsuite, setup_listener_does_resolved_IP6_and_IP4_localhost_when_asterisk_and_IP6_supported) {
  make_sut("*");

  EXPECT_CALL(*m_mock_socket, get_socket_fd()).WillOnce(Return(SOCKET_OK));
  EXPECT_CALL(*m_mock_factory, create_socket(PSI_NOT_INSTRUMENTED, AF_INET6, SOCK_STREAM, 0))
    .WillOnce(Return(m_mock_socket));

  EXPECT_CALL(*m_mock_system, getaddrinfo(
      StrEq(ALL_INTERFACES_6),
      StrEq(PORT_STRING),
      _,
      _)).WillOnce(Return(POSIX_FAILURE));

  EXPECT_CALL(*m_mock_system, getaddrinfo(
      StrEq(ALL_INTERFACES_4),
      StrEq(PORT_STRING),
      _,
      _)).WillOnce(Return(POSIX_FAILURE));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

TEST_F(Listener_tcp_testsuite, setup_listener_does_resolved_IP4_localhost_when_asterisk_and_IP6_not_supported) {
  make_sut("*");

  EXPECT_CALL(*m_mock_socket, get_socket_fd()).WillOnce(Return(INVALID_SOCKET));
  EXPECT_CALL(*m_mock_factory, create_socket(PSI_NOT_INSTRUMENTED, AF_INET6, SOCK_STREAM, 0))
    .WillOnce(Return(m_mock_socket));

  EXPECT_CALL(*m_mock_system, getaddrinfo(
      StrEq(ALL_INTERFACES_4),
      StrEq(PORT_STRING),
      _,
      _)).WillOnce(Return(POSIX_FAILURE));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

struct TimeOutAndExpectedRetries
{
  TimeOutAndExpectedRetries(const uint32 timeout, const uint32 expected_retries)
  : m_timeout(timeout),
    m_expected_retries(expected_retries){
  }

  uint32 m_timeout;
  uint32 m_expected_retries;
};

class Listener_tcp_retry_testsuite: public Listener_tcp_testsuite, public WithParamInterface<TimeOutAndExpectedRetries> {};

TEST_P(Listener_tcp_retry_testsuite, setup_listener_retry_socket_allocation_when_it_is_in_use) {
  addrinfo ai = get_ai_ipv6();

  make_sut(ALL_INTERFACES_6, PORT, GetParam().m_timeout);

  EXPECT_CALL(*m_mock_system, getaddrinfo(
      StrEq(ALL_INTERFACES_6),
      StrEq(PORT_STRING),
      _,
      _)).WillOnce(DoAll(SetArgPointee<3>(&ai),Return(POSIX_OK)));

  const int n = GetParam().m_expected_retries;

  EXPECT_CALL(*m_mock_socket, get_socket_fd())
    .Times(n).WillRepeatedly(Return(INVALID_SOCKET));
  EXPECT_CALL(*m_mock_factory, create_socket(KEY_socket_x_tcpip, AF_INET6, SOCK_STREAM, 0))
    .Times(n).WillRepeatedly(Return(m_mock_socket));
  EXPECT_CALL(*m_mock_system, get_socket_error_and_message(_,_))
    .Times(n);
  EXPECT_CALL(*m_mock_system, get_socket_errno())
    .Times(n).WillRepeatedly(Return(SOCKET_EADDRINUSE));
  EXPECT_CALL(*m_mock_system, sleep(Gt(0)))
    .Times(n);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

INSTANTIATE_TEST_CASE_P(Instantiation_tcp_retry_when_already_in_use, Listener_tcp_retry_testsuite,
                        Values(TimeOutAndExpectedRetries(0, 1),
                               TimeOutAndExpectedRetries(1, 2),
                               TimeOutAndExpectedRetries(5, 3),
                               TimeOutAndExpectedRetries(6, 3),
                               TimeOutAndExpectedRetries(7, 4),
                               TimeOutAndExpectedRetries(PORT_TIMEOUT, 10))); //123, 10

TEST_F(Listener_tcp_testsuite, setup_listener_bind_failure) {
  addrinfo ai = get_ai_ipv6();

  expect_create_socket(
      ai,
      ALL_INTERFACES_6,
      AF_INET6,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));
  EXPECT_CALL(*m_mock_socket, set_socket_thread_owner());

  EXPECT_CALL(*m_mock_socket, bind(ai.ai_addr, ai.ai_addrlen))
    .WillOnce(Return(POSIX_FAILURE));
  EXPECT_CALL(*m_mock_system, get_socket_error_and_message(_,_));
  EXPECT_CALL(*m_mock_system, get_socket_errno())
    .WillRepeatedly(Return(SOCKET_ETIMEDOUT));

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

TEST_F(Listener_tcp_testsuite, setup_listener_listen_failure) {
  addrinfo ai = get_ai_ipv6();

  expect_create_socket(
      ai,
      ALL_INTERFACES_6,
      AF_INET6,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));
  EXPECT_CALL(*m_mock_socket, set_socket_thread_owner());
  EXPECT_CALL(*m_mock_socket, bind(ai.ai_addr, ai.ai_addrlen))
    .WillOnce(Return(POSIX_OK));

  EXPECT_CALL(*m_mock_socket, listen(BACKLOG))
    .WillOnce(Return(POSIX_FAILURE));
  EXPECT_CALL(*m_mock_system, get_socket_error_and_message(_,_));
  EXPECT_CALL(*m_mock_system, get_socket_errno())
    .WillRepeatedly(Return(SOCKET_ETIMEDOUT));

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));
}

TEST_F(Listener_tcp_testsuite, setup_listener_ipv6_success) {
  addrinfo ai = get_ai_ipv6();

  expect_create_socket(
      ai,
      ALL_INTERFACES_6,
      AF_INET6,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  expect_listen_socket(m_mock_socket, ai);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*m_mock_socket, close());
}

TEST_F(Listener_tcp_testsuite, setup_listener_ipv4_success) {
  addrinfo ai = get_ai_ipv4();

  expect_create_socket(
      ai,
      ALL_INTERFACES_4,
      AF_INET,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  expect_listen_socket(m_mock_socket, ai);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*m_mock_socket, close());
}

TEST_F(Listener_tcp_testsuite, setup_listener_failure_when_socket_event_registry_failed) {
  addrinfo ai = get_ai_ipv4();

  expect_create_socket(
      ai,
      ALL_INTERFACES_4,
      AF_INET,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  const bool socket_event_listen_failed = false;
  expect_listen_socket(m_mock_socket, ai, socket_event_listen_failed);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_FALSE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_initializing));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
}

TEST_F(Listener_tcp_testsuite, setup_listener_ipv4_and_ip6_addresses_successful_is_ip4) {
  addrinfo ai4 = get_ai_ipv4();
  addrinfo ai6 = get_ai_ipv6();

  ai4.ai_next = &ai6;

  expect_create_socket(
      ai4,
      ALL_INTERFACES_4,
      AF_INET,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  expect_listen_socket(m_mock_socket, ai4);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai4));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*m_mock_socket, close());
}

TEST_F(Listener_tcp_testsuite, setup_listener_ipv4_and_ip6_addresses_successful_is_ip4_beacause_it_is_always_first_to_try) {
  addrinfo ai4 = get_ai_ipv4();
  addrinfo ai6 = get_ai_ipv6();

  ai4.ai_next = &ai6;

  expect_create_socket(
      ai4,
      ALL_INTERFACES_6,
      AF_INET,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  expect_listen_socket(m_mock_socket, ai4);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai4));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*m_mock_socket, close());
}

TEST_F(Listener_tcp_testsuite, setup_listener_ipv4_and_ip6_addresses_successful_is_ip6_at_retry) {
  addrinfo ai4 = get_ai_ipv4();
  addrinfo ai6 = get_ai_ipv6();

  ai4.ai_next = &ai6;

  expect_create_socket(
      ai4,
      ALL_INTERFACES_6,
      AF_INET,
      INVALID_SOCKET);

  ngs::shared_ptr<ngs::test::Mock_socket> mock_socket_ipv6(
      new StrictMock<ngs::test::Mock_socket>());
  EXPECT_CALL(*mock_socket_ipv6, get_socket_fd())
    .WillOnce(Return(SOCKET_OK));
  EXPECT_CALL(*m_mock_factory, create_socket(KEY_socket_x_tcpip, AF_INET6, SOCK_STREAM, 0))
    .WillOnce(Return(mock_socket_ipv6));

  #ifdef IPV6_V6ONLY
  EXPECT_CALL(*mock_socket_ipv6, set_socket_opt(IPPROTO_IPV6, IPV6_V6ONLY, _, sizeof(int)))
    .WillRepeatedly(Return(POSIX_OK));
  #endif

  EXPECT_CALL(*mock_socket_ipv6, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_OK));

  expect_listen_socket(mock_socket_ipv6, ai6);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai4));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*mock_socket_ipv6, close());
}

TEST_F(Listener_tcp_testsuite, setup_listener_success_evean_socket_opt_fails) {
  addrinfo ai = get_ai_ipv6();

  expect_create_socket(
      ai,
      ALL_INTERFACES_6,
      AF_INET6,
      SOCKET_OK);

  EXPECT_CALL(*m_mock_socket, set_socket_opt(SOL_SOCKET, SO_REUSEADDR, _, sizeof(int)))
    .WillOnce(Return(POSIX_FAILURE));
  EXPECT_CALL(*m_mock_system, get_socket_errno());

  expect_listen_socket(m_mock_socket, ai);

  EXPECT_CALL(*m_mock_system, freeaddrinfo(&ai));

  ASSERT_TRUE(sut->setup_listener(NULL));
  ASSERT_TRUE(sut->get_state().is(ngs::State_listener_prepared));

  // SUT destructor
  ASSERT_NO_FATAL_FAILURE(assert_verify_and_reinitailize_rules());
  EXPECT_CALL(*m_mock_socket, close());
}

TEST_F(Listener_tcp_testsuite, is_handled_by_socket_event_always_true) {
  make_sut(ALL_INTERFACES_6);

  ASSERT_TRUE(sut->is_handled_by_socket_event());
}

TEST_F(Listener_tcp_testsuite, get_name_and_configuration) {
  make_sut(ALL_INTERFACES_6, 2222);

  ASSERT_STREQ("TCP (bind-address:'::', port:2222)", sut->get_name_and_configuration().c_str());
}

TEST_F(Listener_tcp_testsuite, close_listener_does_nothing_when_socket_not_started) {
  make_sut(ALL_INTERFACES_6);

  sut->close_listener();

  //After stopping, start must not work !
  sut->setup_listener(NULL);
}

TEST_F(Listener_tcp_testsuite, loop_does_nothing_always) {
  make_sut(ALL_INTERFACES_6);

  sut->loop();
}

} // namespace tests

} // namespace xpl
