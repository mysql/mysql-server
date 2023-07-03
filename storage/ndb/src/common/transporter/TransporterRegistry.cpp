/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include <ndb_global.h>

#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"

#include "Transporter.hpp"
#include <SocketAuthenticator.hpp>
#include "BlockNumbers.h"

#include "TCP_Transporter.hpp"
#include "Multi_Transporter.hpp"
#include "Loopback_Transporter.hpp"

#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
#include "SHM_Transporter.hpp"
#endif

#include "NdbOut.hpp"
#include "NdbSleep.h"
#include "NdbMutex.h"
#include "NdbSpin.h"
#include "InputStream.hpp"
#include "OutputStream.hpp"
#include "portlib/NdbTCP.h"

#include <mgmapi/mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi/mgmapi_debug.h>

#include <EventLogger.hpp>

#if 0
#define DEBUG_FPRINTF(arglist) do { fprintf arglist ; } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

/**
 * There is a requirement in the Transporter design that
 * ::performReceive() and ::update_connections()
 * on the same 'TransporterReceiveHandle' should not be 
 * run concurrently. class TransporterReceiveWatchdog provides a
 * simple mechanism to assert that this rule is followed.
 * Does nothing if NDEBUG is defined (in production code)
 */
class TransporterReceiveWatchdog
{
public:
#ifdef NDEBUG
 TransporterReceiveWatchdog(TransporterReceiveHandle & /*recvdata*/) {}

#else
  TransporterReceiveWatchdog(TransporterReceiveHandle& recvdata)
    : m_recvdata(recvdata)
  {
    assert(m_recvdata.m_active == false);
    m_recvdata.m_active = true;
  }

  ~TransporterReceiveWatchdog()
  {
    assert(m_recvdata.m_active == true);
    m_recvdata.m_active = false;
  }

private:
  TransporterReceiveHandle& m_recvdata;
#endif
};


struct in6_addr
TransporterRegistry::get_connect_address(NodeId node_id) const
{
  return theNodeIdTransporters[node_id]->m_connect_address;
}

Uint64
TransporterRegistry::get_bytes_sent(NodeId node_id) const
{
  return theNodeIdTransporters[node_id]->get_bytes_sent();
}

Uint64
TransporterRegistry::get_bytes_received(NodeId node_id) const
{
  return theNodeIdTransporters[node_id]->get_bytes_received();
}

SocketServer::Session * TransporterService::newSession(ndb_socket_t sockfd)
{
  /* The connection is currently running over a plain network socket.
     If m_auth is a TlsAuthenticator, it might get upgraded to a TLS socket
     in server_authenticate().
  */
  NdbSocket secureSocket;
  secureSocket.init_from_new(sockfd);

  DBUG_ENTER("SocketServer::Session * TransporterService::newSession");
  DEBUG_FPRINTF((stderr, "New session created\n"));
  if (m_auth && !m_auth->server_authenticate(secureSocket))
  {
    DEBUG_FPRINTF((stderr, "Failed to authenticate new session\n"));
    secureSocket.close_with_reset(true); // Close with reset
    DBUG_RETURN(0);
  }

  BaseString msg;
  bool close_with_reset = true;
  bool log_failure = false;
  if (!m_transporter_registry->connect_server(secureSocket,
                                              msg,
                                              close_with_reset,
                                              log_failure))
  {
    DEBUG_FPRINTF((stderr, "New session failed in connect_server\n"));
    secureSocket.close_with_reset(close_with_reset);
    if (log_failure)
    {
      g_eventLogger->warning("TR : %s", msg.c_str());
    }
    DBUG_RETURN(0);
  }

  DBUG_RETURN(0);
}

TransporterReceiveData::TransporterReceiveData()
  : m_transporters(),
    m_recv_transporters(),
    m_has_data_transporters(),
    m_handled_transporters(),
    m_bad_data_transporters(),
    m_last_trp_id(0)
{
  /**
   * With multi receiver threads
   *   an interface to reassign these is needed...
   */
  m_transporters.set();            // Handle all
  m_transporters.clear(Uint32(0)); // Except wakeup socket...

#if defined(HAVE_EPOLL_CREATE)
  m_epoll_fd = -1;
  m_epoll_events = nullptr;
#endif
}

bool
TransporterReceiveData::init(unsigned maxTransporters)
{
  maxTransporters += 1; /* wakeup socket */
  m_spintime = 0;
  m_total_spintime = 0;
#if defined(HAVE_EPOLL_CREATE)
  m_epoll_fd = epoll_create(maxTransporters);
  if (m_epoll_fd == -1)
  {
    perror("epoll_create failed... falling back to poll()!");
    goto fallback;
  }
  m_epoll_events = new struct epoll_event[maxTransporters];
  if (m_epoll_events == nullptr)
  {
    perror("Failed to alloc epoll-array... falling back to select!");
    close(m_epoll_fd);
    m_epoll_fd = -1;
    goto fallback;
  }
  memset(m_epoll_events, 0, maxTransporters * sizeof(struct epoll_event));
  return true;
fallback:
#endif
  return m_socket_poller.set_max_count(maxTransporters);
}

bool
TransporterReceiveData::epoll_add(Transporter *t [[maybe_unused]])
{
  assert(m_transporters.get(t->getTransporterIndex()));
#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_fd != -1)
  {
    bool add = true;
    struct epoll_event event_poll;
    memset(&event_poll, 0, sizeof(event_poll));
    ndb_socket_t sock_fd = t->getSocket();
    int node_id = t->getRemoteNodeId();
    int op = EPOLL_CTL_ADD;
    int ret_val, error;

    if (!ndb_socket_valid(sock_fd))
      return false;

    event_poll.data.u32 = t->getTransporterIndex();
    event_poll.events = EPOLLIN;
    ret_val = epoll_ctl(m_epoll_fd, op, ndb_socket_get_native(sock_fd),
                        &event_poll);
    if (!ret_val)
      goto ok;
    error= errno;
    if (error == ENOENT && !add)
    {
      /*
       * Could be that socket was closed premature to this call.
       * Not a problem that this occurs.
       */
      goto ok;
    }
    if (!add || (add && (error != ENOMEM)))
    {
      /*
       * Serious problems, we are either using wrong parameters,
       * have permission problems or the socket doesn't support
       * epoll!!
       */
      g_eventLogger->info("Failed to %s epollfd: %u fd: %d "
                          " node %u to epoll-set,"
                          " errno: %u %s",
                          add ? "ADD" : "DEL", m_epoll_fd,
                          ndb_socket_get_native(sock_fd), node_id, error,
                          strerror(error));
      abort();
    }
    g_eventLogger->info("We lacked memory to add the socket for node id %u",
                        node_id);
    return false;
  }

ok:
#endif
  return true;
}

TransporterReceiveData::~TransporterReceiveData()
{
#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_fd != -1)
  {
    close(m_epoll_fd);
    m_epoll_fd = -1;
  }

  if (m_epoll_events)
  {
    delete [] m_epoll_events;
    m_epoll_events = nullptr;
  }
#endif
}

TransporterRegistry::TransporterRegistry(TransporterCallback *callback,
                                         TransporterReceiveHandle *recvHandle,
                                         unsigned _maxTransporters) :
  callbackObj(callback),
  receiveHandle(recvHandle),
  m_mgm_handle(nullptr),
  sendCounter(1),
  localNodeId(0),
  maxTransporters(_maxTransporters),
  nTransporters(0),
  nMultiTransporters(0),
  nTCPTransporters(0), nSHMTransporters(0),
  connectBackoffMaxTime(0),
  m_transp_count(1),
  m_total_max_send_buffer(0)
{
  if (receiveHandle != nullptr)
  {
    receiveHandle->nTCPTransporters = 0;
    receiveHandle->nSHMTransporters = 0;
  }
  DBUG_ENTER("TransporterRegistry::TransporterRegistry");

  allTransporters     = new Transporter*      [maxTransporters];
  theTCPTransporters  = new TCP_Transporter * [maxTransporters];
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  theSHMTransporters  = new SHM_Transporter * [maxTransporters];
#endif
  theTransporterTypes = new TransporterType   [MAX_NODES];
  theNodeIdTransporters = new Transporter   * [MAX_NODES];
  theMultiTransporters = new Multi_Transporter * [MAX_NODES];
  performStates       = new PerformState      [MAX_NODES];
  ioStates            = new IOState           [MAX_NODES]; 
  peerUpIndicators    = new bool              [maxTransporters];
  connectingTime      = new Uint32            [maxTransporters];
  m_disconnect_errnum = new int               [maxTransporters];
  m_disconnect_enomem_error = new Uint32      [maxTransporters];
  m_error_states      = new ErrorState        [maxTransporters];

  m_has_extra_wakeup_socket = false;

#ifdef ERROR_INSERT
  m_blocked.clear();
  m_blocked_trp.clear();
  m_blocked_disconnected.clear();
  m_sendBlocked.clear();

  m_mixology_level = 0;
#endif

  // Initialize the transporter arrays
  ErrorState default_error_state = { TE_NO_ERROR, (const char *)~(UintPtr)0 };
  for (unsigned i = 0; i < MAX_NODES; i++)
  {
    theNodeIdTransporters[i] = nullptr;
    theMultiTransporters[i] = nullptr;
    performStates[i]      = DISCONNECTED;
    ioStates[i]           = NoHalt;
    peerUpIndicators[i]   = true; // Assume all nodes are up, will be
                                  // cleared at first connect attempt
    connectingTime[i]     = 0;
    m_disconnect_errnum[i]= 0;
    m_disconnect_enomem_error[i] = 0;
    m_error_states[i]     = default_error_state;
  }
  for (unsigned i = 0; i < maxTransporters; i++)
  {
    allTransporters[i]    = nullptr;
    theTCPTransporters[i] = nullptr;
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
    theSHMTransporters[i] = nullptr;
#endif
  }
  theMultiTransporterMutex = NdbMutex_Create();
  DBUG_VOID_RETURN;
}

void TransporterRegistry::set_mgm_handle(NdbMgmHandle h)
{
  DBUG_ENTER("TransporterRegistry::set_mgm_handle");
  if (m_mgm_handle)
    ndb_mgm_destroy_handle(&m_mgm_handle);
  m_mgm_handle= h;
  ndb_mgm_set_timeout(m_mgm_handle, 5000);
#ifndef NDEBUG
  if (h)
  {
    char buf[256];
    DBUG_PRINT("info",("handle set with connectstring: %s",
		       ndb_mgm_get_connectstring(h,buf, sizeof(buf))));
  }
  else
  {
    DBUG_PRINT("info",("handle set to NULL"));
  }
#endif
  DBUG_VOID_RETURN;
}

TransporterRegistry::~TransporterRegistry()
{
  DBUG_ENTER("TransporterRegistry::~TransporterRegistry");
 
  disconnectAll(); 
  removeAll();
  
  delete[] allTransporters;
  delete[] theTCPTransporters;
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  delete[] theSHMTransporters;
#endif
  delete[] theTransporterTypes;
  delete[] theMultiTransporters;
  delete[] theNodeIdTransporters;
  delete[] performStates;
  delete[] ioStates;
  delete[] peerUpIndicators;
  delete[] connectingTime;
  delete[] m_disconnect_errnum;
  delete[] m_disconnect_enomem_error;
  delete[] m_error_states;

  if (m_mgm_handle)
    ndb_mgm_destroy_handle(&m_mgm_handle);

  if (m_has_extra_wakeup_socket)
  {
    ndb_socket_close(m_extra_wakeup_sockets[0]);
    ndb_socket_close(m_extra_wakeup_sockets[1]);
  }
  NdbMutex_Destroy(theMultiTransporterMutex);
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::removeAll()
{
  for (Uint32 i = 0; i < nTCPTransporters; i++)
  {
    delete theTCPTransporters[i];
  }
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 i = 0; i < nSHMTransporters; i++)
  {
    delete theSHMTransporters[i];
  }
#endif
  for (Uint32 i = 0; i < nMultiTransporters; i++)
  {
    delete theMultiTransporters[i];
  }
  nTransporters = 0;
  nTCPTransporters = 0;
  nSHMTransporters = 0;
  nMultiTransporters = 0;
}

void
TransporterRegistry::disconnectAll(){
  DEBUG_FPRINTF((stderr, "(%u)doDisconnect(all), line: %d\n",
               localNodeId, __LINE__));
  for (Uint32 i = 0; i < nTCPTransporters; i++)
  {
    theTCPTransporters[i]->doDisconnect();
  }
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 i = 0; i < nSHMTransporters; i++)
  {
    theSHMTransporters[i]->doDisconnect();
  }
#endif
}

bool
TransporterRegistry::init(NodeId nodeId) {
  DBUG_ENTER("TransporterRegistry::init");
  assert(localNodeId == 0 ||
         localNodeId == nodeId);

  localNodeId = nodeId;

  DEBUG("TransporterRegistry started node: " << localNodeId);

  if (receiveHandle)
  {
    if (!init(* receiveHandle))
      DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

bool
TransporterRegistry::init(TransporterReceiveHandle& recvhandle)
{
  recvhandle.nTCPTransporters = nTCPTransporters;
  recvhandle.nSHMTransporters = nSHMTransporters;
  return recvhandle.init(maxTransporters);
}

bool
TransporterRegistry::connect_server(NdbSocket & socket,
                                    BaseString & msg,
                                    bool& close_with_reset,
                                    bool& log_failure)
{
  DBUG_ENTER("TransporterRegistry::connect_server(sockfd)");

  log_failure = true;

  // Read "hello" that consists of node id and other info
  // from client
  SecureSocketInputStream s_input(socket);
  char buf[256]; // <int> <int> <int> <int> <..expansion..>
  if (s_input.gets(buf, sizeof(buf)) == nullptr) {
    /* Could be spurious connection, need not log */
    log_failure = false;
    msg.assfmt("Ignored connection attempt as failed to "
               "read 'hello' from client");
    DBUG_PRINT("error", ("%s", msg.c_str()));
    DEBUG_FPRINTF((stderr, "%s", msg.c_str()));
    DBUG_RETURN(false);
  }

  int nodeId;
  int remote_transporter_type;
  int serverNodeId = -1;
  int multi_transporter_instance = -1;
  int r= sscanf(buf, "%d %d %d %d",
                &nodeId,
                &remote_transporter_type,
                &serverNodeId,
                &multi_transporter_instance);
  switch (r) {
  case 4:
    /* Latest version client */
    break;
  case 3:
    /* Older client, sending just nodeid, transporter type, serverNodeId */
    break;
  case 2:
    /* Older client, sending just nodeid and transporter type */
    break;
  default:
    /* Could be spurious connection, need not log */
    log_failure = false;
    msg.assfmt("Ignored connection attempt as failed to "
               "parse 'hello' from client.  >%s<", buf);
    DBUG_PRINT("error", ("%s", msg.c_str()));
    DEBUG_FPRINTF((stderr, "%s", msg.c_str()));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("Client hello, nodeId: %d transporter type: %d "
                      "server nodeid %d instance %d",
		      nodeId,
                      remote_transporter_type,
                      serverNodeId,
                      multi_transporter_instance));
  /*
  DEBUG_FPRINTF((stderr, "Client hello, nodeId: %d transporter type: %d "
                         "server nodeid %d instance %d",
		         nodeId,
                         remote_transporter_type,
                         serverNodeId,
                         multi_transporter_instace));
  */

  // Check that nodeid is in range before accessing the arrays
  if (nodeId < 0 ||
      nodeId > (int)MAX_NODES)
  {
    /* Strange, log it */
    msg.assfmt("Ignored connection attempt as client "
               "nodeid %u out of range", nodeId);
    DBUG_PRINT("error", ("%s", msg.c_str()));
    DBUG_RETURN(false);
  }

  lockMultiTransporters();
  // Check that transporter is allocated
  Transporter *t= theNodeIdTransporters[nodeId];
  if (t == nullptr)
  {
    unlockMultiTransporters();
    /* Strange, log it */
    msg.assfmt("Ignored connection attempt as client "
               "nodeid %u is undefined.",
               nodeId);
    DBUG_PRINT("error", ("%s", msg.c_str()));
    DBUG_RETURN(false);
  }

  // Check transporter type
  if (remote_transporter_type != t->m_type &&
      t->m_type != tt_Multi_TRANSPORTER) // Checked later
  {
    unlockMultiTransporters();
    /* Strange, log it */
    msg.assfmt("Connection attempt from client node %u failed as transporter "
               "type %u is not as expected %u.",
               nodeId,
               remote_transporter_type,
               t->m_type);
    DBUG_RETURN(false);
  }

  // Check that the serverNodeId is correct
  if (serverNodeId != -1)
  {
    /* Check that incoming connection was meant for us */
    if (serverNodeId != t->getLocalNodeId())
    {
      unlockMultiTransporters();
      /* Strange, log it */
      msg.assfmt("Ignored connection attempt as client "
                 "node %u attempting to connect to node %u, "
                 "but this is node %u.",
                 nodeId,
                 serverNodeId,
                 t->getLocalNodeId());
      DBUG_PRINT("error", ("%s", msg.c_str()));
      DBUG_RETURN(false);
    }
  }

  bool correct_state = false;
  Multi_Transporter *multi_trp =
    get_node_multi_transporter(nodeId);
  if (multi_trp &&
      multi_transporter_instance > 0)
  {
    /* Specific path for non-zero multi transporter instances */
    DEBUG_FPRINTF((stderr, "connect_server multi trp, node %u instance %u\n",
                   t->getRemoteNodeId(),
                   multi_transporter_instance));

    if (performStates[nodeId] == TransporterRegistry::CONNECTED)
    {
      /**
       * The connection is ok if the following is true:
       * 1) The transporter is a Multi_Transporter object
       * 2) The instance exists as inactive transporter
       * 3) The inactive instance referred to isn't already
       *    connected
       *
       * If this all are true we will replace the multi transporter
       * instance with the transporter instance to connect in this
       * instance.
       */
      if ((multi_transporter_instance - 1) <
           int(multi_trp->get_num_inactive_transporters()))
      {
        Transporter *inst_trp =
          multi_trp->get_inactive_transporter(multi_transporter_instance - 1);

        /* Check type now that we have the actual instance */
        if (remote_transporter_type == inst_trp->m_type)
        {
          if (!inst_trp->isConnected())
          {
            /**
             * Continue connection setup with
             * multi-transporter specific instance
             */
            correct_state = true;
            t = inst_trp;
          }
          else
          {
            /* Strange, log it */
            msg.assfmt("Ignored connection attempt from node %u as multi "
                       "transporter instance %u already connected.",
                       nodeId,
                       multi_transporter_instance);
          }
        }
        else
        {
          /* Strange, log it */
          msg.assfmt("Ignored multi transporter connection attempt "
                     "from node %u instance %u as transporter "
                     "type %u is not as expected %u",
                     nodeId,
                     multi_transporter_instance,
                     remote_transporter_type,
                     inst_trp->m_type);
        }
      }
      else
      {
        /* Strange, log it */
        msg.assfmt("Ignored connection attempt from node %u as multi "
                   "transporter instance %u is not in range.",
                   nodeId,
                   multi_transporter_instance);
      }
    }
  }
  else
  {
    /**
     * Normal connection setup
     * Sub cases :
     *   Normal setup for non-multi trp        : multi = 0, instance = 0
     *   Normal setup from non-multi trp from
     *     old version                         : multi = 0, instance = -1
     *   Normal setup for multi trp instance 0 : multi = 1, instance = 0
     *   Normal setup from for multi trp
     *     from old version                    : multi = 1, instance = -1
     *
     * Not supported :
     *   multi = 0, instance > 0               : Invalid
     */
    if (!multi_trp)
    {
      if (multi_transporter_instance > 0)
      {
        unlockMultiTransporters();
        /* Strange, log it */
        msg.assfmt("Ignored connection attempt from node %u as multi "
                   "transporter instance %d specified for non multi-transporter",
                   nodeId, multi_transporter_instance);
        DBUG_RETURN(false);
      }
    }
    else
    {
      /* multi_trp */
      require(multi_transporter_instance <= 0);

      /* Continue connection setup with specific instance 0 */
      require(multi_trp->get_num_active_transporters() == 1);
      t = multi_trp->get_active_transporter(0);
    }

    /**
     * Normal connection setup requires state to be in CONNECTING
     */
    if (performStates[nodeId] == TransporterRegistry::CONNECTING)
    {
      correct_state = true;
    }
    else
    {
      msg.assfmt("Ignored connection attempt as this node "
                 "is not expecting a connection from node %u. "
                 "State %u",
                 nodeId,
                 performStates[nodeId]);

      /**
       * This is expected if the transporter state is DISCONNECTED,
       * otherwise it's a bit strange
       */
      log_failure = (performStates[nodeId] != TransporterRegistry::DISCONNECTED);

      DEBUG_FPRINTF((stderr, "%s", msg.c_str()));
    }
  }
  unlockMultiTransporters();

  // Check that the transporter should be connecting
  if (!correct_state)
  {
    DBUG_PRINT("error", ("Transporter for node id %d in wrong state %s",
                         nodeId, msg.c_str()));
    /*
    DEBUG_FPRINTF((stderr, "Transporter for node id %d in wrong state %s\n",
                           nodeId, msg.c_str()));
    */

    // Avoid TIME_WAIT on server by requesting client to close connection
    SecureSocketOutputStream s_output(socket);
    if (s_output.println("BYE") < 0)
    {
      // Failed to request client close
      DBUG_PRINT("error", ("Failed to send client BYE"));
      DBUG_RETURN(false);
    }

    // Wait for to close connection by reading EOF(i.e read returns 0)
    const int read_eof_timeout = 1000; // Fairly short timeout
    if (socket.read(read_eof_timeout, buf, sizeof(buf)) == 0)
    {
      // Client gracefully closed connection, turn off close_with_reset
      close_with_reset = false;
      DBUG_RETURN(false);
    }

    // Failed to request client close
    DBUG_RETURN(false);
  }

  // Send reply to client
  SecureSocketOutputStream s_output(socket);
  if (s_output.println("%d %d", t->getLocalNodeId(), t->m_type) < 0)
  {
    /* Strange, log it */
    msg.assfmt("Connection attempt failed due to error sending "
               "reply to client node %u",
               nodeId);
    DBUG_PRINT("error", ("%s", msg.c_str()));
    DBUG_RETURN(false);
  }

  // Setup transporter (transporter responsible for closing sockfd)
  DEBUG_FPRINTF((stderr, "connect_server for trp_id %u\n",
                 t->getTransporterIndex()));
  DBUG_RETURN(t->connect_server(socket, msg));
}

void
TransporterRegistry::insert_allTransporters(Transporter *t)
{
  TrpId trp_id = t->getTransporterIndex();
  if (trp_id == 0)
  {
    nTransporters++;
    require(allTransporters[nTransporters] == nullptr);
    allTransporters[nTransporters] = t;
    t->setTransporterIndex(nTransporters);
  }
  else
  {
    require(allTransporters[trp_id] == nullptr);
    allTransporters[trp_id] = t;
  }
}

void
TransporterRegistry::remove_allTransporters(Transporter *t)
{
  TrpId trp_id = t->getTransporterIndex();
  if (trp_id == 0)
  {
    return;
  }
  else if (t == allTransporters[trp_id])
  {
    DEBUG_FPRINTF((stderr,
                   "remove trp_id %u for node %u from allTransporters\n",
                   trp_id,
                   t->getRemoteNodeId()));
    allTransporters[trp_id] = nullptr;
  }
}

void
TransporterRegistry::insert_node_transporter(NodeId node_id, Transporter *t)
{
  theNodeIdTransporters[node_id] = t;
}

void
TransporterRegistry::lockMultiTransporters()
{
  NdbMutex_Lock(theMultiTransporterMutex);
}

void
TransporterRegistry::unlockMultiTransporters()
{
  NdbMutex_Unlock(theMultiTransporterMutex);
}

Uint32
TransporterRegistry::get_num_multi_transporters()
{
  return nMultiTransporters;
}

Multi_Transporter*
TransporterRegistry::get_multi_transporter(Uint32 index)
{
  require(index < nMultiTransporters);
  return theMultiTransporters[index];
}

bool
TransporterRegistry::configureTransporter(TransporterConfiguration *config)
{
  NodeId remoteNodeId = config->remoteNodeId;

  assert(localNodeId);
  assert(config->localNodeId == localNodeId);

  if (remoteNodeId > MAX_NODES)
    return false;

  Transporter* t = theNodeIdTransporters[remoteNodeId];
  if(t != nullptr)
  {
    // Transporter already exist, try to reconfigure it
    require(!t->isMultiTransporter());
    require(!t->isPartOfMultiTransporter());
    return t->configure(config);
  }

  DEBUG("Configuring transporter from " << localNodeId
	<< " to " << remoteNodeId);

  switch (config->type){
  case tt_TCP_TRANSPORTER:
    return createTCPTransporter(config);
  case tt_SHM_TRANSPORTER:
    return createSHMTransporter(config);
  default:
    abort();
    break;
  }
  return false;
}

bool
TransporterRegistry::createMultiTransporter(Uint32 node_id, Uint32 num_trps)
{
  Multi_Transporter *multi_trp = nullptr;
  lockMultiTransporters();
  Transporter *base_trp = theNodeIdTransporters[node_id];
  require(!base_trp->isMultiTransporter());
  require(!base_trp->isPartOfMultiTransporter());
  multi_trp = new Multi_Transporter(*this, base_trp);
  theMultiTransporters[nMultiTransporters] = multi_trp;
  nMultiTransporters++;
  Uint32 nodeId = base_trp->getRemoteNodeId();
  TransporterType type = theTransporterTypes[nodeId];
  for (Uint32 i = 0; i < num_trps; i++)
  {
    if (type == tt_TCP_TRANSPORTER)
    {
      TCP_Transporter *tcp_trp = (TCP_Transporter*)base_trp;
      TCP_Transporter *new_trp = new TCP_Transporter(*this, tcp_trp);
      require(new_trp->initTransporter());
      multi_trp->add_not_used_trp(new_trp);
      new_trp->set_multi_transporter_instance(i + 1);
      theTCPTransporters[nTCPTransporters++] = new_trp;
    }
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
    else if (type == tt_SHM_TRANSPORTER)
    {
      SHM_Transporter *shm_trp = (SHM_Transporter*)base_trp;
      SHM_Transporter *new_trp = new SHM_Transporter(*this, shm_trp);
      require(new_trp->initTransporter());
      multi_trp->add_not_used_trp(new_trp);
      new_trp->set_multi_transporter_instance(i + 1);
      theSHMTransporters[nSHMTransporters++] = new_trp;
    }
#endif
    else
    {
      require(false);
    }
  }
  multi_trp->add_active_trp(base_trp);
  unlockMultiTransporters();
  return true;
}

bool
TransporterRegistry::createTCPTransporter(TransporterConfiguration *config) {

  TCP_Transporter * t = nullptr;
  /* Don't use index 0, special use case for extra  transporters */
  config->transporterIndex = nTransporters + 1;
  if (config->remoteNodeId == config->localNodeId)
  {
    t = new Loopback_Transporter(* this, config);
  }
  else
  {
    t = new TCP_Transporter(*this, config);
  }

  if (t == nullptr)
    return false;
  else if (!t->initTransporter())
  {
    delete t;
    return false;
  }

  // Put the transporter in the transporter arrays
  nTransporters++;
  allTransporters[nTransporters]            = t;
  theTCPTransporters[nTCPTransporters]      = t;
  theNodeIdTransporters[t->getRemoteNodeId()] = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_TCP_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  nTCPTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();
  return true;
}

bool
TransporterRegistry::createSHMTransporter(TransporterConfiguration *config [[maybe_unused]])
{
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  DBUG_ENTER("TransporterRegistry::createTransporter SHM");

  /* Don't use index 0, special use case for extra  transporters */
  config->transporterIndex = nTransporters + 1;

  SHM_Transporter * t = new SHM_Transporter(*this,
                                            config->transporterIndex,
					    config->localHostName,
					    config->remoteHostName,
					    config->s_port,
					    config->isMgmConnection,
					    localNodeId,
					    config->remoteNodeId,
					    config->serverNodeId,
					    config->checksum,
					    config->signalId,
					    config->shm.shmKey,
					    config->shm.shmSize,
					    config->preSendChecksum,
                                            config->shm.shmSpintime,
                                            config->shm.sendBufferSize);
  if (t == nullptr)
    return false;

  // Put the transporter in the transporter arrays
  nTransporters++;
  allTransporters[nTransporters]            = t;
  theSHMTransporters[nSHMTransporters]      = t;
  theNodeIdTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SHM_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  
  nSHMTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();

  DBUG_RETURN(true);
#else
  ndbout_c("Shared memory transporters not supported on Windows");
  return false;
#endif
}

/**
 * prepareSend() - queue a signal for later asynchronous sending.
 *
 * A successful prepareSend() only guarantee that the signal has been
 * stored in some send buffers. Normally it will later be sent, but could
 * also be discarded if the transporter *later* disconnects.
 *
 * Signal memory is allocated with the implementation dependent
 * ::getWritePtr(). On multithreaded implementations, allocation may
 * take place in thread-local buffer pools which is later 'flushed'
 * to a global send buffer.
 *
 * Asynchronous to prepareSend() there may be Transporters
 * (dis)connecting which are signaled to the upper layers by calling
 * disable_/enable_send_buffers().
 *
 * The 'sendHandle' interface has the method ::isSendEnabled() which
 * provides us with a way to check whether communication with a node
 * is possible. Depending on the sendHandle implementation, 
 * isSendEnabled() may either have a 'synchronized' or 'optimistic'
 * implementation:
 *  - A (thread-) 'synchronized' implementation guarantee that the
 *    send buffers really are enabled, both thread local and global,
 *    at the time of send buffer allocation. (May disconnect later though)
 *  - An 'optimistic' implementation does not really know whether the
 *    send buffers are (globally) enabled. Send buffers may always be
 *    allocated and possibly silently discarded later.
 *    (SEND_DISCONNECTED will never be returned)
 *
 * The trp_client implementation is 'synchronized', while the mt-/non-mt
 * data node implementation is not. Note that a 'SEND_DISCONNECTED'
 * and 'SEND_BLOCKED' return has always been handled as an 'OK' on 
 * the data nodes. So not being able to detect 'SEND_DISCONNECTED'
 * should not matter.
 *
 * Note that sending behaves differently wrt disconnect / reconnect
 * syncing compared to 'receive'. Receiver side *is* synchroinized with
 * the receiver transporter disconnect / reconnect by both requiring the
 * 'poll-right'. Thus receiver logic may check Transporter::isConnected()
 * directly.
 *
 * See further comments as part of ::performReceive().
 */
template <typename AnySectionArg>
SendStatus
TransporterRegistry::prepareSendTemplate(
                                 TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader * signalHeader,
                                 Uint8 prio,
                                 const Uint32 * signalData,
                                 NodeId nodeId,
                                 Transporter *t,
                                 AnySectionArg section)
{
  assert(t != nullptr);

  if(
    likely((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
           (signalHeader->theReceiversBlockNumber == QMGR) ||
           (signalHeader->theReceiversBlockNumber == API_CLUSTERMGR))
  {
    if (likely(sendHandle->isSendEnabled(nodeId)))
    {
      const Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, section.m_ptr);
      if (likely(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE))
      {
        TrpId trp_id = t->getTransporterIndex();
        SendStatus error = SEND_OK;
	Uint32 *insertPtr = getWritePtr(sendHandle,
                                        t,
                                        trp_id,
                                        lenBytes,
                                        prio,
                                        &error);
        if (likely(insertPtr != nullptr))
	{
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	  updateWritePtr(sendHandle, t, trp_id, lenBytes, prio);
	  return SEND_OK;
	}
        if (unlikely(error == SEND_MESSAGE_TOO_BIG))
        {
          g_eventLogger->info("Send message too big");
	  return SEND_MESSAGE_TOO_BIG;
        }
        set_status_overloaded(nodeId, true);
        const int sleepTime = 2;

	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
	for (int i = 0; i < 100; i++)
	{
	  NdbSleep_MilliSleep(sleepTime);
          /* FC : Consider counting sleeps here */
	  insertPtr = getWritePtr(sendHandle,
                                  t,
                                  trp_id,
                                  lenBytes,
                                  prio,
                                  &error);
	  if (likely(insertPtr != nullptr))
	  {
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	    updateWritePtr(sendHandle, t, trp_id, lenBytes, prio);
	    DEBUG_FPRINTF((stderr, "TE_SEND_BUFFER_FULL\n"));
	    /**
	     * Send buffer full, but resend works
	     */
	    report_error(nodeId, TE_SEND_BUFFER_FULL);
	    return SEND_OK;
	  }
          if (unlikely(error == SEND_MESSAGE_TOO_BIG))
          {
            g_eventLogger->info("Send message too big");
	    return SEND_MESSAGE_TOO_BIG;
          }
	}

	WARNING("Signal to " << nodeId << " lost(buffer)");
	DEBUG_FPRINTF((stderr, "TE_SIGNAL_LOST_SEND_BUFFER_FULL\n"));
	report_error(nodeId, TE_SIGNAL_LOST_SEND_BUFFER_FULL);
	return SEND_BUFFER_FULL;
      }
      else
      {
        g_eventLogger->info("Send message too big: length %u", lenBytes);
	return SEND_MESSAGE_TOO_BIG;
      }
    }
    else
    {
#ifdef ERROR_INSERT
      if (m_blocked.get(nodeId))
      {
        /* Looks like it disconnected while blocked.  We'll pretend
         * not to notice for now
         */
        WARNING("Signal to " << nodeId << " discarded as node blocked + disconnected");
        return SEND_OK;
      }
#endif
      DEBUG("Signal to " << nodeId << " lost(disconnect) ");
      return SEND_DISCONNECTED;
    }
  }
  else
  {
    DEBUG("Discarding message to block: " 
	  << signalHeader->theReceiversBlockNumber 
	  << " node: " << nodeId);
    
    return SEND_BLOCKED;
  }
}


Transporter*
TransporterRegistry::prepareSend_getTransporter(
                                const SignalHeader *signalHeader,
                                NodeId nodeId,
                                TrpId &trp_id,
                                SendStatus& status)
{
  Transporter *node_trp = theNodeIdTransporters[nodeId];
  if (unlikely(node_trp == nullptr))
  {
    DEBUG("Discarding message to unknown node: " << nodeId);
    status = SEND_UNKNOWN_NODE;
    return nullptr;
  }
  assert(!node_trp->isPartOfMultiTransporter());
  Transporter *t;
  t = node_trp->get_send_transporter(signalHeader->theReceiversBlockNumber,
                                     signalHeader->theSendersBlockRef);
  assert(!t->isMultiTransporter());
  trp_id = t->getTransporterIndex();
  if (unlikely(trp_id == 0))
  {
    /**
     * Can happen in disconnect situations, node is disconnected, so send
     * to it is successful since the node won't be there to receive the
     * message.
     */
    DEBUG("Discarding message due to trp_id = 0");
    status = SEND_OK;
    return nullptr;
  }
  return t;
}

SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 TrpId &trp_id,
                                 const LinearSectionPtr ptr[3])
{
  SendStatus status;
  Transporter *t = prepareSend_getTransporter(signalHeader, nodeId, trp_id,
                                              status);
  if (unlikely(t == nullptr))
    return status;

  const Packer::LinearSectionArg section(ptr);
  return prepareSendTemplate(sendHandle,
                             signalHeader,
                             prio,
                             signalData,
                             nodeId,
                             t,
                             section);
}


SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 TrpId &trp_id,
                                 class SectionSegmentPool &thePool,
                                 const SegmentedSectionPtr ptr[3])
{
  SendStatus status;
  Transporter *t = prepareSend_getTransporter(signalHeader, nodeId, trp_id,
                                              status);
  if (unlikely(t == nullptr))
    return status;

  const Packer::SegmentedSectionArg section(thePool,ptr);
  return prepareSendTemplate(sendHandle,
                             signalHeader,
                             prio,
                             signalData,
                             nodeId,
                             t,
                             section);
}


SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 const GenericSectionPtr ptr[3])
{
  TrpId trp_id = 0;
  SendStatus status;
  Transporter *t = prepareSend_getTransporter(signalHeader, nodeId, trp_id,
                                              status);
  if (unlikely(t == nullptr))
    return status;

  const Packer::GenericSectionArg section(ptr);
  return prepareSendTemplate(sendHandle,
                             signalHeader,
                             prio,
                             signalData,
                             nodeId,
                             t,
                             section);
}

SendStatus
TransporterRegistry::prepareSendOverAllLinks(
                                 TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 TrpBitmask &trp_ids)
{
  // node_trp handling copied from first part of prepareSend_getTransporter
  Transporter *node_trp = theNodeIdTransporters[nodeId];
  if (unlikely(node_trp == nullptr))
  {
    DEBUG("Discarding message to unknown node: " << nodeId);
    return SEND_UNKNOWN_NODE;
  }
  assert(!node_trp->isPartOfMultiTransporter());

  require(signalHeader->m_noOfSections == 0);
  const Packer::LinearSectionArg section(nullptr);

  if (!node_trp->isMultiTransporter())
  {
    Transporter *t = node_trp;
    // t handling copied from second part of prepareSend_getTransporter
    TrpId trp_id = t->getTransporterIndex();
    if (unlikely(trp_id == 0))
    {
      /**
       * Can happen in disconnect situations, node is disconnected, so send
       * to it is successful since the node won't be there to receive the
       * message.
       */
      DEBUG("Discarding message due to trp_id = 0");
      return SEND_OK;
    }

    SendStatus status = prepareSendTemplate(sendHandle,
                                            signalHeader,
                                            prio,
                                            signalData,
                                            nodeId,
                                            t,
                                            section);

    if (likely(status == SEND_OK))
    {
      require(trp_id < MAX_NTRANSPORTERS);
      trp_ids.set(trp_id);
    }
    return status;
  }
  else
  {
    Multi_Transporter *multi_trp = get_node_multi_transporter(nodeId);
    require(multi_trp == node_trp);

    SendStatus return_status = SEND_OK;
    Uint32 num_trps = multi_trp->get_num_active_transporters();
    for (Uint32 i = 0; i < num_trps; i++)
    {
      Transporter *t = multi_trp->get_active_transporter(i);
      require(t != nullptr);
      const TrpId trp_id = t->getTransporterIndex();
      if (unlikely(trp_id == 0))
        continue;
      SendStatus status = prepareSendTemplate(sendHandle,
                                              signalHeader,
                                              prio,
                                              signalData,
                                              nodeId,
                                              t,
                                              section);
      if (likely(status == SEND_OK))
      {
        require(trp_id < MAX_NTRANSPORTERS);
        trp_ids.set(trp_id);
      }
      else if (status != SEND_BLOCKED && status != SEND_DISCONNECTED)
      {
        /*
         * Treat SEND_BLOCKED and SEND_DISCONNECTED as SEND_OK.
         * Else take the last bad status returned.
         */
        return_status = status;
      }
    }
    return return_status;
  }
}

bool
TransporterRegistry::setup_wakeup_socket(TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  if (m_has_extra_wakeup_socket)
  {
    return true;
  }

  assert(!recvdata.m_transporters.get(0));

  if (ndb_socketpair(m_extra_wakeup_sockets))
  {
    perror("socketpair failed!");
    return false;
  }

  if (!TCP_Transporter::setSocketNonBlocking(m_extra_wakeup_sockets[0]) ||
      !TCP_Transporter::setSocketNonBlocking(m_extra_wakeup_sockets[1]))
  {
    goto err;
  }

#if defined(HAVE_EPOLL_CREATE)
  if (recvdata.m_epoll_fd != -1)
  {
    int sock = ndb_socket_get_native(m_extra_wakeup_sockets[0]);
    struct epoll_event event_poll;
    memset(&event_poll, 0, sizeof(event_poll));
    event_poll.data.u32 = 0;
    event_poll.events = EPOLLIN;
    int ret_val = epoll_ctl(recvdata.m_epoll_fd, EPOLL_CTL_ADD, sock,
                            &event_poll);
    if (ret_val != 0)
    {
      int error= errno;
      g_eventLogger->info("Failed to add extra sock %u to epoll-set: %u", sock,
                          error);
      goto err;
    }
  }
#endif
  m_has_extra_wakeup_socket = true;
  recvdata.m_transporters.set(Uint32(0));
  return true;

err:
  ndb_socket_close(m_extra_wakeup_sockets[0]);
  ndb_socket_close(m_extra_wakeup_sockets[1]);
  ndb_socket_invalidate(m_extra_wakeup_sockets+0);
  ndb_socket_invalidate(m_extra_wakeup_sockets+1);
  return false;
}

void
TransporterRegistry::wakeup()
{
  if (m_has_extra_wakeup_socket)
  {
    static char c = 37;
    ndb_send(m_extra_wakeup_sockets[1], &c, 1, 0);
  }
}

Uint32
TransporterRegistry::check_TCP(TransporterReceiveHandle& recvdata,
                               Uint32 timeOutMillis)
{
  Uint32 retVal = 0;
#if defined(HAVE_EPOLL_CREATE)
  if (likely(recvdata.m_epoll_fd != -1))
  {
    int tcpReadSelectReply = 0;
    Uint32 num_trps = nTCPTransporters + nSHMTransporters +
                      (m_has_extra_wakeup_socket ? 1 : 0);

    if (num_trps)
    {
      tcpReadSelectReply = epoll_wait(recvdata.m_epoll_fd,
                                      recvdata.m_epoll_events,
                                      num_trps, timeOutMillis);
      retVal = tcpReadSelectReply;
    }

    int num_socket_events = tcpReadSelectReply;
    if (num_socket_events > 0)
    {
      for (int i = 0; i < num_socket_events; i++)
      {
        const Uint32 trpid = recvdata.m_epoll_events[i].data.u32;
        /**
         * check that it's assigned to "us"
         */
        assert(recvdata.m_transporters.get(trpid));

        recvdata.m_recv_transporters.set(trpid);
      }
    }
    else if (num_socket_events < 0)
    {
      assert(errno == EINTR);
    }
  }
  else
#endif
  {
    retVal = poll_TCP(timeOutMillis, recvdata);
  }
  return retVal;
}

Uint32
TransporterRegistry::poll_SHM(TransporterReceiveHandle& recvdata,
                              NDB_TICKS start_poll,
                              Uint32 micros_to_poll)
{
  Uint32 res;
  Uint64 micros_passed;
  do
  {
    bool any_connected = false;
    res = poll_SHM(recvdata, any_connected);
    if (res || !any_connected)
    {
      /**
       * If data found or no SHM transporter connected there is no
       * reason to continue spinning.
       */
      break;
    }
    NDB_TICKS now = NdbTick_getCurrentTicks();
    micros_passed =
      NdbTick_Elapsed(start_poll, now).microSec();
  } while (micros_passed < Uint64(micros_to_poll));
  return res;
}

Uint32
TransporterRegistry::poll_SHM(TransporterReceiveHandle& recvdata [[maybe_unused]],
                              bool &any_connected)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  Uint32 retVal = 0;
  any_connected = false;
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 i = 0; i < recvdata.nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 node_id = t->getRemoteNodeId();
    Uint32 trp_id = t->getTransporterIndex();

    if (!recvdata.m_transporters.get(trp_id))
      continue;

    if (t->isConnected() && is_connected(node_id))
    {
      any_connected = true;
      if (t->hasDataToRead())
      {
        recvdata.m_has_data_transporters.set(trp_id);
        retVal = 1;
      }
    }
  }
#endif
  return retVal;
}

Uint32
TransporterRegistry::spin_check_transporters(
                          TransporterReceiveHandle& recvdata [[maybe_unused]])
{
  Uint32 res = 0;
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  Uint64 micros_passed = 0;
  bool any_connected = false;
  Uint64 spintime = Uint64(recvdata.m_spintime);

  if (spintime == 0)
  {
    return res;
  }
  NDB_TICKS start = NdbTick_getCurrentTicks();
  do
  {
    {
      res = poll_SHM(recvdata, any_connected);
      if (res || !any_connected)
        break;
    }
    if (res || !any_connected)
      break;
    res = check_TCP(recvdata, 0);
    if (res)
      break;
#ifdef NDB_HAVE_CPU_PAUSE
    NdbSpin();
#endif
    NDB_TICKS now = NdbTick_getCurrentTicks();
    micros_passed =
      NdbTick_Elapsed(start, now).microSec();
  } while (micros_passed < Uint64(recvdata.m_spintime));
  recvdata.m_total_spintime += micros_passed;
#endif
  return res;
}

Uint32
TransporterRegistry::pollReceive(Uint32 timeOutMillis,
                                 TransporterReceiveHandle& recvdata [[maybe_unused]])
{
  bool sleep_state_set [[maybe_unused]] = false;
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  Uint32 retVal = 0;
  recvdata.m_recv_transporters.clear();

  /**
   * If any transporters have left-over data that was not fully executed in
   * last loop, don't wait and return 'data available' even if nothing new
   */
  if (!recvdata.m_has_data_transporters.isclear())
  {
    timeOutMillis = 0;
    retVal = 1;
  }
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  if (recvdata.nSHMTransporters > 0)
  {
    /**
     * We start by checking shared memory transporters without
     * any mutexes or other protection. If we find something to
     * read we will set timeout to 0 and check the TCP transporters
     * before returning.
     */
    bool any_connected = false;
    Uint32 res = poll_SHM(recvdata, any_connected);
    if(res)
    {
      retVal |= res;
      timeOutMillis = 0;
    }
    else if (timeOutMillis > 0 && any_connected)
    {
      /**
       * We are preparing to wait for socket events. We will start by
       * polling for a configurable amount of microseconds before we
       * go to sleep. We will check both shared memory transporters and
       * TCP transporters in this period. We will check shared memory
       * transporter four times and then check TCP transporters in a
       * loop.
       *
       * After this polling period, if we are still waiting for data
       * we will prepare to go to sleep by informing the other side
       * that we are going to sleep.
       *
       * To do this we first grab the mutex used by the sender
       * to check for sleep/awake state, next we poll the shared
       * memory holding this mutex. If this check also returns
       * without finding any data we set the state to sleep,
       * release the mutex and go to sleep (on an epoll/poll)
       * that can be woken up by incoming data or a wakeup byte
       * sent to SHM transporter.
       */
      res = spin_check_transporters(recvdata);
      if (res)
      {
        retVal |= res;
        timeOutMillis = 0;
      }
      else
      {
        int res = reset_shm_awake_state(recvdata, sleep_state_set);
        if (res || !sleep_state_set)
        {
          /**
           * If sleep_state_set is false here it means that the
           * all SHM transporters were disconnected. Alternatively
           * there was data available on all the connected SHM
           * transporters. Read the data from TCP transporters and
           * return.
           */
          retVal |= 1;
          timeOutMillis = 0;
        }
      }
    }
  }
#endif
  retVal |= check_TCP(recvdata, timeOutMillis);
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  if (recvdata.nSHMTransporters > 0)
  {
    /**
     * If any SHM transporter was put to sleep above we will
     * set all connected SHM transporters to awake now.
     */
    if (sleep_state_set)
    {
      set_shm_awake_state(recvdata);
    }
    bool any_connected = false;
    int res = poll_SHM(recvdata, any_connected);
    retVal |= res;
  }
#endif
  return retVal;
}

/**
 * Every time a SHM transporter is sending data and wants the other side
 * to wake up to handle the data, it follows this procedure.
 * 1) Write the data in shared memory
 * 2) Acquire the mutex protecting the awake flag on the receive side.
 * 3) Read flag
 * 4) Release mutex
 * 5.1) If flag says that receiver is awake we're done
 * 5.2) If flag says that receiver is asleep we will send a byte on the
 *      transporter socket for the SHM transporter to wake up the other
 *      side.
 *
 * The reset_shm_awake_state is called right before we are going to go
 * to sleep. To ensure that we don't miss any signals from the other
 * side we will first check if there is data available on shared memory.
 * We first grab the mutex before checking this. If no data is available
 * we can proceed to go to sleep after setting the flag to indicate that
 * we are asleep. The above procedure used when sending means that we
 * know that the sender will always know the correct state. The only
 * error in this is that the sender might think that we are asleep when
 * we actually is still on our way to go to sleep. In this case no harm
 * has been done since the only thing that have happened is that one byte
 * is sent on the SHM socket that wasn't absolutely needed.
 */
int
TransporterRegistry::reset_shm_awake_state(TransporterReceiveHandle& recvdata [[maybe_unused]],
                                       bool& sleep_state_set [[maybe_unused]])
{
  int res = 0;
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 i = 0; i < recvdata.nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 node_id = t->getRemoteNodeId();
    Uint32 trp_id = t->getTransporterIndex();

    if (!recvdata.m_transporters.get(trp_id))
      continue;

    if (t->isConnected())
    {
      t->lock_mutex();
      if (is_connected(node_id))
      {
        if (t->hasDataToRead())
        {
          recvdata.m_has_data_transporters.set(trp_id);
          res = 1;
        }
        else
        {
          sleep_state_set = true;
          t->set_awake_state(0);
        }
      }
      t->unlock_mutex();
    }
  }
#endif
  return res;
}

/**
 * We have been sleeping for a while, before proceeding we need to set
 * the flag to awake in the shared memory. This will flag to all other
 * nodes using shared memory to communicate that we're awake and don't
 * need any socket communication to wake up.
 */
void
TransporterRegistry::set_shm_awake_state(TransporterReceiveHandle& recvdata [[maybe_unused]])
{
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 i = 0; i < recvdata.nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 id = t->getTransporterIndex();

    if (!recvdata.m_transporters.get(id))
      continue;
    if (t->isConnected())
    {
      t->lock_mutex();
      t->set_awake_state(1);
      t->unlock_mutex();
    }
  }
#endif
}

/**
 * We do not want to hold any transporter locks during select(), so there
 * is no protection against a disconnect closing the socket during this call.
 *
 * That does not matter, at most we will get a spurious wakeup on the wrong
 * socket, which will be handled correctly in performReceive() (which _is_
 * protected by transporter locks on upper layer).
 */
Uint32
TransporterRegistry::poll_TCP(Uint32 timeOutMillis,
                              TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  recvdata.m_socket_poller.clear();

  const bool extra_socket = m_has_extra_wakeup_socket;
  if (extra_socket && recvdata.m_transporters.get(0))
  {
    const ndb_socket_t socket = m_extra_wakeup_sockets[0];
    assert(&recvdata == receiveHandle); // not used by ndbmtd...

    // Poll the wakup-socket for read
    recvdata.m_socket_poller.add_readable(socket);
  }

  Uint16 idx[MAX_NTRANSPORTERS];
  Uint32 i = 0;
  for (; i < recvdata.nTCPTransporters; i++)
  {
    TCP_Transporter * t = theTCPTransporters[i];
    const ndb_socket_t socket = t->getSocket();
    Uint32 node_id = t->getRemoteNodeId();
    Uint32 trp_id = t->getTransporterIndex();

    idx[i] = maxTransporters + 1;
    if (!recvdata.m_transporters.get(trp_id))
      continue;

    if (is_connected(node_id) && t->isConnected() && ndb_socket_valid(socket))
    {
      idx[i] = recvdata.m_socket_poller.add_readable(socket);
    }
  }

#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  for (Uint32 j = 0; j < recvdata.nSHMTransporters; j++)
  {
    /**
     * We need to listen to socket also for shared memory transporters.
     * These sockets are used as a wakeup mechanism, so we're not sending
     * any data in it. But we need a socket to be able to wake up things
     * when the receiver is not awake and we've only sent data on shared
     * memory transporter.
     */
    SHM_Transporter * t = theSHMTransporters[j];
    const ndb_socket_t socket = t->getSocket();
    Uint32 node_id = t->getRemoteNodeId();
    Uint32 trp_id = t->getTransporterIndex();
    idx[i] = maxTransporters + 1;
    if (!recvdata.m_transporters.get(trp_id))
    {
      i++;
      continue;
    }
    if (is_connected(node_id) && t->isConnected() && ndb_socket_valid(socket))
    {
      idx[i] = recvdata.m_socket_poller.add_readable(socket);
    }
    i++;
  }
#endif
  int tcpReadSelectReply = recvdata.m_socket_poller.poll_unsafe(timeOutMillis);

  if (tcpReadSelectReply > 0)
  {
    if (extra_socket)
    {
      if (recvdata.m_socket_poller.has_read(0))
      {
        assert(recvdata.m_transporters.get(0));
        recvdata.m_recv_transporters.set((Uint32)0);
      }
    }
    i = 0;
    for (; i < recvdata.nTCPTransporters; i++)
    {
      TCP_Transporter * t = theTCPTransporters[i];
      if (idx[i] != maxTransporters + 1)
      {
        Uint32 trp_id = t->getTransporterIndex();
        if (recvdata.m_socket_poller.has_read(idx[i]))
        {
          recvdata.m_recv_transporters.set(trp_id);
        }
      }
    }
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
    for (Uint32 j = 0; j < recvdata.nSHMTransporters; j++)
    {
      /**
       * If a shared memory transporter have data on its socket we
       * will get it now, the data is only an indication for us to
       * wake up, so we're not interested in the data as such.
       * But to integrate it with epoll handling we will read it
       * in performReceive still.
       */
      SHM_Transporter * t = theSHMTransporters[j];
      if (idx[i] != maxTransporters + 1)
      {
        Uint32 trp_id = t->getTransporterIndex();
        if (recvdata.m_socket_poller.has_read(idx[i]))
          recvdata.m_recv_transporters.set(trp_id);
      }
      i++;
    }
#endif
  }

  return tcpReadSelectReply;
}

void
TransporterRegistry::set_recv_thread_idx(Transporter *t,
                                         Uint32 recv_thread_idx)
{
  t->set_recv_thread_idx(recv_thread_idx);
}

/**
 * Receive from the set of transporters in the bitmask
 * 'recvdata.m_transporters'. These has been polled by 
 * ::pollReceive() which recorded transporters with 
 * available data in the subset 'recvdata.m_recv_transporters'.
 *
 * In multi-threaded datanodes, there might be multiple 
 * receiver threads, each serving a disjunct set of
 * 'm_transporters'.
 *
 * Single-threaded datanodes does all ::performReceive
 * from the scheduler main-loop, and thus it will handle
 * all 'm_transporters'.
 *
 * Clients has to acquire a 'poll right' (see TransporterFacade)
 * which gives it the right to temporarily acts as a receive 
 * thread with the right to poll *all* transporters.
 *
 * Reception takes place on a set of transporters knowing to be in a
 * 'CONNECTED' state. Transporters can (asynch) become 'DISCONNECTING' 
 * while we performReceive(). There is *no* mutex lock protecting
 * 'disconnecting' from being started while we are in the receive-loop!
 * However, the contents of the buffers++  should still be in a 
 * consistent state, such that the current receive can complete
 * without failures. 
 *
 * With regular intervals we have to ::update_connections()
 * in order to bring DISCONNECTING transporters into
 * a DISCONNECTED state. At earliest at this point, resources
 * used by performReceive() may be reset or released.
 * A transporter should be brought to the DISCONNECTED state
 * before it can reconnect again. (Note: There is a break of
 * this rule in ::do_connect, see own note here)
 *
 * To not interfere with ::poll- or ::performReceive(),
 * ::update_connections() has to be synched with with these
 * methods. Either by being run within the same
 * receive thread (dataNodes), or protected by the 'poll rights'.
 *
 * In case we were unable to receive due to job buffers being full.
 * Returns 0 when receive succeeded from all Transporters having data,
 * else 1.
 */
Uint32
TransporterRegistry::performReceive(TransporterReceiveHandle& recvdata,
                                    Uint32 recv_thread_idx)
{
  (void)recv_thread_idx;
  TransporterReceiveWatchdog guard(recvdata);
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  bool stopReceiving = false;

  if (recvdata.m_recv_transporters.get(0))
  {
    assert(recvdata.m_transporters.get(0));
    assert(&recvdata == receiveHandle); // not used by ndbmtd
    recvdata.m_recv_transporters.clear(Uint32(0));
    consume_extra_sockets();
  }

#ifdef ERROR_INSERT
  if (!m_blocked_trp.isclear())
  {
    /* Exclude receive from blocked sockets. */
    recvdata.m_recv_transporters.bitANDC(m_blocked_trp);

    if (recvdata.m_recv_transporters.isclear()  &&
        recvdata.m_has_data_transporters.isclear())
    {
        /* poll sees data, but we want to ignore for now
         * sleep a little to avoid busy loop
         */
      NdbSleep_MilliSleep(1);
    }
  }
#endif

  /**
   * m_recv_transporters set indicates that there might be data
   * available on the socket used by the transporter. The
   * doReceive call will read the socket. For TCP transporters
   * the doReceive call will return an indication if there is
   * data to receive on socket. This will set m_has_data_transporters.
   * For SHM transporter the socket is only used to send wakeup
   * bytes. The m_has_data_transporters bitmap was set already in
   * pollReceive for SHM transporters.
   */
  for(Uint32 trp_id = recvdata.m_recv_transporters.find_first();
      trp_id != BitmaskImpl::NotFound;
      trp_id = recvdata.m_recv_transporters.find_next(trp_id + 1))
  {
    Transporter *transp = allTransporters[trp_id];
    NodeId node_id = transp->getRemoteNodeId();
    if (transp->getTransporterType() == tt_TCP_TRANSPORTER)
    {
      TCP_Transporter * t = (TCP_Transporter*)transp;
      assert(recvdata.m_transporters.get(trp_id));
      assert(recv_thread_idx == transp->get_recv_thread_idx());

      /**
       * First check connection 'is CONNECTED.
       * A connection can only be set into, or taken out of, is_connected'
       * state by ::update_connections(). See comment there about 
       * synchronication between ::update_connections() and 
       * performReceive()
       *
       * Transporter::isConnected() state my change asynch.
       * A mismatch between the TransporterRegistry::is_connected(),
       * and Transporter::isConnected() state is possible, and indicate 
       * that a change is underway. (Completed by update_connections())
       */
      if (is_connected(node_id))
      {
        if (t->isConnected())
        {
          int nBytes = t->doReceive(recvdata);
          if (nBytes > 0)
          {
            recvdata.transporter_recv_from(node_id);
            recvdata.m_has_data_transporters.set(trp_id);
          }
        }
      }
    }
    else
    {
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
      require(transp->getTransporterType() == tt_SHM_TRANSPORTER);
      SHM_Transporter * t = (SHM_Transporter*)transp;
      assert(recvdata.m_transporters.get(trp_id));
      if (is_connected(node_id) && t->isConnected())
      {
        t->doReceive();
        /**
         * Ignore any data we read, the data wasn't collected by the
         * shared memory transporter, it was simply read and thrown
         * away, it is only a wakeup call to send data over the socket
         * for shared memory transporters.
         */
      }
#else
      require(false);
#endif
    }
  }
  recvdata.m_recv_transporters.clear();

  /**
   * Unpack data either received above or pending from prev rounds.
   * For the Shared memory transporter m_has_data_transporters can
   * be set in pollReceive as well.
   *
   * TCP Transporter
   * ---------------
   * Data to be processed at this stage is in the Transporter 
   * receivebuffer. The data *is received*, and will stay in
   * the  receiveBuffer even if a disconnect is started during
   * unpack. 
   * When ::update_connection() finally completes the disconnect,
   * (synced with ::performReceive()), 'm_has_data_transporters'
   * will be cleared, which will terminate further unpacking.
   *
   * NOTE:
   *  Without reading inconsistent date, we could have removed
   *  the 'connected' checks below, However, there is a requirement
   *  in the CLOSE_COMREQ/CONF protocol between TRPMAN and QMGR
   *  that no signals arrives from disconnecting nodes after a
   *  CLOSE_COMCONF was sent. For the moment the risk of taking
   *  advantage of this small optimization is not worth the risk.
   */
  Uint32 trp_id = recvdata.m_last_trp_id;
  while ((trp_id = recvdata.m_has_data_transporters.find_next(trp_id + 1)) !=
	 BitmaskImpl::NotFound)
  {
    bool hasdata = false;
    Transporter * t = (Transporter*)allTransporters[trp_id];
    NodeId node_id = t->getRemoteNodeId();

    assert(recvdata.m_transporters.get(trp_id));

    if (is_connected(node_id))
    {
      if (t->isConnected())
      {
        if (unlikely(recvdata.checkJobBuffer()))
        {
          recvdata.m_last_trp_id = trp_id;  //Resume from node after 'last_node'
          return 1;     // Full, can't unpack more
        }
        if (unlikely(recvdata.m_handled_transporters.get(trp_id)))
          continue;     // Skip now to avoid starvation
        if (t->getTransporterType() == tt_TCP_TRANSPORTER)
        {
          TCP_Transporter *t_tcp = (TCP_Transporter*)t;
          Uint32 * ptr;
          Uint32 sz = t_tcp->getReceiveData(&ptr);
          Uint32 szUsed = unpack(recvdata,
                                 ptr,
                                 sz,
                                 node_id,
                                 ioStates[node_id],
                                 stopReceiving);
          if (likely(szUsed))
          {
            assert(recv_thread_idx == t_tcp->get_recv_thread_idx());
            t_tcp->updateReceiveDataPtr(szUsed);
            hasdata = t_tcp->hasReceiveData();
          }
        }
        else
        {
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
          require(t->getTransporterType() == tt_SHM_TRANSPORTER);
          SHM_Transporter *t_shm = (SHM_Transporter*)t;
          Uint32 * readPtr, * eodPtr, * endPtr;
          t_shm->getReceivePtr(&readPtr, &eodPtr, &endPtr);
          recvdata.transporter_recv_from(node_id);
          Uint32 *newPtr = unpack(recvdata,
                                  readPtr,
                                  eodPtr,
                                  endPtr,
                                  node_id,
                                  ioStates[node_id],
				  stopReceiving);
          t_shm->updateReceivePtr(recvdata, newPtr);
          /**
           * Set hasdata dependent on if data is still available in
           * transporter to ensure we follow rules about setting
           * m_has_data_transporters and m_handled_transporters
           * when returning from performReceive.
           */
          hasdata = t_shm->hasDataToRead();
#else
          require(false);
#endif
        }
        // else, we didn't unpack anything:
        //   Avail ReceiveData to short to be useful, need to
        //   receive more before we can resume this transporter.
      }
    }
    // If transporter still have data, make sure that it's remember to next time
    recvdata.m_has_data_transporters.set(trp_id, hasdata);
    recvdata.m_handled_transporters.set(trp_id, hasdata);

    if (unlikely(stopReceiving))
    {
      recvdata.m_last_trp_id = trp_id;  //Resume from node after 'last_node'
      return 1;
    }
  }
  recvdata.m_handled_transporters.clear();
  recvdata.m_last_trp_id = 0;
  return 0;
}

void
TransporterRegistry::consume_extra_sockets()
{
  char buf[4096];
  ssize_t ret;
  int err;
  ndb_socket_t sock = m_extra_wakeup_sockets[0];
  do
  {
    ret = ndb_recv(sock, buf, sizeof(buf), 0);
    err = ndb_socket_errno();
  } while (ret == sizeof(buf) || (ret == -1 && err == EINTR));

  /* Notify upper layer of explicit wakeup */
  callbackObj->reportWakeup();
}

/**
 * performSend() - Call physical transporters to 'doSend'
 * of previously prepareSend() signals.
 *
 * The doSend() implementations will call
 * TransporterCallback::get_bytes_to_send_iovec() to fetch
 * any available data from the send buffer.
 *
 * *This* ^^ is the synch point where we under mutex protection
 * may check for specific nodes being disconnected/disabled.
 * For disabled nodes we may drain the send buffers instead of
 * returning anything from get_bytes_to_send_iovec().
 * Also see comments for prepareSend() above.
 *
 * Note that since disconnection may happen asynch from other
 * threads, we can not reliably check the 'connected' state
 * before doSend(). Instead we must require that the 
 * TransporterCallback implementation provide necessary locking
 * of get_bytes_to_send() vs enable/disable of send buffers.
 *
 * Returns:
 *   true if anything still remains to be sent.
 *   Will require another ::performSend()
 *
 *   false: if nothing more remains, either due to
 *   the send buffers being empty, we succeeded
 *   sending everything, or we found the node to be
 *   disconnected and thus discarded the contents.
 */
bool
TransporterRegistry::performSendNode(NodeId nodeId, bool need_wakeup)
{
  Transporter *t = get_node_transporter(nodeId);
  if (t != nullptr)
  {
#ifdef ERROR_INSERT
    if (m_sendBlocked.get(nodeId))
    {
      return true;
    }
#endif
    return t->doSend(need_wakeup);
  }
  return false;
}

bool
TransporterRegistry::performSend(TrpId trp_id, bool need_wakeup)
{
  Transporter *t = get_transporter(trp_id);
  if (t != nullptr)
  {
#ifdef ERROR_INSERT
    NodeId nodeId = t->getRemoteNodeId();
    if (m_sendBlocked.get(nodeId))
    {
      return true;
    }
#endif
    return t->doSend(need_wakeup);
  }
  return false;
}

void
TransporterRegistry::performSend()
{
  Uint32 i; 
  sendCounter = 1;

  lockMultiTransporters();
  for (i = m_transp_count; i < (nTransporters + 1); i++) 
  {
    Transporter *t = allTransporters[i];
    if (t != nullptr
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  for (i = 1; i < m_transp_count && i < (nTransporters + 1); i++) 
  {
    Transporter *t = allTransporters[i];
    if (t != nullptr
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  m_transp_count++;
  if (m_transp_count == (nTransporters + 1))
    m_transp_count = 1;
  unlockMultiTransporters();
}

#ifdef DEBUG_TRANSPORTER
void
TransporterRegistry::printState()
{
  ndbout << "-- TransporterRegistry -- " << endl << endl
         << "Transporters = " << nTransporters << endl;
  for(Uint32 i = 0; i < maxTransporters; i++)
  {
    if (allTransporters[i] != NULL)
    {
      const NodeId remoteNodeId = allTransporters[i]->getRemoteNodeId();
      ndbout << "Transporter: " << remoteNodeId 
             << " PerformState: " << performStates[remoteNodeId]
             << " IOState: " << ioStates[remoteNodeId] << endl;
    }
  }
}
#else
void
TransporterRegistry::printState()
{
}
#endif

#ifdef ERROR_INSERT
bool
TransporterRegistry::isBlocked(NodeId nodeId)
{
  return m_blocked.get(nodeId);
}

void
TransporterRegistry::blockReceive(TransporterReceiveHandle& recvdata,
                                  NodeId nodeId)
{
  TrpId ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;
  lockMultiTransporters();
  get_trps_for_node(nodeId, ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);

  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  for (Uint32 i = 0; i < num_ids; i++)
  {
    Uint32 trp_id = ids[i];
    if (recvdata.m_transporters.get(trp_id))
      m_blocked_trp.set(trp_id);
  }
  /* Check that node is not already blocked?
   * Stop pulling from its socket (but track received data etc)
   */
  /* Shouldn't already be blocked with data */
  bool last_call = true;
  for (Uint32 i = 0; i < num_ids; i++)
  {
    Uint32 trp_id = ids[i];
    if (!m_blocked_trp.get(trp_id))
      last_call = false;
  }
  if (last_call)
    m_blocked.set(nodeId);
  unlockMultiTransporters();
}

void
TransporterRegistry::unblockReceive(TransporterReceiveHandle& recvdata,
                                    NodeId nodeId)
{
  TrpId ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;
  lockMultiTransporters();
  get_trps_for_node(nodeId, ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);

  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  for (Uint32 i = 0; i < num_ids; i++)
  {
    Uint32 trp_id = ids[i];
    if (recvdata.m_transporters.get(trp_id))
    {
      assert(m_blocked_trp.get(trp_id));
      assert(!recvdata.m_has_data_transporters.get(trp_id));
      m_blocked_trp.clear(trp_id);
    }
  }
  bool last_call = true;
  for (Uint32 i = 0; i < num_ids; i++)
  {
    Uint32 trp_id = ids[i];
    if (m_blocked_trp.get(trp_id))
    {
      last_call = false;
    }
  }

  /* Check that node is blocked?
   * Resume pulling from its socket
   * Ensure in-flight data is processed if there was some
   */
  if (last_call)
    m_blocked.clear(nodeId);
  unlockMultiTransporters();
  if (m_blocked_disconnected.get(nodeId))
  {
    /* Process disconnect notification/handling now */
    report_disconnect(recvdata, nodeId, m_disconnect_errors[nodeId]);
  }
  if (last_call)
  {
    lockMultiTransporters();
    m_blocked_disconnected.clear(nodeId);
    unlockMultiTransporters();
  }
}

bool
TransporterRegistry::isSendBlocked(NodeId nodeId) const
{
  return m_sendBlocked.get(nodeId);
}

void TransporterRegistry::blockSend(TransporterReceiveHandle &recvdata
                                    [[maybe_unused]],
                                    NodeId nodeId)
{
#ifdef VM_TRACE
  TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;
  lockMultiTransporters();
  get_trps_for_node(nodeId, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);
  unlockMultiTransporters();
#endif
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  m_sendBlocked.set(nodeId);
}

void TransporterRegistry::unblockSend(TransporterReceiveHandle &recvdata
                                      [[maybe_unused]],
                                      NodeId nodeId)
{
#ifdef VM_TRACE
  TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;
  lockMultiTransporters();
  get_trps_for_node(nodeId, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);
  unlockMultiTransporters();
#endif
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  m_sendBlocked.clear(nodeId);
}

#endif

#ifdef ERROR_INSERT
Uint32
TransporterRegistry::getMixologyLevel() const
{
  return m_mixology_level;
}

extern Uint32 MAX_RECEIVED_SIGNALS;  /* Packer.cpp */

#define MIXOLOGY_MIX_INCOMING_SIGNALS 4

void
TransporterRegistry::setMixologyLevel(Uint32 l)
{
  m_mixology_level = l;
  
  if (m_mixology_level & MIXOLOGY_MIX_INCOMING_SIGNALS)
  {
    g_eventLogger->info("MIXOLOGY_MIX_INCOMING_SIGNALS on");
    /* Max one signal per transporter */
    MAX_RECEIVED_SIGNALS = 1;
  }

  /* TODO : Add mixing of Send from NdbApi / MGMD */
}
#endif

IOState
TransporterRegistry::ioState(NodeId nodeId) const { 
  return ioStates[nodeId]; 
}

void
TransporterRegistry::setIOState(NodeId nodeId, IOState state) {
  if (ioStates[nodeId] == state)
    return;

  DEBUG("TransporterRegistry::setIOState("
        << nodeId << ", " << state << ")");

  ioStates[nodeId] = state;
}

extern "C" void *
run_start_clients_C(void * me)
{
  ((TransporterRegistry*) me)->start_clients_thread();
  return nullptr;
}

/**
 * This method is used to initiate connection, called from the TRPMAN block.
 *
 * This works asynchronously, no actions are taken directly in the calling
 * thread.
 */
void
TransporterRegistry::do_connect(NodeId node_id)
{
  PerformState &curr_state = performStates[node_id];
  switch(curr_state){
  case DISCONNECTED:
    break;
  case CONNECTED:
    return;
  case CONNECTING:
    return;
  case DISCONNECTING:
    /**
     * NOTE (Need future work)
     * Going directly from DISCONNECTING to CONNECTING creates
     * a possible race with ::update_connections(): It will
     * see either of the *ING states, and bring the connection
     * into CONNECTED or *DISCONNECTED* state. Furthermore, the
     * state may be overwritten to CONNECTING by this method.
     * We should probably have waited for DISCONNECTED state,
     * before allowing reCONNECTING ....
     */
    assert(false);
    break;
  }
  DEBUG_FPRINTF((stderr, "(%u)REG:do_connect(%u)\n", localNodeId, node_id));
  DBUG_ENTER("TransporterRegistry::do_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTING",node_id));

  Transporter * t = theNodeIdTransporters[node_id];
  if (t != nullptr)
  {
    if (t->isMultiTransporter())
    {
      Multi_Transporter *multi_trp = (Multi_Transporter*)t;
      require(multi_trp->get_num_active_transporters() == 1);
      t = multi_trp->get_active_transporter(0);
    }
    require(!t->isPartOfMultiTransporter());
    require(!t->isMultiTransporter());
    DEBUG_FPRINTF((stderr, "(%u)REG:resetBuffers(%u)\n",
                           localNodeId, node_id));
    t->resetBuffers();
  }
 
  DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = CONNECTING\n",
                 localNodeId, node_id));
  curr_state= CONNECTING;
  DBUG_VOID_RETURN;
}

/**
 * This method is used to initiate disconnect from TRPMAN. It is also called
 * from the TCP/SHM transporter in case of an I/O error on the socket.
 *
 * This works asynchronously, similar to do_connect().
 */
bool
TransporterRegistry::do_disconnect(NodeId node_id,
                                   int errnum,
                                   bool send_source)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:do_disconnect(%u, %d)\n",
                         localNodeId, node_id, errnum));
  PerformState &curr_state = performStates[node_id];
  switch(curr_state){
  case DISCONNECTED:
  {
    return true;
  }
  case CONNECTED:
  {
    break;
  }
  case CONNECTING:
    /**
     * This is a correct transition. But it should only occur for nodes
     * that lack resources, e.g. lack of shared memory resources to
     * setup the transporter. Therefore we assert here to get a simple
     * handling of test failures such that we can fix the test config.
     */
    //assert(false);
    break;
  case DISCONNECTING:
  {
    return true;
  }
  }
  if (errnum == ENOENT)
  {
    m_disconnect_enomem_error[node_id]++;
    if (m_disconnect_enomem_error[node_id] < 10)
    {
      NdbSleep_MilliSleep(40);
      g_eventLogger->info("Socket error %d on nodeId: %u in state: %u",
                          errnum, node_id, (Uint32)curr_state);
      return false;
    }
  }
  if (errnum == 0)
  {
    g_eventLogger->info("Node %u disconnected in state: %d",
                        node_id, (int)curr_state);
  }
  else
  {
    g_eventLogger->info("Node %u disconnected in %s with errnum: %d"
                        " in state: %d",
                        node_id,
                        send_source ? "send" : "recv",
                        errnum,
                        (int)curr_state);
  }
  DBUG_ENTER("TransporterRegistry::do_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTING",node_id));
  curr_state= DISCONNECTING;
  DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = DISCONNECTING\n",
                 localNodeId, node_id));
  m_disconnect_errnum[node_id] = errnum;
  DBUG_RETURN(false);
}

/**
 * report_connect() / report_disconnect()
 *
 * Connect or disconnect the 'TransporterReceiveHandle' and 
 * enable/disable the send buffers.
 *
 * To prevent races wrt poll/receive of data, these methods must
 * either be called from the same (receive-)thread as performReceive(),
 * or by the (API) client holding the poll-right.
 *
 * The send buffers needs similar protection against concurrent
 * enable/disable of the same send buffers. Thus the sender
 * side is also handled here.
 */
void
TransporterRegistry::report_connect(TransporterReceiveHandle& recvdata,
                                    NodeId node_id)
{
  Transporter *t = theNodeIdTransporters[node_id];
  if (t->isMultiTransporter())
  {
    Multi_Transporter *multi_trp = (Multi_Transporter*)t;
    require(multi_trp->get_num_active_transporters() == 1);
    t = multi_trp->get_active_transporter(0);
  }
  require(!t->isMultiTransporter());
  require(!t->isPartOfMultiTransporter());
  Uint32 id = t->getTransporterIndex();
  DEBUG_FPRINTF((stderr, "(%u)REG:report_connect(%u)\n",
                         localNodeId, node_id));
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));
  assert(recvdata.m_transporters.get(id));

  DBUG_ENTER("TransporterRegistry::report_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTED",node_id));

  if (recvdata.epoll_add(t))
  {
    callbackObj->enable_send_buffer(node_id, id);
    DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = CONNECTED\n",
                   localNodeId, node_id));
    performStates[node_id] = CONNECTED;
    recvdata.reportConnect(node_id);
    DBUG_VOID_RETURN;
  }

  /**
   * Failed to add to epoll_set...
   *   disconnect it (this is really really bad)
   */
  DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = DISCONNECTING\n",
                 localNodeId, node_id));
  performStates[node_id] = DISCONNECTING;
  DBUG_VOID_RETURN;
}

Multi_Transporter*
TransporterRegistry::get_node_multi_transporter(NodeId node_id)
{
  Uint32 i;
  Uint32 num_multi_transporters = get_num_multi_transporters();
  for (i = 0; i < num_multi_transporters; i++)
  {
    Multi_Transporter *multi_trp = get_multi_transporter(i);
    if (multi_trp->getRemoteNodeId() == node_id)
    {
      return multi_trp;
    }
  }
  return (Multi_Transporter*)nullptr;

}

void
TransporterRegistry::report_disconnect(TransporterReceiveHandle& recvdata,
                                       NodeId node_id, int errnum)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:report_disconnect(%u, %d)\n",
                         localNodeId, node_id, errnum));
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  DBUG_ENTER("TransporterRegistry::report_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTED",node_id));

#ifdef ERROR_INSERT
  if (m_blocked.get(node_id))
  {
    /* We are simulating real latency, so control events experience
     * it too
     */
    m_blocked_disconnected.set(node_id);
    m_disconnect_errors[node_id] = errnum;
    DBUG_VOID_RETURN;
  }
#endif

  /**
   * No one else should be using the transporter now,
   * reset its send buffer and recvdata.
   *
   * Note that we may 'do_disconnect' due to transporter failure,
   * while trying to 'CONNECTING'. This cause a transition
   * from CONNECTING to DISCONNECTING without first being CONNECTED.
   * Thus there can be multiple reset & disable of the buffers (below)
   * without being 'enabled' in between.
   */
  TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;
  lockMultiTransporters();
  get_trps_for_node(node_id, trp_ids, num_ids, MAX_NODE_GROUP_TRANSPORTERS);

  bool ready_to_disconnect = true;
  Transporter *node_trp = theNodeIdTransporters[node_id];
  for (Uint32 i = 0; i < num_ids; i++)
  {
    Uint32 trp_id = trp_ids[i];
    DEBUG_FPRINTF((stderr, "trp_id = %u, node_id = %u\n", trp_id, node_id));
    if (recvdata.m_transporters.get(trp_id))
    {
      /**
       * disable_send_buffer ensures that no more signals will be sent
       * to the disconnected node. Every time we collect data for sending
       * we will check the send buffer enabled flag holding the m_send_lock
       * thereby ensuring that after the disable_send_buffer method is
       * called no more signals are sent.
       */
      callbackObj->disable_send_buffer(node_id, trp_id);
      recvdata.m_recv_transporters.clear(trp_id);
      recvdata.m_has_data_transporters.clear(trp_id);
      recvdata.m_handled_transporters.clear(trp_id);
    }
    else
    {
      /**
       * Transporter is handled by another receive thread. If the
       * transporter is still in the allTransporters array it means
       * that we haven't called report_disconnect in this receive
       * thread yet. Thus we are not yet ready to disconnect.
       */
      require(node_trp->isMultiTransporter());
      if (allTransporters[trp_id] != nullptr)
      {
        ready_to_disconnect = false;
        DEBUG_FPRINTF((stderr, "trp_id = %u, node_id = %u,"
                               " ready_to_disconnect = false\n",
                               trp_id, node_id));
      }
    }
  }
  if (node_trp->isMultiTransporter())
  {
    /**
     * Switch back to having only one active transporter which is the
     * original TCP or SHM transporter. We only handle the single
     * transporter case when setting up a new connection. We can
     * only move from single transporter to multi transporter when
     * the transporter is connected.
     */
    Multi_Transporter *multi_trp = (Multi_Transporter*)node_trp;
    for (Uint32 i = 0; i < num_ids; i++)
    {
      Uint32 trp_id = trp_ids[i];
      if (recvdata.m_transporters.get(trp_id))
      {
        /**
         * Remove the transporter from the set of transporters handled in
         * this receive thread.
         *
         * Removing it from this bitmask means that the receive thread
         * will ignore this transporter when receiving and polling.
         */
        DEBUG_FPRINTF((stderr, "trp_id = %u, node_id = %u,"
                               " multi remove_all\n",
                               trp_id, node_id));
        Transporter *t = multi_trp->get_active_transporter(i);
        t->doDisconnect();
        if (t->isPartOfMultiTransporter())
        {
          require(num_ids > 1);
          remove_allTransporters(t);
        }
        else
        {
          require(num_ids == 1);
          Uint32 num_multi_trps = multi_trp->get_num_inactive_transporters();
          for (Uint32 i = 0; i < num_multi_trps; i++)
          {
            Transporter *remove_trp = multi_trp->get_inactive_transporter(i);
            TrpId remove_trp_id = remove_trp->getTransporterIndex();
            if (remove_trp_id != 0)
            {
              NodeId remove_node_id = remove_trp->getRemoteNodeId();
              require(node_id == remove_node_id);
              callbackObj->disable_send_buffer(node_id, remove_trp_id);
              remove_trp->doDisconnect();
              remove_allTransporters(remove_trp);
            }
          }
        }
      }
    }
    if (ready_to_disconnect)
    {
      /**
       * All multi transporter objects for this node have been removed
       * from being handled by any receive thread. We set the single
       * transporter used for setup of connections to be handled by
       * this receive thread. This assignment means that we can get
       * an unbalanced load on receive threads. However normally the
       * transporters should return to using multiple transporters as
       * soon as the connection is setup again. So this imbalance should
       * be temporary.
       */
      DEBUG_FPRINTF((stderr, "node_id = %u, new_node_trp\n",
                     node_id));
      if (multi_trp->get_num_active_transporters() > 1)
      {
        /**
         * Multi socket setup is in use, thus switch to single transporter.
         * In rare situations we can still have data to send on base
         * transporter, so need to remove the send buffer before
         * disconnecting. Also ensure that connection is dropped here.
         */
        multi_trp->switch_active_trp();
        Transporter *base_trp = multi_trp->get_active_transporter(0);
        NodeId base_node_id = base_trp->getRemoteNodeId();
        TrpId base_trp_id = base_trp->getTransporterIndex();
        require(base_node_id == node_id);
        callbackObj->disable_send_buffer(node_id, base_trp_id);
        base_trp->doDisconnect();
      }
    }
  }
  else
  {
    Multi_Transporter *multi_trp = get_node_multi_transporter(node_id);
    (void)multi_trp;
    DEBUG_FPRINTF((stderr, "node_id = %u, multi_trp not %s\n",
                   node_id, multi_trp == 0 ? "existing" : "active"));
  }
  recvdata.m_bad_data_transporters.clear(node_id);
  recvdata.m_last_trp_id = 0;
  if (ready_to_disconnect)
  {
    DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = DISCONNECTED\n",
                   localNodeId, node_id));
    performStates[node_id] = DISCONNECTED;
    recvdata.reportDisconnect(node_id, errnum);
  }
  unlockMultiTransporters();
  DBUG_VOID_RETURN;
}

/**
 * We only call TransporterCallback::reportError() from
 * TransporterRegistry::update_connections().
 *
 * In other places we call this method to enqueue the error that will later be
 * picked up by update_connections().
 */
void
TransporterRegistry::report_error(NodeId nodeId, TransporterError errorCode,
                                  const char *errorInfo)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:report_error(%u, %d, %s\n",
                         localNodeId, nodeId, (int)errorCode, errorInfo));
  if (m_error_states[nodeId].m_code == TE_NO_ERROR &&
      m_error_states[nodeId].m_info == (const char *)~(UintPtr)0)
  {
    m_error_states[nodeId].m_code = errorCode;
    m_error_states[nodeId].m_info = errorInfo;
  }
}

/**
 * Data node transporter model (ndbmtd)
 * ------------------------------------
 * The concurrency protection of transporters uses a number of mutexes
 * together with some things that can only happen in a single thread.
 * To explain this we will consider send and receive separately.
 * We also need to consider multi socket transporters distinctively since
 * they add another layer to the concurrency model.
 *
 * Each transporter is protected by two mutexes. One to protect send and
 * one to protect receives. Actually the send mutex is divided into two
 * mutexes as well.
 *
 * Starting with send when we send a signal we write it only in thread-local
 * buffers at first. Next at regular times we move them to buffers per
 * transporter. To move them from these thread-local buffers to the
 * transporter buffers requires acquiring the send buffer mutex. This
 * activity is normally called flush buffers.
 *
 * Next the transporter buffers are sent, this requires holding the
 * buffer mutex as well as the send mutex when moving the buffers to the
 * transporter. While sending only the send mutex is required. This mutex
 * is held also when performing the send call on the transporter.
 *
 * The selection of the transporter to send to next is handled by a
 * a global send thread mutex. This mutex ensures that we send 50% of the
 * time to our neighbour nodes (data nodes within the same nodegroup) and
 * 50% to the rest of the nodes in the cluster. Thus we give much higher
 * priority to messages performing write transactions in the cluster as
 * well as local reads within the same nodegroup.
 *
 * For receive we use a model where a transporter is only handled by one
 * receive thread. This thread has its own mutex to protect against
 * interaction with other threads for transporter connect and disconnect.
 * This mutex is held while calling performReceive. performReceive
 * receives data from those transporters that signalled that data was
 * available in pollReceive. The mutex isn't held while calling pollReceive.
 *
 * Connect of the transporter requires not so much protection. Nothing
 * will be received until we add the transporter to the epoll set (even
 * shared memory transporters use epoll sets (implemented using poll in
 * non-Linux platforms). The epoll_add call happens in the receive thread
 * owning the transporter, thus no call to pollReceive will happen on the
 * transporter while we are calling update_connections where the
 * report_connect is called from when a connect has happened.
 *
 * After adding the transporter to the epoll set we enable the send
 * buffers, this uses both the send mutex and the send buffer mutex
 * on the transporter.
 *
 * Finally a signal is sent to TRPMAN in the receive thread (actually
 * the same thread where the update_connections is sent from. This signal
 * is simply sent onwards to QMGR plus some logging of the connection
 * event.
 *
 * CONNECT_REP in QMGR updates the node information and initiates the
 * CM_REGREQ protocol for data nodes and API_REGREQ for API nodes to
 * include the node in an organised manner. At this point only QMGR
 * is allowed to receive signals from the node. QMGR will allow for
 * any block to communicate with the node when finishing the
 * registration.
 *
 * The actual connect logic happens in start_clients_thread that
 * executes in a single thread that handles all client connect setup.
 * This thread regularly wakes up and calls connect on transporters
 * not yet connected. It has backoff logic to ensure these connects
 * don't happen too often. It will only perform this connect client
 * attempts when the node is in the state CONNECTING. The QMGR code
 * will ensure that we set this state when we are ready to include
 * the node into the cluster again. After a node failure we have to
 * handle the node failure to completion before we allow the node
 * to reenter the cluster.
 *
 * Connecting a client requires both a connection to be setup, in
 * addition the client and the server must execute an authentication
 * protocol and a protocol where we signal what type of transporter
 * that is connecting.
 *
 * The server port to connect the client is either provided by the
 * configuration (recommended in any scenario where firewalls exists)
 * in the network. If not configured the server uses a dynamic port
 * and informs the management server about this port number.
 *
 * When the client has successfully connected and the authentication
 * protocol is successfully completed and the information about the
 * transporter type is sent, then we will set the transporter state
 * to connected. This will be picked up by update_connections that
 * will perform the logic above.
 *
 * When the connection has been successfully completed we set the
 * variable theSocket in the transporter. To ensure that we don't
 * call any socket actions on a socket we already closed we protect
 * this assignment with both the send mutex and the receive mutex.
 *
 * The actual network logic used by the start_clients_thread is
 * found in the SocketClient class.
 *
 * Similar logic exists also for transporters where the node is the
 * server part. As part of starting the data node we setup a socket
 * which we bind to the hostname and server port to use for this
 * data node. Next we will start to listen for connect attempts by
 * clients.
 *
 * When a client attempts to connect we will see this through the
 * accept call on the socket. The verification of this client
 * attempt is handled by the newSession call. For data nodes
 * this is the newSession here in TransporterRegistry.cpp.
 * This method will first handle the authentication protocol where
 * the client follows the NDB connect setup protocol to verify that
 * it is an NDB connection being setup. Next we call connect_server
 * in the TransporterRegistry.
 *
 * connect_server will receive information about node id, transporter type
 * that will enable it to verify that the connect is ok to perform at the
 * moment. It is very normal that there are many attempts to connect that
 * are unsuccessful since the API nodes are not allowed to connect until
 * the data node is started and they will regularly attempt to connect
 * while the data node is starting.
 *
 * When the connect has been successfully setup we will use the same logic
 * as for clients where we set theSocket and set the transporter state to
 * connected to ensure that update_connections will see the new
 * connection. So there is a lot of logic to setup the connections, but
 * really only the setting of theSocket requires mutex protection.
 *
 * Disconnects can be discovered by both the send logic as well as the
 * receive logic when calling socket send and socket recv. In both cases
 * the transporter will call do_disconnect. The only action here is to
 * set the state to DISCONNECTING (no action if already set or the state
 * is already set to DISCONNECTED). While calling this function we can
 * either hold the send mutex or the receive mutex. But none of these
 * are needed since we only set the DISCONNECTING state. This state
 * change will be picked up by start_clients_thread (note that it will
 * be picked by this method independent of if it connected as a client
 * or as a server. This method will call doDisconnect on the transporter.
 * This method will update the state and next it will call
 * disconnectImpl on the transporter that will close the socket and
 * ensure that the variable theSocket no longer points to a valid
 * socket. This is again protected by the send mutex and the receive
 * mutex to ensure that it doesn't happen while we are calling any
 * send or recv calls. Here we will also close the socket.
 *
 * Setting the state to not connected in doDisconnect will flag to
 * update_connections that it should call report_disconnect. This
 * call will disable the send buffer (holding the send mutex and
 * send buffer mutex), it will also clear bitmasks for the transporter
 * used by the receive thread. It requires no mutex to protect these
 * changes since they are performed in the receive thread that is
 * responsible for receiving on this transporter. The call can be
 * performed in all receive threads, but will be ignored by any
 * receive thread not responsible for it.
 *
 * Next it will call reportDisconnect that will send a signal
 * DISCONNECT_REP to our TRPMAN instance. This signal will be
 * sent to QMGR, the disconnect event will be logged and we will
 * send a signal to CMVMI to cancel subscriptions for the node.
 *
 * QMGR will update node state information and will initiate
 * node failure handling if not already started.
 *
 * QMGR will control when it is ok to communicate with the node
 * using CLOSE_COMREQ and OPEN_COMREQ calls to TRPMAN.
 *
 * Closing the communication to a node can also be initiated from
 * receiving a signal from another node that the node is dead.
 * In this case QMGR will send CLOSE_COMREQ to TRPMAN instances and
 * TRPMAN will set the IO state to HaltIO to ensure no more signals
 * are handled and it will call do_disconnect in TransporterRegistry
 * to ensure that the above close sequence is called although the
 * transporters are still functional.
 *
 * Introducing multi transporters
 * ------------------------------
 * To enable higher communication throughput between data nodes it is
 * possible to setup multiple transporters for communicating with another
 * data node. Currently this feature only allows for multiple transporters
 * when communicating within the same node group. There is no principal
 * problem with multiple transporters for other communication between
 * data nodes as well. For API nodes we already have the ability to
 * use multiple transporters from an application program by using
 * multiple node ids. Thus it is not necessary to add multiple
 * transporters to communicate with API nodes.
 *
 * The connect process cannot use multiple transporters. The reason is
 * that we cannot be certain that the connecting node supports multiple
 * transporters between two data nodes. We can only check this once the
 * connection is setup.
 *
 * This means that the change from a single socket to multiple sockets
 * is handled when the node is in a connected state and thus traffic
 * between the nodes is happening as we setup the multi socket setup.
 *
 * Given that multiple transporters can only be used between data nodes
 * we don't need to handle the Shared memory transporter as used for
 * multiple transporters since Shared memory transporter can only be
 * used to communicate between data nodes and API nodes.
 *
 * Thus multiple transporters is only handled by TCP transporters.
 *
 * Additionally to setup multiple transporters we need to know which
 * nodes are part of the same node group. This information is available
 * already in start phase 3 for initial starts and for cluster restarts.
 * For node restarts and initial node restarts we will wait until start
 * phase 4 to setup multiple transporters. In addition we have the case
 * where node groups are added while nodes are up and running, in this
 * case the nodes in the new node group will setup multiple transporters
 * when the new node group creation is committed.
 * 
 * The setup logic for multiple transporters is described in detail in
 * QmgrMain.cpp. Here we will focus on how it changes the connect/disconnect
 * logic and what concurrency protection is required to handle the
 * multiple transporters.
 *
 * We introduce a new mutex that is locked by calling lockMultiTransporters
 * and unlocked through the call unlockMultiTransporters. This mutex should
 * always be taken before any other mutex is acquired to avoid deadlocks.
 *
 * During setup we communicate between the nodes in the same nodegroup to
 * decide on the number of multi sockets to use for one transporter. The
 * multiple transporters are handled also by a new Transporter class called
 * Multi_Transporter. When the change is performed to use multi transporters
 * this transporter is placed into the node transporter array. It is never
 * removed from this array even after a node failure.
 *
 * When we setup the multi transporters we assign them to different recv
 * threads. We also ensure that the port number is copied from the base
 * transporter to the new multi transporters to ensure that we use the
 * dynamic port number for a new node after a restart.
 *
 * The way to activate the connect of the multiple transporters happens by
 * inserting them into the allTransporters array. By inserting them into
 * this array they will be handled by the start_clients_thread.
 *
 * The protocol to setup a multi transporter is slightly different since we
 * need to know the node id, transporter type and additionally the instance
 * number of the transporter being setup in the connection. To accept a
 * connection we must be in the CONNECTED state for the node and the instance
 * number must exist in the multi transporter and must not be already
 * connected.
 *
 * Next step is to perform switch activity, when we agreed with the other
 * node on to perform this action we will send a crucial signal
 * ACTIVATE_TRP_REQ. This signal will be sent on the base transporter.
 * Immediately after sending this signal we will switch to using multi
 * transporters.
 *
 * Before sending this signal we ensure that the only thread active is the
 * main thread and we lock all send mutex for both the base transporter
 * and the new multi transporters. We perform this by a new set of
 * signals to freeze threads.
 *
 * When receiving this signal in the other node we need to use
 * SYNC_THREAD_VIA_REQ. This ensures that all signals sent using the
 * base transporter have been executed before we start executing the
 * signals arriving on the new multi transporters. This ensures that
 * we always keep signal order even in the context of changing
 * the number of transporters for a node.
 *
 * After this we will send the activation request for each new transporter
 * to each of the TRPMAN instances. When this request arrives the new
 * transporter will be added to the epoll set of the receive thread. So
 * from here on the signals sent after the ACTIVATE_TRP_REQ signal can
 * be processed in the receiving node. When all ACTIVATE_TRP_REQ have
 * been processed we can send ACTIVATE_TRP_CONF to the requesting node.
 * If we have received ACTIVATE_TRP_CONF from the other node and we
 * have received ACTIVATE_TRP_REQ from the other node, then we are ready
 * to close the socket of the base transporter. While doing this we call
 * lockMultiTransporters and we lock both the send mutex and the receive
 * thread mutex to ensure that we don't interact with any socket calls
 * when performing this action. At this point we also disable the send
 * buffer of the base transporter.
 *
 * At this point the switch to the multi transporter setup is completed.
 * If we are performing a restart during this setup and the other node
 * crashes we will also crash since we are in a restart. So only when
 * we are switching while we are up will this be an issue. When switching
 * and being up we must handle crashes in all phases of the setup.
 * We have added a test case that both tests crashes in all phases as
 * well as long sleeps in all phases.
 *
 * Closing the base transporter will cause do_disconnect to be called
 * on this transporter while the node is still up. Thus we have to
 * handle this call separately for non-active connections. This should
 * only happen in the receive thread, so we don't take the send
 * mutex before changing theSocket here. Since the transporter isn't
 * active it should not perform any send activity here.
 *
 *
 * Disconnects when using multi transporters still happens through the
 * do_disconnect call, either activated by an error on any of the multi
 * sockets or activated by blocks discovering a failed node. This means
 * that performStates for the node is set to DISCONNECTING. This will
 * trigger call to doDisconnect from start_clients_thread for each of
 * the multi transporters and thus every multi transporter will have
 * the disconnectImpl method called on the transporter.
 *
 * This in turn will lead to that update_connections will call
 * report_disconnect for each of the multi transporters.
 *
 * The report_disconnect will discover that a multi transporter is
 * involved. It will disable send buffers and receive thread bitmasks
 * will be cleared. It will remove the multi transporter from the
 * allTransporters array. This is synchronized by acquiring the
 * lockMultiTransporters before these actions.
 *
 * It is possible to arrive here after the switch to multi transporter
 * still having data in the send buffer (still waiting to send the
 * ACTIVATE_TRP_REQ signal). This case must be handled by calling
 * disable send buffer as well as calling doDisconnect on the
 * base transporter that we already switched away from, otherwise these
 * send buffers will remain when starting the node up again and cause
 * a crash.
 *
 * As part of report_disconnect we will check if all multi transporters
 * have been handled by report_disconnect, we check this by looking into
 * allTransporters array to see if we removed the transporter from this
 * array for all multi transporters. If all have been removed we switch
 * back to the base transporter as the active transporter.
 *
 * If the failure happens before we switch to the multi transporters we will
 * disable send buffer and call doDisconnect also on the multi transporters
 * just in case.
 *
 * Finally when all multi transporters have been handled and we have
 * switched back to the base transporter we will send the DISCONNECT_REP
 * signal through the reportDisconnect call in the receive thread.
 *
 * After this we are again ready to setup the multi transporter again
 * after node failure handling have completed.
 *
 * No special handling of epoll sets (and poll sets in non-Linux platforms)
 * is required since the sockets are removed from those sets when the
 * socket is closed.
 *
 * update_connections()
 * --------------------
 * update_connections(), together with the thread running in
 * start_clients_thread(), handle the state changes for transporters as they
 * connect and disconnect.
 *
 * update_connections on a specific set of recvdata *must not* be run
 * concurrently with :performReceive() on the same recvdata. Thus,
 * it must either be called from the same (receive-)thread as
 * performReceive(), or protected by acquiring the (client) poll rights.
 */
Uint32
TransporterRegistry::update_connections(TransporterReceiveHandle& recvdata,
                                        Uint32 max_spintime)
{
  Uint32 spintime = 0;
  TransporterReceiveWatchdog guard(recvdata);
  assert((receiveHandle == &recvdata) || (receiveHandle == nullptr));

  for (Uint32 i = 1; i < (nTransporters + 1); i++)
  {
    require(i < MAX_NTRANSPORTERS);
    Transporter * t = allTransporters[i];
    if (!t)
      continue;

    const NodeId nodeId = t->getRemoteNodeId();
    if (!recvdata.m_transporters.get(i))
      continue;

    TransporterError code = m_error_states[nodeId].m_code;
    const char *info = m_error_states[nodeId].m_info;
    if (code != TE_NO_ERROR && info != (const char *)~(UintPtr)0)
    {
      if (performStates[nodeId] == CONNECTING)
      {
        g_eventLogger->info(
            "update_connections while CONNECTING, nodeId:%d, error:%d", nodeId,
            code);
        /* Failed during CONNECTING -> we are still DISCONNECTED */
        assert(!t->isConnected());
        assert(false);
        performStates[nodeId] = DISCONNECTED;
      }

      recvdata.reportError(nodeId, code, info);
      m_error_states[nodeId].m_code = TE_NO_ERROR;
      m_error_states[nodeId].m_info = (const char *)~(UintPtr)0;
    }

    switch(performStates[nodeId]){
    case CONNECTED:
#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
      if (t->getTransporterType() == tt_SHM_TRANSPORTER)
      {
        SHM_Transporter *shm_trp = (SHM_Transporter*)t;
        spintime = MAX(spintime, shm_trp->get_spintime());
      }
#endif
      break;
    case DISCONNECTED:
      break;
    case CONNECTING:
      if(t->isConnected())
	report_connect(recvdata, nodeId);
      break;
    case DISCONNECTING:
      if(!t->isConnected())
	report_disconnect(recvdata, nodeId, m_disconnect_errnum[nodeId]);
      break;
    }
  }
  recvdata.nTCPTransporters = nTCPTransporters;
  recvdata.nSHMTransporters = nSHMTransporters;
  recvdata.m_spintime = MIN(spintime, max_spintime);
  return spintime; //Inform caller of spintime calculated on this level
}

/**
 * Run as own thread
 * Possible blocking parts of transporter connect and diconnect
 * is supposed to be handled here.
 */
void
TransporterRegistry::start_clients_thread()
{
  int persist_mgm_count= 0;
  DBUG_ENTER("TransporterRegistry::start_clients_thread");
  while (m_run_start_clients_thread) {
    NdbSleep_MilliSleep(100);
    persist_mgm_count++;
    if(persist_mgm_count==50)
    {
      ndb_mgm_check_connection(m_mgm_handle);
      persist_mgm_count= 0;
    }
    lockMultiTransporters();
    for (Uint32 i = 1;
         i < (nTransporters +1) && m_run_start_clients_thread;
         i++)
    {
      require(i < MAX_NTRANSPORTERS);
      Transporter * t = allTransporters[i];
      if (!t)
	continue;

      const NodeId nodeId = t->getRemoteNodeId();
      switch(performStates[nodeId]){
      case CONNECTING:
      {
        if (t->isPartOfMultiTransporter())
        {
          break;
        }
        if (!t->isConnected() && !t->isServer)
        {
          if (get_and_clear_node_up_indicator(nodeId))
          {
            // Other node have indicated that node nodeId is up, try connect
            // now and restart backoff sequence
            backoff_reset_connecting_time(nodeId);
          }
          if (!backoff_update_and_check_time_for_connect(nodeId))
          {
            // Skip connect this time
            continue;
          }

	  bool connected= false;
	  /**
	   * First, we try to connect (if we have a port number).
	   */

	  if (t->get_s_port())
          {
            DBUG_PRINT("info", ("connecting to node %d using port %d",
                                nodeId, t->get_s_port()));
            unlockMultiTransporters();
            connected= t->connect_client();
            lockMultiTransporters();
          }

	  /**
	   * If dynamic, get the port for connecting from the management server
	   */
	  if (!connected && t->get_s_port() <= 0) // Port is dynamic
          {
	    int server_port= 0;
            unlockMultiTransporters();

            DBUG_PRINT("info", ("connection to node %d should use "
                                "dynamic port",
                                nodeId));

	    if(!ndb_mgm_is_connected(m_mgm_handle))
	      ndb_mgm_connect(m_mgm_handle, 0, 0, 0);

	    if(ndb_mgm_is_connected(m_mgm_handle))
	    {
              DBUG_PRINT("info", ("asking mgmd which port to use for node %d",
                                  nodeId));

              const int res=
		ndb_mgm_get_connection_int_parameter(m_mgm_handle,
						     t->getRemoteNodeId(),
						     t->getLocalNodeId(),
						     CFG_CONNECTION_SERVER_PORT,
                                                     &server_port);

	      DBUG_PRINT("info",("Got dynamic port %d for %d -> %d (ret: %d)",
				 server_port,t->getRemoteNodeId(),
				 t->getLocalNodeId(),res));
	      if( res >= 0 )
	      {
                DBUG_PRINT("info", ("got port %d to use for connection to %d",
                                    server_port, nodeId));

		if (server_port != 0)
                {
                  if (t->get_s_port() != server_port)
                  {
                    // Got a different port number, reset backoff
                    backoff_reset_connecting_time(nodeId);
                  }
                  // Save the new port number
		  t->set_s_port(server_port);
                }
                else
                {
                  // Got port number 0, port is not known.  Keep the old.
                }
	      }
	      else if(ndb_mgm_is_connected(m_mgm_handle))
	      {
                DBUG_PRINT("info", ("Failed to get dynamic port, res: %d",
                                    res));
                g_eventLogger->info("Failed to get dynamic port, res: %d",
                                    res);
		ndb_mgm_disconnect(m_mgm_handle);
	      }
	      else
	      {
                DBUG_PRINT("info", ("mgmd close connection early"));
                g_eventLogger->info
                  ("Management server closed connection early. "
                   "It is probably being shut down (or has problems). "
                   "We will retry the connection. %d %s %s line: %d",
                   ndb_mgm_get_latest_error(m_mgm_handle),
                   ndb_mgm_get_latest_error_desc(m_mgm_handle),
                   ndb_mgm_get_latest_error_msg(m_mgm_handle),
                   ndb_mgm_get_latest_error_line(m_mgm_handle)
                   );
	      }
	    }
	    /** else
	     * We will not be able to get a new port unless
	     * the m_mgm_handle is connected. Note that not
	     * being connected is an ok state, just continue
	     * until it is able to connect. Continue using the
	     * old port until we can connect again and get a
	     * new port.
	     */
            lockMultiTransporters();
	  }
	}
	break;
      }
      case DISCONNECTING:
	if(t->isConnected())
        {
          DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %u\n",
                         localNodeId, t->getRemoteNodeId(), __LINE__));
	  t->doDisconnect();
        }
	break;
      case DISCONNECTED:
      {
        if (t->isConnected())
        {
          g_eventLogger->warning("Found connection to %u in state DISCONNECTED "
                                 " while being connected, disconnecting!",
                                 t->getRemoteNodeId());
          DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %u\n",
                         localNodeId, t->getRemoteNodeId(), __LINE__));
          t->doDisconnect();
        }
        break;
      }
      case CONNECTED:
      {
        if (t->isPartOfMultiTransporter() &&
            !t->isConnected() &&
            !t->isServer)
        {
          require(t->get_s_port());
          DBUG_PRINT("info", ("connecting multi-transporter to node %d"
                     " using port %d",
                     nodeId,
                     t->get_s_port()));
          unlockMultiTransporters();
          t->connect_client();
          DEBUG_FPRINTF((stderr, "Connect client of trp id %u, res: %u\n",
                        t->getTransporterIndex(),
                        t->isConnected()));
          lockMultiTransporters();
        }
      }
      default:
	break;
      }
    }
    unlockMultiTransporters();
  }
  DBUG_VOID_RETURN;
}

struct NdbThread*
TransporterRegistry::start_clients()
{
  m_run_start_clients_thread= true;
  m_start_clients_thread= NdbThread_Create(run_start_clients_C,
					   (void**)this,
                                           0, // default stack size
					   "ndb_start_clients",
					   NDB_THREAD_PRIO_LOW);
  if (m_start_clients_thread == nullptr)
  {
    m_run_start_clients_thread= false;
  }
  return m_start_clients_thread;
}

bool
TransporterRegistry::stop_clients()
{
  if (m_start_clients_thread) {
    m_run_start_clients_thread= false;
    void* status;
    NdbThread_WaitFor(m_start_clients_thread, &status);
    NdbThread_Destroy(&m_start_clients_thread);
  }
  return true;
}

void
TransporterRegistry::add_transporter_interface(NodeId remoteNodeId,
					       const char *interf, 
					       int s_port)
{
  DBUG_ENTER("TransporterRegistry::add_transporter_interface");
  DBUG_PRINT("enter",("interface=%s, s_port= %d", interf, s_port));
  if (interf && strlen(interf) == 0)
    interf= nullptr;

  for (unsigned i= 0; i < m_transporter_interface.size(); i++)
  {
    Transporter_interface &tmp= m_transporter_interface[i];
    if (s_port != tmp.m_s_service_port || tmp.m_s_service_port==0)
      continue;
    if (interf != nullptr && tmp.m_interface != nullptr &&
	strcmp(interf, tmp.m_interface) == 0)
    {
      DBUG_VOID_RETURN; // found match, no need to insert
    }
    if (interf == nullptr && tmp.m_interface == nullptr)
    {
      DBUG_VOID_RETURN; // found match, no need to insert
    }
  }
  Transporter_interface t;
  t.m_remote_nodeId= remoteNodeId;
  t.m_s_service_port= s_port;
  t.m_interface= interf;
  m_transporter_interface.push_back(t);
  DBUG_PRINT("exit",("interface and port added"));
  DBUG_VOID_RETURN;
}

bool
TransporterRegistry::start_service(SocketServer& socket_server)
{
  DBUG_ENTER("TransporterRegistry::start_service");
  if (m_transporter_interface.size() > 0 &&
      localNodeId == 0)
  {
    g_eventLogger->error("INTERNAL ERROR: not initialized");
    DBUG_RETURN(false);
  }

  for (unsigned i= 0; i < m_transporter_interface.size(); i++)
  {
    Transporter_interface &t= m_transporter_interface[i];

    unsigned short port= (unsigned short)t.m_s_service_port;
    if(t.m_s_service_port<0)
      port= -t.m_s_service_port; // is a dynamic port
    TransporterService *transporter_service =
      new TransporterService(new SocketAuthSimple("ndbd", "ndbd passwd"));
    if(!socket_server.setup(transporter_service,
			    &port, t.m_interface))
    {
      DBUG_PRINT("info", ("Trying new port"));
      port= 0;
      if(t.m_s_service_port>0
	 || !socket_server.setup(transporter_service,
				 &port, t.m_interface))
      {
	/*
	 * If it wasn't a dynamically allocated port, or
	 * our attempts at getting a new dynamic port failed
	 */

        char buf[512];
        char* sockaddr_string =
            Ndb_combine_address_port(buf,
                                     sizeof(buf),
                                     t.m_interface,
                                     t.m_s_service_port);
        g_eventLogger->error("Unable to setup transporter service port: %s!\n"
                             "Please check if the port is already used,\n"
                             "(perhaps the node is already running)",
                             sockaddr_string);

	delete transporter_service;
	DBUG_RETURN(false);
      }
    }
    t.m_s_service_port= (t.m_s_service_port<=0)?-port:port; // -`ve if dynamic
    DBUG_PRINT("info", ("t.m_s_service_port = %d",t.m_s_service_port));
    transporter_service->setTransporterRegistry(this);
  }
  DBUG_RETURN(true);
}

void
TransporterRegistry::startReceiving()
{
  DBUG_ENTER("TransporterRegistry::startReceiving");

#ifdef NDB_SHM_TRANSPORTER_SUPPORTED
  m_shm_own_pid = getpid();
#endif
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::stopReceiving(){
}

void
TransporterRegistry::startSending(){
}

void
TransporterRegistry::stopSending(){
}

NdbOut & operator <<(NdbOut & out, SignalHeader & sh){
  out << "-- Signal Header --" << endl;
  out << "theLength:    " << sh.theLength << endl;
  out << "gsn:          " << sh.theVerId_signalNumber << endl;
  out << "recBlockNo:   " << sh.theReceiversBlockNumber << endl;
  out << "sendBlockRef: " << sh.theSendersBlockRef << endl;
  out << "sendersSig:   " << sh.theSendersSignalId << endl;
  out << "theSignalId:  " << sh.theSignalId << endl;
  out << "trace:        " << (int)sh.theTrace << endl;
  return out;
} 

int
TransporterRegistry::get_transporter_count() const
{
  assert(nTransporters > 0);
  return nTransporters;
}

bool
TransporterRegistry::is_shm_transporter(TrpId trp_id)
{
  assert(trp_id < maxTransporters);
  Transporter *trp = allTransporters[trp_id];
  if (trp->getTransporterType() == tt_SHM_TRANSPORTER)
    return true;
  else
    return false;
}

Transporter*
TransporterRegistry::get_transporter(TrpId trp_id) const
{
  assert(trp_id < MAX_NTRANSPORTERS);
  return allTransporters[trp_id];
}

Transporter*
TransporterRegistry::get_node_transporter(NodeId nodeId) const
{
  assert(nodeId <= MAX_NODES);
  return theNodeIdTransporters[nodeId];
}

bool TransporterRegistry::connect_client(NdbMgmHandle *h)
{
  DBUG_ENTER("TransporterRegistry::connect_client(NdbMgmHandle)");

  Uint32 mgm_nodeid= ndb_mgm_get_mgmd_nodeid(*h);

  if(!mgm_nodeid)
  {
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    return false;
  }
  Transporter * t = theNodeIdTransporters[mgm_nodeid];
  if (!t)
  {
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    return false;
  }

  if (t->isMultiTransporter())
  {
    Multi_Transporter *multi_trp = (Multi_Transporter*)t;
    require(multi_trp->get_num_active_transporters() == 1);
    t = multi_trp->get_active_transporter(0);
  }
  require(!t->isMultiTransporter());
  require(!t->isPartOfMultiTransporter());
  bool res = t->connect_client(connect_ndb_mgmd(h));
  if (res == true)
  {
    DEBUG_FPRINTF((stderr, "(%u)performStates[%u] = DISCONNECTING,"
                   " connect_client\n", localNodeId, mgm_nodeid));
    performStates[mgm_nodeid] = TransporterRegistry::CONNECTING;
  }
  DBUG_RETURN(res);
}


bool TransporterRegistry::report_dynamic_ports(NdbMgmHandle h) const
{
  // Fill array of nodeid/port pairs for those ports which are dynamic
  unsigned num_ports = 0;
  ndb_mgm_dynamic_port ports[MAX_NODES];
  for(unsigned i = 0; i < m_transporter_interface.size(); i++)
  {
    const Transporter_interface& ti = m_transporter_interface[i];
    if (ti.m_s_service_port >= 0)
      continue; // Not a dynamic port

    assert(num_ports < NDB_ARRAY_SIZE(ports));
    ports[num_ports].nodeid = ti.m_remote_nodeId;
    ports[num_ports].port = ti.m_s_service_port;
    num_ports++;
  }

  if (num_ports == 0)
  {
    // No dynamic ports in use, nothing to report
    return true;
  }

  // Send array of nodeid/port pairs to mgmd
  if (ndb_mgm_set_dynamic_ports(h, localNodeId,
                                ports, num_ports) < 0)
  {
    g_eventLogger->error("Failed to register dynamic ports, error: %d  - '%s'",
                         ndb_mgm_get_latest_error(h),
                         ndb_mgm_get_latest_error_desc(h));
    return false;
  }

  return true;
}


/**
 * Given a connected NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
ndb_socket_t TransporterRegistry::connect_ndb_mgmd(NdbMgmHandle *h)
{
  ndb_socket_t sockfd = ndb_socket_create();

  DBUG_ENTER("TransporterRegistry::connect_ndb_mgmd(NdbMgmHandle)");

  if ( h==nullptr || *h == nullptr )
  {
    g_eventLogger->error("Mgm handle is NULL (%s:%d)", __FILE__, __LINE__);
    DBUG_RETURN(sockfd);
  }

  if (!report_dynamic_ports(*h))
  {
    ndb_mgm_destroy_handle(h);
    DBUG_RETURN(sockfd);
  }

  /**
   * convert_to_transporter also disposes of the handle (i.e. we don't leak
   * memory here.
   */
  DBUG_PRINT("info", ("Converting handle to transporter"));
  sockfd= ndb_mgm_convert_to_transporter(h);
  if (!ndb_socket_valid(sockfd))
  {
    g_eventLogger->error("Failed to convert to transporter (%s: %d)",
                         __FILE__, __LINE__);
    ndb_mgm_destroy_handle(h);
  }
  DBUG_RETURN(sockfd);
}

/**
 * Given a SocketClient, creates a NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
ndb_socket_t
TransporterRegistry::connect_ndb_mgmd(const char* server_name,
                                      unsigned short server_port)
{
  NdbMgmHandle h= ndb_mgm_create_handle();
  ndb_socket_t s= ndb_socket_create();

  DBUG_ENTER("TransporterRegistry::connect_ndb_mgmd(SocketClient)");

  if ( h == nullptr )
  {
    DBUG_RETURN(s);
  }

  /**
   * Set connectstring
   */
  {
    BaseString cs;
    cs.assfmt("%s %u", server_name, server_port);
    ndb_mgm_set_connectstring(h, cs.c_str());
  }

  if(ndb_mgm_connect(h, 0, 0, 0)<0)
  {
    DBUG_PRINT("info", ("connection to mgmd failed"));
    ndb_mgm_destroy_handle(&h);
    DBUG_RETURN(s);
  }

  DBUG_RETURN(connect_ndb_mgmd(&h));
}

/**
 * The calls below are used by all implementations: NDB API, ndbd and
 * ndbmtd. The calls to handle->getWritePtr, handle->updateWritePtr
 * are handled by special implementations for NDB API, ndbd and
 * ndbmtd.
 */

Uint32 *
TransporterRegistry::getWritePtr(TransporterSendBufferHandle *handle,
                                 Transporter* t,
                                 Uint32 trp_id,
                                 Uint32 lenBytes,
                                 Uint32 prio,
                                 SendStatus *error)
{
  NodeId nodeId = t->getRemoteNodeId();
  Uint32 *insertPtr = handle->getWritePtr(nodeId,
                                          trp_id,
                                          lenBytes,
                                          prio,
                                          t->get_max_send_buffer(),
                                          error);

  if (unlikely(insertPtr == nullptr && *error != SEND_MESSAGE_TOO_BIG))
  {
    //-------------------------------------------------
    // Buffer was completely full. We have severe problems.
    // We will attempt to wait for a small time
    //-------------------------------------------------
    if (t->send_is_possible(10))
    {
      //-------------------------------------------------
      // Send is possible after the small timeout.
      //-------------------------------------------------
      if (!handle->forceSend(nodeId, trp_id))
      {
	return nullptr;
      }
      else
      {
	//-------------------------------------------------
	// Since send was successful we will make a renewed
	// attempt at inserting the signal into the buffer.
	//-------------------------------------------------
        insertPtr = handle->getWritePtr(nodeId,
                                        trp_id,
                                        lenBytes,
                                        prio,
                                        t->get_max_send_buffer(),
                                        error);
      }//if
    }
    else
    {
      return nullptr;
    }//if
  }
  return insertPtr;
}

void
TransporterRegistry::updateWritePtr(TransporterSendBufferHandle *handle,
                                    Transporter* t,
                                    Uint32 trp_id,
                                    Uint32 lenBytes,
                                    Uint32 prio)
{
  NodeId nodeId = t->getRemoteNodeId();
  Uint32 used = handle->updateWritePtr(nodeId, trp_id, lenBytes, prio);
  t->update_status_overloaded(used);

  if (t->send_limit_reached(used))
  {
    //-------------------------------------------------
    // Buffer is full and we are ready to send. We will
    // not wait since the signal is already in the buffer.
    // Force flag set has the same indication that we
    // should always send. If it is not possible to send
    // we will not worry since we will soon be back for
    // a renewed trial.
    //-------------------------------------------------
    if (t->send_is_possible(0))
    {
      //-------------------------------------------------
      // Send was possible, attempt at a send.
      //-------------------------------------------------
      handle->forceSend(nodeId, trp_id);
    }//if
  }
}
	   
void 
TransporterRegistry::inc_overload_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theNodeIdTransporters[nodeId] != nullptr);
  theNodeIdTransporters[nodeId]->inc_overload_count();
}

void 
TransporterRegistry::inc_slowdown_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theNodeIdTransporters[nodeId] != nullptr);
  theNodeIdTransporters[nodeId]->inc_slowdown_count();
}

Uint32
TransporterRegistry::get_overload_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theNodeIdTransporters[nodeId] != nullptr);
  return theNodeIdTransporters[nodeId]->get_overload_count();
}

Uint32
TransporterRegistry::get_slowdown_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theNodeIdTransporters[nodeId] != nullptr);
  return theNodeIdTransporters[nodeId]->get_slowdown_count();
}

Uint32
TransporterRegistry::get_connect_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theNodeIdTransporters[nodeId] != nullptr);
  return theNodeIdTransporters[nodeId]->get_connect_count();
}

void
TransporterRegistry::get_trps_for_node(Uint32 nodeId,
                                       TrpId *trp_ids,
                                       Uint32 &num_ids,
                                       Uint32 max_size)
{
  Transporter *t = theNodeIdTransporters[nodeId];
  if (!t)
  {
    num_ids = 0;
  }
  else if (t->isMultiTransporter())
  {
    Multi_Transporter *multi_trp = (Multi_Transporter*)t;
    num_ids = multi_trp->get_num_active_transporters();
    num_ids = MIN(num_ids, max_size);
    for (Uint32 i = 0; i < num_ids; i++)
    {
      Transporter* tmp_trp = multi_trp->get_active_transporter(i);
      trp_ids[i] = tmp_trp->getTransporterIndex();
      require(trp_ids[i] != 0);
    }
  }
  else
  {
    num_ids = 1;
    trp_ids[0] = t->getTransporterIndex();
    require(trp_ids[0] != 0);
  }
  require(max_size >= 1);
}

TrpId
TransporterRegistry::getTransporterIndex(Transporter* t)
{
  return t->getTransporterIndex();
}

bool
TransporterRegistry::isMultiTransporter(Transporter* t)
{
  return t->isMultiTransporter();
}

void
TransporterRegistry::switch_active_trp(Multi_Transporter *t)
{
  require(t->isMultiTransporter());
  t->switch_active_trp();
}

Uint32
TransporterRegistry::get_num_active_transporters(Multi_Transporter *t)
{
  require(t->isMultiTransporter());
  {
    return t->get_num_active_transporters();
  }
}

/**
 * We calculate the risk level for a send buffer.
 * The primary instrument is the current size of
 * the node send buffer. However if the total
 * buffer for all send buffers is also close to
 * empty, then we will adjust the node send
 * buffer size for this. In this manner a very
 * contested total buffer will also slow down
 * the entire node operation.
 */
void
calculate_send_buffer_level(Uint64 node_send_buffer_size,
                            Uint64 total_send_buffer_size,
                            Uint64 total_used_send_buffer_size,
                            Uint32 /*num_threads*/,
                            SB_LevelType &level)
{
  Uint64 percentage =
    (total_used_send_buffer_size * 100) / total_send_buffer_size;

  if (percentage < 90)
  {
    ;
  }
  else if (percentage < 95)
  {
    node_send_buffer_size *= 2;
  }
  else if (percentage < 97)
  {
    node_send_buffer_size *= 4;
  }
  else if (percentage < 98)
  {
    node_send_buffer_size *= 8;
  }
  else if (percentage < 99)
  {
    node_send_buffer_size *= 16;
  }
  else
  {
    level = SB_CRITICAL_LEVEL;
    return;
  }
  
  if (node_send_buffer_size < 128 * 1024)
  {
    level = SB_NO_RISK_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 256 * 1024)
  {
    level = SB_LOW_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 384 * 1024)
  {
    level = SB_MEDIUM_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 1024 * 1024)
  {
    level = SB_HIGH_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 2 * 1024 * 1024)
  {
    level = SB_RISK_LEVEL;
    return;
  }
  else
  {
    level = SB_CRITICAL_LEVEL;
    return;
  }
}

template class Vector<TransporterRegistry::Transporter_interface>;
