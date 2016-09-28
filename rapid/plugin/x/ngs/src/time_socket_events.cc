/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs/time_socket_events.h"
#include "ngs/interface/connection_acceptor_interface.h"
#include "ngs_common/connection_vio.h"
using namespace ngs;

class Connection_acceptor_socket : public Connection_acceptor_interface
{
public:
  Connection_acceptor_socket(const my_socket listener)
  : m_socket_listener(listener)
  {
  }

  Vio *accept()
  {
    Vio *vio;
    sockaddr_storage accept_address;
    int err = 0;
    std::string strerr;
    my_socket sock = INVALID_SOCKET;

    for (int i = 0; i < MAX_ACCEPT_REATTEMPT; ++i)
    {
      socklen_t accept_len = sizeof(accept_address);
      sock = Connection_vio::accept(m_socket_listener, (struct sockaddr*)&accept_address, accept_len, err, strerr);

      if (INVALID_SOCKET != sock)
        break;
    }

    if (INVALID_SOCKET == sock)
      return NULL;

    const bool is_tcpip = (accept_address.ss_family == AF_INET || accept_address.ss_family == AF_INET6);
    vio = vio_new(sock, is_tcpip ? VIO_TYPE_TCPIP : VIO_TYPE_SOCKET, 0);
    if (!vio)
      throw std::bad_alloc();

    // enable TCP_NODELAY
    vio_fastsend(vio);
    vio_keepalive(vio, TRUE);

    return vio;
  }

private:
  my_socket m_socket_listener;
  static const int MAX_ACCEPT_REATTEMPT = 10;
};


struct Time_and_socket_events::Timer_data
{
  boost::function<bool ()> callback;
  event ev;
  timeval tv;
  Time_and_socket_events *self;

  static void free(Timer_data *data)
  {
    evtimer_del(&data->ev);
    delete data;
  }
};


struct Time_and_socket_events::Socket_data
{
  boost::function<void (Connection_acceptor_interface &)> callback;
  event ev;

  static void free(Socket_data *data)
  {
    event_del(&data->ev);
    delete data;
  }
};


Time_and_socket_events::Time_and_socket_events()
{
  m_evbase = event_base_new();

  if (!m_evbase)
    throw std::bad_alloc();
}

Time_and_socket_events::~Time_and_socket_events()
{
  std::for_each(m_timer_events.begin(),
                m_timer_events.end(),
                &Timer_data::free);

  std::for_each(m_socket_events.begin(),
                m_socket_events.end(),
                &Socket_data::free);

  event_base_free(m_evbase);

}

bool Time_and_socket_events::listen(my_socket s, boost::function<void (Connection_acceptor_interface &)> callback)
{
  m_socket_events.push_back(new Socket_data());
  Socket_data *socket_event = m_socket_events.back();

  socket_event->callback = callback;

  event_set(&socket_event->ev, static_cast<int>(s), EV_READ|EV_PERSIST, &Time_and_socket_events::socket_data_avaiable, socket_event);
  event_base_set(m_evbase, &socket_event->ev);

  return 0 == event_add(&socket_event->ev, NULL);
}

/** Register a callback to be executed in a fixed time interval.

The callback is called from the server's event loop thread, until either
the server is stopped or the callback returns false.

NOTE: This method may only be called from the same thread as the event loop.
*/
void Time_and_socket_events::add_timer(const std::size_t delay_ms, boost::function<bool ()> callback)
{
  Timer_data *data = new Timer_data();
  data->tv.tv_sec = static_cast<long>(delay_ms / 1000);
  data->tv.tv_usec = (delay_ms % 1000) * 1000;
  data->callback = callback;
  data->self = this;
  //XXX use persistent timer events after switch to libevent2
  evtimer_set(&data->ev, timeout_call, data);
  event_base_set(m_evbase, &data->ev);
  evtimer_add(&data->ev, &data->tv);

  Mutex_lock lock(m_timers_mutex);
  m_timer_events.push_back(data);
}

void Time_and_socket_events::loop()
{
  event_base_loop(m_evbase, 0);
}

void Time_and_socket_events::break_loop()
{
  event_base_loopbreak(m_evbase);
}

void Time_and_socket_events::timeout_call(int sock, short which, void *arg)
{
  Timer_data *data = (Timer_data*)arg;
  if (!data->callback())
  {
    evtimer_del(&data->ev);
    {
      Mutex_lock timer_lock(data->self->m_timers_mutex);
      data->self->m_timer_events.erase(std::remove(data->self->m_timer_events.begin(), data->self->m_timer_events.end(), data),
                data->self->m_timer_events.end());
    }
    delete data;
  }
  else
  {
    // schedule for another round
    evtimer_add(&data->ev, &data->tv);
  }
}

void Time_and_socket_events::socket_data_avaiable(int sock, short which, void *arg)
{
  Socket_data *data = (Socket_data*)arg;
  Connection_acceptor_socket acceptor(sock);
  data->callback(acceptor);
}
