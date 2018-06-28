/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "plugin/x/ngs/include/ngs/server_client_timeout.h"
#include "unittest/gunit/xplugin/xpl/mock/session.h"

namespace ngs {

namespace test {

using namespace ::testing;
// The ngs::chrono is missing string to time_point conversion
// lets make initialize time constants in constructor
// relatively from now()
const ngs::chrono::time_point TIMEPOINT_RELEASE_ALL_BEFORE = chrono::now();
const ngs::chrono::duration DELTA_TO_RELEASE_1 =
    ngs::chrono::milliseconds(-500);
const ngs::chrono::duration DELTA_TO_RELEASE_2 =
    ngs::chrono::milliseconds(-1000);
const ngs::chrono::duration DELTA_TO_RELEASE_3 =
    ngs::chrono::milliseconds(-2000);
const ngs::chrono::duration DELTA_NOT_TO_RELEASE_1 =
    ngs::chrono::milliseconds(2000);
const ngs::chrono::duration DELTA_NOT_TO_RELEASE_2 =
    ngs::chrono::milliseconds(1000);
const ngs::chrono::duration DELTA_NOT_TO_RELEASE_3 =
    ngs::chrono::milliseconds(500);

const ngs::chrono::time_point TP_TO_RELEASE_1 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_TO_RELEASE_1;
const ngs::chrono::time_point TP_TO_RELEASE_2 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_TO_RELEASE_2;
const ngs::chrono::time_point TP_TO_RELEASE_3 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_TO_RELEASE_3;
const ngs::chrono::time_point TP_NOT_TO_RELEASE_1 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_NOT_TO_RELEASE_1;
const ngs::chrono::time_point TP_NOT_TO_RELEASE_2 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_NOT_TO_RELEASE_2;
const ngs::chrono::time_point TP_NOT_TO_RELEASE_3 =
    TIMEPOINT_RELEASE_ALL_BEFORE + DELTA_NOT_TO_RELEASE_3;

class ServerClientTimeoutTestSuite : public Test {
 public:
  ServerClientTimeoutTestSuite()
      : sut(new Server_client_timeout(TIMEPOINT_RELEASE_ALL_BEFORE)) {}

  ngs::shared_ptr<Client_interface> expectClientValid(
      const ngs::chrono::time_point &tp,
      const ngs::Client_interface::Client_state state) {
    ngs::shared_ptr<StrictMock<::xpl::test::Mock_client>> result;

    result.reset(new StrictMock<::xpl::test::Mock_client>());

    EXPECT_CALL(*result.get(), get_accept_time()).WillOnce(Return(tp));
    EXPECT_CALL(*result.get(), get_state()).WillOnce(Return(state));

    sut->validate_client_state(result);

    return result;
  }

  ngs::shared_ptr<Client_interface> expectClientNotValid(
      const ngs::chrono::time_point &tp,
      const ngs::Client_interface::Client_state state) {
    ngs::shared_ptr<StrictMock<::xpl::test::Mock_client>> result;

    result.reset(new StrictMock<::xpl::test::Mock_client>());

    EXPECT_CALL(*result.get(), get_accept_time()).WillOnce(Return(tp));
    EXPECT_CALL(*result.get(), get_state()).WillOnce(Return(state));
    EXPECT_CALL(*result.get(), on_auth_timeout_void());
    EXPECT_CALL(*result.get(), client_id());

    sut->validate_client_state(result);

    return result;
  }

  ngs::unique_ptr<Server_client_timeout> sut;
};

TEST_F(ServerClientTimeoutTestSuite,
       returnInvalidDate_whenNoClientWasProcessed) {
  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

struct ClientParams {
  ClientParams(const ngs::chrono::duration &duration,
               const ngs::Client_interface::Client_state state)
      : m_duration(duration),
        m_tp(TIMEPOINT_RELEASE_ALL_BEFORE + duration),
        m_state(state) {}

  ngs::chrono::duration m_duration;
  ngs::chrono::time_point m_tp;
  ngs::Client_interface::Client_state m_state;
};

void PrintTo(const ClientParams &x, ::std::ostream *os) {
  *os << "{ state:" << static_cast<int>(x.m_state)
      << ", durations:" << x.m_duration.count() << " }";
}

class ServerClientTimeoutTestSuiteWithClientsState
    : public ServerClientTimeoutTestSuite,
      public ::testing::WithParamInterface<ClientParams> {};

class ExpiredClient : public ServerClientTimeoutTestSuiteWithClientsState {};
class NoExpiredClient_stateNotOk
    : public ServerClientTimeoutTestSuiteWithClientsState {};
class NoExpiredClient_stateOk
    : public ServerClientTimeoutTestSuiteWithClientsState {};

TEST_P(ExpiredClient,
       returnInvalidDateNoFurtherNeedOfChecking_clientReleasedInitiated) {
  expectClientNotValid(GetParam().m_tp, GetParam().m_state);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

TEST_P(NoExpiredClient_stateNotOk,
       returnClientsAcceptanceDate_thereIsANeedOfFutureChecking) {
  const ngs::chrono::time_point clients_tp = GetParam().m_tp;
  expectClientValid(clients_tp, GetParam().m_state);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(clients_tp, sut->get_oldest_client_accept_time());
}

TEST_P(NoExpiredClient_stateOk,
       returnInvalidDate_clientRunsCorrectlyNoNeedOfFutureChecking) {
  const ngs::chrono::time_point clients_tp = GetParam().m_tp;
  expectClientValid(clients_tp, GetParam().m_state);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

INSTANTIATE_TEST_CASE_P(
    InstantiationOfClientsThatExpiredAndAreInNotValidState, ExpiredClient,
    Values(ClientParams(DELTA_TO_RELEASE_1,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_TO_RELEASE_2,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_TO_RELEASE_3,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_TO_RELEASE_1,
                        ngs::Client_interface::Client_authenticating_first),
           ClientParams(DELTA_TO_RELEASE_2,
                        ngs::Client_interface::Client_authenticating_first),
           ClientParams(DELTA_TO_RELEASE_3,
                        ngs::Client_interface::Client_authenticating_first)));

INSTANTIATE_TEST_CASE_P(
    InstantiationOfClientsThatExpiredAndAreInNotValidState,
    NoExpiredClient_stateNotOk,
    Values(ClientParams(DELTA_NOT_TO_RELEASE_1,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_NOT_TO_RELEASE_2,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_NOT_TO_RELEASE_3,
                        ngs::Client_interface::Client_accepted),
           ClientParams(DELTA_NOT_TO_RELEASE_1,
                        ngs::Client_interface::Client_authenticating_first),
           ClientParams(DELTA_NOT_TO_RELEASE_2,
                        ngs::Client_interface::Client_authenticating_first),
           ClientParams(DELTA_NOT_TO_RELEASE_3,
                        ngs::Client_interface::Client_authenticating_first)));

INSTANTIATE_TEST_CASE_P(
    InstantiationOfClientsThatNoExpiredAndAreInValidState,
    NoExpiredClient_stateOk,
    Values(
        ClientParams(DELTA_NOT_TO_RELEASE_1,
                     ngs::Client_interface::Client_accepted_with_session),
        ClientParams(DELTA_NOT_TO_RELEASE_1,
                     ngs::Client_interface::Client_running),
        ClientParams(DELTA_NOT_TO_RELEASE_1,
                     ngs::Client_interface::Client_closing),
        ClientParams(DELTA_NOT_TO_RELEASE_1,
                     ngs::Client_interface::Client_closed),
        ClientParams(DELTA_TO_RELEASE_1,
                     ngs::Client_interface::Client_accepted_with_session),
        ClientParams(DELTA_TO_RELEASE_1, ngs::Client_interface::Client_running),
        ClientParams(DELTA_TO_RELEASE_1, ngs::Client_interface::Client_closing),
        ClientParams(DELTA_TO_RELEASE_1,
                     ngs::Client_interface::Client_closed)));

TEST_F(
    ServerClientTimeoutTestSuite,
    returnDateOfOldestProcessedClient_whenMultipleValidNonAuthClientWereProcessed) {
  expectClientValid(TP_NOT_TO_RELEASE_1, Client_interface::Client_accepted);
  expectClientValid(TP_NOT_TO_RELEASE_2, Client_interface::Client_accepted);
  expectClientValid(TP_NOT_TO_RELEASE_3, Client_interface::Client_accepted);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(TP_NOT_TO_RELEASE_3, sut->get_oldest_client_accept_time());
}

TEST_F(ServerClientTimeoutTestSuite,
       returnDateOfOldestNotExpiredNotAuthClient_whenWithMixedClientSet) {
  expectClientValid(TP_NOT_TO_RELEASE_1, Client_interface::Client_accepted);
  expectClientValid(TP_NOT_TO_RELEASE_2, Client_interface::Client_accepted);
  expectClientValid(TP_NOT_TO_RELEASE_3, Client_interface::Client_accepted);
  expectClientNotValid(TP_TO_RELEASE_1, Client_interface::Client_accepted);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(TP_NOT_TO_RELEASE_3, sut->get_oldest_client_accept_time());
}

TEST_F(ServerClientTimeoutTestSuite,
       returnInvalidDate_whenAllClientAreAuthenticated) {
  expectClientValid(TP_TO_RELEASE_1, Client_interface::Client_running);
  expectClientValid(TP_TO_RELEASE_2, Client_interface::Client_closing);
  expectClientValid(TP_TO_RELEASE_3, Client_interface::Client_closing);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

TEST_F(ServerClientTimeoutTestSuite, returnInvalidDate_whenNoInitializedDate) {
  ngs::chrono::time_point not_set_time_point;

  expectClientValid(not_set_time_point, Client_interface::Client_invalid);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

}  // namespace test

}  // namespace ngs
