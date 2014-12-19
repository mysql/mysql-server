/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_state_exchange.h"
#include "gcs_corosync_communication_interface.h"

#include <time.h>

Member_state::Member_state(Gcs_corosync_view_identifier* view_id_arg,
                           vector<uchar> *exchangeable_data)
{

  this->view_id= NULL;
  if(view_id_arg != NULL)
  {
    this->view_id= new Gcs_corosync_view_identifier
                                            (view_id_arg->get_fixed_part(),
                                             view_id_arg->get_monotonic_part());
  }

  this->data= NULL;
  if(exchangeable_data != NULL)
  {
    this->data= new vector<uchar>(*exchangeable_data);
  }
}

Member_state::Member_state(const uchar* data, size_t len)
{
  this->view_id= NULL;

  long fixed_view_id= 0;
  int monotonic_view_id= 0;

  const uchar* slider= data;

  memcpy(&fixed_view_id, slider, VARIABLE_VIEW_ID_LENGTH);
  slider+= VARIABLE_VIEW_ID_LENGTH;

  memcpy(&monotonic_view_id, slider, VIEW_ID_LENGTH);
  slider+= VIEW_ID_LENGTH;

  this->view_id= new Gcs_corosync_view_identifier(fixed_view_id,
                                                  monotonic_view_id);

  size_t exchangeable_data_size= len - (VIEW_ID_LENGTH + VARIABLE_VIEW_ID_LENGTH);

  this->data= NULL;
  if(exchangeable_data_size != 0)
  {
    this->data= new vector<uchar>();

    this->data->insert(this->data->end(),
                       slider,
                       slider+exchangeable_data_size);
  }
}

Member_state::~Member_state()
{
  delete this->view_id;

  if(this->data != NULL)
  {
    delete this->data;
  }
}

void Member_state::encode(vector<uchar>* buffer)
{
  long fixed_view_id= 0;
  int  monotonic_view_id=0;

  if(this->view_id != NULL)
  {
    fixed_view_id= this->view_id->get_fixed_part();
    monotonic_view_id= this->view_id->get_monotonic_part();
  }

  uchar fixed_view_id_buffer[VARIABLE_VIEW_ID_LENGTH];
  memcpy(&fixed_view_id_buffer,
         &fixed_view_id,
         VARIABLE_VIEW_ID_LENGTH);

  buffer->insert(buffer->end(),
                 fixed_view_id_buffer,
                 fixed_view_id_buffer + VARIABLE_VIEW_ID_LENGTH);

  uchar view_id_buffer[VIEW_ID_LENGTH];
  memcpy(&view_id_buffer,
         &monotonic_view_id,
         VIEW_ID_LENGTH);

  buffer->insert(buffer->end(),
                 view_id_buffer,
                 view_id_buffer + VIEW_ID_LENGTH);

  if(data != NULL && data->size() != 0)
  {
    buffer->insert(buffer->end(),
                   data->begin(),
                   data->end());
  }
}

//Gcs_corosync_state_exchange implementation
const int Gcs_corosync_state_exchange::state_exchange_header_code= 9999;

Gcs_corosync_state_exchange::Gcs_corosync_state_exchange
                                          (Gcs_communication_interface* comm)
          : broadcaster(comm), last_view_id(NULL), max_view_id(NULL)
{
  group_name= new string();
}

Gcs_corosync_state_exchange::~Gcs_corosync_state_exchange()
{
  reset();

  delete group_name;
}

void
Gcs_corosync_state_exchange::init()
{
  last_view_id= NULL;
}

void
Gcs_corosync_state_exchange::reset()
{
  max_view_id= 0;

  if(last_view_id != NULL)
  {
    delete last_view_id;
    last_view_id= NULL;
  }

  set<Gcs_member_identifier*>::iterator member_it;

  if(ms_total.size() > 0)
  {
    for(member_it= ms_total.begin(); member_it != ms_total.end(); member_it++)
    {
        delete (*member_it);
    }
    ms_total.clear();
  }

  if(ms_left.size() > 0)
  {
    for(member_it= ms_left.begin(); member_it != ms_left.end(); member_it++)
    {
        delete (*member_it);
    }
    ms_left.clear();
  }

  if (ms_joined.size() > 0)
  {
    for (member_it= ms_joined.begin(); member_it != ms_joined.end(); member_it++)
    {
      delete (*member_it);
    }
    ms_joined.clear();
  }

  map<Gcs_member_identifier, Member_state*>::iterator state_it;
  if(member_states.size() > 0)
  {
    for (state_it= member_states.begin(); state_it != member_states.end();
         state_it++)
    {
      delete (*state_it).second;
    }
    member_states.clear();
  }
}

bool
Gcs_corosync_state_exchange::state_exchange(const cpg_address *total,
                                            size_t total_entries,
                                            const struct cpg_address *left,
                                            size_t left_entries,
                                            const struct cpg_address *joined,
                                            size_t joined_entries,
                                            string* group,
                                            vector<uchar> *data,
                                            Gcs_view* current_view,
                                            Gcs_member_identifier* local_info)
{
  /* Store member state for later broadcast */
  local_information= local_info;
  exchangeable_data= data;

  group_name->clear();
  group_name->append(group->c_str());

  if(current_view != NULL) //I am a joiner and i am not the only one.
  {
    //Make a copy of the current view identifier
    Gcs_corosync_view_identifier coro_view_id=
                   *(Gcs_corosync_view_identifier*)current_view->get_view_id();
    last_view_id= new Gcs_corosync_view_identifier(coro_view_id);
  }
  else if(current_view == NULL && total_entries == 1)
  {
    /*
     This case means that i am the first one in a group.
     I don't have a view and the list only has one member.
     */
    time_t current_time= time(0);
    last_view_id= new Gcs_corosync_view_identifier(current_time,
                                                   0);
  }

  fill_member_set(total, total_entries, ms_total);
  fill_member_set(joined, joined_entries, ms_joined);
  fill_member_set(left, left_entries, ms_left);

  /*
   Calculate if i am leaving...
   If so, SE will be interrupted and it will return true...
  */
  bool leaving= is_leaving();

  if(!leaving)
  {
    update_awaited_vector();
    broadcast_state();
  }

  return leaving;
}

bool
Gcs_corosync_state_exchange::is_leaving()
{
  bool is_leaving= false;

  set<Gcs_member_identifier*>::iterator it;

  for(it= ms_left.begin(); it != ms_left.end() && !is_leaving; it++)
  {
    is_leaving= (*(*it) == *local_information);
  }

  return is_leaving;
}

void
Gcs_corosync_state_exchange::broadcast_state()
{
  uchar header_buffer[STATE_EXCHANGE_HEADER_CODE_LENGTH];
  memcpy(&header_buffer,
         &state_exchange_header_code,
         STATE_EXCHANGE_HEADER_CODE_LENGTH);

  Member_state member_state(last_view_id,
                            exchangeable_data);

  vector<uchar> encoded_state;
  member_state.encode(&encoded_state);

  Gcs_group_identifier group_id(*(this->group_name));
  Gcs_message *message= new Gcs_message(*local_information, group_id, UNIFORM);

  message->append_to_header(header_buffer, STATE_EXCHANGE_HEADER_CODE_LENGTH);
  message->append_to_payload(&(encoded_state.front()), encoded_state.size());

  Gcs_corosync_communication_interface* binding_broadcaster=
              static_cast<Gcs_corosync_communication_interface*>(broadcaster);

  binding_broadcaster->send_binding_message(message);

  delete message;
}

void
Gcs_corosync_state_exchange::update_awaited_vector()
{
  set<Gcs_member_identifier*>::iterator it;
  Gcs_member_identifier* p_id;

  for (it= ms_total.begin(), p_id= *it; it != ms_total.end(); ++it, p_id= *it)
  {
    awaited_vector[*p_id]++;
  }

  for (it= ms_left.begin(), p_id= *it; it != ms_left.end(); ++it, p_id= *it)
  {
    awaited_vector.erase(*p_id);
  }
}

bool
Gcs_corosync_state_exchange::process_member_state(Member_state *ms_info,
                                                  Gcs_member_identifier p_id)
{

  /*
    max_view_id former setter gets overridden in case of the same
    view-id different sender.
    Eventually at install_view() the max view-id setter is the last
    that showed a maximum value of view-id.
  */

  if (max_view_id == NULL ||
      max_view_id->get_monotonic_part() <=
                               ms_info->get_view_id()->get_monotonic_part())
  {
    /* and its view_id is higher than found so far so it's memorized. */
    max_view_id= ms_info->get_view_id();
  }
  member_states[p_id]= ms_info;

  /*
    The rule of updating the awaited_vector at receiving is simply to
    decrement the counter in the right index. When the value drops to
    zero the index is discarded from the vector.

    Installation goes into terminal phase when all expected state
    messages have arrived which is indicated by the empty vector.
  */
  if (--awaited_vector[p_id] == 0)
  {
    awaited_vector.erase(p_id);
  }

  bool can_install_view= (awaited_vector.size() == 0);

  return can_install_view;
}

bool
Gcs_corosync_state_exchange::is_state_exchange_message(Gcs_message* to_verify)
{
  //State exchange only takes a state_exchange_header_code in the header
  if( to_verify->get_header_length() != STATE_EXCHANGE_HEADER_CODE_LENGTH)
    return false;

  int header_code= 0;
  memcpy(&header_code,
         to_verify->get_header(),
         STATE_EXCHANGE_HEADER_CODE_LENGTH);

  return header_code == state_exchange_header_code;
}

void
Gcs_corosync_state_exchange::fill_member_set(const struct cpg_address *list,
                                             size_t num,
                                             set<Gcs_member_identifier*>& pset)
{
  for (size_t i= 0; i < num; i++)
  {
    Gcs_member_identifier* member
              = Gcs_corosync_utils::build_corosync_member_id(list[i].nodeid,
                                                             list[i].pid);

    pset.insert(member);
  }
}

