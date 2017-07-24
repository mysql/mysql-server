/* Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mock/session.h"
#include "ngs/server_client_timeout.h"


namespace ngs
{

namespace test
{

using namespace ::testing;
// The ngs::chrono is missing string to time_point conversion
// lets make initialize time constants in constructor
// relativly from now()
const ngs::chrono::time_point TIMEPOINT_RELEASE_ALL_BEFORE = chrono::now();
const ngs::chrono::time_point TIMEPOINT_TO_RELEASE_1 = TIMEPOINT_RELEASE_ALL_BEFORE - ngs::chrono::milliseconds(500);
const ngs::chrono::time_point TIMEPOINT_TO_RELEASE_2 = TIMEPOINT_RELEASE_ALL_BEFORE - ngs::chrono::milliseconds(1000);
const ngs::chrono::time_point TIMEPOINT_TO_RELEASE_3 = TIMEPOINT_RELEASE_ALL_BEFORE - ngs::chrono::milliseconds(2000);
const ngs::chrono::time_point TIMEPOINT_NOT_TO_RELEASE_1 = TIMEPOINT_RELEASE_ALL_BEFORE + ngs::chrono::milliseconds(2000);
const ngs::chrono::time_point TIMEPOINT_NOT_TO_RELEASE_2 = TIMEPOINT_RELEASE_ALL_BEFORE + ngs::chrono::milliseconds(1000);
const ngs::chrono::time_point TIMEPOINT_NOT_TO_RELEASE_3 = TIMEPOINT_RELEASE_ALL_BEFORE + ngs::chrono::milliseconds(500);


class ServerClientTimeoutTestSuite: public Test
{
public:

  ServerClientTimeoutTestSuite()
  : sut(new Server_client_timeout(TIMEPOINT_RELEASE_ALL_BEFORE)) {
  }


  ngs::shared_ptr<Client_interface> expectClientValid(
      const ngs::chrono::time_point &tp,
      const ngs::Client_interface::Client_state state) {
    ngs::shared_ptr<StrictMock< ::xpl::test::Mock_client > > result;

    result.reset(new StrictMock< ::xpl::test::Mock_client >());

    EXPECT_CALL(*result.get(), get_accept_time()).WillOnce(Return(tp));
    EXPECT_CALL(*result.get(), get_state()).WillOnce(Return(state));

    sut->validate_client_state(result);

    return result;
  }

  ngs::shared_ptr<Client_interface> expectClientNotValid(
      const ngs::chrono::time_point &tp,
      const ngs::Client_interface::Client_state state) {
    ngs::shared_ptr<StrictMock< ::xpl::test::Mock_client > > result;

    result.reset(new StrictMock< ::xpl::test::Mock_client >());

    EXPECT_CALL(*result.get(), get_accept_time()).WillOnce(Return(tp));
    EXPECT_CALL(*result.get(), get_state()).WillOnce(Return(state));
    EXPECT_CALL(*result.get(), on_auth_timeout_void());
    EXPECT_CALL(*result.get(), client_id());

    sut->validate_client_state(result);

    return result;
  }

  ngs::unique_ptr<Server_client_timeout> sut;
};

TEST_F(ServerClientTimeoutTestSuite, returnInvalidDate_whenNoClientWasProcessed)
{
  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

struct ClinetParams {
  ClinetParams(
      const ngs::chrono::time_point &tp,
      const ngs::Client_interface::Client_state state)
  : m_tp(tp), m_state(state) {}

  ngs::chrono::time_point m_tp;
  ngs::Client_interface::Client_state m_state;
};

class ServerClientTimeoutTestSuiteWithClientsState :
    public ServerClientTimeoutTestSuite,
    public ::testing::WithParamInterface<
      ClinetParams> { };

class ExpiredClient: public ServerClientTimeoutTestSuiteWithClientsState {};
class NoExpiredClient_stateNotOk: public ServerClientTimeoutTestSuiteWithClientsState {};
class NoExpiredClient_stateOk: public ServerClientTimeoutTestSuiteWithClientsState {};

TEST_P(ExpiredClient, returnInvalidDateNoFurtherNeedOfChecking_clientReleasedInitiated)
{
  expectClientNotValid(
      GetParam().m_tp,
      GetParam().m_state);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

TEST_P(NoExpiredClient_stateNotOk, returnClientsAcceptanceDate_thereIsANeedOfFutureChecking)
{
  const ngs::chrono::time_point clients_tp = GetParam().m_tp;
  expectClientValid(
      clients_tp,
      GetParam().m_state);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(clients_tp, sut->get_oldest_client_accept_time());
}

TEST_P(NoExpiredClient_stateOk, returnInvalidDate_clientRunsCorrectlyNoNeedOfFutureChecking)
{
  const ngs::chrono::time_point clients_tp = GetParam().m_tp;
  expectClientValid(
      clients_tp,
      GetParam().m_state);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

INSTANTIATE_TEST_CASE_P(InstantiationOfClientsThatExpiredAndAreInNotValidState,
    ExpiredClient,
        Values(
            ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_accepted),
            ClinetParams(TIMEPOINT_TO_RELEASE_2, ngs::Client_interface::Client_accepted),
            ClinetParams(TIMEPOINT_TO_RELEASE_3, ngs::Client_interface::Client_accepted),
            ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_authenticating_first),
            ClinetParams(TIMEPOINT_TO_RELEASE_2, ngs::Client_interface::Client_authenticating_first),
            ClinetParams(TIMEPOINT_TO_RELEASE_3, ngs::Client_interface::Client_authenticating_first)
            ));

INSTANTIATE_TEST_CASE_P(InstantiationOfClientsThatExpiredAndAreInNotValidState,
    NoExpiredClient_stateNotOk,
    Values(
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_accepted),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_2, ngs::Client_interface::Client_accepted),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_3, ngs::Client_interface::Client_accepted),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_authenticating_first),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_2, ngs::Client_interface::Client_authenticating_first),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_3, ngs::Client_interface::Client_authenticating_first)
        ));

INSTANTIATE_TEST_CASE_P(InstantiationOfClientsThatNoExpiredAndAreInValidState,
    NoExpiredClient_stateOk,
    Values(
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_accepted_with_session),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_running),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_closing),
        ClinetParams(TIMEPOINT_NOT_TO_RELEASE_1, ngs::Client_interface::Client_closed),
        ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_accepted_with_session),
        ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_running),
        ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_closing),
        ClinetParams(TIMEPOINT_TO_RELEASE_1, ngs::Client_interface::Client_closed)
        ));

TEST_F(ServerClientTimeoutTestSuite, returnDateOfOldestProcessedClient_whenMultipleValidNonAuthClientWereProcessed)
{
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_1, Client_interface::Client_accepted);
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_2, Client_interface::Client_accepted);
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_3, Client_interface::Client_accepted);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(TIMEPOINT_NOT_TO_RELEASE_3, sut->get_oldest_client_accept_time());
}

TEST_F(ServerClientTimeoutTestSuite, returnDateOfOldestNotExpiredNotAuthClient_whenWithMixedClientSet)
{
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_1, Client_interface::Client_accepted);
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_2, Client_interface::Client_accepted);
  expectClientValid(TIMEPOINT_NOT_TO_RELEASE_3, Client_interface::Client_accepted);
  expectClientNotValid(TIMEPOINT_TO_RELEASE_1, Client_interface::Client_accepted);

  ASSERT_TRUE(chrono::is_valid(sut->get_oldest_client_accept_time()));
  ASSERT_EQ(TIMEPOINT_NOT_TO_RELEASE_3, sut->get_oldest_client_accept_time());
}

TEST_F(ServerClientTimeoutTestSuite, returnInvalidDate_whenAllClientAreAuthenticated)
{
  expectClientValid(TIMEPOINT_TO_RELEASE_1, Client_interface::Client_running);
  expectClientValid(TIMEPOINT_TO_RELEASE_2, Client_interface::Client_closing);
  expectClientValid(TIMEPOINT_TO_RELEASE_3, Client_interface::Client_closing);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

TEST_F(ServerClientTimeoutTestSuite, returnInvalidDate_whenNoInitializedDate)
{
  ngs::chrono::time_point not_set_time_point;

  expectClientValid(not_set_time_point, Client_interface::Client_invalid);

  ASSERT_FALSE(chrono::is_valid(sut->get_oldest_client_accept_time()));
}

} // namespace test

} // namespace ngs
