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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mysql/gcs/gcs_log_system.h"
#include "gcs_xcom_interface.h"

using ::testing::Return;
using ::testing::WithArgs;
using ::testing::Invoke;
using ::testing::_;
using ::testing::Eq;
using ::testing::ContainsRegex;
using ::testing::AnyNumber;

namespace gcs_logging_unittest
{
class Mock_ext_logger : public Ext_logger_interface
{
public:
  Mock_ext_logger()
  {
    ON_CALL(*this, initialize()).WillByDefault(Return(GCS_OK));
    ON_CALL(*this, finalize()).WillByDefault(Return(GCS_OK));
  }

  ~Mock_ext_logger() {}
  MOCK_METHOD0(initialize, enum_gcs_error());
  MOCK_METHOD0(finalize, enum_gcs_error());
  MOCK_METHOD2(log_event, void(gcs_log_level_t l, const char *m));
};

class LoggingInfrastructureTest : public ::testing::Test
{
protected:
  LoggingInfrastructureTest() : logger(NULL) {};

  virtual void SetUp()
  {
    logger= new Mock_ext_logger();
  }

  virtual void TearDown()
  {
    Gcs_logger::finalize();
    delete logger;
    logger= NULL;
  }

  Mock_ext_logger *logger;
};

TEST_F(LoggingInfrastructureTest, InjectedMockLoggerTest)
{
  EXPECT_CALL(*logger, initialize()).Times(1);
  EXPECT_CALL(*logger, log_event(_,_)).Times(6);

  Gcs_logger::initialize(logger);

  // Logger 1 initialized
  ASSERT_EQ(true, Gcs_logger::get_logger() != NULL);
  ASSERT_EQ(logger, Gcs_logger::get_logger());

  // Log some messages on logger
  int l;
  for(l= GCS_FATAL; l <= GCS_TRACE; l++)
  {
    MYSQL_GCS_LOG((gcs_log_level_t) l, gcs_log_levels[l]
      << "This is a logging message with level " << l);
  }

  // Initialize new mock logger
  Mock_ext_logger *anotherLogger= new Mock_ext_logger();
  Gcs_logger::initialize(anotherLogger);

  // anotherLogger initialized
  ASSERT_EQ(true, Gcs_logger::get_logger() != NULL);
  ASSERT_EQ(anotherLogger, Gcs_logger::get_logger());

  Gcs_logger::finalize();
  delete anotherLogger;
}


class Mock_gcs_log_events_recipient : public Gcs_log_events_recipient_interface
{
public:
  Mock_gcs_log_events_recipient()
  {
    ON_CALL(*this, process(_,_)).WillByDefault(Invoke(&real_r, &Gcs_log_events_default_recipient::process));
  }

  ~Mock_gcs_log_events_recipient() {}

  MOCK_METHOD2(process, bool(gcs_log_level_t l, std::string m));

private:
  Gcs_log_events_default_recipient real_r;
};

class LoggingSystemTest : public ::testing::Test
{
protected:
  LoggingSystemTest() : logger(NULL), r(NULL) {};

  virtual void SetUp()
  {
    r= new Mock_gcs_log_events_recipient();
    logger= new Gcs_ext_logger_impl(r);
  }

  virtual void TearDown()
  {
    Gcs_logger::finalize();

    delete logger;
    logger= NULL;

    delete r;
    r= NULL;
  }

  Gcs_ext_logger_impl *logger;
  Mock_gcs_log_events_recipient *r;
};


TEST_F(LoggingSystemTest, DefaultLifecycle)
{
  int times= 7;
#ifdef WITH_LOG_DEBUG
  times= 14;
#endif
#ifdef WITH_LOG_TRACE
  times= 21;
#endif


#if defined(WIN32) || defined(WIN64)
  times++;
#endif

  EXPECT_CALL(*r, process(_,_)).Times(times);

  // on some machines an info message will be displayed stating
  // that a network interface was not successfully probed
  // we cannot predict how many network interfaces are in the
  // machine that cannot be probed
  EXPECT_CALL(*r,
    process(GCS_INFO,
            ContainsRegex("Unable to probe network interface .*"))).
    Times(AnyNumber());

  ASSERT_EQ(true, Gcs_logger::get_logger() == NULL);

  Gcs_logger::initialize(logger);

  Gcs_group_identifier *group_id= new Gcs_group_identifier("only_group");
  Gcs_interface_parameters if_params;

  if_params.add_parameter("group_name", group_id->get_group_id());
  if_params.add_parameter("peer_nodes", "127.0.0.1:12345");
  if_params.add_parameter("local_node", "127.0.0.1:12345");
  if_params.add_parameter("bootstrap_group", "true");
  if_params.add_parameter("poll_spin_loops", "100");

  // just to make the log entries count below deterministic, otherwise,
  // there would be additional info messages due to automatically adding
  // addresses to the whitelist
  if_params.add_parameter("ip_whitelist", Gcs_ip_whitelist::DEFAULT_WHITELIST);

  Gcs_interface *xcom_if= Gcs_xcom_interface::get_interface();
  enum_gcs_error initialized= xcom_if->initialize(if_params);

  ASSERT_EQ(GCS_OK, initialized);

  std::cout << "Interface initialization should have inserted 6 logging events."
    << std::endl;

  ASSERT_EQ(true, Gcs_logger::get_logger() != NULL);

  gcs_log_level_t level;

  for(int i= 0; i < 6; i++)
  {
    level= (gcs_log_level_t) i;
    std::string msg("This message belongs to logging level ");
    msg += gcs_log_levels[level];

    const char *c_msg= msg.c_str();

    Gcs_logger::get_logger()->log_event(level, c_msg);
  }

  std::cout << "Inserted all 6 user logging events. Finalizing logger..."
    << std::endl;

  enum_gcs_error finalize_error= xcom_if->finalize();

  ASSERT_EQ(GCS_OK, finalize_error);

  Gcs_xcom_interface::cleanup();

  delete group_id;

  ASSERT_EQ(true, Gcs_logger::get_logger() == NULL);
}

}
