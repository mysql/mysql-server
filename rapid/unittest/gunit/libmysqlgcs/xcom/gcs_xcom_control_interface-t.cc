/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_control_interface.h"
#include "gcs_xcom_state_exchange.h"
#include "gcs_xcom_utils.h"
#include "gcs_xcom_notification.h"
#include "synode_no.h"

#include "xcom_vp.h"
#include "node_set.h"

using ::testing::Return;
using ::testing::WithArgs;
using ::testing::Invoke;
using ::testing::_;
using ::testing::Eq;
using ::testing::DoAll;
using ::testing::ByRef;
using ::testing::SetArgReferee;

using std::string;

namespace gcs_xcom_control_unittest
{
typedef enum
{
  JJ= 0,
  LL= 1,
  JL= 2,
  LJ= 3
} InvocationOrder;


class InvocationHelper
{
public:
  InvocationHelper(Gcs_xcom_control *x, InvocationOrder o)
    : xcom_control_if(x), mutex(), counter(0), order(o),
      count_fail(0), count_success(0)
  {
    mutex.init(NULL);
  }

  ~InvocationHelper()
  {
    mutex.destroy();
  }

  void invokeMethod()
  {
    int mycounter= 0;
    enum_gcs_error ret= GCS_OK;

    if(order == JJ)
    {
      ret= xcom_control_if->join();
    }
    else if(order == LL)
    {
      ret= xcom_control_if->leave();
    }
    else if(order == JL)
    {
      mutex.lock();
      mycounter= counter++;
      if(mycounter == 0)
      {
        ret= xcom_control_if->join();
      }
      else
      {
        ret= xcom_control_if->leave();
      }
      mutex.unlock();
    }
    else if(order == LJ)
    {
      mutex.lock();
      mycounter= counter++;
      if(mycounter == 0)
      {
        ret= xcom_control_if->leave();
      }
      else
      {
        ret= xcom_control_if->join();
      }
      mutex.unlock();
    }
    else
    {
      assert(false);
    }

    mutex.lock();
    if (ret == GCS_OK)
    {
      count_success++;
    }
    else
    {
      count_fail++;
    }
    mutex.unlock();

  }

private:
  Gcs_xcom_control *xcom_control_if;
  My_xp_mutex_impl mutex;
  int counter;
  InvocationOrder order;

public:
  int count_fail;
  int count_success;
};

void
homemade_free_site_def(unsigned int n, site_def *s, node_address *node_addrs)
{
  // TODO: replace the following with free_site_def(site_config) once
  //       the header file in site_def.h is fixed
  for (unsigned int i= 0; i < n; i++)
    free(node_addrs[i].uuid.data.data_val);
  free_node_set(&s->global_node_set);
  free_node_set(&s->local_node_set);
  remove_node_list(n, node_addrs, &s->nodes);
  free(s->nodes.node_list_val);
  free(s);
}


class mock_gcs_xcom_view_change_control_interface :
                          public Gcs_xcom_view_change_control_interface
{
private:
  Gcs_view* m_current_view;
  bool m_belongs_to_group;
  My_xp_mutex_impl m_mutex_current_view;
  My_xp_mutex_impl m_joining_leaving_mutex;
  bool m_joining;
  bool m_leaving;

public:
  mock_gcs_xcom_view_change_control_interface()
  : m_current_view(NULL), m_belongs_to_group(false),
    m_mutex_current_view(), m_joining_leaving_mutex(),
    m_joining(false), m_leaving(false)

  {
    m_mutex_current_view.init(NULL);
    m_joining_leaving_mutex.init(NULL);
  }

  virtual ~mock_gcs_xcom_view_change_control_interface()
  {
    m_mutex_current_view.destroy();
    m_joining_leaving_mutex.destroy();
  }

  MOCK_METHOD0(start_view_exchange, void());
  MOCK_METHOD0(end_view_exchange, void());
  MOCK_METHOD0(wait_for_view_change_end, void());
  MOCK_METHOD0(is_view_changing, bool());

  bool start_leave()
  {
    bool retval= false;

    m_joining_leaving_mutex.lock();
    retval= m_joining || m_leaving;
    if(!retval)
      m_leaving= true;
    m_joining_leaving_mutex.unlock();

    return !retval;
  }

  void end_leave()
  {
    m_joining_leaving_mutex.lock();
    m_leaving= false;
    m_joining_leaving_mutex.unlock();
  }

  bool is_leaving()
  {
    bool retval;

    m_joining_leaving_mutex.lock();
    retval= m_leaving;
    m_joining_leaving_mutex.unlock();

    return retval;
  }

  bool start_join()
  {
    bool retval= false;

    m_joining_leaving_mutex.lock();
    retval= m_joining || m_leaving;
    if(!retval)
      m_joining= true;
    m_joining_leaving_mutex.unlock();

    return !retval;
  }

  void end_join()
  {
    m_joining_leaving_mutex.lock();
    m_joining= false;
    m_joining_leaving_mutex.unlock();
  }

  bool is_joining()
  {
    bool retval;

    m_joining_leaving_mutex.lock();
    retval= m_joining;
    m_joining_leaving_mutex.unlock();

    return retval;
  }

  void set_current_view(Gcs_view *view)
  {
    m_mutex_current_view.lock();
    delete m_current_view;
    m_current_view= view;
    m_mutex_current_view.unlock();
  }

  void set_unsafe_current_view(Gcs_view *view)
  {
    set_current_view(view);
  }

  Gcs_view *get_current_view()
  {
    Gcs_view *view= NULL;

    m_mutex_current_view.lock();
    if (m_current_view != NULL)
      view= new Gcs_view(*m_current_view);
    m_mutex_current_view.unlock();

    return view;
  }

  Gcs_view *get_unsafe_current_view()
  {
    return m_current_view;
  }

  bool belongs_to_group()
  {
    return m_belongs_to_group;
  }

  void set_belongs_to_group(bool belong)
  {
    m_belongs_to_group= belong;
  }
};


// This typedef is needed because GMock can't deal with multiple template args.
typedef std::map<Gcs_member_identifier, Xcom_member_state *> Stored_States;

class mock_gcs_xcom_state_exchange_interface :
                                     public Gcs_xcom_state_exchange_interface
{
private:
  int m_process_member_state_iteration;

  bool free_xcom_member_state(Xcom_member_state *m)
  {
    delete m;
    if (m_process_member_state_iteration==0)
    {
      m_process_member_state_iteration++;
      return false;
    }
    else
      return true;
  }

  bool free_members_joined(std::vector<Gcs_member_identifier *> &total,
                           std::vector<Gcs_member_identifier *> &joined)
  {
    std::vector<Gcs_member_identifier *>::iterator it;

    for (it= total.begin(); it != total.end(); it ++)
      delete (*it);
    total.clear();

    for (it= joined.begin(); it != joined.end(); it ++)
      delete (*it);
    joined.clear();

    return false;
  }

public:
  mock_gcs_xcom_state_exchange_interface() : m_process_member_state_iteration(0)
  {
    ON_CALL(*this, process_member_state(_,_)).WillByDefault(
            WithArgs<0>(Invoke(
              this,
              &mock_gcs_xcom_state_exchange_interface::free_xcom_member_state)));

    ON_CALL(*this, state_exchange(_,_,_,_,_,_,_,_)).WillByDefault(
            WithArgs<1,3>(Invoke(
              this,
              &mock_gcs_xcom_state_exchange_interface::free_members_joined)));
  }

  MOCK_METHOD0(init, void());
  MOCK_METHOD0(reset, void());
  MOCK_METHOD0(reset_with_flush, void());
  MOCK_METHOD0(end, void());

  MOCK_METHOD8(state_exchange, bool(synode_no configuration_id,
                                    std::vector<Gcs_member_identifier *> &total,
                                    std::vector<Gcs_member_identifier *> &left,
                                    std::vector<Gcs_member_identifier *> &joined,
                                    std::vector<Gcs_message_data *> &exchangeable_data,
                                    Gcs_view *current_view,
                                    std::string *group,
                                    Gcs_member_identifier *local_info));
  MOCK_METHOD2(process_member_state, bool(Xcom_member_state *ms_info,
                                          const Gcs_member_identifier &p_id));
  MOCK_METHOD0(get_new_view_id, Gcs_xcom_view_identifier *());
  MOCK_METHOD0(get_joined, std::set<Gcs_member_identifier *> *());
  MOCK_METHOD0(get_left, std::set<Gcs_member_identifier *> *());
  MOCK_METHOD0(get_total, std::set<Gcs_member_identifier *> *());
  MOCK_METHOD0(get_group, string *());
  MOCK_METHOD0(get_member_states, Stored_States *());
};


class mock_gcs_xcom_proxy : public Gcs_xcom_proxy
{
private:
  node_list nl;

  void reset_me()
  {
    ::delete_node_address(nl.node_list_len, nl.node_list_val);
  }


public:
  mock_gcs_xcom_proxy()
  {
    ON_CALL(*this, xcom_open_handlers(_,_)).WillByDefault(Return(false));
    ON_CALL(*this, xcom_init(_)).WillByDefault(Return(0));
    ON_CALL(*this, xcom_exit(_)).WillByDefault(Return(0));
    ON_CALL(*this, xcom_close_handlers()).WillByDefault(Return(false));
    ON_CALL(*this, xcom_client_boot(_,_)).WillByDefault(Return(1));
    ON_CALL(*this, xcom_client_add_node(_,_,_)).WillByDefault(Return(false));
    ON_CALL(*this, xcom_client_send_data(_,_)).WillByDefault(Return(10));
    ON_CALL(*this, new_node_address_uuid(_,_,_)).WillByDefault(
            WithArgs<0,1,2>(Invoke(::new_node_address_uuid)));
    ON_CALL(*this, delete_node_address(_,_)).WillByDefault(
            WithArgs<0,1>(Invoke(::delete_node_address)));
    ON_CALL(*this, xcom_wait_ready()).WillByDefault(Return(GCS_OK));
    ON_CALL(*this, xcom_wait_for_xcom_comms_status_change(_)).WillByDefault(
            SetArgReferee<0>(XCOM_COMMS_OK));
    ON_CALL(*this, xcom_wait_exit()).WillByDefault(Return(GCS_OK));
  }

  MOCK_METHOD3(new_node_address_uuid, node_address *(unsigned int n, char *names[], blob uuids[]));
  MOCK_METHOD2(delete_node_address, void(unsigned int n, node_address *na));
  MOCK_METHOD3(xcom_client_add_node,
               int(connection_descriptor* con, node_list *nl, uint32_t group_id));
  MOCK_METHOD3(xcom_client_remove_node,
               int(connection_descriptor* con, node_list* nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_remove_node,
               int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_boot, int(node_list *nl, uint32_t group_id));
  MOCK_METHOD2(xcom_client_open_connection, connection_descriptor* (std::string, xcom_port port));
  MOCK_METHOD1(xcom_client_close_connection, int(connection_descriptor* con));
  MOCK_METHOD2(xcom_client_send_data, int(unsigned long long size, char *data));
  MOCK_METHOD1(xcom_init, int(xcom_port listen_port));
  MOCK_METHOD1(xcom_exit, int(bool xcom_handlers_open));
  MOCK_METHOD0(xcom_set_cleanup, void());
  MOCK_METHOD1(xcom_get_ssl_mode, int(const char* mode));
  MOCK_METHOD1(xcom_set_ssl_mode, int(int mode));
  MOCK_METHOD0(xcom_init_ssl, int());
  MOCK_METHOD0(xcom_destroy_ssl, void());
  MOCK_METHOD0(xcom_use_ssl, int());
  MOCK_METHOD10(xcom_set_ssl_parameters,
                    void(const char *server_key_file, const char *server_cert_file,
                    const char *client_key_file, const char *client_cert_file,
                    const char *ca_file, const char *ca_path,
                    const char *crl_file, const char *crl_path,
                    const char *cipher, const char *tls_version));
  MOCK_METHOD1(find_site_def, site_def const *(synode_no synode));
  MOCK_METHOD2(xcom_open_handlers, bool(std::string saddr, xcom_port port));
  MOCK_METHOD0(xcom_close_handlers, bool());
  MOCK_METHOD0(xcom_acquire_handler, int());
  MOCK_METHOD1(xcom_release_handler, void(int index));
  MOCK_METHOD0(xcom_wait_ready, enum_gcs_error());
  MOCK_METHOD0(xcom_is_ready, bool());
  MOCK_METHOD1(xcom_set_ready, void(bool value));
  MOCK_METHOD0(xcom_signal_ready, void());
  MOCK_METHOD1(xcom_wait_for_xcom_comms_status_change,
      void(int& status));
  MOCK_METHOD0(xcom_has_comms_status_changed,
      bool());
  MOCK_METHOD1(xcom_set_comms_status,
      void(int status));
  MOCK_METHOD1(xcom_signal_comms_status_changed,
      void(int status));
  MOCK_METHOD0(xcom_wait_exit, enum_gcs_error());
  MOCK_METHOD0(xcom_is_exit, bool());
  MOCK_METHOD1(xcom_set_exit, void(bool));
  MOCK_METHOD0(xcom_signal_exit, void());
  MOCK_METHOD3(xcom_client_force_config, int(connection_descriptor *fd, node_list *nl,
                                             uint32_t group_id));
  MOCK_METHOD2(xcom_client_force_config, int(node_list *nl,
                                             uint32_t group_id));
};


class mock_gcs_control_event_listener : public Gcs_control_event_listener
{
public:
  MOCK_CONST_METHOD2(on_view_changed,
                     void(const Gcs_view &new_view,
                          const Exchanged_data &exchanged_data));
  MOCK_CONST_METHOD0(get_exchangeable_data, Gcs_message_data *());
  MOCK_CONST_METHOD2(on_suspicions,
                     void(const std::vector<Gcs_member_identifier>& members,
                          const std::vector<Gcs_member_identifier>& unreachable));
};

class mock_my_xp_socket_util : public My_xp_socket_util
{
public:
  mock_my_xp_socket_util()
  {
    ON_CALL(*this, disable_nagle_in_socket(_)).WillByDefault(Return(0));
  }

  MOCK_METHOD1(disable_nagle_in_socket, int(int fd));
};

class mock_gcs_xcom_control : public Gcs_xcom_control
{
public:
  mock_gcs_xcom_control(
    Gcs_xcom_group_member_information *group_member_information,
    std::vector<Gcs_xcom_group_member_information *> &xcom_peers,
    Gcs_group_identifier group_identifier,
    Gcs_xcom_proxy *xcom_proxy,
    Gcs_xcom_engine *gcs_engine,
    Gcs_xcom_state_exchange_interface *state_exchange,
    Gcs_xcom_view_change_control_interface *view_control,
    bool boot,
    My_xp_socket_util *socket_util):
  Gcs_xcom_control(
    group_member_information,
    xcom_peers,
    group_identifier,
    xcom_proxy,
    gcs_engine,
    state_exchange,
    view_control,
    boot,
    socket_util)
  {
  }

  enum_gcs_error join()
  {
    return join(NULL);
  }

  enum_gcs_error join(Gcs_view *view)
  {
    enum_gcs_error ret= GCS_NOK;

    if(!m_view_control->start_join())
    {
      return GCS_NOK;
    }

    if (belongs_to_group())
    {
      m_view_control->end_join();
      return GCS_NOK;
    }

    if (!m_boot && m_initial_peers.empty())
    {
      m_view_control->end_join();
      return GCS_NOK;
    }

    ret= do_join(false);

    if (ret == GCS_OK)
    {
      m_view_control->set_current_view(view);
      m_view_control->set_belongs_to_group(true);
    }

    return ret;
  }

  enum_gcs_error leave()
  {
    enum_gcs_error ret= GCS_NOK;

    if(!m_view_control->start_leave())
    {
      return GCS_NOK;
    }

    if (!belongs_to_group())
    {
      m_view_control->end_leave();
      return GCS_NOK;
    }

    ret= do_leave();

    if (ret == GCS_OK)
    {
      m_view_control->set_current_view(NULL);
      m_view_control->set_belongs_to_group(false);
    }

    return ret;
  }

  void set_xcom_running(bool running)
  {
    m_xcom_running= running;
  }
};

static mock_gcs_xcom_control *extern_xcom_control_if;

void finalize_xcom()
{
   extern_xcom_control_if->do_leave();
}

class XComControlTest : public ::testing::Test
{
protected:
  XComControlTest() {};


  virtual void SetUp()
  {
    m_wait_called= false;
    m_wait_called_mutex.init(NULL);
    m_wait_called_cond.init();

    mock_se=  new mock_gcs_xcom_state_exchange_interface();

    mock_vce= new mock_gcs_xcom_view_change_control_interface();

    group_member_information=
      new Gcs_xcom_group_member_information("127.0.0.1:12345");
    peers.push_back(new Gcs_xcom_group_member_information("127.0.0.1:12345"));
    peers.push_back(new Gcs_xcom_group_member_information("127.0.0.1:12346"));
    peers.push_back(new Gcs_xcom_group_member_information("127.0.0.1:12347"));

    string group_name("only_group");
    group_id= new Gcs_group_identifier(group_name);

    mock_socket_util= new mock_my_xp_socket_util();

    gcs_engine.initialize(NULL);

    xcom_control_if= new mock_gcs_xcom_control(group_member_information,
                                               peers,
                                               *group_id,
                                               &proxy,
                                               &gcs_engine,
                                               mock_se,
                                               mock_vce,
                                               true,
                                               mock_socket_util);
    extern_xcom_control_if= xcom_control_if;

    My_xp_util::init_time();

    logger= new Gcs_simple_ext_logger_impl();
    Gcs_logger::initialize(logger);
  }


  virtual void TearDown()
  {
    gcs_engine.finalize(finalize_xcom);

    delete mock_socket_util;
    delete group_id;
    delete xcom_control_if;
    Gcs_logger::finalize();
    delete logger;
    delete mock_se;
    delete mock_vce;
    delete group_member_information;
    std::vector<Gcs_xcom_group_member_information *>::iterator it;
    for (it= peers.begin(); it != peers.end(); ++it)
      delete (*it);
    peers.clear();

    m_wait_called_mutex.destroy();
    m_wait_called_cond.destroy();
  }

  Gcs_xcom_group_member_information *group_member_information;
  std::vector<Gcs_xcom_group_member_information *> peers;

  Gcs_group_identifier *group_id;

  mock_gcs_xcom_proxy proxy;
  mock_gcs_control_event_listener mock_ev_listener;

  mock_gcs_xcom_control *xcom_control_if;
  mock_gcs_xcom_state_exchange_interface *mock_se;
  mock_gcs_xcom_view_change_control_interface *mock_vce;
  mock_my_xp_socket_util *mock_socket_util;
  Ext_logger_interface *logger;

  bool m_wait_called;
  My_xp_mutex_impl m_wait_called_mutex;
  My_xp_cond_impl m_wait_called_cond;

  Gcs_xcom_engine gcs_engine;

public:
  void notify_sync_point()
  {
    m_wait_called_mutex.lock();
    m_wait_called= true;
    m_wait_called_cond.broadcast();
    m_wait_called_mutex.unlock();
  }

  void wait_for_sync_point()
  {
    m_wait_called_mutex.lock();
    while(!m_wait_called)
      m_wait_called_cond.wait(m_wait_called_mutex.get_native_mutex());
    m_wait_called_mutex.unlock();
  }

  Gcs_view *create_fake_view()
  {
    string address= group_member_information->get_member_address();
    Gcs_member_identifier local_member_information(address);

    std::vector<Gcs_member_identifier> members;
    members.push_back(local_member_information);

    Gcs_xcom_view_identifier view_id(111111, 1);
    std::vector<Gcs_member_identifier> leaving;
    std::vector<Gcs_member_identifier> joined;

    Gcs_group_identifier fake_group_id(group_id->get_group_id());

    Gcs_view *fake_view= new Gcs_view(members, view_id, leaving, joined,
                                      fake_group_id);
    return fake_view;
  }
};


TEST_F(XComControlTest, JoinLeaveTest)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_))
                     .Times(1)
                     .WillOnce(SetArgReferee<0>(GCS_OK));
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_wait_exit()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);

  enum_gcs_error result= xcom_control_if->join(create_fake_view());
  ASSERT_EQ(GCS_OK, result);
  ASSERT_TRUE(xcom_control_if->is_xcom_running());

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());
}


TEST_F(XComControlTest, JoinTestFailedMultipleJoins)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_))
                     .Times(1)
                     .WillOnce(SetArgReferee<0>(GCS_OK));
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_wait_exit()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);

  enum_gcs_error result= xcom_control_if->join(create_fake_view());
  ASSERT_EQ(GCS_OK, result);
  ASSERT_TRUE(xcom_control_if->is_xcom_running());

  result= xcom_control_if->join();
  ASSERT_EQ(GCS_NOK, result);
  ASSERT_TRUE(xcom_control_if->is_xcom_running());

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());
}


TEST_F(XComControlTest, JoinTestFailedToStartComms)
{
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(0);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);

  /*
    The join is forced to wait until the XCOM's tread is running.
    In this test case though, we make the operation fail.
  */
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_))
                 .Times(1)
                 .WillOnce(SetArgReferee<0>(XCOM_COMMS_OTHER));

  enum_gcs_error result= xcom_control_if->join();
  ASSERT_EQ(GCS_NOK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());
}


TEST_F(XComControlTest, JoinTestTimeoutStartingComms)
{
  Gcs_xcom_proxy *my_proxy = new Gcs_xcom_proxy_impl();

  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);

  /*
    The join is forced to wait until the XCOM's tread is running.
    In this test case though, we make the operation time out.
  */
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_))
    .Times(1)
    .WillOnce(Invoke(my_proxy,
      &Gcs_xcom_proxy::xcom_wait_for_xcom_comms_status_change));

  enum_gcs_error result= xcom_control_if->join();
  ASSERT_EQ(GCS_NOK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());

  delete my_proxy;
}


TEST_F(XComControlTest, JoinTestFailedToStartXCom)
{
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1).WillOnce(Return(GCS_NOK));
  EXPECT_CALL(proxy, xcom_init(_)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(proxy, xcom_exit(_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_)).Times(1);

  enum_gcs_error result= xcom_control_if->join();

  ASSERT_EQ(GCS_NOK, result);
}


TEST_F(XComControlTest, JoinTestTimeoutStartingXCom)
{
  Gcs_xcom_proxy *my_proxy = new Gcs_xcom_proxy_impl();

  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1)
    .WillOnce(Invoke(my_proxy, &Gcs_xcom_proxy::xcom_wait_ready));
  EXPECT_CALL(proxy, xcom_init(_)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(proxy, xcom_exit(_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_)).Times(1);

  enum_gcs_error result= xcom_control_if->join();

  ASSERT_EQ(GCS_NOK, result);

  delete my_proxy;
}


TEST_F(XComControlTest, JoinTestWithoutBootNorPeers)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(0);
  EXPECT_CALL(proxy, xcom_init(_)).Times(0);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, delete_node_address(_,_)).Times(0);

  xcom_control_if->set_boot_node(false);
  std::vector<Gcs_xcom_group_member_information *> peers;
  xcom_control_if->set_peer_nodes(peers);

  enum_gcs_error result= xcom_control_if->join();
  ASSERT_EQ(GCS_NOK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());
}


TEST_F(XComControlTest, JoinTestSkipOwnNodeAndCycleThroughPeerNodes)
{
  connection_descriptor *con =
    (connection_descriptor*) malloc(sizeof(connection_descriptor *));
  con->fd= 0;

  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_open_connection(Eq("127.0.0.1"),Eq(12346))).
                     Times(2).
                     WillRepeatedly(Return((connection_descriptor *) NULL));
  EXPECT_CALL(proxy, xcom_client_open_connection(Eq("127.0.0.1"),Eq(12347))).
                     Times(2).
                     WillOnce(Return((connection_descriptor *) NULL)).
                     WillOnce(Return((connection_descriptor *) con));
  EXPECT_CALL(proxy, xcom_client_add_node(_,_,_)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(proxy, xcom_client_close_connection(_)).Times(1).
                     WillOnce(Return(0));

  xcom_control_if->set_boot_node(false);
  enum_gcs_error result= xcom_control_if->join(create_fake_view());
  ASSERT_EQ(GCS_OK, result);
  ASSERT_TRUE(xcom_control_if->is_xcom_running());

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());

  free(con);
}


TEST_F(XComControlTest, LeaveTestWithoutJoin)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(0);
  EXPECT_CALL(proxy, xcom_init(_)).Times(0);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_remove_node(_,_)).Times(0);

  enum_gcs_error result= xcom_control_if->leave();
  ASSERT_EQ(GCS_NOK, result);
  ASSERT_FALSE(xcom_control_if->is_xcom_running());
}


TEST_F(XComControlTest, LeaveTestMultiMember)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_remove_node(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);
  EXPECT_CALL(proxy, delete_node_address(_,_)).Times(1);

  string member_id_1= group_member_information->get_member_address();
  Gcs_member_identifier local_member_information_1(member_id_1);

  string member_id_2("127.0.0.1:12343");
  Gcs_member_identifier local_member_information_2(member_id_2);

  std::vector<Gcs_member_identifier> members;
  members.push_back(local_member_information_1);
  members.push_back(local_member_information_2);

  Gcs_xcom_view_identifier view_id(111111, 1);
  std::vector<Gcs_member_identifier> leaving;
  std::vector<Gcs_member_identifier> joined;

  Gcs_group_identifier fake_group_id(group_id->get_group_id());

  Gcs_view *fake_old_view= new Gcs_view(members, view_id, leaving, joined,
                                        fake_group_id);

  enum_gcs_error result= xcom_control_if->join(fake_old_view);
  ASSERT_EQ(GCS_OK, result);

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);
}


TEST_F(XComControlTest, GetLocalInformationTest)
{
  Gcs_member_identifier result= xcom_control_if->get_local_member_identifier();
  std::string address= group_member_information->get_member_address();
  ASSERT_EQ(address, result.get_member_id());
}


TEST_F(XComControlTest, SetEventListenerTest)
{
  mock_gcs_control_event_listener control_listener;

  int reference= xcom_control_if->add_event_listener(control_listener);

  ASSERT_NE(0, reference);
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->size());
}


TEST_F(XComControlTest, SetEventListenersTest)
{
  mock_gcs_control_event_listener control_listener;
  mock_gcs_control_event_listener another_control_listener;

  int reference= xcom_control_if->add_event_listener(control_listener);
  int another_reference= xcom_control_if->add_event_listener
                                                  (another_control_listener);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)2,
            xcom_control_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);
}


TEST_F(XComControlTest, RemoveEventListenerTest)
{
  mock_gcs_control_event_listener control_listener;
  mock_gcs_control_event_listener another_control_listener;

  int reference= xcom_control_if->add_event_listener(control_listener);
  int another_reference=
    xcom_control_if->add_event_listener(another_control_listener);

  xcom_control_if->remove_event_listener(reference);

  ASSERT_NE(0, reference);
  ASSERT_NE(0, another_reference);
  ASSERT_EQ((long unsigned int)0,
            xcom_control_if->get_event_listeners()->count(reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->count(another_reference));
  ASSERT_EQ((long unsigned int)1,
            xcom_control_if->get_event_listeners()->size());
  ASSERT_NE(reference, another_reference);
}

Gcs_message *create_state_exchange_msg(Gcs_member_identifier &member_id,
                                       Gcs_group_identifier &group_id,
                                       Stored_States *out_stored_states)
{
  Gcs_message_data *dummy= new Gcs_message_data(0, 3);
  uchar to_append=         (uchar)1;
  uchar *buffer= NULL;
  uint64_t buffer_len= 0;

  dummy->append_to_payload(&to_append, 1);
  dummy->append_to_payload(&to_append, 1);
  dummy->append_to_payload(&to_append, 1);
  dummy->encode(&buffer, &buffer_len);
  dummy->release_ownership();

  const Gcs_xcom_view_identifier view_id(999999, 1);
  synode_no configuration_id= null_synode;
  Xcom_member_state *member_state=
    new Xcom_member_state(view_id, configuration_id, buffer, buffer_len);
  free(buffer);
  buffer= NULL;

  (*out_stored_states)[member_id]= member_state;

  buffer_len= member_state->get_encode_size();
  buffer= static_cast<uchar *>(malloc(buffer_len * sizeof(uchar)));
  member_state->encode(buffer, &buffer_len);

  Gcs_message *msg= new Gcs_message(
    member_id,
    group_id,
    new Gcs_message_data(0, buffer_len)
  );
  msg->get_message_data().append_to_payload(buffer, buffer_len);

  delete dummy;
  free(buffer);

  return msg;
}

TEST_F(XComControlTest, ViewChangedJoiningTest)
{
  Gcs_uuid uuid_1= Gcs_uuid::create_uuid();
  blob blob_1 = {
     {
      uuid_1.size,
      static_cast<char *>(malloc(uuid_1.size * sizeof(char *)))
     }
  };
  uuid_1.encode(reinterpret_cast<uchar **>(&blob_1.data.data_val));

  Gcs_uuid uuid_2= Gcs_uuid::create_uuid();
  blob blob_2 = {
     {
      uuid_2.size,
      static_cast<char *>(malloc(uuid_2.size * sizeof(char *)))
     }
  };
  uuid_2.encode(reinterpret_cast<uchar **>(&blob_2.data.data_val));

  node_address node_addrs[2]= {
    {(char *)"127.0.0.1:12345", blob_1, {x_1_0, x_1_2}},
    {(char *)"127.0.0.1:12346", blob_2, {x_1_0, x_1_2}}
  };

  // Common unit test data
  Gcs_xcom_view_identifier *view_id= new Gcs_xcom_view_identifier(999999, 27);

  string member_addr_1(node_addrs[0].address);
  Gcs_member_identifier *node1_member_id= new Gcs_member_identifier(member_addr_1);

  string member_addr_2(node_addrs[1].address);
  Gcs_member_identifier *node2_member_id= new Gcs_member_identifier(member_addr_2);

  std::set<Gcs_member_identifier *> *total_set=
    new std::set<Gcs_member_identifier *>();
  std::set<Gcs_member_identifier *> *join_set=
    new std::set<Gcs_member_identifier *>();
  std::set<Gcs_member_identifier *> *left_set=
    new std::set<Gcs_member_identifier *>();

  total_set->insert(node1_member_id);
  total_set->insert(node2_member_id);

  join_set->insert(node2_member_id);

  site_def *site_config= new_site_def();
  init_site_def(2, node_addrs, site_config);
  site_config->nodeno= 1;

  node_set nodes;
  alloc_node_set(&nodes, 2);
  set_node_set(&nodes);

  Stored_States stored_states;
  Gcs_message *state_message1= create_state_exchange_msg(*node1_member_id,
                                                         *group_id,
                                                         &stored_states);
  Gcs_message *state_message2= create_state_exchange_msg(*node2_member_id,
                                                         *group_id,
                                                         &stored_states);

  EXPECT_CALL(proxy, find_site_def(_)).Times(0);
  EXPECT_CALL(mock_ev_listener, on_view_changed(_,_)).Times(1);
  EXPECT_CALL(*mock_se, state_exchange(_,_,_,_,_,_,_,_)).Times(1);
  EXPECT_CALL(*mock_se, process_member_state(_,_)).Times(2);
  EXPECT_CALL(*mock_se, get_new_view_id()).Times(1).WillOnce(Return(view_id));
  EXPECT_CALL(*mock_se, get_joined()).Times(1).WillOnce(Return(join_set));
  EXPECT_CALL(*mock_se, get_left()).Times(1).WillOnce(Return(left_set));
  EXPECT_CALL(*mock_se, get_total()).Times(1).WillOnce(Return(total_set));
  EXPECT_CALL(*mock_vce, start_view_exchange()).Times(1);
  EXPECT_CALL(*mock_vce, end_view_exchange()).Times(1);
  EXPECT_CALL(*mock_se, reset()).Times(0);
  EXPECT_CALL(*mock_se, reset_with_flush()).Times(1);
  EXPECT_CALL(*mock_se, end()).Times(1);
  EXPECT_CALL(*mock_se, get_member_states()).Times(1)
             .WillOnce(Return(&stored_states));
  EXPECT_CALL(*mock_vce, is_view_changing()).WillRepeatedly(Return(true));

  xcom_control_if->add_event_listener(mock_ev_listener);

  /*
    Initially the node does nto belong to a group and has not
    installed any view.
  */
  ASSERT_FALSE(xcom_control_if->belongs_to_group());
  ASSERT_TRUE(xcom_control_if->get_current_view() == NULL);

  synode_no message_id;
  message_id.group_id= Gcs_xcom_utils::build_xcom_group_id(*this->group_id);
  message_id.msgno=    4;
  message_id.node=     0;

  Gcs_xcom_nodes *xcom_nodes= new Gcs_xcom_nodes(site_config, nodes);

  /*
    Process a global view message delivered by XCOM but say
    that a view with such information was never installed.

    Note that nodes are freed by the caller.
  */
  bool view_accepted= !xcom_control_if->xcom_receive_global_view(message_id, xcom_nodes, false);
  ASSERT_TRUE(view_accepted);

  /*
    Process a global view message delivered by XCOM but say
    that a view with such information was already installed.

    Note that nodes are freed by the caller.
  */
  view_accepted= !xcom_control_if->xcom_receive_global_view(message_id, xcom_nodes, true);
  ASSERT_FALSE(view_accepted);

  /*
    Process the state exchange messages so that the new
    view can be installed.
  */
  xcom_control_if->process_control_message(state_message1);
  xcom_control_if->process_control_message(state_message2);

  Gcs_view *current_view= xcom_control_if->get_current_view();
  ASSERT_TRUE(xcom_control_if->belongs_to_group());
  ASSERT_TRUE(current_view != NULL);

  const Gcs_view_identifier &current_view_id= current_view->get_view_id();
  ASSERT_TRUE((&current_view_id) != NULL);
  ASSERT_EQ(typeid(Gcs_xcom_view_identifier).name(),
            typeid(current_view_id).name());

  Gcs_xcom_view_identifier *xcom_view_id=
    (Gcs_xcom_view_identifier *)&current_view_id;

  ASSERT_EQ(view_id->get_fixed_part(), xcom_view_id->get_fixed_part());
  ASSERT_EQ(view_id->get_monotonic_part() + 1,
            xcom_view_id->get_monotonic_part());
  ASSERT_EQ((size_t)2, current_view->get_members().size());
  ASSERT_EQ((size_t)1, current_view->get_joined_members().size());

  delete view_id;
  delete node2_member_id;
  delete node1_member_id;
  delete total_set;
  delete join_set;
  delete left_set;
  delete current_view;
  delete xcom_nodes;
  mock_vce->set_current_view(NULL);

  // TODO: replace the following with free_site_def(site_config) once
  //       the header file in site_def.h is fixed
  homemade_free_site_def(2, site_config, node_addrs);
  free_node_set(&nodes);

  // reclaim Xcom_member_states
  Stored_States::iterator it;
  for (it= stored_states.begin(); it != stored_states.end(); it++)
    delete (*it).second;
}


TEST_F(XComControlTest, FailedNodeRemovalTest)
{
  // Setting Expectations and Return Values
  // First the node joins the group.
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(2);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(mock_ev_listener, on_view_changed(_,_)).Times(1);

  // Then it leaves the group.
  EXPECT_CALL(proxy, xcom_client_remove_node(_,_)).Times(2)
            .WillOnce(DoAll(InvokeWithoutArgs(this, &XComControlTest::notify_sync_point), Return(0)))
            .WillOnce(Return(0));

  Gcs_uuid uuid_1= Gcs_uuid::create_uuid();
  blob blob_1 = {
     {
      uuid_1.size,
      static_cast<char *>(malloc(uuid_1.size * sizeof(char *)))
     }
  };
  uuid_1.encode(reinterpret_cast<uchar **>(&blob_1.data.data_val));

  Gcs_uuid uuid_2= Gcs_uuid::create_uuid();
  blob blob_2 = {
     {
      uuid_2.size,
      static_cast<char *>(malloc(uuid_2.size * sizeof(char *)))
     }
  };
  uuid_2.encode(reinterpret_cast<uchar **>(&blob_2.data.data_val));

  node_address node_addrs[2]= {
    {(char *)"127.0.0.1:12345", blob_1, {x_1_0, x_1_2}},
    {(char *)"127.0.0.1:12343", blob_2, {x_1_0, x_1_2}}
  };

  site_def *site_config=  new_site_def();
  init_site_def(2, node_addrs, site_config);
  site_config->nodeno= 0;

  node_set nodes;
  alloc_node_set(&nodes, 2);
  set_node_set(&nodes);
  nodes.node_set_val[1]= 0;

  EXPECT_CALL(proxy, find_site_def(_)).Times(0);

  // Setting fake values
  string address= group_member_information->get_member_address();
  Gcs_member_identifier local_member_information_1(address, uuid_1);

  string member_id_2("127.0.0.1:12343");
  Gcs_member_identifier local_member_information_2(member_id_2, uuid_2);

  std::vector<Gcs_member_identifier> members;
  members.push_back(local_member_information_1);
  members.push_back(local_member_information_2);

  Gcs_xcom_view_identifier view_id(111111, 1);
  std::vector<Gcs_member_identifier> leaving;
  std::vector<Gcs_member_identifier> joined;

  Gcs_group_identifier fake_group_id(group_id->get_group_id());

  Gcs_view *fake_old_view= new Gcs_view(members, view_id, leaving, joined,
                                        fake_group_id);

  // registering the listener
  int listener_handle= xcom_control_if->add_event_listener(mock_ev_listener);

  // Test
  enum_gcs_error result= xcom_control_if->join(fake_old_view);
  ASSERT_EQ(GCS_OK, result);

  synode_no message_id;
  message_id.group_id= Gcs_xcom_utils::build_xcom_group_id(*this->group_id);
  message_id.msgno=    2;
  message_id.node=     0;

  Gcs_xcom_nodes *xcom_nodes= new Gcs_xcom_nodes(site_config, nodes);

  bool view_accepted= xcom_control_if->xcom_receive_global_view(message_id, xcom_nodes, false);
  ASSERT_TRUE(view_accepted);

  // Process a local view.
  // Define nodes and emulate the failure of the second node.
  std::vector<Gcs_member_identifier> unreachable;
  unreachable.push_back(local_member_information_2);

  EXPECT_CALL(mock_ev_listener, on_suspicions(members, unreachable)).Times(1);
  xcom_control_if->xcom_receive_local_view(xcom_nodes);

  // Wait to allow thread to remove failed node
  this->wait_for_sync_point();

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);

  xcom_control_if->remove_event_listener(listener_handle);

  homemade_free_site_def(2, site_config, node_addrs);
  delete xcom_nodes;
  free_node_set(&nodes);
}

/*
  Create a global view where the member is marked as faulty.
*/
void check_view_ok(const Gcs_view& view)
{
  ASSERT_EQ(view.get_error_code(), Gcs_view::OK);
}

void check_view_expelled(const Gcs_view& view)
{
  ASSERT_EQ(view.get_error_code(), Gcs_view::MEMBER_EXPELLED);
}

TEST_F(XComControlTest, FailedNodeGlobalViewTest)
{
  // Setting Expectations and Return Values
  // First the node joins the group.
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(mock_ev_listener, on_suspicions(_,_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_remove_node(_,_)).Times(1);

  EXPECT_CALL(mock_ev_listener, on_view_changed(_,_)).Times(2)
              .WillOnce(WithArgs<0>(Invoke(check_view_expelled)))
              .WillOnce(WithArgs<0>(Invoke(check_view_ok)));

  Gcs_uuid uuid_1= Gcs_uuid::create_uuid();
  blob blob_1 = {
     {
      uuid_1.size,
      static_cast<char *>(malloc(uuid_1.size * sizeof(char *)))
     }
  };
  uuid_1.encode(reinterpret_cast<uchar **>(&blob_1.data.data_val));

  Gcs_uuid uuid_2= Gcs_uuid::create_uuid();
  blob blob_2 = {
     {
      uuid_2.size,
      static_cast<char *>(malloc(uuid_2.size * sizeof(char *)))
     }
  };
  uuid_2.encode(reinterpret_cast<uchar **>(&blob_2.data.data_val));

  node_address node_addrs[2]= {
    {(char *)"127.0.0.1:12345", blob_1, {x_1_0, x_1_2}},
    {(char *)"127.0.0.1:12343", blob_2, {x_1_0, x_1_2}}
  };

  site_def *site_config=  new_site_def();
  init_site_def(2, node_addrs, site_config);
  site_config->nodeno= 0;

  node_set nodes;
  alloc_node_set(&nodes, 2);
  set_node_set(&nodes);
  nodes.node_set_val[0]= 0;

  EXPECT_CALL(proxy, find_site_def(_)).Times(0);

  // Setting fake values
  string address_1=      group_member_information->get_member_address();
  Gcs_member_identifier local_member_information_1(address_1);

  string address_2("127.0.0.1:12343");
  Gcs_member_identifier local_member_information_2(address_2);

  std::vector<Gcs_member_identifier> members;
  members.push_back(local_member_information_1);
  members.push_back(local_member_information_2);

  Gcs_xcom_view_identifier view_id(111111, 1);
  std::vector<Gcs_member_identifier> leaving;
  std::vector<Gcs_member_identifier> joined;

  Gcs_group_identifier fake_group_id(group_id->get_group_id());

  Gcs_view *fake_old_view= new Gcs_view(members, view_id, leaving, joined,
                                        fake_group_id);

  // registering the listener
  int listener_handle= xcom_control_if->add_event_listener(mock_ev_listener);

  // Test
  enum_gcs_error result= xcom_control_if->join(fake_old_view);
  ASSERT_EQ(GCS_OK, result);

  synode_no message_id;
  message_id.group_id= Gcs_xcom_utils::build_xcom_group_id(*this->group_id);
  message_id.msgno=    2;
  message_id.node=     0;

  Gcs_xcom_nodes *xcom_nodes= new Gcs_xcom_nodes(site_config, nodes);

  bool view_accepted= xcom_control_if->xcom_receive_global_view(message_id, xcom_nodes, true);
  ASSERT_TRUE(view_accepted);

  result= xcom_control_if->leave();
  ASSERT_EQ(GCS_OK, result);

  xcom_control_if->remove_event_listener(listener_handle);

  homemade_free_site_def(2, site_config, node_addrs);
  delete xcom_nodes;
  free_node_set(&nodes);
}

void *parallel_invocation(void *ptr)
{
  InvocationHelper *helper = (InvocationHelper *) ptr;
  helper->invokeMethod();

  return NULL;
}

TEST_F(XComControlTest, ParallellJoinsTest)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_for_xcom_comms_status_change(_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);

  InvocationHelper *helper= new InvocationHelper(xcom_control_if, JJ);

  My_xp_thread_impl thread;
  thread.create(NULL, parallel_invocation, (void*) helper);

  helper->invokeMethod();

  thread.join(NULL);

  ASSERT_EQ(helper->count_success, 1);
  ASSERT_EQ(helper->count_fail, 1);

  // release memory
  delete helper;
}

TEST_F(XComControlTest, ParallelLeavesTest)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, xcom_client_remove_node(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_close_handlers()).Times(1);
  EXPECT_CALL(proxy, delete_node_address(_,_)).Times(1);

  enum_gcs_error result= xcom_control_if->join(create_fake_view());
  ASSERT_EQ(GCS_OK, result);

  InvocationHelper *helper= new InvocationHelper(xcom_control_if, LL);

  My_xp_thread_impl thread;
  thread.create(NULL, parallel_invocation, (void*) helper);

  helper->invokeMethod();

  thread.join(NULL);

  ASSERT_EQ(helper->count_success, 1);
  ASSERT_EQ(helper->count_fail, 1);

  // release memory
  delete helper;
}


TEST_F(XComControlTest, ParallelLeaveAndDelayedJoinTest)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(2);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(2);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(2);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(2);
  EXPECT_CALL(proxy, xcom_init(_)).Times(2);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);

  enum_gcs_error result= xcom_control_if->join(create_fake_view());
  ASSERT_EQ(GCS_OK, result);

  InvocationHelper *helper= new InvocationHelper(xcom_control_if, LJ);

  My_xp_thread_impl thread;
  thread.create(NULL, parallel_invocation, (void*) helper);

  helper->invokeMethod();

  thread.join(NULL);

  ASSERT_EQ(helper->count_success, 2);

  // release memory
  delete helper;
}

TEST_F(XComControlTest, ParallelJoinAndDelayedLeaveTest)
{
  EXPECT_CALL(proxy, new_node_address_uuid(_,_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_open_handlers(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_client_boot(_,_)).Times(1);
  EXPECT_CALL(proxy, xcom_wait_ready()).Times(1);
  EXPECT_CALL(proxy, xcom_init(_)).Times(1);
  EXPECT_CALL(proxy, xcom_exit(_)).Times(0);
  EXPECT_CALL(proxy, delete_node_address(_,_)).Times(1);

  InvocationHelper *helper= new InvocationHelper(xcom_control_if, JL);

  My_xp_thread_impl thread;
  thread.create(NULL, parallel_invocation, (void*) helper);

  helper->invokeMethod();

  thread.join(NULL);

  ASSERT_EQ(helper->count_success, 2);

  // release memory
  delete helper;
}

}
