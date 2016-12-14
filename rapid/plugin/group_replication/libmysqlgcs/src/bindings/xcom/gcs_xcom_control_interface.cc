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

#include "gcs_logging.h"

#include "gcs_xcom_control_interface.h"

#include "gcs_xcom_view_identifier.h"
#include "gcs_xcom_group_member_information.h"
#include "gcs_xcom_communication_interface.h"
#include "gcs_xcom_notification.h"

#include "gcs_xcom_utils.h"

using std::map;
using std::set;
using std::string;

#include <iostream>
#include <list>

#include "node_no.h"

/*
  This is used to disable the arbitrator hack in XCOM just
  in case it is not disabled by default.
*/
extern int ARBITRATOR_HACK;

static void *expel_member_from_group_thread(void *ptr)
{
  MYSQL_GCS_LOG_DEBUG("Expelling members from group thread.")
  assert(ptr != NULL);

  nodes_to_kill *ntk= (nodes_to_kill *) ptr;
  unsigned int len= 0;
  char **addrs= NULL;

  len= static_cast<unsigned int>(ntk->nodes->size());
  addrs= static_cast<char **>(malloc(len * sizeof(char *)));
  node_list nl;

  std::vector<Gcs_member_identifier *>::const_iterator nodes_it= ntk->nodes->begin();
  std::vector<Gcs_member_identifier *>::const_iterator nodes_end= ntk->nodes->end();
  for (int i= 0; nodes_it != nodes_end; i++, ++nodes_it)
  {
    addrs[i]= const_cast<char *>((*nodes_it)->get_member_id().c_str());
    MYSQL_GCS_LOG_TRACE(
      "expel_member_from_group_thread():: "
      << "Node[" << i << "]=" << addrs[i]
    );
  }

  nl.node_list_len= len;
  nl.node_list_val= ntk->proxy->new_node_address(len, addrs);
  free(addrs);

  MYSQL_GCS_LOG_TRACE(
    "::expel_member_from_group_thread():: "
    << "Thread killing suspect nodes..." << ntk->nodes->size()
  )

  nodes_it= ntk->nodes->begin();
  for (; nodes_it != nodes_end; ++nodes_it)
    delete (*nodes_it);

  ntk->proxy->xcom_client_remove_node(&nl, ntk->group_id_hash);
  MYSQL_GCS_LOG_TRACE(
    "::expel_member_from_group_thread():: "
    << "Thread killed suspect nodes."
  )

  delete(ntk->nodes);
  ntk->proxy->delete_node_address(nl.node_list_len, nl.node_list_val);
  free(ptr);

  My_xp_thread_util::exit(0);
/* purecov: begin deadcode */
  return NULL;
/* purecov: end */
}


static void *xcom_taskmain_startup(void *ptr)
{
  Gcs_xcom_control *gcs_ctrl= (Gcs_xcom_control *) ptr;
  Gcs_xcom_proxy *proxy= gcs_ctrl->get_xcom_proxy();
  xcom_port port= gcs_ctrl->get_local_member_info()->get_member_port();

  proxy->xcom_init(port);

  My_xp_thread_util::exit(0);
/* purecov: begin deadcode */
  return NULL;
/* purecov: end */
}

/* purecov: begin deadcode */
map<int, const Gcs_control_event_listener &> *
Gcs_xcom_control::get_event_listeners()
{
  return &event_listeners;
}
/* purecov: end */


Gcs_xcom_control::
Gcs_xcom_control(Gcs_xcom_group_member_information *group_member_information,
                 std::vector<Gcs_xcom_group_member_information *> &xcom_peers,
                 Gcs_group_identifier m_group_identifier,
                 Gcs_xcom_proxy *xcom_proxy,
                 Gcs_xcom_engine *gcs_engine,
                 Gcs_xcom_state_exchange_interface *state_exchange,
                 Gcs_xcom_view_change_control_interface *view_control,
                 bool boot,
                 My_xp_socket_util *socket_util)
  :m_gid(NULL),
  m_gid_hash(0),
  m_xcom_proxy(xcom_proxy),
  event_listeners(),
  m_local_member_id(NULL),
  m_local_member_id_hash(0),
  m_state_exchange(state_exchange),
  m_local_node_info(NULL),
  m_xcom_thread(),
  m_hash(0),
  m_node_list_me(),
  m_uuid(0),
  m_socket_util(socket_util),
  m_xcom_running(false),
  m_boot(boot),
  m_initial_peers(),
  m_view_control(view_control),
  m_gcs_engine(gcs_engine)
{
  set_local_node_info(group_member_information);

  m_node_list_me.node_list_len= 0;
  m_gid= new Gcs_group_identifier(m_group_identifier.get_group_id());
  m_gid_hash= Gcs_xcom_utils::
                    mhash((unsigned char *) m_gid->get_group_id().c_str(),
                          m_gid->get_group_id().size());
  /*
    Clone the members - we may want to pass parameters directly to the
    control interface through the join operation on a later refactoring
    thence the set of members configured initially and then later may
    differ.
  */
  set_peer_nodes(xcom_peers);

  ARBITRATOR_HACK= 0;
}


Gcs_xcom_control::~Gcs_xcom_control()
{
  delete m_gid;
  delete m_local_member_id;

  if (m_node_list_me.node_list_len)
    delete_node_address(m_node_list_me.node_list_len,
                        m_node_list_me.node_list_val);

  clear_peer_nodes();
}


Gcs_xcom_group_member_information *Gcs_xcom_control::get_local_member_info()
{
  return m_local_node_info;
}


Gcs_xcom_proxy *Gcs_xcom_control::get_xcom_proxy()
{
  return m_xcom_proxy;
}

/* purecov: begin deadcode */
void Gcs_xcom_control::set_boot_node(bool boot)
{
  m_boot= boot;
}


My_xp_socket_util* Gcs_xcom_control::get_socket_util()
{
  return m_socket_util;
}
/* purecov: end */

void Gcs_xcom_control::wait_for_xcom_thread()
{
  m_xcom_thread.join(NULL);
}


bool Gcs_xcom_control::is_xcom_running()
{
  return m_xcom_running;
}


/**
  Initializes address list.
*/
void Gcs_xcom_control::init_me()
{
  Gcs_xcom_group_member_information *node= m_local_node_info;
  char *addr= (char *)node->get_member_address().c_str();
  blob *uuid;

  m_node_list_me.node_list_len= 1;
  m_node_list_me.node_list_val=
    m_xcom_proxy->new_node_address(m_node_list_me.node_list_len, &addr);
  uuid= &m_node_list_me.node_list_val[0].uuid;

  uuid->data.data_len= sizeof(m_local_member_id_hash);
  uuid->data.data_val= (char *) calloc(1, uuid->data.data_len);
  memcpy(uuid->data.data_val, &m_local_member_id_hash, uuid->data.data_len);
 }


void do_function_join(Gcs_control_interface *control_if)
{
  static_cast<void>(static_cast<Gcs_xcom_control *>(control_if)->do_join());
}


enum_gcs_error Gcs_xcom_control::join()
{
  MYSQL_GCS_LOG_TRACE("Joining a group.")

  /*
    It is not possilbe to call join or leave if the node is already
    trying to join or leave the group. The start_join() method
    verifies it and updates a flag to indicate that the join is
    taking place.
  */
  if(!m_view_control->start_join())
  {
    MYSQL_GCS_LOG_ERROR(
      "The member is already leaving or joining a group."
    )
    return GCS_NOK;
  }

  /*
    This is an optimistic attempt to avoid trying to join a group when the
    node already belongs to one.

    Note that although MySQL GCS internal interfaces are designed to allow
    a node to join multiple groups, the current join() and leave() methods
    don't take this into account.
  */
  if (belongs_to_group())
  {
    MYSQL_GCS_LOG_ERROR(
      "The member is trying to join a group when it is already a member."
    )
    m_view_control->end_join();
    return GCS_NOK;
  }

  if (!m_boot && m_initial_peers.empty())
  {
    MYSQL_GCS_LOG_ERROR("Unable to join the group: peers not configured. ")
    m_view_control->end_join();
    return GCS_NOK;
  }

  Gcs_xcom_notification *notification=
    new Control_notification(do_function_join, this);
  bool scheduled= m_gcs_engine->push(notification);
  if (!scheduled)
  {
    MYSQL_GCS_LOG_DEBUG(
      "Tried to enqueue a join request but the member is about to stop."
    )
    delete notification;
  }

  return scheduled ? GCS_OK: GCS_NOK;
}


enum_gcs_error Gcs_xcom_control::do_join()
{
  /* Used to initialize xcom */
  int local_port= m_local_node_info->get_member_port();
  connection_descriptor* con= NULL;
  int comm_status= XCOM_COMM_STATUS_UNDEFINED;
  enum_gcs_error is_xcom_ready= GCS_NOK;
  bool xcom_handlers_open= false;

  if (m_xcom_running)
  {
    MYSQL_GCS_LOG_ERROR(
      "Previous join was already requested and eventually "
      "a view will be delivered."
    )
    m_view_control->end_join();
    return GCS_NOK;
  }

  /*
    Clean up notification flags that are used to check whether XCOM
    is running or not.
  */
  m_xcom_proxy->xcom_set_cleanup();

  /* Spawn XCom's main loop thread. */
  if (local_port != 0)
  {
    m_xcom_thread.create(NULL, xcom_taskmain_startup, (void *) this);
  }
  else
  {
    MYSQL_GCS_LOG_ERROR("Error initializing the group communication engine.")
    goto err;
  }

  //Wait for XCom comms to become ready
  m_xcom_proxy->xcom_wait_for_xcom_comms_status_change(comm_status);

  if(comm_status == XCOM_COMMS_ERROR)
  {
    MYSQL_GCS_LOG_ERROR("Error joining the group while waiting for" <<
                        " the network layer to become ready.")
    goto err;
  }

  // Initialize the node_list
  init_me();

  /*
    Connect to the local xcom instance.
    This is needed to push data to consensus.
  */
  if (m_xcom_proxy->xcom_open_handlers(m_local_node_info->get_member_ip(),
                                       m_local_node_info->get_member_port()))
  {
    MYSQL_GCS_LOG_ERROR("Error connecting to the local group communication" <<
                        " engine instance.")
    goto err;
  }
  xcom_handlers_open= true;

  if (m_boot)
  {
    MYSQL_GCS_LOG_TRACE(
      "::join():: I am the boot node. "
      << "Calling xcom_client_boot"
    )

    int error= 0;
    if ((error= m_xcom_proxy->xcom_client_boot(&m_node_list_me, m_gid_hash))
        <= 0)
    {
      MYSQL_GCS_LOG_ERROR(
        "Error booting the group communication engine."
        << " Error=" << error
      )
      goto err;
    }
  }
  else
  {
    assert(!m_initial_peers.empty());
    MYSQL_GCS_LOG_TRACE(
      "::join():: I am NOT the boot node."
    )

    int n= 0;
    xcom_port port=  0;
    char *addr= NULL;
    std::vector<Gcs_xcom_group_member_information *>::iterator it;
    std::string *local_node_info_str=
      m_local_node_info->get_member_representation();

    while(con == NULL && n < Gcs_xcom_proxy::connection_attempts)
    {
      for(it= m_initial_peers.begin();
          con == NULL && it != m_initial_peers.end();
          it++)
      {
        Gcs_xcom_group_member_information *peer= *(it);
        std::string *peer_rep= peer->get_member_representation();

        if(peer_rep->compare(*local_node_info_str) == 0)
        {
          MYSQL_GCS_LOG_TRACE(
            "::join():: Skipping own address."
          )
          // Skip own address if configured in the peer list
          delete peer_rep;
          continue;
        }
        delete peer_rep;

        port= peer->get_member_port();
        addr= (char *)peer->get_member_ip().c_str();

        MYSQL_GCS_LOG_TRACE(
          "Client local port: "  << local_port
          << " xcom_client_open_connection to " << addr << ":" << port
        )

        if((con= m_xcom_proxy->xcom_client_open_connection(addr, port)) == NULL)
        {
          MYSQL_GCS_LOG_ERROR(
            "Error on opening a connection to " << addr <<":"<< port <<
            " on local port: " << local_port
            << ". Error= " << con
          )
        }
      }
      n++;
    }

    // Not needed anymore since it was only used in the loop
    // above. As such, claim back memory.
    delete local_node_info_str;

    if (con != NULL)
    {
      if(m_socket_util->disable_nagle_in_socket(con->fd) < 0)
      {
        m_xcom_proxy->xcom_client_close_connection(con);
        goto err;
      }
      MYSQL_GCS_LOG_TRACE(
        "::join():: Calling xcom_client_add_node " <<
        local_port << " connected to " << addr << ":" << port <<
        " to join"
      )
      m_xcom_proxy->xcom_client_add_node(con, &m_node_list_me, m_gid_hash);
      m_xcom_proxy->xcom_client_close_connection(con);
    }
    else
    {
      MYSQL_GCS_LOG_ERROR(
        "Error connecting to all peers. Member join failed. Local port: "
        << local_port
      )
      goto err;
    }
  }

  /* Wait for xcom to become ready */
  is_xcom_ready= m_xcom_proxy->xcom_wait_ready();
  if(is_xcom_ready == GCS_NOK)
  {
    MYSQL_GCS_LOG_ERROR("The group communication engine is not ready" <<
                        " for the member to join. Local port: " <<
                        local_port);
    goto err;
  }

  m_xcom_running= true;

  MYSQL_GCS_LOG_DEBUG("The member has joined the group. Local port: " <<
                      local_port);

  m_view_control->end_join();

  return GCS_OK;

err:
  if (local_port != 0)
  {
    /*
      We need the handlers opened in order to send a request to kill
      XCOM.
    */
    MYSQL_GCS_LOG_DEBUG(
      "Killing the group communication engine because the member failed to" <<
      " join. Local port: " << local_port
    );
    if (m_xcom_proxy->xcom_exit(xcom_handlers_open))
    {
      MYSQL_GCS_LOG_WARN("Failed to kill the group communication engine " <<
                         "after the member failed to join. Local port: " <<
                         local_port
      );
    }
    wait_for_xcom_thread();
  }

  /* Cleanup - close handlers */
  m_xcom_proxy->xcom_close_handlers();

  /* Free memory */
  if (m_node_list_me.node_list_len != 0)
  {
    m_xcom_proxy->delete_node_address(m_node_list_me.node_list_len,
                                      m_node_list_me.node_list_val);
    m_node_list_me.node_list_len= 0;
  }

  MYSQL_GCS_LOG_ERROR("The member was unable to join the group. Local port: "
                      << local_port)

  m_xcom_running= false;

  m_view_control->end_join();

  return GCS_NOK;
}


void do_function_leave(Gcs_control_interface *control_if)
{
  static_cast<void>(static_cast<Gcs_xcom_control *>(control_if)->do_leave());
}


enum_gcs_error Gcs_xcom_control::leave()
{
  MYSQL_GCS_LOG_DEBUG("The member is leaving the group.")

  /*
    It is not possilbe to call join or leave if the node is already
    trying to join or leave the group. The start_leave() method
    verifies it and updates a flag to indicate that the leave is
    taking place.
  */
  if(!m_view_control->start_leave())
  {
    MYSQL_GCS_LOG_ERROR(
      "The member is already leaving or joining a group."
    )
    return GCS_NOK;
  }

  /*
    This is an optimistic attempt to avoid trying to leave a group when the
    node does not belong to one.

    Note that although MySQL GCS internal interfaces are designed to allow
    a node to join multiple groups, the current join() and leave() methods
    don't take this into account.
  */
  if (!belongs_to_group())
  {
    MYSQL_GCS_LOG_ERROR(
      "The member is leaving a group without being on one."
    )
    m_view_control->end_leave();
    return GCS_NOK;
  }

  Gcs_xcom_notification *notification=
    new Control_notification(do_function_leave, this);
  bool scheduled= m_gcs_engine->push(notification);
  if (!scheduled)
  {
    MYSQL_GCS_LOG_DEBUG(
      "Tried to enqueue a leave request but the member is about to stop."
    )
    delete notification;
  }

  return scheduled ? GCS_OK: GCS_NOK;
}


enum_gcs_error Gcs_xcom_control::do_leave()
{
  if (!m_xcom_running)
  {
    MYSQL_GCS_LOG_ERROR(
      "Previous join was not requested and the member does not belong "
      "to a group."
    )
    m_view_control->end_leave();
    return GCS_NOK;
  }

  MYSQL_GCS_LOG_TRACE("::leave():: Contacting local node")
  m_xcom_proxy->xcom_client_remove_node(&m_node_list_me, m_gid_hash);

  /*
    Wait until the XCOM's thread exits.
  */
  int is_xcom_exit= m_xcom_proxy->xcom_wait_exit();

  if(is_xcom_exit == GCS_NOK)
  {
    MYSQL_GCS_LOG_ERROR(
      "The member has failed to gracefully leave the group."
    )
    /*
      We have to really kill the XCOM's thread at this point because
      an attempt to make it gracefully exit apparently has failed.
    */
    if (m_xcom_proxy->xcom_exit(true))
    {
      MYSQL_GCS_LOG_WARN("Failed to kill the group communication engine "
                         "after the member has failed to leave the group."
      );
    }
  }
  wait_for_xcom_thread();

  /*
    There is no need to interact with the local xcom anymore so we
    will can close local handlers.
  */
  if (m_xcom_proxy->xcom_close_handlers())
  {
    MYSQL_GCS_LOG_ERROR(
      "Error on closing a connection to a group member while leaving "
      "the group."
    )
  }

  /*
    Clean up local structures so that it can be reused next time.
  */
  if (m_node_list_me.node_list_len != 0)
  {
    m_xcom_proxy->delete_node_address(m_node_list_me.node_list_len,
                                      m_node_list_me.node_list_val);
    m_node_list_me.node_list_len= 0;
  }

  m_xcom_running= false;

  MYSQL_GCS_LOG_DEBUG("The member left the group.")

  m_view_control->end_leave();

  /*
    There is no need to synchronize here and this method can access
    the current_view member stored in the view controller directly.
  */
  Gcs_view *current_view= m_view_control->get_unsafe_current_view();

  if(current_view == NULL)
  {
    /*
      XCOM has stopped but will not proceed with any view install. The
      current view might be NULL due to the fact that the view with
      the join still hasn't been delivered.
    */
    MYSQL_GCS_LOG_WARN("The member has left the group but the new view" <<
                       " will not be installed, probably because it has not" <<
                       " been delivered yet.")
    /*
      If the node leaves and joins within a 5 second window, it may not
      get a global view. See BUG#23718481.
    */
    My_xp_util::sleep_seconds(5);

    return GCS_OK;
  }

  /*
    Notify that the node has left the group because someone has
    requested to do so.
  */
  install_leave_view(Gcs_view::OK);

  /*
    Set that the node does not belong to a group anymore. Note there
    is a small window when the node does not belong to the group
    anymore but the view is not NULL.
  */
  m_view_control->set_belongs_to_group(false);

  /*
    Delete current view and set it to NULL.
  */
  m_view_control->set_current_view(NULL);

  /*
    If the node leaves and joins within a 5 second window, it may not
    get a global view. See BUG#23718481.
  */
  My_xp_util::sleep_seconds(5);

  return GCS_OK;
}


bool Gcs_xcom_control::belongs_to_group()
{
  return m_view_control->belongs_to_group();
}


Gcs_view *Gcs_xcom_control::get_current_view()
{
  return m_view_control->get_current_view();
}


const Gcs_member_identifier Gcs_xcom_control::get_local_member_identifier() const
{
  return *m_local_member_id;
}


int Gcs_xcom_control::
add_event_listener(const Gcs_control_event_listener &event_listener)
{
  int handler_key= 0;
  do
  {
    handler_key= rand();
  }
  while (event_listeners.count(handler_key) != 0);

  std::pair<int,const Gcs_control_event_listener &> to_insert(handler_key,
                                                              event_listener);
  event_listeners.insert(to_insert);

  return handler_key;
}


void Gcs_xcom_control::remove_event_listener(int event_listener_handle)
{
  event_listeners.erase(event_listener_handle);
}

struct Gcs_member_identifier_pointer_comparator
{
  explicit
  Gcs_member_identifier_pointer_comparator(const Gcs_member_identifier &one)
    :one(one)
  {}

  bool operator()(Gcs_member_identifier *other)
  {
    return one == *other;
  }

private:
  const Gcs_member_identifier &one;
};



void Gcs_xcom_control::
build_total_members(Gcs_xcom_nodes *xcom_nodes,
                    std::vector<Gcs_member_identifier *> &alive_members,
                    std::vector<Gcs_member_identifier *> &failed_members)
{
  const std::vector<std::string> &addresses= xcom_nodes->get_addresses();
  const std::vector<bool> &statuses= xcom_nodes->get_statuses();
  unsigned int size= xcom_nodes->get_size();

  for (unsigned int i= 0; i < size; i++)
  {
    /*
      Build the member identifier from the address reported.
    */
    string *member_id_str=
      Gcs_xcom_utils::build_xcom_member_id(addresses[i]);
    Gcs_member_identifier *member_id=
      new Gcs_member_identifier(*member_id_str);

    /*
      Check whether the node is reported as alive or faulty.
    */
    if (statuses[i])
    {
      alive_members.push_back(member_id);
    }
    else
    {
      failed_members.push_back(member_id);
    }

    delete member_id_str;
  }
}


void Gcs_xcom_control::
build_joined_members(std::vector<Gcs_member_identifier *> &joined_members,
                     std::vector<Gcs_member_identifier *> &alive_members,
                     const std::vector<Gcs_member_identifier> *current_members)
{
  std::vector<Gcs_member_identifier *>::iterator alive_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  for (alive_members_it= alive_members.begin();
       alive_members_it != alive_members.end();
       alive_members_it++)
  {
    /*
      If there is no previous view installed, there is no current set
      of members. In this case, all nodes reported as alive will be
      considered nodes that are joining.
    */
    bool joined= true;
    if (current_members != NULL)
    {
      current_members_it= std::find(current_members->begin(),
                                    current_members->end(),
                                    *(*alive_members_it));
      if (current_members_it != current_members->end())
        joined= false;
    }

    if (joined)
      joined_members.push_back(
        new Gcs_member_identifier((*alive_members_it)->get_member_id())
      );
  }
}


void
Gcs_xcom_control::
build_left_members(std::vector<Gcs_member_identifier *> &left_members,
                   std::vector<Gcs_member_identifier *> &alive_members,
                   std::vector<Gcs_member_identifier *> &failed_members,
                   const std::vector<Gcs_member_identifier> *current_members)
{
  std::vector<Gcs_member_identifier *>::iterator alive_members_it;
  std::vector<Gcs_member_identifier *>::iterator failed_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  /*
    If there isn't a set of current members, this means that a view hasn't
    been installed before and nobody can leave something that does not
    exist.
  */
  if (current_members == NULL)
    return;

  for (current_members_it= current_members->begin();
       current_members_it != current_members->end();
       current_members_it++)
  {
    alive_members_it=
      std::find_if(alive_members.begin(), alive_members.end(),
                   Gcs_member_identifier_pointer_comparator(*current_members_it));

    failed_members_it=
      std::find_if(failed_members.begin(), failed_members.end(),
                   Gcs_member_identifier_pointer_comparator(*current_members_it));

    /*
      Node in the current view is not found in the set of alive or failed
      members meaning that it has been expelled from the cluster.
    */
    if (alive_members_it == alive_members.end() &&
        failed_members_it == failed_members.end())
    {
      left_members.push_back(new Gcs_member_identifier(*current_members_it));
    }
  }
}


void
Gcs_xcom_control::
build_expel_members(std::vector<Gcs_member_identifier *> &expel_members,
                    std::vector<Gcs_member_identifier *> &failed_members,
                    const std::vector<Gcs_member_identifier> *current_members)
{
  std::vector<Gcs_member_identifier *>::iterator failed_members_it;
  std::vector<Gcs_member_identifier>::const_iterator current_members_it;

  /*
    If there isn't a set of current members, this means that a view hasn't
    been installed before and nobody will be expelled by this node.
  */
  if (current_members == NULL)
    return;

  for (current_members_it= current_members->begin();
       current_members_it != current_members->end();
       current_members_it++)
  {
    failed_members_it=
      std::find_if(failed_members.begin(), failed_members.end(),
                   Gcs_member_identifier_pointer_comparator(*current_members_it));

    /*
      If a node in the current view is in the set of failed nodes it must
      be expelled.
    */
    if (failed_members_it != failed_members.end())
    {
      expel_members.push_back(new Gcs_member_identifier(*(*failed_members_it)));
    }
  }
}


bool
Gcs_xcom_control::is_killer_node(
  std::vector<Gcs_member_identifier *> &alive_members)
{
  /*
    Note that the member elected to remove another members from the group
    if they are considered faulty is the first one in the list of alive
    members.
  */
  assert(alive_members.size() != 0 && alive_members[0] != NULL);
  bool ret= get_local_member_identifier() == *alive_members[0];
  MYSQL_GCS_LOG_DEBUG(
    "The member "
    << get_local_member_identifier().get_member_id().c_str()
    << " will be responsible for killing: " << ret
  )
  return ret;
}

bool
Gcs_xcom_control::
xcom_receive_local_view(Gcs_xcom_nodes *xcom_nodes)
{
  unsigned int i= 0;
  std::map<int, const Gcs_control_event_listener &>::const_iterator callback_it;
  std::vector<Gcs_member_identifier> members;
  std::vector<Gcs_member_identifier> unreachable;
  Gcs_view *current_view= m_view_control->get_unsafe_current_view();
  unsigned int size= xcom_nodes->get_size();
  const std::vector<std::string> &addresses= xcom_nodes->get_addresses();
  const std::vector<bool> &statuses= xcom_nodes->get_statuses();

  // ignore
  if (size <= 0)
    goto end;

  // if I am not aware of any view at all
  if (current_view != NULL)
  {
    const std::vector<Gcs_member_identifier>& cv_members=
      current_view->get_members();

    // build the sets of servers
    for (i= 0; i < size; i++)
    {
      Gcs_member_identifier gcs_id(addresses[i]);

      // filter out those that are not yet in the current view
      // delivered to the application. For example, they might
      // exist only in the state exchange phase for now, but once
      // that is done, the current view gets updated and such
      // members will be in it. In other words, there could be
      // members that are not yet visible to upper layers.
      if (std::find(cv_members.begin(),
                    cv_members.end(),
                    gcs_id) != cv_members.end())
      {
        members.push_back(gcs_id);

        if (!statuses[i])
          unreachable.push_back(gcs_id);
      }
    }

    // always notify local views
    for(callback_it= event_listeners.begin();
        callback_it != event_listeners.end();
        callback_it ++)
    {
      callback_it->second.on_suspicions(members, unreachable);
    }
  }
end:
  return false;
}


void Gcs_xcom_control::install_leave_view(Gcs_view::Gcs_view_error_code error_code)
{
  Gcs_view *current_view= m_view_control->get_unsafe_current_view();

  // Create the new view id here, based in the previous one plus 1
  Gcs_xcom_view_identifier *new_view_id=
    new Gcs_xcom_view_identifier((Gcs_xcom_view_identifier &)
      current_view->get_view_id()
  );
  new_view_id->increment_by_one();

  // Build a best-effort view...
  set<Gcs_member_identifier *> *total, *left, *joined;
  total=  new set<Gcs_member_identifier *>();
  left=   new set<Gcs_member_identifier *>();
  joined= new set<Gcs_member_identifier *>();

  // Build left... just me...
  left->insert(new Gcs_member_identifier(*m_local_member_id));

  // Build total... all but me...
  std::vector<Gcs_member_identifier>::const_iterator old_total_it;
  for (old_total_it= current_view->get_members().begin();
      old_total_it != current_view->get_members().end();
      old_total_it++)
  {
    if (*old_total_it == *m_local_member_id)
      continue;

    total->insert(new Gcs_member_identifier(*old_total_it));
  }

  MYSQL_GCS_LOG_DEBUG("Installing leave view.")

  Gcs_group_identifier gid(current_view->get_group_id().get_group_id());
  install_view(new_view_id,
               gid,
               NULL,
               total,
               left,
               joined,
               error_code);

  set<Gcs_member_identifier *>::iterator total_it;
  for (total_it= total->begin(); total_it != total->end(); total_it++)
    delete (*total_it);
  delete total;

  set<Gcs_member_identifier *>::iterator left_it;
  for (left_it= left->begin(); left_it != left->end(); left_it++)
    delete (*left_it);

  delete left;
  delete joined;
  delete new_view_id;
}


bool Gcs_xcom_control::is_considered_faulty(
    std::vector<Gcs_member_identifier *> *failed_members)
{
  bool is_faulty= false;

  std::vector<Gcs_member_identifier *>::iterator it;

  for (it= failed_members->begin(); it != failed_members->end() && !is_faulty; ++it)
  {
    is_faulty= (*(*it) == *m_local_member_id);
  }

  return is_faulty;
}


bool Gcs_xcom_control::
xcom_receive_global_view(synode_no message_id, Gcs_xcom_nodes *xcom_nodes,
                         bool same_view)
{
  bool ret= false;
  bool free_built_members= false;

  std::vector<Gcs_member_identifier *> alive_members;
  std::vector<Gcs_member_identifier *> failed_members;
  std::vector<Gcs_member_identifier *> left_members;
  std::vector<Gcs_member_identifier *> joined_members;
  std::vector<Gcs_member_identifier *> expel_members;
  std::vector<Gcs_member_identifier *>::iterator it;

  std::string group_name(m_gid->get_group_id());

  std::map<int, const Gcs_control_event_listener &>::const_iterator
    listener_it;
  std::map<int, const Gcs_control_event_listener &>::const_iterator
    listener_ends;
  std::vector<Gcs_message_data *> exchange_data;

  /*
    If there is no previous view installed, there is no current set
    of members.
  */
  Gcs_view *current_view= m_view_control->get_unsafe_current_view();
  std::vector<Gcs_member_identifier> *current_members= NULL;
  if (current_view != NULL)
    current_members= const_cast<std::vector<Gcs_member_identifier> *>
                     (&current_view->get_members());

  MYSQL_GCS_LOG_TRACE(
    "::xcom_receive_global_view():: My "
    << "node_id is " << xcom_nodes->get_node_no()
  )

  /*
    Identify which nodes are alive and which are considered faulty.

    Note that there may be new nodes that are marked as faulty because the
    connections among their peers are still beeing established.
  */
  build_total_members(xcom_nodes,
                      alive_members,
                      failed_members);
  /*
    Build the set of joined members which are all alive members that are not
    part of the current members.

    In other words, joined = (alive - current).
  */
  build_joined_members(joined_members,
                       alive_members,
                       current_members);

  /*
    Build the set of left members which has any member that is part of the
    current members but it is not in the set of alive or failed members.

    In other words, left = current - (alive \/ failed).
  */
  build_left_members(left_members,
                     alive_members,
                     failed_members,
                     current_members);

  /*
    Build the set of nodes that must be expelled by a killer node. These
    nodes are those that are part of the current members and are marked
    as faulty in the view.
    In other words, left = current /\ failed.
  */
  build_expel_members(expel_members,
                      failed_members,
                      current_members);


  /*
    Note that this code may try to kill the same node several times.
    Although this may generate additional traffic, there is no harm.
  */
  if (expel_members.size() > 0)
  {
    assert(current_view != NULL);
    MYSQL_GCS_LOG_DEBUG(
      "::xcom_receive_global_view():: Failed "
      << "members detected. My node_id is " <<
      xcom_nodes->get_node_no()
    )

    bool should_i_kill= is_killer_node(alive_members);
    if (should_i_kill)
    {
      MYSQL_GCS_LOG_INFO(
        "Removing members that have failed while processing new view."
      )

      // Using thread to remove nodes
      nodes_to_kill *args= (nodes_to_kill *) malloc(sizeof(nodes_to_kill));
      args->nodes= new std::vector<Gcs_member_identifier *>();

      std::vector<Gcs_member_identifier *>::iterator members_failed_it;
      for (members_failed_it= failed_members.begin();
           members_failed_it != failed_members.end();
           ++members_failed_it)
      {
        args->nodes->push_back(new Gcs_member_identifier(*(*members_failed_it)));
      }

      args->group_id_hash= m_gid_hash;
      args->proxy=         m_xcom_proxy;

      My_xp_thread_impl thread_to_kill;
      thread_to_kill.create(NULL, expel_member_from_group_thread, (void *) args);
      thread_to_kill.detach();
    }
  }

  MYSQL_GCS_DEBUG_EXECUTE(
    unsigned int node_no= xcom_nodes->get_node_no();
    for(it= alive_members.begin(); it != alive_members.end(); it++)
      MYSQL_GCS_LOG_TRACE(
        "(My node_id is " << node_no << ") Node " <<
        "considered alive in the cluster: " << (*it)->get_member_id()
      );

    for(it= failed_members.begin(); it != failed_members.end(); it++)
      MYSQL_GCS_LOG_TRACE(
        "(My node_id is " << node_no << ") Node " <<
        "considered faulty in the cluster: " << (*it)->get_member_id()
      );

    for(it= left_members.begin(); it != left_members.end(); it++)
      MYSQL_GCS_LOG_TRACE(
        "(My node_id is " << node_no << ") Node " <<
        "leaving the cluster: " << (*it)->get_member_id()
      );

    for(it= joined_members.begin(); it != joined_members.end(); it++)
      MYSQL_GCS_LOG_TRACE(
        "(My node_id is " << node_no << ") Node " <<
        "joining the cluster: " << (*it)->get_member_id()
      );

    for(it= expel_members.begin(); it != expel_members.end(); it++)
      MYSQL_GCS_LOG_TRACE(
        "(My node_id is " << node_no << ") Node " <<
        "being expelled from the cluster: " << (*it)->get_member_id()
      );
  )

  /*
    If nobody has joined or left the group, there is no need to install any view.
    This fact is denoted by the same_view flag and the execution only gets
    so far because we may have to kill some faulty members. Note that the code
    could be optimized but we are focused on correctness for now and any
    possible optimization will be handled in the future.

    Views that contain a node marked as faulty are also ignored. This is done
    because XCOM does not deliver messages to or accept messages from such
    nodes. So if we have considered them, the state exchange phase would block
    expecting messages that would never arrive.

    So faulty members that are part of an old view will be eventually expelled
    and a clean view, i.e. a view without faulty members, will be eventually
    delivered. Note, however, that we are not considering failures during the
    the join phase which includes the state exchange phase as well. This will
    require implementing some sort of join timeout.

    If the execution has managed to pass these steps, the node is alive and
    it is time to start building the view.
  */
  if (same_view || failed_members.size() != 0)
  {
    MYSQL_GCS_LOG_TRACE(
      "(My node_id is " <<
      xcom_nodes->get_node_no() << ") ::xcom_receive_global_view():: "
      "Discarding view because nothing has changed."
      " Same view flag is " << same_view << ","
      ", number of failed nodes is " << failed_members.size() <<
      ", number of joined nodes is " << joined_members.size() <<
      ", number of left nodes is " << left_members.size()
    )

    if (current_view != NULL && is_considered_faulty(&failed_members))
    {
      install_leave_view(Gcs_view::MEMBER_EXPELLED);
    }

    ret= true;
    free_built_members= true;
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
  if(m_view_control->is_view_changing())
  {
    MYSQL_GCS_LOG_TRACE(
      "View exchange is ongoing. "
      << " Resetting state exchange. My node_id is "
      << xcom_nodes->get_node_no()
    )
    MYSQL_GCS_DEBUG_EXECUTE(
      for(it= left_members.begin(); it != left_members.end(); it++)
        MYSQL_GCS_LOG_TRACE(
          "(My node_id is " << xcom_nodes->get_node_no() << ") Node " <<
          "leaving the cluster: " << (*it)->get_member_id()
        );

      for(it= joined_members.begin(); it != joined_members.end(); it++)
        MYSQL_GCS_LOG_TRACE(
          "(My node_id is " << xcom_nodes->get_node_no() << ") Node " <<
          "joining the cluster: " << (*it)->get_member_id()
        );
    )
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
  listener_ends= event_listeners.end();
  for (listener_it= event_listeners.begin(); listener_it != listener_ends;
       ++listener_it)
  {
    Gcs_message_data *msg_data= (*listener_it).second.get_exchangeable_data();
    exchange_data.push_back(msg_data);
  }
  m_state_exchange->state_exchange(message_id,
                                   alive_members,
                                   left_members,
                                   joined_members,
                                   exchange_data,
                                   current_view,
                                   &group_name,
                                   m_local_member_id);

  MYSQL_GCS_LOG_TRACE("::xcom_receive_global_view():: state exchange started.")

end:
  if (free_built_members)
  {
    // clean up tentative sets

    for(it= left_members.begin(); it != left_members.end(); it++)
      delete *it;
    left_members.clear();

    for(it= joined_members.begin(); it != joined_members.end(); it++)
      delete *it;
    joined_members.clear();

    for(it= alive_members.begin(); it != alive_members.end(); it++)
      delete *it;
    alive_members.clear();

    for(it= failed_members.begin(); it != failed_members.end(); it++)
      delete *it;
    failed_members.clear();

    for(it= expel_members.begin(); it != expel_members.end(); it++)
      delete *it;
    expel_members.clear();
  }

  return ret;
}


void Gcs_xcom_control::process_control_message(Gcs_message *msg)
{
  MYSQL_GCS_LOG_TRACE(
    "::process_control_message():: Received a control message"
  )

  Xcom_member_state *ms_info=
    new Xcom_member_state(msg->get_message_data().get_payload(),
                          msg->get_message_data().get_payload_length());

  MYSQL_GCS_LOG_TRACE(
    "Reading message that carries exchangeable data: (payload)=" <<
    msg->get_message_data().get_payload_length()
  );

  MYSQL_GCS_LOG_TRACE(
    "::process_control_message():: From: "
    << msg->get_origin().get_member_id().c_str() << " regarding view_id:"
    << ms_info->get_view_id()->get_representation().c_str() << " in " <<
    get_local_member_info()->get_member_address()
  )

  /*
    XCOM does not preserver FIFO and for that reason a message from
    a previous state exchange phase may arrive when the newest phase
    has already finished.
  */
  MYSQL_GCS_DEBUG_EXECUTE(
    synode_no configuration_id= ms_info->get_configuration_id();
  )
  if (!m_view_control->is_view_changing())
  {
    MYSQL_GCS_LOG_DEBUG(
      "There is no state exchange going on. Ignoring exchangeable data "
      "because its from a previous state exchange phase. Message is "
      "from group_id(" << configuration_id.group_id << "), msg_no( "
      << configuration_id.msgno << "), node_no("
      << configuration_id.node << ")."
    )
    delete msg;
    return;
  }
  MYSQL_GCS_LOG_DEBUG(
    "There is a state exchange going on. Message is "
    "from group_id(" << configuration_id.group_id << "), msg_no( "
    << configuration_id.msgno << "), node_no("
    << configuration_id.node << ")."
  )

  bool can_install_view=
    m_state_exchange->process_member_state(ms_info, msg->get_origin());

  // If state exchange has finished
  if (can_install_view)
  {
    MYSQL_GCS_LOG_TRACE(
      "::process_control_message()::Install new view"
    )

    // Make a copy of the state exchange provided view id
    Gcs_xcom_view_identifier *provided_view_id=
      m_state_exchange->get_new_view_id();

    Gcs_xcom_view_identifier *new_view_id=
      new Gcs_xcom_view_identifier(*provided_view_id);

    new_view_id->increment_by_one();

    install_view(new_view_id,
                 *m_gid,
                 m_state_exchange->get_member_states(),
                 m_state_exchange->get_total(),
                 m_state_exchange->get_left(),
                 m_state_exchange->get_joined());

    delete new_view_id;
  }
  else
  {
    MYSQL_GCS_LOG_TRACE(
      "::process_control_message()::"
      << "Still waiting for more State Exchange messages: " <<
      m_local_member_id->get_member_id().c_str()
    )
  }

  delete msg;
}


void
Gcs_xcom_control::
install_view(Gcs_xcom_view_identifier *new_view_id,
             const Gcs_group_identifier &group,
             std::map<Gcs_member_identifier, Xcom_member_state *> *states,
             set<Gcs_member_identifier *> *total,
             set<Gcs_member_identifier *> *left,
             set<Gcs_member_identifier *> *join,
             Gcs_view::Gcs_view_error_code error_code)

{
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
  Gcs_view *current_view=
    new Gcs_view(members, v_id, left_members, joined_members, group,
                 error_code);

  // Build the exchanged data
  Exchanged_data data_to_deliver;
  if (states != NULL)
  {
    std::map<Gcs_member_identifier, Xcom_member_state *>::iterator states_it;
    for (states_it= states->begin();
         states_it != states->end();
         states_it++)
    {
      MYSQL_GCS_LOG_DEBUG(
        "Processing exchanged data while installing the new view"
      )

      Gcs_member_identifier *member_id=
        new Gcs_member_identifier((*states_it).first);

      Xcom_member_state *data_exchanged= (*states_it).second;

      Gcs_message_data *data_exchanged_holder=
        new Gcs_message_data(data_exchanged->get_data_size());

      if (data_exchanged != NULL)
      {
        data_exchanged_holder->decode(
          data_exchanged->get_data(), data_exchanged->get_data_size()
        );
      }

      std::pair<Gcs_member_identifier *, Gcs_message_data *>
        state_pair(member_id, data_exchanged_holder);

      data_to_deliver.push_back(state_pair);
    }
  }
  else
  {
    MYSQL_GCS_LOG_TRACE(
      "::install_view():: No exchanged data"
    )
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

  map<int, const Gcs_control_event_listener &>::iterator callback_it=
    event_listeners.begin();

  while (callback_it != event_listeners.end())
  {
    (*callback_it).second.on_view_changed(*current_view, data_to_deliver);

    MYSQL_GCS_LOG_TRACE(
      "::install_view():: View delivered to "
      << "client handler= "<< (*callback_it).first
    )

    ++callback_it;
  }

  Exchanged_data::const_iterator it;
  for (it= data_to_deliver.begin(); it != data_to_deliver.end(); it++)
  {
    delete (*it).first;
    delete (*it).second;
  }

  m_view_control->end_view_exchange();

  m_state_exchange->end();
}


void Gcs_xcom_control::
build_member_list(set<Gcs_member_identifier *> *origin,
                  std::vector<Gcs_member_identifier> *to_fill)
{
  std::set<Gcs_member_identifier *>::iterator it;

  for (it= origin->begin(); it != origin->end(); it++)
  {
    Gcs_member_identifier member_id(*(*it));

    to_fill->push_back(member_id);
  }
}


void Gcs_xcom_control::set_local_node_info(Gcs_xcom_group_member_information *group_member_information)
{
  m_local_node_info= group_member_information;

  string address= group_member_information->get_member_address();

  string *member_id= Gcs_xcom_utils::build_xcom_member_id(address);
  delete m_local_member_id;
  m_local_member_id= new Gcs_member_identifier(*member_id);
  m_local_member_id_hash=
    Gcs_xcom_utils::mhash((unsigned char *) m_local_member_id->get_member_id().c_str(),
                          m_local_member_id->get_member_id().size());
  delete member_id;
}

void Gcs_xcom_control::set_peer_nodes(std::vector<Gcs_xcom_group_member_information *> &xcom_peers)
{
  clear_peer_nodes();

  std::vector<Gcs_xcom_group_member_information *>::iterator it;
  for (it= xcom_peers.begin(); it != xcom_peers.end(); ++it)
  {
    m_initial_peers.push_back(
      new Gcs_xcom_group_member_information((*it)->get_member_address()));
  }
}

void Gcs_xcom_control::clear_peer_nodes()
{
  if (!m_initial_peers.empty())
  {
    std::vector<Gcs_xcom_group_member_information *>::iterator it;
    for (it= m_initial_peers.begin(); it != m_initial_peers.end(); ++it)
      delete (*it);

    m_initial_peers.clear();
  }
}
