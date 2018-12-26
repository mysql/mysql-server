
/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "ngs/capabilities/handler_tls.h"
#include "ngs/capabilities/handler_auth_mech.h"
#include "mock/session.h"
#include "mock/capabilities.h"
#include "mock/ngs_general.h"


namespace ngs
{

namespace test
{


using namespace ::testing;
using ::Mysqlx::Datatypes::Any;
using ::Mysqlx::Datatypes::Scalar;

class No_delete
{
public:
  template <typename T>
  void operator() (T *memory)
  {
  }
};

class CapabilityHanderTlsTestSuite : public Test
{
public:
  CapabilityHanderTlsTestSuite()
  : sut(mock_client)
  {
    mock_options.reset(new StrictMock<Mock_options_session>());
    EXPECT_CALL(mock_client, connection()).WillRepeatedly(ReturnRef(mock_connection));
    EXPECT_CALL(mock_connection, options()).WillRepeatedly(Return(IOptions_session_ptr(mock_options.get(), No_delete())));
  }

  StrictMock<Mock_connection>        mock_connection;
  ngs::shared_ptr<Mock_options_session>    mock_options;
  StrictMock<xpl::test::Mock_client> mock_client;

  Capability_tls                     sut;
};


TEST_F(CapabilityHanderTlsTestSuite, isSupported_returnsCurrentConnectionOption_on_supported_connection_type)
{
  EXPECT_CALL(*mock_options, supports_tls()).WillOnce(Return(true)).WillOnce(Return(false));
  EXPECT_CALL(mock_connection, connection_type()).WillOnce(Return(Connection_tcpip)).WillOnce(Return(Connection_tcpip));

  ASSERT_TRUE(sut.is_supported());
  ASSERT_FALSE(sut.is_supported());
}

TEST_F(CapabilityHanderTlsTestSuite, isSupported_returnsFailure_on_unsupported_connection_type)
{
  EXPECT_CALL(*mock_options, supports_tls()).WillOnce(Return(true)).WillOnce(Return(false));
  EXPECT_CALL(mock_connection, connection_type()).WillOnce(Return(Connection_namedpipe)).WillOnce(Return(Connection_namedpipe));

  ASSERT_FALSE(sut.is_supported());
  ASSERT_FALSE(sut.is_supported());
}


TEST_F(CapabilityHanderTlsTestSuite, name_returnsTls_always)
{
  ASSERT_STREQ("tls", sut.name().c_str());
}


TEST_F(CapabilityHanderTlsTestSuite, get_returnsCurrentConnectionOption_always)
{
  const bool  expected_result = true;
  Any         any;

  EXPECT_CALL(*mock_options, active_tls()).WillOnce(Return(expected_result));

  sut.get(any);

  ASSERT_EQ(Any::SCALAR,     any.type());
  ASSERT_EQ(Scalar::V_BOOL,  any.scalar().type());
  ASSERT_EQ(expected_result, any.scalar().v_bool());
}


class Set_params
{
public:
  Set_params(bool any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_BOOL);
    m_any.mutable_scalar()->set_v_bool(any);

    m_tls_active = tls;
  }

  Set_params(int any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_SINT);
    m_any.mutable_scalar()->set_v_signed_int(any);

    m_tls_active = tls;
  }

  Set_params(unsigned int any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_UINT);
    m_any.mutable_scalar()->set_v_unsigned_int(any);

    m_tls_active = tls;
  }

  Set_params(float any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_FLOAT);
    m_any.mutable_scalar()->set_v_float(any);

    m_tls_active = tls;
  }

  Set_params(double any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_DOUBLE);
    m_any.mutable_scalar()->set_v_double(any);

    m_tls_active = tls;
  }

  Set_params(const char *any, bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_STRING);
    m_any.mutable_scalar()->mutable_v_string()->set_value(any);

    m_tls_active = tls;
  }

  Set_params(bool tls): m_tls_active(tls)
  {
    m_any.mutable_scalar()->set_type(Scalar::V_NULL);

    m_tls_active = tls;
  }

  Set_params(const Set_params &other)
  : m_any(other.m_any), m_tls_active(other.m_tls_active)
  {}

  Any  m_any;
  bool m_tls_active;
};

::std::ostream& operator<<(::std::ostream& os, const Set_params& set_param)
{
  return os << "tls-active:" << set_param.m_tls_active << std::endl;
}



class SuccessSetCapabilityHanderTlsTestSuite : public CapabilityHanderTlsTestSuite , public WithParamInterface<Set_params>
{
public:
};


TEST_P(SuccessSetCapabilityHanderTlsTestSuite, get_success_forValidParametersAndTlsSupportedOnTcpip)
{
  Set_params s = GetParam();

  EXPECT_CALL(*mock_options, active_tls()).WillOnce(Return(s.m_tls_active));
  EXPECT_CALL(*mock_options, supports_tls()).WillOnce(Return(true));
  EXPECT_CALL(mock_connection, connection_type()).WillOnce(Return(Connection_tcpip));

  ASSERT_TRUE(sut.set(s.m_any));

  EXPECT_CALL(mock_client, activate_tls_void());

  sut.commit();
}

TEST_P(SuccessSetCapabilityHanderTlsTestSuite, get_failure_forValidParametersAndTlsSupportedOnNamedPipe)
{
  Set_params s = GetParam();

  EXPECT_CALL(*mock_options, active_tls()).WillOnce(Return(s.m_tls_active));
  EXPECT_CALL(*mock_options, supports_tls()).WillOnce(Return(true));
  EXPECT_CALL(mock_connection, connection_type()).WillOnce(Return(Connection_namedpipe));

  ASSERT_FALSE(sut.set(s.m_any));
}

TEST_P(SuccessSetCapabilityHanderTlsTestSuite, get_failure_forValidParametersAndTlsIsntSupported)
{
  Set_params s = GetParam();

  EXPECT_CALL(*mock_options, active_tls()).WillOnce(Return(s.m_tls_active));
  EXPECT_CALL(*mock_options, supports_tls()).WillOnce(Return(false));
  EXPECT_CALL(mock_connection, connection_type()).WillOnce(Return(Connection_tcpip));

  ASSERT_FALSE(sut.set(s.m_any));
}


INSTANTIATE_TEST_CASE_P(SuccessInstantiation, SuccessSetCapabilityHanderTlsTestSuite,
                        ::testing::Values(Set_params(true, false),
                                          Set_params(1,    false),
                                          Set_params(2,    false),
                                          Set_params(3u,   false),
                                          Set_params(1.0,  false)));


class FaildSetCapabilityHanderTlsTestSuite : public SuccessSetCapabilityHanderTlsTestSuite {};


TEST_P(FaildSetCapabilityHanderTlsTestSuite, get_failure_forValidParameters)
{
  Set_params s = GetParam();

  EXPECT_CALL(*mock_options, active_tls()).WillOnce(Return(s.m_tls_active));

  ASSERT_FALSE(sut.set(s.m_any));

  sut.commit();
}


INSTANTIATE_TEST_CASE_P(FaildInstantiationAlreadySet, FaildSetCapabilityHanderTlsTestSuite,
                        ::testing::Values(Set_params(true, true),
                                          Set_params(1,    true),
                                          Set_params(2,    true),
                                          Set_params(3u,   true),
                                          Set_params(1.0,  true)));


INSTANTIATE_TEST_CASE_P(FaildInstantiationCantDisable, FaildSetCapabilityHanderTlsTestSuite,
                        ::testing::Values(Set_params(false, true),
                                          Set_params(0,     true),
                                          Set_params(0u,    true),
                                          Set_params(0.0,   true)));


INSTANTIATE_TEST_CASE_P(FaildInstantiationAlreadyDisabled, FaildSetCapabilityHanderTlsTestSuite,
                        ::testing::Values(Set_params(0,     false),
                                          Set_params(false, false)));


class CapabilityHanderAuthMechTestSuite : public Test
{
public:
  CapabilityHanderAuthMechTestSuite()
  : sut(mock_client)
  {
    mock_server = ngs::make_shared< StrictMock<Mock_server> >();

    EXPECT_CALL(mock_client, connection()).WillRepeatedly(ReturnRef(mock_connection));
    EXPECT_CALL(mock_client, server()).WillRepeatedly(ReturnRef(*mock_server));
  }

  ngs::shared_ptr<StrictMock<Mock_server> > mock_server;

  StrictMock<Mock_connection>        mock_connection;
  StrictMock<xpl::test::Mock_client> mock_client;

  Capability_auth_mech sut;
};


TEST_F(CapabilityHanderAuthMechTestSuite, isSupported_returnsTrue_always)
{
  ASSERT_TRUE(sut.is_supported());
}


TEST_F(CapabilityHanderAuthMechTestSuite, set_returnsFalse_always)
{
  Set_params set(1, false);

  ASSERT_FALSE(sut.set(set.m_any));
}


TEST_F(CapabilityHanderAuthMechTestSuite, commit_doesNothing_always)
{
  sut.commit();
}


TEST_F(CapabilityHanderAuthMechTestSuite, name)
{
  ASSERT_STREQ("authentication.mechanisms", sut.name().c_str());
}


TEST_F(CapabilityHanderAuthMechTestSuite, get_doesNothing_whenEmptySetReceive)
{
  std::vector<std::string> names;
  Any any;

  //EXPECT_CALL(mock_connection, is_option_set(Connection::Option_active_tls)).WillOnce(Return(true));
  EXPECT_CALL(*mock_server, get_authentication_mechanisms_void(_, Ref(mock_client))).WillOnce(DoAll(SetArgReferee<0>(names),Return(true)));

  sut.get(any);

  ASSERT_EQ(Any::ARRAY, any.type());
  EXPECT_EQ(0, any.array().value_size());
}


TEST_F(CapabilityHanderAuthMechTestSuite, get_returnAuthMethodsFromServer_always)
{
  std::vector<std::string> names;
  Any any;

  names.push_back("first");
  names.push_back("second");

  EXPECT_CALL(*mock_server, get_authentication_mechanisms_void(_, Ref(mock_client))).WillOnce(DoAll(SetArgReferee<0>(names),Return(true)));

  sut.get(any);

  ASSERT_EQ(Any::ARRAY, any.type());
  ASSERT_EQ(static_cast<int>(names.size()), any.array().value_size());

  for(std::size_t i = 0; i < names.size(); ++i)
  {
    const Any &a = any.array().value(static_cast<int>(i));

    ASSERT_EQ(Any::SCALAR, a.type());
    ASSERT_EQ(Scalar::V_STRING, a.scalar().type());
    ASSERT_STREQ(names[i].c_str(), a.scalar().v_string().value().c_str());
  }
}


} // namespace test

} // namespace ngs
