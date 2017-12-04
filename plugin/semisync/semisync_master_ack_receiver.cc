/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "semisync_master.h"
#include "semisync_master_ack_receiver.h"
#include "semisync_master_socket_listener.h"

extern ReplSemiSyncMaster repl_semisync;

#ifdef HAVE_PSI_INTERFACE
extern PSI_stage_info stage_waiting_for_semi_sync_ack_from_slave;
extern PSI_stage_info stage_waiting_for_semi_sync_slave;
extern PSI_stage_info stage_reading_semi_sync_ack;
extern PSI_mutex_key key_ss_mutex_Ack_receiver_mutex;
extern PSI_cond_key key_ss_cond_Ack_receiver_cond;
extern PSI_thread_key key_ss_thread_Ack_receiver_thread;
#endif

/* Callback function of ack receive thread */
extern "C" void *ack_receive_handler(void *arg)
{
  my_thread_init();
  reinterpret_cast<Ack_receiver *>(arg)->run();
  my_thread_end();
  my_thread_exit(0);
  return NULL;
}

Ack_receiver::Ack_receiver()
{
  const char *kWho = "Ack_receiver::Ack_receiver";
  function_enter(kWho);

  m_status= ST_DOWN;
  mysql_mutex_init(key_ss_mutex_Ack_receiver_mutex, &m_mutex,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_ss_cond_Ack_receiver_cond, &m_cond);

  function_exit(kWho);
}

Ack_receiver::~Ack_receiver()
{
  const char *kWho = "Ack_receiver::~Ack_receiver";
  function_enter(kWho);

  stop();
  mysql_mutex_destroy(&m_mutex);
  mysql_cond_destroy(&m_cond);

  function_exit(kWho);
}

bool Ack_receiver::start()
{
  const char *kWho = "Ack_receiver::start";
  function_enter(kWho);

  if(m_status == ST_DOWN)
  {
    my_thread_attr_t attr;

    m_status= ST_UP;

    if (DBUG_EVALUATE_IF("rpl_semisync_simulate_create_thread_failure", 1, 0) ||
        my_thread_attr_init(&attr) != 0 ||
        my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE) != 0 ||
#ifndef _WIN32
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0 ||
#endif
        mysql_thread_create(key_ss_thread_Ack_receiver_thread, &m_pid,
                            &attr, ack_receive_handler, this))
    {
      sql_print_error("Failed to start semi-sync ACK receiver thread, "
                      " could not create thread(errno:%d)", errno);

      m_status= ST_DOWN;
      return function_exit(kWho, true);
    }
    (void) my_thread_attr_destroy(&attr);
  }
  return function_exit(kWho, false);
}

void Ack_receiver::stop()
{
  const char *kWho = "Ack_receiver::stop";
  function_enter(kWho);
  int ret;

  if (m_status == ST_UP)
  {
    mysql_mutex_lock(&m_mutex);
    m_status= ST_STOPPING;
    mysql_cond_broadcast(&m_cond);

    while (m_status == ST_STOPPING)
      mysql_cond_wait(&m_cond, &m_mutex);
    mysql_mutex_unlock(&m_mutex);

    /*
      When arriving here, the ack thread already exists. Join failure has no
      side effect aganst semisync. So we don't return an error.
    */
    ret= my_thread_join(&m_pid, NULL);
    if (DBUG_EVALUATE_IF("rpl_semisync_simulate_thread_join_failure", -1, ret))
      sql_print_error("Failed to stop ack receiver thread on my_thread_join, "
                      "errno(%d)", errno);
  }
  function_exit(kWho);
}

bool Ack_receiver::add_slave(THD *thd)
{
  Slave slave;
  const char *kWho = "Ack_receiver::add_slave";
  function_enter(kWho);

  slave.thd= thd;
  slave.vio= *thd->get_protocol_classic()->get_vio();
  slave.vio.mysql_socket.m_psi= NULL;
  slave.vio.read_timeout= 1;

  /* push_back() may throw an exception */
  try
  {
    mysql_mutex_lock(&m_mutex);

    DBUG_EXECUTE_IF("rpl_semisync_simulate_add_slave_failure", throw 1;);

    m_slaves.push_back(slave);
    m_slaves_changed= true;
    mysql_cond_broadcast(&m_cond);
    mysql_mutex_unlock(&m_mutex);
  }
  catch (...)
  {
    mysql_mutex_unlock(&m_mutex);
    return function_exit(kWho, true);
  }
  return function_exit(kWho, false);
}

void Ack_receiver::remove_slave(THD *thd)
{
  const char *kWho = "Ack_receiver::remove_slave";
  function_enter(kWho);

  mysql_mutex_lock(&m_mutex);
  Slave_vector_it it;

  for (it= m_slaves.begin(); it != m_slaves.end(); it++)
  {
    if (it->thd == thd)
    {
      m_slaves.erase(it);
      m_slaves_changed= true;
      break;
    }
  }
  mysql_mutex_unlock(&m_mutex);
  function_exit(kWho);
}

inline void Ack_receiver::set_stage_info(const PSI_stage_info &stage)
{
#ifdef HAVE_PSI_STAGE_INTERFACE
  MYSQL_SET_STAGE(stage.m_key, __FILE__, __LINE__);
#endif /* HAVE_PSI_STAGE_INTERFACE */
}

inline void Ack_receiver::wait_for_slave_connection()
{
  set_stage_info(stage_waiting_for_semi_sync_slave);
  mysql_cond_wait(&m_cond, &m_mutex);
}

/* Auxilary function to initialize a NET object with given net buffer. */
static void init_net(NET *net, unsigned char *buff, unsigned int buff_len)
{
  memset(net, 0, sizeof(NET));
  net->max_packet= buff_len;
  net->buff= buff;
  net->buff_end= buff + buff_len;
  net->read_pos= net->buff;
}

void Ack_receiver::run()
{
  NET net;
  unsigned char net_buff[REPLY_MESSAGE_MAX_LENGTH];
  uint i;
#ifdef HAVE_POLL
  Poll_socket_listener listener(m_slaves);
#else
  Select_socket_listener listener(m_slaves);
#endif //HAVE_POLL

  sql_print_information("Starting ack receiver thread");

  init_net(&net, net_buff, REPLY_MESSAGE_MAX_LENGTH);

  mysql_mutex_lock(&m_mutex);
  m_slaves_changed= true;
  mysql_mutex_unlock(&m_mutex);

  while (1)
  {
    Slave_vector_it it;
    int ret;

    mysql_mutex_lock(&m_mutex);
    if (unlikely(m_status == ST_STOPPING))
      goto end;

    set_stage_info(stage_waiting_for_semi_sync_ack_from_slave);
    if (unlikely(m_slaves_changed))
    {
      if (unlikely(m_slaves.empty()))
      {
        wait_for_slave_connection();
        mysql_mutex_unlock(&m_mutex);
        continue;
      }
      if (!listener.init_slave_sockets())
        goto end;
      m_slaves_changed= false;
    }
    ret= listener.listen_on_sockets();
    if (ret <= 0)
    {
      mysql_mutex_unlock(&m_mutex);

      ret= DBUG_EVALUATE_IF("rpl_semisync_simulate_select_error", -1, ret);

      if (ret == -1 && errno != EINTR)
        sql_print_information("Failed to wait on semi-sync dump sockets, "
                              "error: errno=%d", socket_errno);
      /* Sleep 1us, so other threads can catch the m_mutex easily. */
      my_sleep(1);
      continue;
    }

    set_stage_info(stage_reading_semi_sync_ack);
    i= 0;
    while (i < m_slaves.size())
    {
      if (listener.is_socket_active(i))
      {
        ulong len;
        net.vio= &m_slaves[i].vio;
        /*
          Set compress flag. This is needed to support
          Slave_compress_protocol flag enabled Slaves
        */
        net.compress=
          m_slaves[i].thd->get_protocol_classic()->get_compression();

        do {
          net_clear(&net, 0);

          len= my_net_read(&net);
          if (likely(len != packet_error))
            repl_semisync.reportReplyPacket(m_slaves[i].server_id(),
                                            net.read_pos, len);
          else if (net.last_errno == ER_NET_READ_ERROR)
            listener.clear_socket_info(i);
        } while (net.vio->has_data(net.vio));
      }
      i++;
    }
    mysql_mutex_unlock(&m_mutex);
  }
end:
  sql_print_information("Stopping ack receiver thread");
  m_status= ST_DOWN;
  mysql_cond_broadcast(&m_cond);
  mysql_mutex_unlock(&m_mutex);
}
