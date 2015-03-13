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

#include "member_info.h"

using std::string;
using std::vector;
using std::map;

Group_member_info::
Group_member_info(char* hostname_arg,
                  uint port_arg,
                  char* uuid_arg,
                  Gcs_member_identifier* gcs_member_id_arg,
                  Group_member_info::Group_member_status status_arg)
{
  hostname.append(hostname_arg);
  port= port_arg;
  uuid.append(uuid_arg);
  status= status_arg;
  gcs_member_id= new Gcs_member_identifier(*gcs_member_id_arg->get_member_id());
}

Group_member_info::Group_member_info(Group_member_info& other)
{
  hostname.append(other.get_hostname()->c_str());
  port= other.get_port();
  uuid.append(other.get_uuid()->c_str());
  status= other.get_recovery_status();
  gcs_member_id= new Gcs_member_identifier(other.get_gcs_member_id()
                                               ->get_member_id()->c_str());
}

Group_member_info::Group_member_info(const uchar* data, size_t len)
{
  const char *slider= (char*) data;

  hostname= string(slider);
  slider+= hostname.length() + 1;
  port= uint2korr(slider);
  slider+= PORT_ENCODED_LENGTH;
  uuid= string(slider);
  slider+= uuid.length() + 1;

  string gcs_member_id_str(slider);
  gcs_member_id= new Gcs_member_identifier(gcs_member_id_str.c_str());

  slider+= gcs_member_id_str.length() + 1;

  status= (Group_member_status) * (uchar*) slider;
  slider+= 1;
}

Group_member_info::~Group_member_info()
{
  delete gcs_member_id;
}

uint
Group_member_info::encode(vector<uchar>* mbuf_ptr)
{
  uchar local_buf[PORT_ENCODED_LENGTH];
  uint encoded_length= 0;

  mbuf_ptr->insert(mbuf_ptr->end(),
                   hostname.c_str(),
                   hostname.c_str() + hostname.length() + 1);

  encoded_length+= (hostname.length() + 1);

  int2store(local_buf, port);
  mbuf_ptr->insert(mbuf_ptr->end(),
                   local_buf,
                   local_buf + PORT_ENCODED_LENGTH);

  encoded_length+= PORT_ENCODED_LENGTH;

  mbuf_ptr->insert(mbuf_ptr->end(),
                   uuid.c_str(),
                   uuid.c_str() + uuid.length() + 1);

  encoded_length+= uuid.length() + 1;

  string* gcs_member_id_str= gcs_member_id->get_member_id();
  mbuf_ptr->insert(mbuf_ptr->end(),
                   gcs_member_id_str->c_str(),
                   gcs_member_id_str->c_str() + gcs_member_id_str->length() + 1);

  encoded_length+= gcs_member_id_str->length() + 1;

  uchar s[1];
  s[0]= (const uchar) status;
  mbuf_ptr->insert(mbuf_ptr->end(), s, s + 1);

  encoded_length+= 1;

  return encoded_length;
}

string*
Group_member_info::get_hostname()
{
  return &hostname;
}

uint
Group_member_info::get_port()
{
  return port;
}

string*
Group_member_info::get_uuid()
{
  return &uuid;
}

Group_member_info::Group_member_status
Group_member_info::get_recovery_status()
{
  return status;
}

Gcs_member_identifier*
Group_member_info::get_gcs_member_id()
{
  return gcs_member_id;
}

void
Group_member_info::update_recovery_status(Group_member_status new_status)
{
  status= new_status;
}

bool
Group_member_info::operator <(Group_member_info& other)
{
  return this->get_uuid()->compare(*other.get_uuid()) < 0;
}

bool
Group_member_info::operator ==(Group_member_info& other)
{
  return this->get_uuid()->compare(*other.get_uuid()) == 0;
}

string
Group_member_info::get_textual_representation()
{
  string text_rep;

  text_rep.append(get_uuid()->c_str());
  text_rep.append(" ");
  text_rep.append(get_hostname()->c_str());

  return text_rep;
}

Group_member_info_manager::
Group_member_info_manager(Group_member_info* local_member_info)
{
  members= new map<string, Group_member_info*>();
  serialized_format= new vector<uchar>();
  this->local_member_info= local_member_info;

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_info group_info_update_mutexes[]=
  {
    { &group_info_manager_key_mutex, "LOCK_group_info_update", 0}
  };

  mysql_mutex_register("group_replication", group_info_update_mutexes, 1);
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(group_info_manager_key_mutex, &update_lock,
                   MY_MUTEX_INIT_FAST);

  add(local_member_info);
}

Group_member_info_manager::~Group_member_info_manager()
{
  clear_members();
  delete members;
  delete serialized_format;
}

int
Group_member_info_manager::get_number_of_members()
{
  return members->size();
}

Group_member_info*
Group_member_info_manager::get_group_member_info(string uuid)
{
  Group_member_info* member= NULL;
  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;

  it= members->find(uuid);

  if(it != members->end())
  {
    member= (*it).second;
  }

  Group_member_info* member_copy= NULL;
  if(member != NULL)
  {
    member_copy= new Group_member_info(*member);
  }

  mysql_mutex_unlock(&update_lock);

  return member_copy;
}

Group_member_info*
Group_member_info_manager::get_group_member_info_by_index(int idx)
{
  Group_member_info* member= NULL;

  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;
  if(idx < (int)members->size())
  {
    int i= 0;
    for(it= members->begin(); i <= idx; i++, it++)
    {
      member= (*it).second;
    }
  }

  Group_member_info* member_copy= NULL;
  if(member != NULL)
  {
    member_copy= new Group_member_info(*member);
  }
  mysql_mutex_unlock(&update_lock);

  return member_copy;
}

Group_member_info*
Group_member_info_manager::
get_group_member_info_by_member_id(Gcs_member_identifier idx)
{
  Group_member_info* member= NULL;

  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;
  for(it= members->begin(); it != members->end(); it++)
  {
    if(*((*it).second->get_gcs_member_id()) == idx)
    {
      member= (*it).second;
      break;
    }
  }

  mysql_mutex_unlock(&update_lock);
  return member;
}


vector<Group_member_info*>*
Group_member_info_manager::get_all_members()
{
  mysql_mutex_lock(&update_lock);

  vector<Group_member_info*>* all_members= new vector<Group_member_info*>();
  map<string, Group_member_info*>::iterator it;
  for(it= members->begin(); it != members->end(); it++)
  {
    all_members->push_back((*it).second);
  }

  mysql_mutex_unlock(&update_lock);
  return all_members;
}

void
Group_member_info_manager::add(Group_member_info* new_member)
{
  mysql_mutex_lock(&update_lock);

  (*members)[*new_member->get_uuid()]= new_member;

  serialized_format->clear();
  this->encode(serialized_format);

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::update(vector<Group_member_info*>* new_members)
{
  mysql_mutex_lock(&update_lock);

  this->clear_members();

  vector<Group_member_info*>::iterator new_members_it;
  for(new_members_it= new_members->begin();
      new_members_it != new_members->end();
      new_members_it++)
  {
    //If this bears the local member to be updated
    // It will add the current reference and update its status
    if(*(*new_members_it) == *local_member_info)
    {
      local_member_info
          ->update_recovery_status((*new_members_it)->get_recovery_status());

      delete (*new_members_it);

      continue;
    }

    (*members)[*(*new_members_it)->get_uuid()]= (*new_members_it);
  }

  serialized_format->clear();
  this->encode(serialized_format);

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::
update_member_status(string uuid,
                     Group_member_info::Group_member_status new_status)
{
  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;

  it= members->find(uuid);

  if(it != members->end())
  {
    (*it).second->update_recovery_status(new_status);
  }

  serialized_format->clear();
  this->encode(serialized_format);

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::clear_members()
{
  map<string, Group_member_info*>::iterator it= members->begin();
  while (it != members->end()) {
    if((*it).second == local_member_info)
    {
      ++it;
      continue;
    }

    delete (*it).second;
    members->erase(it++);
  }
}

vector<uchar>*
Group_member_info_manager::get_exchangeable_format()
{
  return this->serialized_format;
}

void
Group_member_info_manager::encode(vector<uchar>* to_encode)
{
  uchar local_buffer[2];
  int2store(local_buffer, (uint16)members->size());

  to_encode->insert(to_encode->end(),
                    local_buffer,
                    local_buffer+sizeof(local_buffer));

  map<string, Group_member_info*>::iterator it;
  vector<uchar> encoded_member;
  for(it= members->begin(); it != members->end(); it++)
  {
    uint encoded_size= (*it).second->encode(&encoded_member);
    int2store(local_buffer, (uint16)encoded_size);

    to_encode->insert(to_encode->end(),
                      local_buffer,
                      local_buffer+sizeof(local_buffer));

    to_encode->insert(to_encode->end(),
                      encoded_member.begin(),
                      encoded_member.end());

    encoded_member.clear();
  }
}

vector<Group_member_info*>*
Group_member_info_manager::decode(uchar* to_decode)
{
  vector<Group_member_info*>* decoded_members= new vector<Group_member_info*>();

  uchar* slider= to_decode;
  int number_of_members= uint2korr(slider);
  slider+= 2;
  for(int i= 0; i < number_of_members; i++)
  {
    size_t member_length= uint2korr(slider);
    slider+= 2;

    Group_member_info* new_member= new Group_member_info(slider, member_length);
    decoded_members->insert(decoded_members->end(),
                            new_member);

    slider+= member_length;
  }

  return decoded_members;
}
