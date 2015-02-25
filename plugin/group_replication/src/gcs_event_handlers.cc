/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <set>
#include <string>

#include "gcs_event_handlers.h"
#include "plugin_log.h"

Gcs_plugin_events_handler::
Gcs_plugin_events_handler(Applier_module_interface* applier_module,
                          Recovery_module* recovery_module,
                          Cluster_member_info_manager_interface* cluster_mgr,
                          Cluster_member_info* local_node_info,
                          Gcs_plugin_view_modification_notifier* vc_notifier)
{
  this->applier_module= applier_module;
  this->recovery_module= recovery_module;
  this->cluster_info_mgr= cluster_mgr;
  this->local_node_info= local_node_info;
  this->view_change_notifier= vc_notifier;

  this->temporary_states= new set<Cluster_member_info*,
                                  Gcs_member_info_pointer_comparator>();
}

Gcs_plugin_events_handler::~Gcs_plugin_events_handler()
{
  delete temporary_states;
}

void
Gcs_plugin_events_handler::on_message_received(Gcs_message& message)
{
  gcs_payload_message_code message_code=
              Gcs_plugin_message_utils::retrieve_code(message.get_payload());

  switch (message_code)
  {
  case PAYLOAD_TRANSACTION_EVENT:
    handle_transactional_message(message);
    break;

  case PAYLOAD_CERTIFICATION_EVENT:
    handle_certifier_message(message);
    break;

  case PAYLOAD_RECOVERY_EVENT:
    handle_recovery_message(message);
    break;

  default:
    DBUG_ASSERT(0);
  }
}

void
Gcs_plugin_events_handler::handle_transactional_message(Gcs_message& message)
{
  if (this->applier_module)
  {
    uchar* payload_data= Gcs_plugin_message_utils::
                               retrieve_data(&message);
    size_t payload_size= Gcs_plugin_message_utils::
                               retrieve_length(&message);

    this->applier_module->handle(payload_data, payload_size);
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Message received without a proper group replication applier");
  }
}

void
Gcs_plugin_events_handler::handle_certifier_message(Gcs_message& message)
{
  if (this->applier_module == NULL)
  {
    log_message(MY_ERROR_LEVEL,
                "Message received without a proper group replication applier");
    return;
  }

  Certifier_interface *certifier= this->applier_module
                                      ->get_certification_handler()
                                      ->get_certifier();

  uchar* payload_data= Gcs_plugin_message_utils::
                               retrieve_data(&message);
  size_t payload_size= Gcs_plugin_message_utils::
                               retrieve_length(&message);

  if (certifier->handle_certifier_data(payload_data,
                                       payload_size))
  {
    log_message(MY_ERROR_LEVEL, "Error processing message in Certifier");
  }
}

void
Gcs_plugin_events_handler::handle_recovery_message(Gcs_message& message)
{
  Recovery_message recovery_message(message.get_payload(),
                                    message.get_payload_length());

  string *node_uuid= recovery_message.get_node_uuid();

  // The node is declare as online upon receiving this message
  cluster_info_mgr->update_member_status(*node_uuid,
                                         Cluster_member_info::MEMBER_ONLINE);

  bool is_local= !node_uuid->compare(*local_node_info->get_uuid());
  if(is_local)
  {
    log_message(MY_INFORMATION_LEVEL,
                "This server was declared online within the replication group");
  }
  else
  {
    log_message(MY_INFORMATION_LEVEL,
                "Server %s was declared online within the replication group",
                node_uuid->c_str());
  }
}

void
Gcs_plugin_events_handler::on_view_changed(Gcs_view *new_view)
{
  bool is_leaving= is_member_on_vector(new_view->get_leaving_members(),
                                       *local_node_info->get_gcs_member_id());

  bool is_joining= is_member_on_vector(new_view->get_joined_members(),
                                       *local_node_info->get_gcs_member_id());

  //update the Cluster Manager with all the received states
  this->update_cluster_info_manager(new_view, is_leaving);

  //Inform any interested handler that the view changed
  View_change_pipeline_action *vc_action=
    new View_change_pipeline_action(is_leaving);

  applier_module->handle_pipeline_action(vc_action);
  delete vc_action;

  //Handle joining members
  this->handle_joining_nodes(new_view, is_joining);

  //Update any recovery running process and handle state changes
  this->handle_leaving_nodes(new_view, is_joining, is_leaving);
}

void Gcs_plugin_events_handler::update_cluster_info_manager(Gcs_view *new_view,
                                                            bool is_leaving)
{
  //update the Cluster Manager with all the received states
  vector<Cluster_member_info*> to_update;

  if(!is_leaving)
  {
    to_update.insert(to_update.end(),
                     temporary_states->begin(),
                     temporary_states->end());

    //Clean-up members that are leaving
    vector<Gcs_member_identifier>* leaving= new_view->get_leaving_members();
    vector<Gcs_member_identifier>::iterator left_it;
    vector<Cluster_member_info*>::iterator to_update_it;
    for(left_it= leaving->begin(); left_it != leaving->end(); left_it++)
    {
      for(to_update_it= to_update.begin();
          to_update_it != to_update.end();
          to_update_it++)
      {
        if( (*left_it) == *(*to_update_it)->get_gcs_member_id() )
        {
          delete (*to_update_it);

          to_update.erase(to_update_it);
          break;
        }
      }
    }
  }
  cluster_info_mgr->update(&to_update);
  temporary_states->clear();
}

void Gcs_plugin_events_handler::handle_joining_nodes(Gcs_view *new_view,
                                                     bool is_joining)
{
  //nothing to do here
  if (new_view->get_members()->size() == 0)
  {
    return;
  }
  /**
   On the joining list there can be 2 types of members: online/recovering nodes
   coming from old views where this node was not present and new joining nodes
   that still have their status as offline.

   As so, nodes that are offline, their state is changed to member_in_recovery.
  */
  update_node_status
              (new_view->get_joined_members(),
               Cluster_member_info::MEMBER_IN_RECOVERY,
               Cluster_member_info::MEMBER_OFFLINE);

  /*
   If we are joining, two scenarios exist
   1) We are alone so we declare ourselves online
   2) We are in a cluster and recovery must happen
  */
  if (is_joining)
  {
    view_change_notifier->end_view_modification();

    log_message(MY_INFORMATION_LEVEL,
                "Starting group replication recovery with view_id %s",
                new_view->get_view_id()->get_representation());
    /*
     During the view change, a suspension packet is sent to the applier module
     so all posterior transactions inbound are not applied, but queued, until
     the node finishes recovery.
    */
    applier_module->add_suspension_packet();

    /*
     Marking the view in the joiner since the incoming event from the donor
     is discarded in the Recovery process.
     */
    applier_module->add_view_change_packet(new_view->get_view_id()
                                                 ->get_representation());

    /*
     Launch the recovery thread so we can receive missing data and the
     certification information needed to apply the transactions queued after
     this view change.

     Recovery receives a view id, as a means to identify logically on joiners
     and donors alike where this view change happened in the data. With that
     info we can then ask for the donor to give the node all the data until
     this point in the data, and the certification information for all the data
     that comes next.

     When alone, the server will go through Recovery to wait for the consumption
     of his applier relay log that may contain transactions from previous
     executions.
    */
    recovery_module->start_recovery(new_view->get_group_id()->get_group_id(),
                                    new_view->get_view_id()
                                                      ->get_representation());
  }
  else
  {
    log_message(MY_INFORMATION_LEVEL,
                "Marking group replication view change with view_id %s",
                new_view->get_view_id()->get_representation());
    /**
     If not a joining member, all nodes should record on their own binlogs a
     marking event that identifies the frontier between the data the joining
     node was to receive and the data it should queue.
     The joining node can then wait for this event to know it was all the needed
     data.

     This packet will also pass in the certification process at this exact
     frontier giving us the opportunity to gather the necessary certification
     information to certify the transactions that will come after this view
     change. If selected as a donor, this info will also be sent to the joiner.
    */
    applier_module->add_view_change_packet(new_view->get_view_id()
                                                   ->get_representation());
  }
}

void
Gcs_plugin_events_handler::handle_leaving_nodes(Gcs_view* new_view,
                                                bool is_joining,
                                                bool is_leaving)
{
  Cluster_member_info* for_local_status=
        cluster_info_mgr->get_cluster_member_info(*local_node_info->get_uuid());

  Cluster_member_info::Cluster_member_status node_status=
                                        for_local_status->get_recovery_status();

  bool nodes_left= (new_view->get_leaving_members()->size() > 0);

  //if the node is joining or not in recovery, no need to update the process
  if(!is_joining && node_status == Cluster_member_info::MEMBER_IN_RECOVERY)
  {
    /*
     This method has 2 purposes:
     If a donor leaves, recovery needs to switch donor
     If this node leaves, recovery needs to shutdown.
    */
    recovery_module->update_recovery_process(nodes_left);
  }

  if (nodes_left)
  {
    update_node_status(new_view->get_leaving_members(),
                       Cluster_member_info::MEMBER_OFFLINE,
                       Cluster_member_info::MEMBER_END);
  }

  if(is_leaving)
  {
    view_change_notifier->end_view_modification();
  }

  delete for_local_status;
}

bool Gcs_plugin_events_handler::is_member_on_vector
                                      (vector<Gcs_member_identifier>* members,
                                       Gcs_member_identifier member_id)
{
  vector<Gcs_member_identifier>::iterator it;

  it= find(members->begin(), members->end(), member_id);

  return it != members->end();
}

int
Gcs_plugin_events_handler::on_data(vector<uchar>* exchanged_data)
{
  /*
  For now, we are only carrying Cluster Member Info on Exchangeable data
  Since we are receiving the state from all Cluster members, one shall
  store it in a set to ensure that we don't have repetitions.

  All collected data will be given to Cluster Member Manager at view install
  time.
  */
  vector<Cluster_member_info*>* member_infos=
                             cluster_info_mgr->decode(&exchanged_data->front());

  //This construct is here in order to deallocate memory of duplicates
  vector<Cluster_member_info*>::iterator member_infos_it;
  for(member_infos_it= member_infos->begin();
      member_infos_it != member_infos->end();
      member_infos_it++)
  {
    if(temporary_states->count((*member_infos_it)) > 0)
    {
      delete (*member_infos_it);
    }
    else
    {
      this->temporary_states->insert((*member_infos_it));
    }
  }

  member_infos->clear();
  delete member_infos;

  return 0;
}

void
Gcs_plugin_events_handler::update_node_status
                 (vector<Gcs_member_identifier>* members,
                  Cluster_member_info::Cluster_member_status status,
                  Cluster_member_info::Cluster_member_status condition_status)
{
  for (vector<Gcs_member_identifier>::iterator it= members->begin();
       it != members->end();
       ++it)
  {
    Gcs_member_identifier member = *it;
    Cluster_member_info* member_info=
                cluster_info_mgr->get_cluster_member_info_by_member_id(member);

    if(member_info == NULL)
    {
      //Trying to update a non-existing member
      continue;
    }

    if(condition_status == Cluster_member_info::MEMBER_END ||
       member_info->get_recovery_status() == condition_status)
    {
      cluster_info_mgr->update_member_status(*member_info->get_uuid(), status);
    }
  }
}

Gcs_plugin_view_modification_notifier::Gcs_plugin_view_modification_notifier()
{
  view_changing= false;

#ifdef HAVE_PSI_INTERFACE
  PSI_cond_info view_change_notifier_conds[]=
  {
    { &wait_for_view_key_cond, "COND_view_change_notifier", 0}
  };

  PSI_mutex_info view_change_notifier_mutexes[]=
  {
    { &wait_for_view_key_mutex, "LOCK_view_change_notifier", 0}
  };

  register_gcs_psi_keys(view_change_notifier_mutexes, 1,
                        view_change_notifier_conds, 1);
#endif /* HAVE_PSI_INTERFACE */

  mysql_cond_init(wait_for_view_key_cond, &wait_for_view_cond);
  mysql_mutex_init(wait_for_view_key_mutex, &wait_for_view_mutex,
                   MY_MUTEX_INIT_FAST);
}
Gcs_plugin_view_modification_notifier::~Gcs_plugin_view_modification_notifier()
{
  mysql_mutex_destroy(&wait_for_view_mutex);
  mysql_cond_destroy(&wait_for_view_cond);
}

void
Gcs_plugin_view_modification_notifier::start_view_modification()
{
  mysql_mutex_lock(&wait_for_view_mutex);
  view_changing= true;
  mysql_mutex_unlock(&wait_for_view_mutex);
}

void
Gcs_plugin_view_modification_notifier::end_view_modification()
{
  mysql_mutex_lock(&wait_for_view_mutex);
  view_changing= false;
  mysql_cond_broadcast(&wait_for_view_cond);
  mysql_mutex_unlock(&wait_for_view_mutex);
}

bool
Gcs_plugin_view_modification_notifier::wait_for_view_modification(long timeout)
{
  struct timespec ts;
  int result= 0;

  mysql_mutex_lock(&wait_for_view_mutex);
  while (view_changing)
  {
    set_timespec(&ts, timeout);
    result=
         mysql_cond_timedwait(&wait_for_view_cond, &wait_for_view_mutex, &ts);

    if(result != 0) //It means that it broke by timeout or an error.
      break;
  }
  mysql_mutex_unlock(&wait_for_view_mutex);

  return (result != 0);
}
