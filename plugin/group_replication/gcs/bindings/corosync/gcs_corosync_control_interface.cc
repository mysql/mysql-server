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

#include "gcs_corosync_control_interface.h"
#include "gcs_state_exchange.h"

cs_error_t
Gcs_corosync_control_proxy_impl::cpg_join (cpg_handle_t handle,
                                           const struct cpg_name *group)
{
  return ::cpg_join(handle, group);
}

cs_error_t
Gcs_corosync_control_proxy_impl::cpg_leave (cpg_handle_t handle,
                                            const struct cpg_name *group)
{
  return ::cpg_leave(handle, group);
}

cs_error_t
Gcs_corosync_control_proxy_impl::cpg_local_get (cpg_handle_t handle,
                                                unsigned int *local_nodeid)
{
  return ::cpg_local_get(handle, local_nodeid);
}

map<int, Gcs_control_event_listener*>*
Gcs_corosync_control::get_event_listeners()
{
  return &event_listeners;
}

map<int, Gcs_control_data_exchange_event_listener*>*
Gcs_corosync_control::get_data_exchange_listeners()
{
  return &data_exchange_listeners;
}

Gcs_corosync_control::
Gcs_corosync_control(cpg_handle_t handle,
                     Gcs_group_identifier group_identifier,
                     Gcs_corosync_control_proxy* corosync_proxy,
                     Gcs_corosync_state_exchange_interface* se,
                     Gcs_corosync_view_change_control_interface* vce)
      :corosync_handle(handle),
       proxy(corosync_proxy),
       current_view(NULL),
       local_member_information(NULL),
       exchange_data(NULL),
       state_exchange(se),
       view_notif(vce)
{
  this->group_identifier= new Gcs_group_identifier
                                            (group_identifier.get_group_id());
}

Gcs_corosync_control::~Gcs_corosync_control()
{
  delete group_identifier;
  delete local_member_information;
  delete current_view;
}

bool
Gcs_corosync_control::join()
{
  cpg_name name;

  name.length= group_identifier->get_group_id().length();
  strncpy(name.value, group_identifier->get_group_id().c_str(),
          CPG_MAX_NAME_LENGTH);

  int res= 0;
  GCS_COROSYNC_RETRIES(proxy->cpg_join(corosync_handle, &name), res);

  return res != CS_OK;
}

bool
Gcs_corosync_control::leave()
{
  cpg_name name;
  bool rc;

  name.length= group_identifier->get_group_id().length();
  strncpy(name.value, group_identifier->get_group_id().c_str(),
          CPG_MAX_NAME_LENGTH);

  rc= ( proxy->cpg_leave(corosync_handle, &name) != CS_OK );

  return rc;
}

bool
Gcs_corosync_control::belongs_to_group()
{
  return this->joined;
}

Gcs_view*
Gcs_corosync_control::get_current_view()
{
  //If a view chang is occuring, this get will wait for it to finish
  view_notif->wait_for_view_change_end();

  return this->current_view;
}

Gcs_member_identifier*
Gcs_corosync_control::get_local_information()
{
  if(this->local_member_information == NULL)
  {
    uint32 local_nodeid= 0;
    int id_extraction_iter= 0;

    ostringstream builder;
    builder.clear();
    do
    {
     /*
      Due to a corosync bug, the local_nodeid is not always available, being
      however usually available at a second execution.
      */
      proxy->cpg_local_get(this->corosync_handle, &local_nodeid);
      if (local_nodeid != 0)
      {
        break;
      }
      id_extraction_iter++;
      usleep(100);
    }
    while (id_extraction_iter < MAX_NUMBER_OF_ID_EXTRACTION_TENTATIVES);

    if(id_extraction_iter < MAX_NUMBER_OF_ID_EXTRACTION_TENTATIVES)
    {
      local_member_information=
              Gcs_corosync_utils::
                               build_corosync_member_id(local_nodeid, getpid());
    }
  }

  return local_member_information;
}

int
Gcs_corosync_control::add_event_listener(Gcs_control_event_listener* event_listener)
{
  int handler_key= 0;
  do {
    handler_key= rand();
  } while(event_listeners.count(handler_key) != 0);

  event_listeners[handler_key]= event_listener;

  return handler_key;
}

void
Gcs_corosync_control::remove_event_listener(int event_listener_handle)
{
  event_listeners.erase(event_listener_handle);
}

void
Gcs_corosync_control::set_exchangeable_data(vector<uchar> *data)
{
  exchange_data= data;
}

int
Gcs_corosync_control::add_data_exchange_event_listener
                    (Gcs_control_data_exchange_event_listener *event_listener)
{
  int handler_key= 0;
  do {
    handler_key= rand();
  } while(data_exchange_listeners.count(handler_key) != 0);

  data_exchange_listeners[handler_key]= event_listener;

  return handler_key;
}

void
Gcs_corosync_control::remove_data_exchange_event_listener
                                                (int event_listener_handle)
{
  data_exchange_listeners.erase(event_listener_handle);
}

void
Gcs_corosync_control::view_changed
            (const struct cpg_name *name,
             const struct cpg_address *total, size_t total_entries,
             const struct cpg_address *members_left, size_t left_entries,
             const struct cpg_address *members_joined, size_t joined_entries)
{
  /*
   The most notable functionalities of this method are:
   - Inform that a view exchange has started, thus blocking all attempts
     to start a second view change or message sending
   - Inform State Exchange to start its process
   - If the node is leaving, install immediately the new view using an
     optimistic approach on the view id, stating that it will be the (last+1)
   */

  view_notif->start_view_exchange();

  string* group_name= new string(name->value);

  bool leaving= state_exchange->state_exchange(total,
                                               total_entries,
                                               members_left,
                                               left_entries,
                                               members_joined,
                                               joined_entries,
                                               group_name,
                                               exchange_data,
                                               current_view,
                                               get_local_information());

  //if the current node is leaving,
  if(leaving)
  {
    //Create the new view id here, based in the previous one plus 1
    Gcs_corosync_view_identifier* old_view_id=
                static_cast<Gcs_corosync_view_identifier*>
                                   (current_view->get_view_id());

    Gcs_corosync_view_identifier* new_view_id=
                     new Gcs_corosync_view_identifier(*old_view_id);
    new_view_id->increment_by_one();

    install_view(new_view_id,
                 group_name,
                 state_exchange->get_total(),
                 state_exchange->get_left(),
                 state_exchange->get_joined());
  }

  delete group_name;
}

bool
Gcs_corosync_control::process_possible_control_message(Gcs_message *msg)
{
  //Currently only for State Exchange messages
  bool is_state_message=
       state_exchange->is_state_exchange_message(msg);

  if(is_state_message)
  {
    //Decode the member state and hand it out to State Exchange
    Member_state* ms = new Member_state(msg->get_payload(),
                                        msg->get_payload_length());

    bool can_install_view= state_exchange
                                   ->process_member_state(ms,
                                                          *(msg->get_origin()));

    //Deliver Exchangeable data to listeners regardless of view being installed
    map<int, Gcs_control_data_exchange_event_listener*>::iterator
                                  callback_it= data_exchange_listeners.begin();

    while(callback_it != data_exchange_listeners.end())
    {
      (*callback_it).second->on_data(ms->get_data());
      ++callback_it;
    }

    //If State Exchange has finished
    if(can_install_view)
    {
      //Make a copy of the state exchange provided view id
      Gcs_corosync_view_identifier* provided_view_id=
                                             state_exchange->get_new_view_id();

      Gcs_corosync_view_identifier* new_view_id=
         new Gcs_corosync_view_identifier
                                   (*provided_view_id);
      new_view_id->increment_by_one();

      install_view(new_view_id,
                   state_exchange->get_group(),
                   state_exchange->get_total(),
                   state_exchange->get_left(),
                   state_exchange->get_joined());
    }
  }

  return is_state_message;
}

void
Gcs_corosync_control::install_view(Gcs_corosync_view_identifier* new_view_id,
                                   string *group_name,
                                   set<Gcs_member_identifier*> *total,
                                   set<Gcs_member_identifier*> *left,
                                   set<Gcs_member_identifier*> *join)

{
  //Delete current view
  if(current_view != NULL)
  {
    delete current_view;
  }

  //Build all lists of All, Left and Joined members
  vector<Gcs_member_identifier> *members= new vector<Gcs_member_identifier>();
  build_member_list(total, members);

  vector<Gcs_member_identifier> *left_members
                                         = new vector<Gcs_member_identifier>();
  build_member_list(left, left_members);

  vector<Gcs_member_identifier> *joined_members
                                         = new vector<Gcs_member_identifier>();
  build_member_list(join, joined_members);

  //Build the new view id and the group id
  Gcs_view_identifier *v_id= new_view_id;

  Gcs_group_identifier *group_id= new Gcs_group_identifier(*group_name);

  //Create the new view
  current_view= new Gcs_view(members,
                             v_id,
                             left_members,
                             joined_members,
                             group_id);

  //Calculate is_joined
  vector<Gcs_member_identifier>::iterator members_it= members->begin();
  Gcs_member_identifier *member_id= get_local_information();

  joined= false;
  while(member_id != NULL && //it will break if the local information is invalid
        !joined &&
        members_it != members->end())
  {
    joined = ( (*members_it) == *member_id);
    members_it++;
  }

  //Notify all view listeners
  map<int, Gcs_control_event_listener*>::iterator callback_it
                                                   = event_listeners.begin();
  while(callback_it != event_listeners.end())
  {
    callback_it->second->on_view_changed(current_view);
    ++callback_it;
  }

  //Notify that the view is installed
  view_notif->end_view_exchange();

  state_exchange->reset();
}

void Gcs_corosync_control::build_member_list
                            (set<Gcs_member_identifier*> *origin,
                             vector<Gcs_member_identifier> *to_fill)
{

  set<Gcs_member_identifier*>::iterator it;
  for (it= origin->begin(); it != origin->end(); it++)
  {
    Gcs_member_identifier member_id(*(*it)->get_member_id());

    to_fill->push_back(member_id);
  }
}
