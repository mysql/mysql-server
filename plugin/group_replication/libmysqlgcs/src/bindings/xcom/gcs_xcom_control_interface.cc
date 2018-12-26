/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_control_interface.h"

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_view_identifier.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"

using std::map;
using std::set;
using std::string;

#include <assert.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <list>

/*
  This is used to disable the arbitrator hack in XCOM just
  in case it is not disabled by default.
*/
extern int ARBITRATOR_HACK;

// Suspicion timeout in 100s of nanoseconds (60 seconds)
static const uint64_t NON_MEMBER_EXPEL_TIMEOUT = 60 * 10000000;

// Suspicions processing thread period in seconds
static const unsigned int SUSPICION_PROCESSING_THREAD_PERIOD = 15;

static bool terminate_suspicion_thread = false;

static void set_terminate_suspicion_thread(bool val) {
  terminate_suspicion_thread = val;
}

static bool is_terminate_suspicion_thread() {
  return terminate_suspicion_thread;
}

static void *suspicions_processing_thread(void *ptr) {
  Gcs_xcom_control *gcs_ctrl = (Gcs_xcom_control *)ptr;
  Gcs_suspicions_manager *mgr = gcs_ctrl->get_suspicions_manager();

  while (!is_terminate_suspicion_thread()) {
    mgr->process_suspicions();
  }

  My_xp_thread_util::exit(0);
  /* purecov: begin deadcode */
  return NULL;
  /* purecov: end */
}

static void *xcom_taskmain_startup(void *ptr) {
  Gcs_xcom_control *gcs_ctrl = (Gcs_xcom_control *)ptr;
  Gcs_xcom_proxy *proxy = gcs_ctrl->get_xcom_proxy();
  xcom_port port = gcs_ctrl->get_node_address()->get_member_port();

  proxy->set_should_exit(false);
  proxy->xcom_init(port);

  My_xp_thread_util::exit(0);
  /* purecov: begin deadcode */
  return NULL;
  /* purecov: end */
}

/* purecov: begin deadcode */
map<int, const Gcs_control_event_listener &>
    *Gcs_xcom_control::get_event_listeners() {
  return &event_listeners;
}
/* purecov: end */

Gcs_xcom_control::Gcs_xcom_control(
    Gcs_xcom_node_address *xcom_node_address,
    std::vector<Gcs_xcom_node_address *> &xcom_peers,
    Gcs_group_identifier m_group_identifier, Gcs_xcom_proxy *xcom_proxy,
    Gcs_xcom_group_management *xcom_group_management,
    Gcs_xcom_engine *gcs_engine,
    Gcs_xcom_state_exchange_interface *state_exchange,
    Gcs_xcom_view_change_control_interface *view_control, bool boot,
    My_xp_socket_util *socket_util)
    : m_gid(NULL),
      m_gid_hash(0),
      m_xcom_proxy(xcom_proxy),
      m_xcom_group_management(xcom_group_management),
      event_listeners(),
      m_local_node_info(NULL),
      m_local_node_address(NULL),
      m_state_exchange(state_exchange),
      m_xcom_thread(),
      m_socket_util(socket_util),
      m_join_attempts(0),
      m_join_sleep_time(0),
      m_suspicions_manager(new Gcs_suspicions_manager(xcom_proxy, this)),
      m_suspicions_processing_thread(),
      m_xcom_running(false),
      m_leave_view_requested(false),
      m_leave_view_delivered(false),
      m_boot(boot),
      m_initial_peers(),
      m_view_control(view_control),
      m_gcs_engine(gcs_engine) {
  set_node_address(xcom_node_address);

  m_gid = new Gcs_group_identifier(m_group_identifier.get_group_id());
  m_gid_hash =
      Gcs_xcom_utils::mhash((unsigned char *)m_gid->get_group_id().c_str(),
                            m_gid->get_group_id().size());
  /*
    Clone the members - we may want to pass parameters directly to the
    control interface through the join operation on a later refactoring
    thence the set of members configured initially and then later may
    differ.
  */
  set_peer_nodes(xcom_peers);

  ARBITRATOR_HACK = 0;
}

Gcs_xcom_control::~Gcs_xcom_control() {
  delete m_gid;
  delete m_local_node_info;
  delete m_suspicions_manager;

  set_terminate_suspicion_thread(true);
  m_suspicions_manager = NULL;

  clear_peer_nodes();
}

Gcs_xcom_node_address *Gcs_xcom_control::get_node_address() {
  return m_local_node_address;
}

Gcs_xcom_proxy *Gcs_xcom_control::get_xcom_proxy() { return m_xcom_proxy; }

Gcs_suspicions_manager *Gcs_xcom_control::get_suspicions_manager() {
  return m_suspicions_manager;
}

/* purecov: begin deadcode */

void Gcs_xcom_control::set_boot_node(bool boot) { m_boot = boot; }

My_xp_socket_util *Gcs_xcom_control::get_socket_util() { return m_socket_util; }
/* purecov: end */

void Gcs_xcom_control::wait_for_xcom_thread() { m_xcom_thread.join(NULL); }

bool Gcs_xcom_control::is_xcom_running() { return m_xcom_running; }

void Gcs_xcom_control::set_join_behavior(unsigned int join_attempts,
                                         unsigned int join_sleep_time) {
  m_join_attempts = join_attempts;
  MYSQL_GCS_LOG_DEBUG("Configured number of attempts to join: %u",
                      m_join_attempts)

  m_join_sleep_time = join_sleep_time;
  MYSQL_GCS_LOG_DEBUG("Configured time between attempts to join: %u",
                      m_join_sleep_time)
}

void do_function_join(Gcs_control_interface *control_if) {
  static_cast<void>(static_cast<Gcs_xcom_control *>(control_if)->do_join());
}

enum_gcs_error Gcs_xcom_control::join() {
  MYSQL_GCS_LOG_DEBUG("Joining a group.")

  /*
    It is not possible to call join or leave if the node is already
    trying to join or leave the group. The start_join() method
    verifies it and updates a flag to indicate that the join is
    taking place.
  */
  if (!m_view_control->start_join()) {
    MYSQL_GCS_LOG_ERROR("The member is already leaving or joining a group.")
    return GCS_NOK;
  }

  /*
    This is an optimistic attempt to avoid trying to join a group when the
    node already belongs to one.

    Note that although MySQL GCS internal interfaces are designed to allow
    a node to join multiple groups, the current join() and leave() methods
    don't take this into account.
  */
  if (belongs_to_group()) {
    MYSQL_GCS_LOG_ERROR(
        "The member is trying to join a group when it is already a member.")
    m_view_control->end_join();
    return GCS_NOK;
  }

  if (!m_boot && m_initial_peers.empty()) {
    MYSQL_GCS_LOG_ERROR("Unable to join the group: peers not configured. ")
    m_view_control->end_join();
    return GCS_NOK;
  }

  Gcs_xcom_notification *notification =
      new Control_notification(do_function_join, this);
  bool scheduled = m_gcs_engine->push(notification);
  if (!scheduled) {
    MYSQL_GCS_LOG_DEBUG(
        "Tried to enqueue a join request but the member is about to stop.")
    delete notification;
  }

  return scheduled ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_control::do_join(bool retry) {
  unsigned int retry_join_count = m_join_attempts;
  enum_gcs_error ret = GCS_NOK;

  if (m_xcom_running) {
    MYSQL_GCS_LOG_ERROR(
        "Previous join was already requested and eventually "
        "a view will be delivered.")
    m_view_control->end_join();
    return GCS_NOK;
  }

  while (ret == GCS_NOK) {
    ret = retry_do_join();

    retry_join_count--;
    if (retry && m_join_attempts != 0 && ret == GCS_NOK &&
        retry_join_count >= 1) {
      MYSQL_GCS_LOG_DEBUG(
          "Sleeping for %u seconds before retrying to join the group. There "
          "are "
          " %u more attempt(s) before giving up.",
          m_join_sleep_time, retry_join_count);
      My_xp_util::sleep_seconds(m_join_sleep_time);
    } else {
      break;
    }
  }

  m_view_control->end_join();

  return ret;
}

enum_gcs_error Gcs_xcom_control::retry_do_join() {
  /* Used to initialize xcom */
  int local_port = m_local_node_address->get_member_port();
  connection_descriptor *con = NULL;
  int comm_status = XCOM_COMM_STATUS_UNDEFINED;
  enum_gcs_error is_xcom_ready = GCS_NOK;
  bool xcom_handlers_open = false;

  /*
    Clean up notification flags that are used to check whether XCOM
    is running or not.
  */
  m_xcom_proxy->xcom_set_cleanup();

  /* Spawn XCom's main loop thread. */
  if (local_port != 0) {
    m_xcom_thread.create(key_GCS_THD_Gcs_xcom_control_m_xcom_thread, NULL,
                         xcom_taskmain_startup, (void *)this);
  } else {
    MYSQL_GCS_LOG_ERROR("Error initializing the group communication engine.")
    goto err;
  }

  // Wait for XCom comms to become ready
  m_xcom_proxy->xcom_wait_for_xcom_comms_status_change(comm_status);

  if (comm_status != XCOM_COMMS_OK) {
    MYSQL_GCS_LOG_ERROR("Error joining the group while waiting for"
                        << " the network layer to become ready.")
    goto err;
  }

  init_me();

  /*
    Connect to the local xcom instance.
    This is needed to push data to consensus.
  */
  if (m_xcom_proxy->xcom_open_handlers(
          m_local_node_address->get_member_ip(),
          m_local_node_address->get_member_port())) {
    MYSQL_GCS_LOG_ERROR("Error connecting to the local group communication"
                        << " engine instance.")
    goto err;
  }
  xcom_handlers_open = true;

  if (m_boot) {
    MYSQL_GCS_LOG_TRACE(
        "::join():: I am the boot node. %d - %s. Calling xcom_client_boot.",
        local_port, m_local_node_info->get_member_uuid().actual_value.c_str())

    int error = 0;
    if ((error = m_xcom_proxy->xcom_boot_node(*m_local_node_info,
                                              m_gid_hash)) <= 0) {
      MYSQL_GCS_LOG_ERROR("Error booting the group communication engine."
                          << " Error=" << error)
      goto err;
    }
  } else {
    assert(!m_initial_peers.empty());
    MYSQL_GCS_LOG_TRACE("::join():: I am NOT the boot node.")

    int n = 0;
    xcom_port port = 0;
    char *addr = NULL;
    std::vector<Gcs_xcom_node_address *>::iterator it;

    std::string local_node_info_str_ip;
    bool resolve_error = false;
    resolve_error = resolve_ip_addr_from_hostname(
        m_local_node_address->get_member_ip(), local_node_info_str_ip);

    if (resolve_error) {
      MYSQL_GCS_LOG_ERROR("Error resolving local address name: "
                          << m_local_node_address->get_member_ip().c_str())
      goto err;
    }

    while (con == NULL && n < Gcs_xcom_proxy::connection_attempts) {
      for (it = m_initial_peers.begin();
           con == NULL && it != m_initial_peers.end(); it++) {
        Gcs_xcom_node_address *peer = *(it);
        std::string peer_rep_ip;

        resolve_error =
            resolve_ip_addr_from_hostname(peer->get_member_ip(), peer_rep_ip);
        if (resolve_error) {
          MYSQL_GCS_LOG_WARN("Unable to resolve peer address "
                             << peer->get_member_ip().c_str()
                             << ". Skipping...")
          continue;
        }

        if (peer_rep_ip.compare(local_node_info_str_ip) == 0 &&
            peer->get_member_port() ==
                m_local_node_address->get_member_port()) {
          MYSQL_GCS_LOG_TRACE("::join():: Skipping own address.")
          // Skip own address if configured in the peer list
          continue;
        }

        port = peer->get_member_port();
        addr = (char *)peer->get_member_ip().c_str();

        MYSQL_GCS_LOG_TRACE(
            "Client local port %d xcom_client_open_connection to %s:%d",
            local_port, addr, port)

        if ((con = m_xcom_proxy->xcom_client_open_connection(addr, port)) ==
            NULL) {
          MYSQL_GCS_LOG_ERROR("Error on opening a connection to "
                              << addr << ":" << port
                              << " on local port: " << local_port << ".")
        }
      }
      n++;
    }

    if (con != NULL) {
      if (m_socket_util->disable_nagle_in_socket(con->fd) < 0) {
        m_xcom_proxy->xcom_client_close_connection(con);
        goto err;
      }
      MYSQL_GCS_LOG_TRACE(
          "::join():: Calling xcom_client_add_node %d_%s connected to %s:%d to "
          "join",
          local_port, m_local_node_info->get_member_uuid().actual_value.c_str(),
          addr, port)

      /*
        Explicitly check the return value so that we are able to distinguish
        between a failure in the synchronous local request from a failure in
        the asynchronous add_node request.
      */
      if (!m_xcom_proxy->xcom_add_node(*con, *m_local_node_info, m_gid_hash)) {
        m_xcom_proxy->xcom_client_close_connection(con);
        goto err;
      }
      m_xcom_proxy->xcom_client_close_connection(con);
    } else {
      MYSQL_GCS_LOG_ERROR(
          "Error connecting to all peers. Member join failed. Local port: "
          << local_port)
      goto err;
    }
  }

  /* Wait for xcom to become ready */
  is_xcom_ready = m_xcom_proxy->xcom_wait_ready();
  if (is_xcom_ready == GCS_NOK) {
    MYSQL_GCS_LOG_ERROR("The group communication engine is not ready"
                        << " for the member to join. Local port: "
                        << local_port);
    goto err;
  }

  m_xcom_running = true;
  MYSQL_GCS_LOG_DEBUG("The member has joined the group. Local port: %d",
                      local_port);

  m_suspicions_manager->set_groupid_hash(m_gid_hash);
  m_suspicions_manager->set_my_info(m_local_node_info);

  set_terminate_suspicion_thread(false);

  // Initialize thread to deal with suspicions
  m_suspicions_processing_thread.create(
      key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread, NULL,
      suspicions_processing_thread, (void *)this);
  MYSQL_GCS_LOG_TRACE("Started the suspicions processing thread...");
  m_view_control->end_join();

  return GCS_OK;

err:
  if (local_port != 0) {
    /*
      We need the handlers opened in order to send a request to kill
      XCOM.
    */
    MYSQL_GCS_LOG_DEBUG(
        "Killing the group communication engine because the member failed to"
        " join. Local port: %d",
        local_port);
    if (comm_status != XCOM_COMMS_ERROR &&
        m_xcom_proxy->xcom_exit(xcom_handlers_open)) {
      MYSQL_GCS_LOG_WARN("Failed to kill the group communication engine "
                         << "after the member failed to join. Local port: "
                         << local_port);
    }
    wait_for_xcom_thread();
  }

  /* Cleanup - close handlers */
  m_xcom_proxy->xcom_close_handlers();

  MYSQL_GCS_LOG_ERROR(
      "The member was unable to join the group. Local port: " << local_port)

  m_xcom_running = false;

  return GCS_NOK;
}

void do_function_leave(Gcs_control_interface *control_if) {
  static_cast<void>(static_cast<Gcs_xcom_control *>(control_if)->do_leave());
}

enum_gcs_error Gcs_xcom_control::leave() {
  MYSQL_GCS_LOG_DEBUG("The member is leaving the group.")

  /*
    It is not possible to call join or leave if the node is already
    trying to join or leave the group. The start_leave() method
    verifies it and updates a flag to indicate that the leave is
    taking place.
  */
  if (!m_view_control->start_leave()) {
    MYSQL_GCS_LOG_ERROR("The member is already leaving or joining a group.")
    return GCS_NOK;
  }

  /*
    This is an optimistic attempt to avoid trying to leave a group when the
    node does not belong to one.

    Note that although MySQL GCS internal interfaces are designed to allow
    a node to join multiple groups, the current join() and leave() methods
    don't take this into account.
  */
  if (!belongs_to_group()) {
    MYSQL_GCS_LOG_ERROR("The member is leaving a group without being on one.")
    m_view_control->end_leave();
    return GCS_NOK;
  }

  Gcs_xcom_notification *notification =
      new Control_notification(do_function_leave, this);
  bool scheduled = m_gcs_engine->push(notification);
  if (!scheduled) {
    MYSQL_GCS_LOG_DEBUG(
        "Tried to enqueue a leave request but the member is about to stop.")
    delete notification;
  }

  return scheduled ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_control::do_leave() {
  if (!m_xcom_running) {
    MYSQL_GCS_LOG_ERROR(
        "Previous join was not requested and the member does not belong "
        "to a group.")
    m_view_control->end_leave();
    return GCS_NOK;
  }

  m_leave_view_delivered = false;
  m_leave_view_requested = true;

  /*
    Request other nodes to remove this one from the membership.
  */
  m_xcom_proxy->xcom_remove_node(*m_local_node_info, m_gid_hash);

  /*
    Wait until the XCOM's thread exits.
  */
  int is_xcom_exit = m_xcom_proxy->xcom_wait_exit();

  if (is_xcom_exit == GCS_NOK) {
    MYSQL_GCS_LOG_ERROR("The member has failed to gracefully leave the group.")
    /*
      We have to really kill the XCOM's thread at this point because
      an attempt to make it gracefully exit apparently has failed.
    */
    if (m_xcom_proxy->xcom_exit(true)) {
      MYSQL_GCS_LOG_WARN(
          "Failed to kill the group communication engine "
          "after the member has failed to leave the group.");
    }
  }
  wait_for_xcom_thread();

  /*
    There is no need to interact with the local xcom anymore so we
    will can close local handlers.
  */
  if (m_xcom_proxy->xcom_close_handlers()) {
    MYSQL_GCS_LOG_ERROR(
        "Error on closing a connection to a group member while leaving "
        "the group.")
  }

  m_xcom_running = false;

  assert(m_xcom_proxy->xcom_is_exit());

  set_terminate_suspicion_thread(true);

  m_suspicions_processing_thread.join(NULL);
  MYSQL_GCS_LOG_TRACE("The suspicions processing thread has joined.");
  MYSQL_GCS_LOG_DEBUG("The member left the group.")

  m_view_control->end_leave();

  do_leave_view();

  /*
    Delete current view and set it to NULL.
  */
  m_view_control->set_current_view(NULL);

  return GCS_OK;
}

void Gcs_xcom_control::do_leave_view() {
  /*
    There is no need to synchronize here and this method can access
    the current_view member stored in the view controller directly.
  */
  Gcs_view *current_view = m_view_control->get_unsafe_current_view();

  if (current_view != NULL && !m_leave_view_delivered) {
    MYSQL_GCS_LOG_DEBUG("Will install leave view: requested %d, delivered %d",
                        m_leave_view_requested, m_leave_view_delivered);
    install_leave_view(m_leave_view_requested ? Gcs_view::OK
                                              : Gcs_view::MEMBER_EXPELLED);
    if (m_leave_view_requested) {
      m_view_control->set_belongs_to_group(false);
    }
    m_leave_view_delivered = m_leave_view_requested;
    MYSQL_GCS_LOG_DEBUG("Installed leave view: requested %d, delivered %d",
                        m_leave_view_requested, m_leave_view_delivered);
  }
}

connection_descriptor *Gcs_xcom_control::get_connection_to_node(
    std::string local_node_ip,
    std::vector<Gcs_xcom_node_address *> *peers_list) {
  connection_descriptor *con = NULL;
  std::vector<Gcs_xcom_node_address *>::iterator it;

  for (it = peers_list->begin(); (con == NULL) && it != peers_list->end();
       it++) {
    Gcs_xcom_node_address *peer = *(it);
    std::string peer_rep_ip;
    xcom_port port = 0;
    char *addr = NULL;

    bool resolve_error =
        resolve_ip_addr_from_hostname(peer->get_member_ip(), peer_rep_ip);

    if (resolve_error) {
      MYSQL_GCS_LOG_WARN("get_connection_to_node: ("
                         << local_node_ip << ") Unable to resolve peer address "
                         << peer->get_member_ip().c_str() << ". Skipping...")
      continue;
    }

    if (peer_rep_ip.compare(local_node_ip) == 0 &&
        peer->get_member_port() == m_local_node_address->get_member_port()) {
      MYSQL_GCS_LOG_TRACE("get_connection_to_node: (%s) Skipping own address.",
                          local_node_ip.c_str())
      // Skip own address if configured in the peer list
      continue;
    }

    port = peer->get_member_port();
    addr = (char *)peer->get_member_ip().c_str();

    MYSQL_GCS_LOG_TRACE(
        "get_connection_to_node: Client local port %s "
        "xcom_client_open_connection to %s:%d",
        local_node_ip.c_str(), addr, port)

    if ((con = m_xcom_proxy->xcom_client_open_connection(addr, port)) == NULL) {
      MYSQL_GCS_LOG_DEBUG(
          "get_connection_to_node: Error while opening a connection to %s:%d "
          "from: %s.",
          addr, port, local_node_ip.c_str())
    } else
      MYSQL_GCS_LOG_DEBUG(
          "get_connection_to_node: Opened connection to %s:%d "
          "from: %s. con is null? %d",
          addr, port, local_node_ip.c_str(), (con == NULL))
  }

  return con;
}

void Gcs_xcom_control::do_remove_node_from_group() {
  if (m_view_control->is_leaving() || !m_view_control->belongs_to_group())
    return;

  m_view_control->start_leave();

  int local_port = m_local_node_address->get_member_port();
  int rm_ret = 0;
  connection_descriptor *con = NULL;

  MYSQL_GCS_LOG_DEBUG("do_remove_node_from_group started! (%d)", local_port);

  /*
    Request other nodes to remove this one from the membership.
    1-check the latest view to see if there are unknown peers
    2-try on known m_initial_peers
  */

  // RESOLVE MY IP ADDRESS
  std::string local_node_info_str_ip;
  bool resolve_error = false;
  resolve_error = resolve_ip_addr_from_hostname(
      m_local_node_address->get_member_ip(), local_node_info_str_ip);

  if (resolve_error) {
    MYSQL_GCS_LOG_ERROR(
        "do_remove_node_from_group: Error resolving local address name: "
        << m_local_node_address->get_member_ip().c_str() << ". Leaving...")
    m_view_control->end_leave();
    return;
  } else {
    MYSQL_GCS_LOG_DEBUG(
        "do_remove_node_from_group: Resolved local_node_info_str_ip= %s",
        local_node_info_str_ip.c_str())
  }

  // VIEW MEMBERS
  Gcs_view *current_view = m_view_control->get_current_view();

  if (current_view != NULL) {
    std::vector<Gcs_member_identifier>::const_iterator it;
    std::vector<Gcs_xcom_node_address *> view_members;

    MYSQL_GCS_LOG_TRACE(
        "do_remove_node_from_group: current view has %ul members.",
        current_view->get_members().size())

    for (it = current_view->get_members().begin();
         !con && it != current_view->get_members().end(); it++) {
      std::string peer_rep_ip;
      Gcs_xcom_node_address *peer =
          new Gcs_xcom_node_address(it->get_member_id());

      view_members.push_back(peer);
    }

    if (!view_members.empty()) {
      con = get_connection_to_node(local_node_info_str_ip, &view_members);

      // CLEAN
      std::vector<Gcs_xcom_node_address *>::iterator clean_it;
      for (clean_it = view_members.begin(); clean_it != view_members.end();
           clean_it++) {
        delete *clean_it;
      }

      view_members.clear();
    }
  }

  if (!con) {
    // GET INITIAL PEERS
    MYSQL_GCS_LOG_DEBUG(
        "do_remove_node_from_group: (%d) Couldn't get a connection from view! "
        "Using initial peers...",
        local_port)

    con = get_connection_to_node(local_node_info_str_ip, &m_initial_peers);
  }

  if (con && !m_leave_view_delivered && m_view_control->belongs_to_group()) {
    MYSQL_GCS_LOG_TRACE(
        "do_remove_node_from_group: (%d) got a connection! "
        "m_leave_view_delivered=%d belongs=%d",
        local_port, m_leave_view_delivered, m_view_control->belongs_to_group())
    Gcs_xcom_nodes nodes_to_remove;
    nodes_to_remove.add_node(*m_local_node_info);
    rm_ret = m_xcom_proxy->xcom_remove_nodes(*con, nodes_to_remove, m_gid_hash);

    MYSQL_GCS_LOG_DEBUG(
        "do_remove_node_from_group: %d invoked xcom_remove_self!", local_port);
  } else {
    MYSQL_GCS_LOG_DEBUG(
        "do_remove_node_from_group: Unable to request another node to "
        "remove me (%d) from the group!",
        local_port);
  }

  if (con) {
    xcom_close_client_connection(con);
  }

  /*
    Destroy this node's stored suspicions and stop thread.
  */
  m_suspicions_manager->clear_suspicions();
  set_terminate_suspicion_thread(true);
  m_suspicions_processing_thread.join(NULL);
  MYSQL_GCS_LOG_TRACE("The suspicions processing thread has joined.");

  MYSQL_GCS_LOG_DEBUG("do_remove_node_from_group finished! Returning %d",
                      rm_ret);
  m_view_control->end_leave();

  return;
}

bool Gcs_xcom_control::belongs_to_group() {
  return m_view_control->belongs_to_group();
}

Gcs_view *Gcs_xcom_control::get_current_view() {
  return m_view_control->get_current_view();
}

const Gcs_member_identifier Gcs_xcom_control::get_local_member_identifier()
    const {
  return m_local_node_info->get_member_id();
}

int Gcs_xcom_control::add_event_listener(
    const Gcs_control_event_listener &event_listener) {
  int handler_key = 0;
  do {
    handler_key = rand();
  } while (event_listeners.count(handler_key) != 0);

  event_listeners.emplace(handler_key, event_listener);

  return handler_key;
}

void Gcs_xcom_control::remove_event_listener(int event_listener_handle) {
  event_listeners.erase(event_listener_handle);
}

struct Gcs_member_identifier_pointer_comparator {
  explicit Gcs_member_identifier_pointer_comparator(
      const Gcs_member_identifier &one)
      : m_one(one) {}

  bool operator()(Gcs_member_identifier *other) { return m_one == *other; }

 private:
  const Gcs_member_identifier &m_one;
};

void Gcs_xcom_control::build_total_members(
    Gcs_xcom_nodes *xcom_nodes,
    std::vector<Gcs_member_identifier *> &alive_members,
    std::vector<Gcs_member_identifier *> &failed_members) {
  const std::vector<Gcs_xcom_node_information> &nodes = xcom_nodes->get_nodes();

  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
  for (nodes_it = nodes.begin(); nodes_it != nodes.end(); ++nodes_it) {
    /*
      Build the member identifier from the address reported.
    */
    Gcs_member_identifier *member_id =
        new Gcs_member_identifier((*nodes_it).get_member_id());

    /*
      Check whether the node is reported as alive or faulty.
    */
    if ((*nodes_it).is_alive()) {
      alive_members.push_back(member_id);
    } else {
      failed_members.push_back(member_id);
    }
  }
}

void Gcs_xcom_control::build_joined_members(
    std::vector<Gcs_member_identifier *> &joined_members,
    std::vector<Gcs_member_identifier *> &alive_members,
    const std::vector<Gcs_member_identifier> *current_members) {
  std::vector<Gcs_member_identifier *>::iterator alive_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  for (alive_members_it = alive_members.begin();
       alive_members_it != alive_members.end(); alive_members_it++) {
    /*
      If there is no previous view installed, there is no current set
      of members. In this case, all nodes reported as alive will be
      considered nodes that are joining.
    */
    bool joined = true;
    if (current_members != NULL) {
      current_members_it =
          std::find(current_members->begin(), current_members->end(),
                    *(*alive_members_it));
      if (current_members_it != current_members->end()) joined = false;
    }

    if (joined) {
      joined_members.push_back(new Gcs_member_identifier(*(*alive_members_it)));
    }
  }
}

void Gcs_xcom_control::build_left_members(
    std::vector<Gcs_member_identifier *> &left_members,
    std::vector<Gcs_member_identifier *> &alive_members,
    std::vector<Gcs_member_identifier *> &failed_members,
    const std::vector<Gcs_member_identifier> *current_members) {
  std::vector<Gcs_member_identifier *>::iterator alive_members_it;
  std::vector<Gcs_member_identifier *>::iterator failed_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  /*
    If there isn't a set of current members, this means that a view hasn't
    been installed before and nobody can leave something that does not
    exist.
  */
  if (current_members == NULL) return;

  for (current_members_it = current_members->begin();
       current_members_it != current_members->end(); current_members_it++) {
    alive_members_it = std::find_if(
        alive_members.begin(), alive_members.end(),
        Gcs_member_identifier_pointer_comparator(*current_members_it));

    failed_members_it = std::find_if(
        failed_members.begin(), failed_members.end(),
        Gcs_member_identifier_pointer_comparator(*current_members_it));

    /*
      Node in the current view is not found in the set of alive or failed
      members meaning that it has been expelled from the cluster.
    */
    if (alive_members_it == alive_members.end() &&
        failed_members_it == failed_members.end()) {
      left_members.push_back(new Gcs_member_identifier(*current_members_it));
    }
  }
}

void Gcs_xcom_control::build_member_suspect_nodes(
    std::vector<Gcs_member_identifier *> &member_suspect_nodes,
    std::vector<Gcs_member_identifier *> &failed_members,
    const std::vector<Gcs_member_identifier> *current_members) {
  std::vector<Gcs_member_identifier *>::iterator failed_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  /*
    If there isn't a set of current members, this means that a view hasn't
    been installed before and nobody will be expelled by this node.
  */
  if ((current_members == NULL) || current_members->empty() ||
      failed_members.empty())
    return;

  for (current_members_it = current_members->begin();
       current_members_it != current_members->end(); current_members_it++) {
    failed_members_it = std::find_if(
        failed_members.begin(), failed_members.end(),
        Gcs_member_identifier_pointer_comparator(*current_members_it));

    /*
      If a node in the current view is in the set of failed nodes it must
      be expelled.
    */
    if (failed_members_it != failed_members.end()) {
      member_suspect_nodes.push_back(
          new Gcs_member_identifier(*(*failed_members_it)));
    }
  }
}

void Gcs_xcom_control::build_non_member_suspect_nodes(
    std::vector<Gcs_member_identifier *> &non_member_suspect_nodes,
    std::vector<Gcs_member_identifier *> &failed_members,
    const std::vector<Gcs_member_identifier> *current_members) {
  std::vector<Gcs_member_identifier *>::iterator failed_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  /*
    If there isn't a set of failed members, this means that there are no
    suspect nodes.
  */
  if ((current_members == NULL) || current_members->empty() ||
      failed_members.empty())
    return;

  for (failed_members_it = failed_members.begin();
       failed_members_it != failed_members.end(); ++failed_members_it) {
    current_members_it =
        std::find(current_members->begin(), current_members->end(),
                  *(*failed_members_it));

    /*
      If a node is in the set of failed nodes and not in the current view,
      it becomes a suspect.
    */
    if (current_members_it == current_members->end()) {
      non_member_suspect_nodes.push_back(
          new Gcs_member_identifier(*(*failed_members_it)));
    }
  }
}

bool Gcs_xcom_control::is_killer_node(
    const std::vector<Gcs_member_identifier *> &alive_members) const {
  /*
    Note that the member elected to remove another members from the group
    if they are considered faulty is the first one in the list of alive
    members.
  */
  assert(alive_members.size() != 0 && alive_members[0] != NULL);
  bool ret = get_local_member_identifier() == *alive_members[0];
  MYSQL_GCS_LOG_DEBUG("The member %s will be responsible for killing: %d",
                      get_local_member_identifier().get_member_id().c_str(),
                      ret)
  return ret;
}

bool Gcs_xcom_control::xcom_receive_local_view(Gcs_xcom_nodes *xcom_nodes) {
  std::map<int, const Gcs_control_event_listener &>::const_iterator callback_it;
  std::vector<Gcs_member_identifier> members;
  std::vector<Gcs_member_identifier> unreachable;
  Gcs_view *current_view = m_view_control->get_unsafe_current_view();
  const std::vector<Gcs_xcom_node_information> &nodes = xcom_nodes->get_nodes();
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;

  // ignore
  if (xcom_nodes->get_size() <= 0) goto end;

  // if I am not aware of any view at all
  if (current_view != NULL) {
    std::vector<Gcs_member_identifier *> alive_members;
    std::vector<Gcs_member_identifier *> failed_members;
    std::vector<Gcs_member_identifier *> left_members;
    std::vector<Gcs_member_identifier *> joined_members;
    std::vector<Gcs_member_identifier *> non_member_suspect_nodes;
    std::vector<Gcs_member_identifier *> member_suspect_nodes;
    std::vector<Gcs_member_identifier *>::iterator it;
    const std::vector<Gcs_member_identifier> &cv_members =
        current_view->get_members();
    for (nodes_it = nodes.begin(); nodes_it != nodes.end(); ++nodes_it) {
      /*
        Build the member identifier from the address reported.
      */
      Gcs_member_identifier member_id((*nodes_it).get_member_id());

      // filter out those that are not yet in the current view
      // delivered to the application. For example, they might
      // exist only in the state exchange phase for now, but once
      // that is done, the current view gets updated and such
      // members will be in it. In other words, there could be
      // members that are not yet visible to upper layers.
      if (std::find(cv_members.begin(), cv_members.end(), member_id) !=
          cv_members.end()) {
        members.push_back(member_id);
        MYSQL_GCS_LOG_DEBUG("Local view with member: %s",
                            member_id.get_member_id().c_str());

        if (!(*nodes_it).is_alive()) {
          unreachable.push_back(member_id);
          MYSQL_GCS_LOG_DEBUG("Local view with suspected member: %s",
                              member_id.get_member_id().c_str());
        }
      }
    }

    /*
      Identify which nodes are alive and which are considered faulty.

      Note that there may be new nodes that are marked as faulty because the
      connections among their peers are still beeing established.
    */
    build_total_members(xcom_nodes, alive_members, failed_members);

    /*
      Build the set of joined members which are all alive members that are
      not part of the current members.

      In other words, joined = (alive - current).
    */
    build_joined_members(joined_members, alive_members, &cv_members);

    /*
      Build the set of left members which has any member that is part of the
      current members but it is not in the set of alive or failed members.

      In other words, left = current - (alive \/ failed).
    */
    build_left_members(left_members, alive_members, failed_members,
                       &cv_members);

    /*
      Build the set of nodes that must be expelled by a killer node. These
      nodes are those that are part of the current members and are marked
      as faulty in the view.
      In other words, left = current /\ failed.
    */
    build_member_suspect_nodes(member_suspect_nodes, failed_members,
                               &cv_members);

    /*
      Build the set of nodes that are suspicious, i.e., could have failed
      while entering the group most probably during state exchange.
      These nodes are those that are not part of the current members and
      are marked as faulty in the view.
      In other words, suspect = !current /\ failed.
    */
    build_non_member_suspect_nodes(non_member_suspect_nodes, failed_members,
                                   &cv_members);

    // Remove and add suspicions
    m_suspicions_manager->process_view(
        xcom_nodes, alive_members, left_members, member_suspect_nodes,
        non_member_suspect_nodes, is_killer_node(alive_members));

    MYSQL_GCS_TRACE_EXECUTE(
        unsigned int node_no = xcom_nodes->get_node_no();
        for (it = alive_members.begin(); it != alive_members.end(); it++)
            MYSQL_GCS_LOG_TRACE("(My node_id is (%u) Node considered alive "
                                "in the cluster: %s",
                                node_no, (*it)->get_member_id().c_str());

        for (it = failed_members.begin(); it != failed_members.end(); it++)
            MYSQL_GCS_LOG_TRACE("(My node_id is (%u) Node considered faulty "
                                "in the cluster: %s",
                                node_no, (*it)->get_member_id().c_str());

        for (it = left_members.begin(); it != left_members.end(); it++)
            MYSQL_GCS_LOG_TRACE(
                "(My node_id is (%d) Node leaving the cluster: %s", node_no,
                (*it)->get_member_id().c_str());

        for (it = joined_members.begin(); it != joined_members.end(); it++)
            MYSQL_GCS_LOG_TRACE(
                "My node_id is (%d) Node joining the cluster: %s", node_no,
                (*it)->get_member_id().c_str());

        for (it = member_suspect_nodes.begin();
             it != member_suspect_nodes.end(); it++)
            MYSQL_GCS_LOG_TRACE("My node_id is (%d) Member node considered "
                                "suspicious in the "
                                "cluster: %s",
                                node_no, (*it)->get_member_id().c_str());

        for (it = non_member_suspect_nodes.begin();
             it != non_member_suspect_nodes.end(); it++)
            MYSQL_GCS_LOG_TRACE("My node_id is (%d) Non-member node considered "
                                "suspicious in the cluster: %s",
                                node_no, (*it)->get_member_id().c_str());)

    // always notify local views
    for (callback_it = event_listeners.begin();
         callback_it != event_listeners.end(); callback_it++) {
      callback_it->second.on_suspicions(members, unreachable);
    }

    // clean up tentative sets
    for (it = left_members.begin(); it != left_members.end(); it++) delete *it;
    left_members.clear();

    for (it = joined_members.begin(); it != joined_members.end(); it++)
      delete *it;
    joined_members.clear();

    for (it = alive_members.begin(); it != alive_members.end(); it++)
      delete *it;
    alive_members.clear();

    for (it = failed_members.begin(); it != failed_members.end(); it++)
      delete *it;
    failed_members.clear();

    for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
         it++)
      delete *it;
    member_suspect_nodes.clear();

    for (it = non_member_suspect_nodes.begin();
         it != non_member_suspect_nodes.end(); it++)
      delete *it;
    non_member_suspect_nodes.clear();
  }
end:
  return false;
}

void Gcs_xcom_control::install_leave_view(
    Gcs_view::Gcs_view_error_code error_code) {
  Gcs_view *current_view = m_view_control->get_unsafe_current_view();

  // Create the new view id here, based in the previous one plus 1
  Gcs_xcom_view_identifier *new_view_id = new Gcs_xcom_view_identifier(
      (Gcs_xcom_view_identifier &)current_view->get_view_id());
  new_view_id->increment_by_one();

  // Build a best-effort view...
  set<Gcs_member_identifier *> *total, *left, *joined;
  total = new set<Gcs_member_identifier *>();
  left = new set<Gcs_member_identifier *>();
  joined = new set<Gcs_member_identifier *>();

  // Build left... just me...
  left->insert(new Gcs_member_identifier(m_local_node_info->get_member_id()));

  // Build total... all but me...
  std::vector<Gcs_member_identifier>::const_iterator old_total_it;
  for (old_total_it = current_view->get_members().begin();
       old_total_it != current_view->get_members().end(); old_total_it++) {
    if (*old_total_it == m_local_node_info->get_member_id()) continue;

    total->insert(new Gcs_member_identifier(*old_total_it));
  }

  MYSQL_GCS_LOG_DEBUG("Installing leave view.")

  Gcs_group_identifier gid(current_view->get_group_id().get_group_id());
  install_view(new_view_id, gid, NULL, total, left, joined, error_code);

  set<Gcs_member_identifier *>::iterator total_it;
  for (total_it = total->begin(); total_it != total->end(); total_it++)
    delete (*total_it);
  delete total;

  set<Gcs_member_identifier *>::iterator left_it;
  for (left_it = left->begin(); left_it != left->end(); left_it++)
    delete (*left_it);

  delete left;
  delete joined;
  delete new_view_id;
}

bool Gcs_xcom_control::is_this_node_in(
    std::vector<Gcs_member_identifier *> *failed_members) {
  bool is_in_vector = false;

  std::vector<Gcs_member_identifier *>::iterator it;

  for (it = failed_members->begin();
       it != failed_members->end() && !is_in_vector; ++it) {
    is_in_vector = (*(*it) == m_local_node_info->get_member_id());
  }

  return is_in_vector;
}

bool Gcs_xcom_control::xcom_receive_global_view(synode_no message_id,
                                                Gcs_xcom_nodes *xcom_nodes,
                                                bool same_view) {
  bool ret = false;
  bool free_built_members = false;

  std::vector<Gcs_member_identifier *> alive_members;
  std::vector<Gcs_member_identifier *> failed_members;
  std::vector<Gcs_member_identifier *> left_members;
  std::vector<Gcs_member_identifier *> joined_members;
  std::vector<Gcs_member_identifier *> non_member_suspect_nodes;
  std::vector<Gcs_member_identifier *> member_suspect_nodes;
  std::vector<Gcs_member_identifier *>::iterator it;

  std::string group_name(m_gid->get_group_id());

  std::map<int, const Gcs_control_event_listener &>::const_iterator listener_it;
  std::map<int, const Gcs_control_event_listener &>::const_iterator
      listener_ends;
  std::vector<Gcs_message_data *> exchange_data;

  /*
    If there is no previous view installed, there is no current set
    of members.
  */
  Gcs_view *current_view = m_view_control->get_unsafe_current_view();
  std::vector<Gcs_member_identifier> *current_members = NULL;
  if (current_view != NULL)
    current_members = const_cast<std::vector<Gcs_member_identifier> *>(
        &current_view->get_members());
  MYSQL_GCS_LOG_TRACE("::xcom_receive_global_view():: My node_id is %d",
                      xcom_nodes->get_node_no())

  /*
    Identify which nodes are alive and which are considered faulty.

    Note that there may be new nodes that are marked as faulty because the
    connections among their peers are still beeing established.
  */
  build_total_members(xcom_nodes, alive_members, failed_members);

  /*
    Build the set of joined members which are all alive members that are not
    part of the current members.

    In other words, joined = (alive - current).
  */
  build_joined_members(joined_members, alive_members, current_members);

  /*
    Build the set of left members which has any member that is part of the
    current members but it is not in the set of alive or failed members.

    In other words, left = current - (alive \/ failed).
  */
  build_left_members(left_members, alive_members, failed_members,
                     current_members);

  /*
    Build the set of nodes that must be expelled by a killer node. These
    nodes are those that are part of the current members and are marked
    as faulty in the view.
    In other words, left = current /\ failed.
  */
  build_member_suspect_nodes(member_suspect_nodes, failed_members,
                             current_members);

  /*
    Build the set of nodes that are suspicious, i.e., could have failed
    while entering the group most probably during state exchange.
    These nodes are those that are not part of the current members and
    are marked as faulty in the view.
    In other words, suspect = !current /\ failed.
  */
  build_non_member_suspect_nodes(non_member_suspect_nodes, failed_members,
                                 current_members);

  // Remove and add suspicions
  m_suspicions_manager->process_view(
      xcom_nodes, alive_members, left_members, member_suspect_nodes,
      non_member_suspect_nodes, is_killer_node(alive_members));

  /*
   We save the information on the nodes reported by the global view.
   This is necessary if want to reconfigure the group. In the future,
   we should revisit this decision and check whether we should copy
   such information to the view.
   */
  m_xcom_group_management->set_xcom_nodes(*xcom_nodes);

  MYSQL_GCS_TRACE_EXECUTE(
      unsigned int node_no = xcom_nodes->get_node_no();
      for (it = alive_members.begin(); it != alive_members.end(); it++)
          MYSQL_GCS_LOG_TRACE("(My node_id is (%u) Node considered alive "
                              "in the cluster: %s",
                              node_no, (*it)->get_member_id().c_str());

      for (it = failed_members.begin(); it != failed_members.end(); it++)
          MYSQL_GCS_LOG_TRACE("(My node_id is (%u) Node considered faulty "
                              "in the cluster: %s",
                              node_no, (*it)->get_member_id().c_str());

      for (it = left_members.begin(); it != left_members.end(); it++)
          MYSQL_GCS_LOG_TRACE(
              "(My node_id is (%d) Node leaving the cluster: %s", node_no,
              (*it)->get_member_id().c_str());

      for (it = joined_members.begin(); it != joined_members.end(); it++)
          MYSQL_GCS_LOG_TRACE("My node_id is (%d) Node joining the cluster: %s",
                              node_no, (*it)->get_member_id().c_str());

      for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
           it++)
          MYSQL_GCS_LOG_TRACE(
              "My node_id is (%d) Member node considered suspicious in the "
              "cluster: %s",
              node_no, (*it)->get_member_id().c_str());

      for (it = non_member_suspect_nodes.begin();
           it != non_member_suspect_nodes.end(); it++)
          MYSQL_GCS_LOG_TRACE("My node_id is (%d) Non-member node considered "
                              "suspicious in the cluster: %s",
                              node_no, (*it)->get_member_id().c_str());)

  const Gcs_xcom_node_information *node_info =
      xcom_nodes->get_node(m_local_node_info->get_member_id());

  if ((current_view != NULL) &&
      ((NULL == node_info) || is_this_node_in(&left_members))) {
    MYSQL_GCS_LOG_TRACE(
        "::xcom_receive_global_view()::I'm node %s and I'm not in the "
        "view! "
        "Installing leave view: MEMBER_EXPELLED!",
        m_local_node_info->get_member_id().get_member_id().c_str())
    install_leave_view(Gcs_view::MEMBER_EXPELLED);
    ret = true;
    free_built_members = true;
    goto end;
  }

  /*
    If nobody has joined or left the group, there is no need to install any
    view. This fact is denoted by the same_view flag and the execution only
    gets so far because we may have to kill some faulty members. Note that
    the code could be optimized but we are focused on correctness for now
    and any possible optimization will be handled in the future.

    Views that contain a node marked as faulty are also ignored. This is
    done because XCOM does not deliver messages to or accept messages from
    such nodes. So if we have considered them, the state exchange phase
    would block expecting messages that would never arrive.

    So faulty members that are part of an old view will be eventually
    expelled and a clean view, i.e. a view without faulty members, will be
    eventually delivered. Note, however, that we are not considering
    failures during the the join phase which includes the state exchange
    phase as well. This will require implementing some sort of join timeout.

    If the execution has managed to pass these steps, the node is alive and
    it is time to start building the view.
  */
  if (same_view || failed_members.size() != 0) {
    MYSQL_GCS_LOG_TRACE(
        "(My node_id is %d) ::xcom_receive_global_view():: "
        "Discarding view because nothing has changed. "
        "Same view flag is %d, number of failed nodes is %llu"
        ", number of joined nodes is %llu, number of left nodes is %llu",
        xcom_nodes->get_node_no(), same_view,
        static_cast<long long unsigned>(failed_members.size()),
        static_cast<long long unsigned>(joined_members.size()),
        static_cast<long long unsigned>(left_members.size()));

    ret = true;
    free_built_members = true;
    goto end;
  }

  /*
    Check if the node is executing the state exchange phase to install a
    new view. If this is the case, we need to stop the old state exchange
    phase and start a new one.

    Note that if there is a high churn rate, the system can never ends
    the state exchange phase. We don't think this is a problem for this
    algorithm because worse things will happen first. If this algorithm
    becomes a problem, we can easily improve it in the future. For now,
    we think it is fine to keep it simple and simply restart the state
    exchange phase.
  */
  if (m_view_control->is_view_changing()) {
    MYSQL_GCS_LOG_TRACE(
        "View exchange is ongoing. Resetting state exchange. My node_id is "
        "%d",
        xcom_nodes->get_node_no())
    MYSQL_GCS_TRACE_EXECUTE(
        for (it = left_members.begin(); it != left_members.end(); it++)
            MYSQL_GCS_LOG_TRACE(
                "(My node_id is %d) Node is leaving the cluster: %s",
                xcom_nodes->get_node_no(), (*it)->get_member_id().c_str());

        for (it = joined_members.begin(); it != joined_members.end(); it++)
            MYSQL_GCS_LOG_TRACE(
                "(My node_id is %d) Node joining the cluster: %s",
                xcom_nodes->get_node_no(), (*it)->get_member_id().c_str());)
    m_state_exchange->reset_with_flush();
  }

  m_view_control->start_view_exchange();

  /*
    We check if the registered listeners have data to be exchanged
    during a view change and put it (i.e. the Gcs_message_data)
    into a vector. Ideally there should be only one Gcs_message_data,
    although the current interfaces do not prohibit that.

    Then the state exchange message is sent to all peers.
  */
  listener_ends = event_listeners.end();
  for (listener_it = event_listeners.begin(); listener_it != listener_ends;
       ++listener_it) {
    Gcs_message_data *msg_data = (*listener_it).second.get_exchangeable_data();
    exchange_data.push_back(msg_data);
  }
  m_state_exchange->state_exchange(
      message_id, alive_members, left_members, joined_members, exchange_data,
      current_view, &group_name, m_local_node_info->get_member_id());
  MYSQL_GCS_LOG_TRACE("::xcom_receive_global_view():: state exchange started.")

end:
  if (free_built_members) {
    // clean up tentative sets

    for (it = left_members.begin(); it != left_members.end(); it++) delete *it;
    left_members.clear();

    for (it = joined_members.begin(); it != joined_members.end(); it++)
      delete *it;
    joined_members.clear();

    for (it = alive_members.begin(); it != alive_members.end(); it++)
      delete *it;
    alive_members.clear();

    for (it = failed_members.begin(); it != failed_members.end(); it++)
      delete *it;
    failed_members.clear();

    for (it = member_suspect_nodes.begin(); it != member_suspect_nodes.end();
         it++)
      delete *it;
    member_suspect_nodes.clear();

    for (it = non_member_suspect_nodes.begin();
         it != non_member_suspect_nodes.end(); it++)
      delete *it;
    non_member_suspect_nodes.clear();
  }

  return ret;
}

void Gcs_xcom_control::process_control_message(Gcs_message *msg,
                                               unsigned int protocol_version) {
  MYSQL_GCS_LOG_TRACE(
      "::process_control_message():: Received a control message")

  Xcom_member_state *ms_info =
      new Xcom_member_state(msg->get_message_data().get_payload(),
                            msg->get_message_data().get_payload_length());

  MYSQL_GCS_LOG_TRACE(
      "Reading message that carries exchangeable data: (payload)=%llu",
      static_cast<long long unsigned>(
          msg->get_message_data().get_payload_length()));

  MYSQL_GCS_LOG_TRACE(
      "::process_control_message():: From: %s regarding view_id: %s in %s",
      msg->get_origin().get_member_id().c_str(),
      ms_info->get_view_id()->get_representation().c_str(),
      get_node_address()->get_member_address().c_str())

  /*
    XCOM does not preserve FIFO and for that reason a message from
    a previous state exchange phase may arrive when the newest phase
    has already finished.
  */
  MYSQL_GCS_DEBUG_EXECUTE(
      synode_no configuration_id = ms_info->get_configuration_id();
      if (!m_view_control->is_view_changing()){MYSQL_GCS_LOG_DEBUG(
          "There is no state exchange going on. Ignoring exchangeable data "
          "because its from a previous state exchange phase. Message is "
          "from group_id (%u), msg_no(%llu), node_no(%llu)",
          configuration_id.group_id,
          static_cast<long long unsigned>(configuration_id.msgno),
          static_cast<long long unsigned>(
              configuration_id
                  .node))} MYSQL_GCS_LOG_DEBUG("There is a state exchange "
                                               "going on. Message is "
                                               "from group_id (%u), "
                                               "msg_no(%llu), "
                                               "node_no(%llu)",
                                               configuration_id.group_id,
                                               static_cast<long long unsigned>(
                                                   configuration_id.msgno),
                                               static_cast<long long unsigned>(
                                                   configuration_id.node)))

  if (!m_view_control->is_view_changing()) {
    delete ms_info;
    delete msg;
    return;
  }

  Gcs_member_identifier pid(msg->get_origin());
  // takes ownership of ms_info
  bool can_install_view =
      m_state_exchange->process_member_state(ms_info, pid, protocol_version);

  // If state exchange has finished
  if (can_install_view) {
    MYSQL_GCS_LOG_TRACE("::process_control_message()::Install new view")

    /*
      Check if all members have a protocol version number that is compatible
      with the current protocol version number in use.
     */
    assert(m_state_exchange->compute_incompatible_protocol_members().size() ==
           0);

    // Make a copy of the state exchange provided view id
    Gcs_xcom_view_identifier *provided_view_id =
        m_state_exchange->get_new_view_id();

    Gcs_xcom_view_identifier *new_view_id =
        new Gcs_xcom_view_identifier(*provided_view_id);

    new_view_id->increment_by_one();

    install_view(new_view_id, *m_gid, m_state_exchange->get_member_states(),
                 m_state_exchange->get_total(), m_state_exchange->get_left(),
                 m_state_exchange->get_joined());

    delete new_view_id;
  } else {
    MYSQL_GCS_LOG_TRACE(
        "::process_control_message():: Still waiting for more State "
        "Exchange "
        "messages: %s",
        m_local_node_info->get_member_id().get_member_id().c_str())
  }

  delete msg;
}

void Gcs_xcom_control::install_view(
    Gcs_xcom_view_identifier *new_view_id, const Gcs_group_identifier &group,
    std::map<Gcs_member_identifier, Xcom_member_state *> *states,
    set<Gcs_member_identifier *> *total, set<Gcs_member_identifier *> *left,
    set<Gcs_member_identifier *> *join,
    Gcs_view::Gcs_view_error_code error_code) {
  // Build all sets of all, left and joined members
  std::vector<Gcs_member_identifier> members;
  build_member_list(total, &members);

  std::vector<Gcs_member_identifier> left_members;
  build_member_list(left, &left_members);

  std::vector<Gcs_member_identifier> joined_members;
  build_member_list(join, &joined_members);

  // Build the new view id and the group id
  Gcs_xcom_view_identifier v_id(*new_view_id);

  // Create the new view
  Gcs_view *current_view = new Gcs_view(members, v_id, left_members,
                                        joined_members, group, error_code);

  // Build the exchanged data
  Exchanged_data data_to_deliver;
  if (states != NULL) {
    std::map<Gcs_member_identifier, Xcom_member_state *>::iterator states_it;
    for (states_it = states->begin(); states_it != states->end(); states_it++) {
      MYSQL_GCS_LOG_DEBUG(
          "Processing exchanged data while installing the new view")

      Gcs_member_identifier *member_id =
          new Gcs_member_identifier((*states_it).first);

      Xcom_member_state *data_exchanged = (*states_it).second;

      Gcs_message_data *data_exchanged_holder = NULL;

      if (data_exchanged != NULL && data_exchanged->get_data_size() != 0) {
        data_exchanged_holder =
            new Gcs_message_data(data_exchanged->get_data_size());
        data_exchanged_holder->decode(data_exchanged->get_data(),
                                      data_exchanged->get_data_size());
      }

      std::pair<Gcs_member_identifier *, Gcs_message_data *> state_pair(
          member_id, data_exchanged_holder);

      data_to_deliver.push_back(state_pair);
    }
  } else {
    MYSQL_GCS_LOG_TRACE("::install_view():: No exchanged data")
  }

  /*
    Set the current view before notifying all listeners.
  */
  m_view_control->set_current_view(current_view);

  /*
    Note that the variable that identifies whether a node belongs to
    a group is set before delivering the view change message so there
    is a small window when it is possible to send messages although
    the view has not been installed.
  */
  m_view_control->set_belongs_to_group(true);

  map<int, const Gcs_control_event_listener &>::iterator callback_it =
      event_listeners.begin();

  while (callback_it != event_listeners.end()) {
    (*callback_it).second.on_view_changed(*current_view, data_to_deliver);

    MYSQL_GCS_LOG_TRACE(
        "::install_view():: View delivered to client handler= %d ",
        (*callback_it).first)

    ++callback_it;
  }

  Exchanged_data::const_iterator it;
  for (it = data_to_deliver.begin(); it != data_to_deliver.end(); it++) {
    delete (*it).first;
    delete (*it).second;
  }

  m_view_control->end_view_exchange();

  m_state_exchange->end();
}

void Gcs_xcom_control::build_member_list(
    set<Gcs_member_identifier *> *origin,
    std::vector<Gcs_member_identifier> *to_fill) {
  std::set<Gcs_member_identifier *>::iterator it;

  for (it = origin->begin(); it != origin->end(); it++) {
    Gcs_member_identifier member_id(*(*it));

    to_fill->push_back(member_id);
  }
}

void Gcs_xcom_control::init_me() {
  assert(m_local_node_info != NULL);
  m_local_node_info->regenerate_member_uuid();
}

void Gcs_xcom_control::set_node_address(
    Gcs_xcom_node_address *xcom_node_address) {
  m_local_node_address = xcom_node_address;
  string address = xcom_node_address->get_member_address();
  delete m_local_node_info;
  /*
    We don't care at this point about the unique identifier associated to
    this object because it will be changed while joining a group by the
    init_me() method.
  */
  m_local_node_info = new Gcs_xcom_node_information(address);
}

void Gcs_xcom_control::set_peer_nodes(
    std::vector<Gcs_xcom_node_address *> &xcom_peers) {
  clear_peer_nodes();

  std::vector<Gcs_xcom_node_address *>::iterator it;
  for (it = xcom_peers.begin(); it != xcom_peers.end(); ++it) {
    m_initial_peers.push_back(
        new Gcs_xcom_node_address((*it)->get_member_address()));
  }
}

void Gcs_xcom_control::clear_peer_nodes() {
  if (!m_initial_peers.empty()) {
    std::vector<Gcs_xcom_node_address *>::iterator it;
    for (it = m_initial_peers.begin(); it != m_initial_peers.end(); ++it)
      delete (*it);

    m_initial_peers.clear();
  }
}

Gcs_suspicions_manager::Gcs_suspicions_manager(Gcs_xcom_proxy *proxy,
                                               Gcs_xcom_control *ctrl)
    : m_proxy(proxy),
      m_control_if(ctrl),
      m_suspicions_processing_period(SUSPICION_PROCESSING_THREAD_PERIOD),
      m_non_member_expel_timeout(NON_MEMBER_EXPEL_TIMEOUT),
      m_member_expel_timeout(0),
      m_gid_hash(0),
      m_suspicions(),
      m_suspicions_mutex(),
      m_suspicions_cond(),
      m_suspicions_parameters_mutex(),
      m_is_killer_node(false) {
  m_suspicions_mutex.init(
      key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_mutex, NULL);
  m_suspicions_cond.init(key_GCS_COND_Gcs_suspicions_manager_m_suspicions_cond);
  m_suspicions_parameters_mutex.init(
      key_GCS_MUTEX_Gcs_suspicions_manager_m_suspicions_parameters_mutex, NULL);
}

Gcs_suspicions_manager::~Gcs_suspicions_manager() {
  m_suspicions_mutex.destroy();
  m_suspicions_cond.destroy();
  m_suspicions_parameters_mutex.destroy();
}

void Gcs_suspicions_manager::remove_suspicions(
    std::vector<Gcs_member_identifier *> nodes) {
  const Gcs_xcom_node_information *xcom_node = NULL;
  std::vector<Gcs_member_identifier *>::iterator non_suspect_it;

  // Foreach received node
  for (non_suspect_it = nodes.begin(); non_suspect_it != nodes.end();
       ++non_suspect_it) {
    const Gcs_xcom_node_information node_to_remove(
        (*non_suspect_it)->get_member_id());
    if ((xcom_node = m_suspicions.get_node(*(*non_suspect_it))) != NULL) {
      m_suspicions.remove_node(node_to_remove);
      MYSQL_GCS_LOG_DEBUG("Removed suspicion on node %s",
                          (*non_suspect_it)->get_member_id().c_str());
    }
  }
}

void Gcs_suspicions_manager::clear_suspicions() {
  m_suspicions_mutex.lock();
  // Cycle through the suspicions
  std::vector<Gcs_xcom_node_information>::iterator susp_it;
  std::vector<Gcs_xcom_node_information> nodes = m_suspicions.get_nodes();

  for (susp_it = nodes.begin(); susp_it != nodes.end(); ++susp_it) {
    // Foreach existing suspicion
    MYSQL_GCS_LOG_TRACE("clear_suspicions: Removing suspicion for %s...",
                        (*susp_it).get_member_id().get_member_id().c_str())
    m_suspicions.remove_node(*susp_it);
  }
  m_suspicions_mutex.unlock();
}

void Gcs_suspicions_manager::process_view(
    Gcs_xcom_nodes *xcom_nodes,
    std::vector<Gcs_member_identifier *> alive_nodes,
    std::vector<Gcs_member_identifier *> left_nodes,
    std::vector<Gcs_member_identifier *> member_suspect_nodes,
    std::vector<Gcs_member_identifier *> non_member_suspect_nodes,
    bool is_killer_node) {
  bool should_wake_up_manager = false;

  m_suspicions_mutex.lock();

  m_is_killer_node = is_killer_node;

  m_has_majority =
      2 * (member_suspect_nodes.size() + non_member_suspect_nodes.size()) <
      xcom_nodes->get_nodes().size();

  /*
    Suspicions are removed for members that are alive. Therefore, the
    remove_suspicions method is only invoked if the group has any active
    members and if there are any stored suspicions.
  */
  if (!m_suspicions.empty() && !alive_nodes.empty()) {
    remove_suspicions(alive_nodes);
  }

  /*
    Suspicions are removed for members that have already left the group.
    Therefore, the remove_suspicions method is only invoked if any members
    left the group and if there are any stored suspicions.
  */
  if (!m_suspicions.empty() && !left_nodes.empty()) {
    remove_suspicions(left_nodes);
  }

  if (!non_member_suspect_nodes.empty() || !member_suspect_nodes.empty()) {
    should_wake_up_manager = add_suspicions(
        xcom_nodes, non_member_suspect_nodes, member_suspect_nodes);
  }

  if (should_wake_up_manager) {
    m_suspicions_cond.signal();
  }
  m_suspicions_mutex.unlock();
}

bool Gcs_suspicions_manager::add_suspicions(
    Gcs_xcom_nodes *xcom_nodes,
    std::vector<Gcs_member_identifier *> non_member_suspect_nodes,
    std::vector<Gcs_member_identifier *> member_suspect_nodes) {
  const Gcs_xcom_node_information *xcom_node = NULL;
  std::vector<Gcs_member_identifier *>::iterator susp_it;
  bool member_suspicions_added = false;

  // Get current timestamp
  uint64_t current_ts = My_xp_util::getsystime();

  for (susp_it = non_member_suspect_nodes.begin();
       susp_it != non_member_suspect_nodes.end(); ++susp_it) {
    if ((xcom_node = m_suspicions.get_node(*(*susp_it))) == NULL) {
      MYSQL_GCS_LOG_DEBUG(
          "add_suspicions: Adding non-member expel suspicion for %s",
          (*susp_it)->get_member_id().c_str())
      xcom_node = xcom_nodes->get_node(*(*susp_it));
      const_cast<Gcs_xcom_node_information *>(xcom_node)
          ->set_suspicion_creation_timestamp(current_ts);
      const_cast<Gcs_xcom_node_information *>(xcom_node)->set_member(false);
      m_suspicions.add_node(*xcom_node);
    } else {
      // Otherwise, ignore node
      MYSQL_GCS_LOG_TRACE(
          "add_suspicions: Not adding non-member expel suspicion for %s. "
          "Already a suspect!",
          (*susp_it)->get_member_id().c_str())
    }
  }

  for (susp_it = member_suspect_nodes.begin();
       susp_it != member_suspect_nodes.end(); ++susp_it) {
    if ((xcom_node = m_suspicions.get_node(*(*susp_it))) == NULL) {
      MYSQL_GCS_LOG_DEBUG(
          "add_suspicions: Adding member expel suspicion for %s",
          (*susp_it)->get_member_id().c_str())
      xcom_node = xcom_nodes->get_node(*(*susp_it));
      const_cast<Gcs_xcom_node_information *>(xcom_node)
          ->set_suspicion_creation_timestamp(current_ts);
      const_cast<Gcs_xcom_node_information *>(xcom_node)->set_member(true);
      m_suspicions.add_node(*xcom_node);
      member_suspicions_added = true;
    } else {
      // Otherwise, ignore node
      MYSQL_GCS_LOG_TRACE(
          "add_suspicions: Not adding member expel suspicion for %s. "
          "Already "
          "a "
          "suspect!",
          (*susp_it)->get_member_id().c_str())
    }
  }

  return member_suspicions_added;
}

void Gcs_suspicions_manager::process_suspicions() {
  int wait_ret = 0;
  struct timespec ts;

  m_suspicions_mutex.lock();
  My_xp_util::set_timespec(&ts, get_suspicions_processing_period());

  const struct timespec *new_ts = &ts;

  wait_ret = m_suspicions_cond.timed_wait(m_suspicions_mutex.get_native_mutex(),
                                          new_ts);

  if (wait_ret == EINVAL) {
    MYSQL_GCS_LOG_ERROR(
        "process_suspicions: The sleeping period for suspicions manager "
        "thread "
        "is invalid!");
  } else if (wait_ret != ETIMEDOUT) {
    MYSQL_GCS_LOG_TRACE(
        "process_suspicions: Suspicions manager thread was awaken to "
        "process "
        "new suspicions!");
  }

  run_process_suspicions(false);

  m_suspicions_mutex.unlock();
}

void Gcs_suspicions_manager::run_process_suspicions(bool lock) {
  if (lock) m_suspicions_mutex.lock();

  if (m_suspicions.empty()) {
    if (lock) m_suspicions_mutex.unlock();
    return;
  }

  // List of nodes to remove
  Gcs_xcom_nodes nodes_to_remove;

  bool force_remove = false;

  uint64_t node_timeout;

  // Get current timestamp
  uint64_t current_ts = My_xp_util::getsystime();

  // Get suspicions timeouts
  uint64_t non_member_expel_timeout = get_non_member_expel_timeout();
  uint64_t member_expel_timeout = get_member_expel_timeout();

  // Cycle through the suspicions
  std::vector<Gcs_xcom_node_information>::iterator susp_it;
  std::vector<Gcs_xcom_node_information> nodes = m_suspicions.get_nodes();

  for (susp_it = nodes.begin(); susp_it != nodes.end(); ++susp_it) {
    node_timeout = (*susp_it).is_member() ? member_expel_timeout
                                          : non_member_expel_timeout;

    /*
      For each timed out suspicion, add node to expel list and remove
      suspicion object from list.
    */
    if ((*susp_it).has_timed_out(current_ts, node_timeout)) {
      /* purecov: begin tested */
      MYSQL_GCS_LOG_TRACE("process_suspicions: Suspect %s has timed out!",
                          (*susp_it).get_member_id().get_member_id().c_str())

      /*
        Check if this suspicion targets the current node and enable
        force_remove so it can remove itself.
      */
      if ((*susp_it).get_member_id().get_member_id() ==
          m_my_info->get_member_id().get_member_id()) {
        force_remove = true;
      }

      nodes_to_remove.add_node(*susp_it);
      m_suspicions.remove_node(*susp_it);
      /* purecov: end */
    } else {
      MYSQL_GCS_LOG_TRACE("process_suspicions: Suspect %s hasn't timed out.",
                          susp_it->get_member_id().get_member_id().c_str())
    }
  }

  if (((m_is_killer_node && m_has_majority) || force_remove) &&
      (nodes_to_remove.get_size() > 0)) {
    MYSQL_GCS_LOG_TRACE(
        "process_suspicions: Expelling suspects that timed out!");
    int remove_ret = m_proxy->xcom_remove_nodes(nodes_to_remove, m_gid_hash);
    if (force_remove && (remove_ret == 1)) {
      // Failed to remove myself from the group so will install leave view
      m_control_if->install_leave_view(Gcs_view::MEMBER_EXPELLED);
    }
  }

  if (lock) m_suspicions_mutex.unlock();
}

unsigned int Gcs_suspicions_manager::get_suspicions_processing_period() {
  unsigned int ret;
  m_suspicions_parameters_mutex.lock();
  ret = m_suspicions_processing_period;
  m_suspicions_parameters_mutex.unlock();
  return ret;
}

void Gcs_suspicions_manager::set_suspicions_processing_period(
    unsigned int sec) {
  m_suspicions_parameters_mutex.lock();
  m_suspicions_processing_period = sec;
  MYSQL_GCS_LOG_DEBUG("Set suspicions processing period to %u seconds.", sec)
  m_suspicions_parameters_mutex.unlock();
}

/* purecov: begin deadcode */
uint64_t Gcs_suspicions_manager::get_non_member_expel_timeout() {
  uint64_t ret;
  m_suspicions_parameters_mutex.lock();
  ret = m_non_member_expel_timeout;
  m_suspicions_parameters_mutex.unlock();
  return ret;
}
/* purecov: end */

void Gcs_suspicions_manager::set_non_member_expel_timeout_seconds(
    unsigned long sec) {
  m_suspicions_parameters_mutex.lock();
  m_non_member_expel_timeout = sec * 10000000ul;
  MYSQL_GCS_LOG_DEBUG("Set non-member expel timeout to %lu seconds (%lu  ns).",
                      sec, m_non_member_expel_timeout * 100);
  m_suspicions_parameters_mutex.unlock();
}

uint64_t Gcs_suspicions_manager::get_member_expel_timeout() {
  uint64_t ret;
  m_suspicions_parameters_mutex.lock();
  ret = m_member_expel_timeout;
  m_suspicions_parameters_mutex.unlock();
  return ret;
}

void Gcs_suspicions_manager::set_member_expel_timeout_seconds(
    unsigned long sec) {
  m_suspicions_parameters_mutex.lock();
  m_member_expel_timeout = sec * 10000000ul;
  MYSQL_GCS_LOG_DEBUG("Set member expel timeout to %lu seconds (%lu  ns).", sec,
                      m_member_expel_timeout * 100);
  m_suspicions_parameters_mutex.unlock();
}

void Gcs_suspicions_manager::set_groupid_hash(unsigned int gid_h) {
  m_gid_hash = gid_h;
}

const Gcs_xcom_nodes &Gcs_suspicions_manager::get_suspicions() const {
  return m_suspicions;
}

void Gcs_suspicions_manager::wake_suspicions_processing_thread(bool terminate) {
  m_suspicions_mutex.lock();

  MYSQL_GCS_LOG_DEBUG("wake_suspicions_processing_thread: Locked mutex!");

  // Set if suspicions processing thread should terminate
  set_terminate_suspicion_thread(terminate);

  // Wake up suspicions processing thread
  int ret = m_suspicions_cond.signal();
  MYSQL_GCS_LOG_DEBUG(
      "wake_suspicions_processing_thread: Signaled cond! Return= %d", ret);

  m_suspicions_mutex.unlock();
  MYSQL_GCS_LOG_DEBUG("wake_suspicions_processing_thread: Unlocked mutex!");
}

void Gcs_suspicions_manager::set_my_info(Gcs_xcom_node_information *node_info) {
  m_my_info = node_info;
}

void Gcs_suspicions_manager::inform_on_majority(bool majority) {
  m_suspicions_mutex.lock();
  m_has_majority = majority;
  m_suspicions_mutex.unlock();
}

bool Gcs_suspicions_manager::has_majority() {
  bool ret;
  m_suspicions_mutex.lock();
  ret = m_has_majority;
  m_suspicions_mutex.unlock();
  return ret;
}
