/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "plugin_psi.h"

using std::string;
using std::vector;
using std::map;

Group_member_info::
Group_member_info(char* hostname_arg,
                  uint port_arg,
                  char* uuid_arg,
                  int write_set_extraction_algorithm_arg,
                  const std::string& gcs_member_id_arg,
                  Group_member_info::Group_member_status status_arg,
                  Member_version& member_version_arg,
                  ulonglong gtid_assignment_block_size_arg,
                  Group_member_info::Group_member_role role_arg,
                  bool in_single_primary_mode,
                  bool has_enforces_update_everywhere_checks)
  : Plugin_gcs_message(CT_MEMBER_INFO_MESSAGE),
    hostname(hostname_arg), port(port_arg), uuid(uuid_arg),
    status(status_arg),
    write_set_extraction_algorithm(write_set_extraction_algorithm_arg),
    gtid_assignment_block_size(gtid_assignment_block_size_arg),
    unreachable(false),
    role(role_arg),
    configuration_flags(0)
{
  gcs_member_id= new Gcs_member_identifier(gcs_member_id_arg);
  member_version= new Member_version(member_version_arg.get_version());

  /* Handle single_primary_mode */
  if (in_single_primary_mode)
    configuration_flags |= CNF_SINGLE_PRIMARY_MODE_F;

  /* Handle enforce_update_everywhere_checks */
  if (has_enforces_update_everywhere_checks)
    configuration_flags |= CNF_ENFORCE_UPDATE_EVERYWHERE_CHECKS_F;
}

Group_member_info::Group_member_info(Group_member_info& other)
  : Plugin_gcs_message(CT_MEMBER_INFO_MESSAGE),
    hostname(other.get_hostname()),
    port(other.get_port()),
    uuid(other.get_uuid()),
    status(other.get_recovery_status()),
    executed_gtid_set(other.get_gtid_executed()),
    retrieved_gtid_set(other.get_gtid_retrieved()),
    write_set_extraction_algorithm(other.get_write_set_extraction_algorithm()),
    gtid_assignment_block_size(other.get_gtid_assignment_block_size()),
    unreachable(other.is_unreachable()),
    role(other.get_role()),
    configuration_flags(other.get_configuration_flags())
{
  gcs_member_id= new Gcs_member_identifier(other.get_gcs_member_id()
                                               .get_member_id());
  member_version= new Member_version(other.get_member_version()
                                         .get_version());
}

Group_member_info::Group_member_info(const uchar* data, size_t len)
  : Plugin_gcs_message(CT_MEMBER_INFO_MESSAGE),
    gcs_member_id(NULL), member_version(NULL),
    unreachable(false)
{
  decode(data, len);
}

Group_member_info::~Group_member_info()
{
  delete gcs_member_id;
  delete member_version;
}

void
Group_member_info::encode_payload(std::vector<unsigned char>* buffer) const
{
  DBUG_ENTER("Group_member_info::encode_payload");

  encode_payload_item_string(buffer, PIT_HOSTNAME,
                             hostname.c_str(),
                             hostname.length());

  uint16 port_aux= (uint16)port;
  encode_payload_item_int2(buffer, PIT_PORT,
                           port_aux);

  encode_payload_item_string(buffer, PIT_UUID,
                             uuid.c_str(),
                             uuid.length());

  encode_payload_item_string(buffer, PIT_GCS_ID,
                             gcs_member_id->get_member_id().c_str(),
                             gcs_member_id->get_member_id().length());

  char status_aux= (uchar)status;
  encode_payload_item_char(buffer, PIT_STATUS,
                           status_aux);

  uint32 version_aux= (uint32)member_version->get_version();
  encode_payload_item_int4(buffer, PIT_VERSION,
                           version_aux);

  uint16 write_set_extraction_algorithm_aux=
      (uint16)write_set_extraction_algorithm;
  encode_payload_item_int2(buffer, PIT_WRITE_SET_EXTRACTION_ALGORITHM,
                           write_set_extraction_algorithm_aux);

  encode_payload_item_string(buffer, PIT_EXECUTED_GTID,
                             executed_gtid_set.c_str(),
                             executed_gtid_set.length());

  encode_payload_item_string(buffer, PIT_RETRIEVED_GTID,
                             retrieved_gtid_set.c_str(),
                             retrieved_gtid_set.length());

  encode_payload_item_int8(buffer, PIT_GTID_ASSIGNMENT_BLOCK_SIZE,
                           gtid_assignment_block_size);

  char role_aux= (uchar)role;
  encode_payload_item_char(buffer, PIT_MEMBER_ROLE, role_aux);

  uint32 configuration_flags_aux= (uint32)configuration_flags;
  encode_payload_item_int4(buffer, PIT_CONFIGURATION_FLAGS,
                           configuration_flags_aux);

  DBUG_VOID_RETURN;
}

void
Group_member_info::decode_payload(const unsigned char* buffer,
                                  size_t length)
{
  DBUG_ENTER("Group_member_info::decode_payload");
  const unsigned char *slider= buffer;
  uint16 payload_item_type= 0;
  unsigned long long payload_item_length= 0;

  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &hostname,
                             &payload_item_length);

  uint16 port_aux= 0;
  decode_payload_item_int2(&slider,
                           &payload_item_type,
                           &port_aux);
  port= (uint)port_aux;

  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &uuid,
                             &payload_item_length);

  std::string gcs_member_id_aux;
  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &gcs_member_id_aux,
                             &payload_item_length);
  delete gcs_member_id;
  gcs_member_id= new Gcs_member_identifier(gcs_member_id_aux);

  unsigned char status_aux= 0;
  decode_payload_item_char(&slider,
                           &payload_item_type,
                           &status_aux);
  status= (Group_member_status)status_aux;

  uint32 member_version_aux= 0;
  decode_payload_item_int4(&slider,
                           &payload_item_type,
                           &member_version_aux);
  delete member_version;
  member_version= new Member_version(member_version_aux);

  uint16 write_set_extraction_algorithm_aux= 0;
  decode_payload_item_int2(&slider,
                           &payload_item_type,
                           &write_set_extraction_algorithm_aux);
  write_set_extraction_algorithm= (uint)write_set_extraction_algorithm_aux;

  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &executed_gtid_set,
                             &payload_item_length);

  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &retrieved_gtid_set,
                             &payload_item_length);

  decode_payload_item_int8(&slider,
                           &payload_item_type,
                           &gtid_assignment_block_size);

  unsigned char role_aux= 0;
  decode_payload_item_char(&slider,
                           &payload_item_type,
                           &role_aux);
  role= (Group_member_role)role_aux;

  uint32 configuration_flags_aux= 0;
  decode_payload_item_int4(&slider,
                           &payload_item_type,
                           &configuration_flags_aux);
  configuration_flags= configuration_flags_aux;

  DBUG_VOID_RETURN;
}

const string&
Group_member_info::get_hostname()
{
  return hostname;
}

uint
Group_member_info::get_port()
{
  return port;
}

const string&
Group_member_info::get_uuid()
{
  return uuid;
}

Group_member_info::Group_member_status
Group_member_info::get_recovery_status()
{
  return status;
}

Group_member_info::Group_member_role
Group_member_info::get_role()
{
  return role;
}

const Gcs_member_identifier&
Group_member_info::get_gcs_member_id()
{
  return *gcs_member_id;
}

void
Group_member_info::update_recovery_status(Group_member_status new_status)
{
  status= new_status;
}

void
Group_member_info::update_gtid_sets(std::string& executed_gtids,
                                    std::string& retrieved_gtids)
{
  executed_gtid_set.assign(executed_gtids);
  retrieved_gtid_set.assign(retrieved_gtids);
}

void
Group_member_info::set_role(Group_member_role new_role)
{
  role= new_role;
}

const Member_version&
Group_member_info::get_member_version()
{
  return *member_version;
}

const std::string&
Group_member_info::get_gtid_executed()
{
  return executed_gtid_set;
}

const std::string&
Group_member_info::get_gtid_retrieved()
{
  return retrieved_gtid_set;
}

uint
Group_member_info::get_write_set_extraction_algorithm()
{
  return write_set_extraction_algorithm;
}

ulonglong
Group_member_info::get_gtid_assignment_block_size()
{
  return gtid_assignment_block_size;
}

uint32
Group_member_info::get_configuration_flags()
{
  return configuration_flags;
}

bool Group_member_info::in_primary_mode()
{
  return get_configuration_flags() & CNF_SINGLE_PRIMARY_MODE_F;
}

bool Group_member_info::has_enforces_update_everywhere_checks()
{
  return get_configuration_flags() & CNF_ENFORCE_UPDATE_EVERYWHERE_CHECKS_F;
}

bool
Group_member_info::is_unreachable()
{
  return unreachable;
}

void
Group_member_info::set_unreachable()
{
  unreachable= true;
}

void
Group_member_info::set_reachable()
{
  unreachable= false;
}

bool
Group_member_info::operator <(Group_member_info& other)
{
  return this->get_uuid().compare(other.get_uuid()) < 0;
}

bool
Group_member_info::operator ==(Group_member_info& other)
{
  return this->get_uuid().compare(other.get_uuid()) == 0;
}

const char*
Group_member_info::get_member_status_string(Group_member_status status)
{
  switch(status)
  {
    case MEMBER_ONLINE:
      return "ONLINE";
    case MEMBER_OFFLINE:
      return "OFFLINE";
    case MEMBER_IN_RECOVERY:
      return "RECOVERING";
    case MEMBER_ERROR:
      return "ERROR";
    case MEMBER_UNREACHABLE:
      return "UNREACHABLE";
    default:
      return "OFFLINE"; /* purecov: inspected */
  }
}

const char*
Group_member_info::get_configuration_flag_string(const uint32 configuation_flag)
{
  switch (configuation_flag)
  {
  case 0:
    return "";
  case CNF_ENFORCE_UPDATE_EVERYWHERE_CHECKS_F:
    return "group_replication_enforce_update_everywhere_checks";
  case CNF_SINGLE_PRIMARY_MODE_F:
    return "group_replication_single_primary_mode";
  default:
    return "UNKNOWN"; /* purecov: inspected */
  }
}

std::string
Group_member_info::get_configuration_flags_string(const uint32 configuation_flags)
{
  std::string result;
  uint32 configuration_flags_mask = 1;

  while (configuration_flags_mask > 0)
  {
    const uint32 current_flag = configuration_flags_mask & configuation_flags;
    const char* current_flag_name = get_configuration_flag_string(current_flag);

    if (current_flag)
    {
      if (!result.empty())
        result += ","; /* purecov: inspected */

      result += current_flag_name;
    }

    configuration_flags_mask = configuration_flags_mask << 1;
  }

  return result;
}

bool
Group_member_info::comparator_group_member_info(Group_member_info *m1,
                                                Group_member_info *m2)
{
  return *m1 < *m2;
}


Group_member_info_manager::
Group_member_info_manager(Group_member_info* local_member_info)
{
  members= new map<string, Group_member_info*>();
  this->local_member_info= local_member_info;

  mysql_mutex_init(key_GR_LOCK_group_info_manager, &update_lock,
                   MY_MUTEX_INIT_FAST);

  add(local_member_info);
}

Group_member_info_manager::~Group_member_info_manager()
{
  clear_members();
  delete members;
}

int
Group_member_info_manager::get_number_of_members()
{
  return members->size();
}

Group_member_info*
Group_member_info_manager::get_group_member_info(const string& uuid)
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
    if((*it).second->get_gcs_member_id() == idx)
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
    Group_member_info* member_copy = new Group_member_info(*(*it).second);
    all_members->push_back(member_copy);
  }

  mysql_mutex_unlock(&update_lock);
  return all_members;
}

void
Group_member_info_manager::add(Group_member_info* new_member)
{
  mysql_mutex_lock(&update_lock);

  (*members)[new_member->get_uuid()]= new_member;

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

    (*members)[(*new_members_it)->get_uuid()]= (*new_members_it);
  }

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::
update_member_status(const string& uuid,
                     Group_member_info::Group_member_status new_status)
{
  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;

  it= members->find(uuid);

  if(it != members->end())
  {
    (*it).second->update_recovery_status(new_status);
  }

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::
update_gtid_sets(const string& uuid,
                 string& gtid_executed,
                 string& gtid_retrieved)
{
  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;

  it= members->find(uuid);

  if(it != members->end())
  {
    (*it).second->update_gtid_sets(gtid_executed, gtid_retrieved);
  }

  mysql_mutex_unlock(&update_lock);
}

void
Group_member_info_manager::
update_member_role(const string& uuid,
                   Group_member_info::Group_member_role new_role)
{
  mysql_mutex_lock(&update_lock);

  map<string, Group_member_info*>::iterator it;

  it= members->find(uuid);

  if(it != members->end())
  {
    (*it).second->set_role(new_role);
  }

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

void
Group_member_info_manager::encode(vector<uchar>* to_encode)
{
  Group_member_info_manager_message *group_info_message=
    new Group_member_info_manager_message(*this);
  group_info_message->encode(to_encode);
  delete group_info_message;
}

vector<Group_member_info*>*
Group_member_info_manager::decode(const uchar* to_decode, size_t length)
{
  vector<Group_member_info*>* decoded_members= NULL;

  Group_member_info_manager_message *group_info_message=
    new Group_member_info_manager_message();
  group_info_message->decode(to_decode, length);
  decoded_members= group_info_message->get_all_members();
  delete group_info_message;

  return decoded_members;
}


Group_member_info_manager_message::Group_member_info_manager_message()
  : Plugin_gcs_message(CT_MEMBER_INFO_MANAGER_MESSAGE)
{
  DBUG_ENTER("Group_member_info_manager_message::Group_member_info_manager_message");
  members= new vector<Group_member_info*>();
  DBUG_VOID_RETURN;
}

Group_member_info_manager_message::Group_member_info_manager_message(
    Group_member_info_manager& group_info)
  : Plugin_gcs_message(CT_MEMBER_INFO_MANAGER_MESSAGE),
    members(group_info.get_all_members())
{
  DBUG_ENTER("Group_member_info_manager_message::Group_member_info_manager_message");
  DBUG_VOID_RETURN;
}

Group_member_info_manager_message::Group_member_info_manager_message(
    Group_member_info* member_info)
  : Plugin_gcs_message(CT_MEMBER_INFO_MANAGER_MESSAGE),
    members(NULL)
{
  DBUG_ENTER("Group_member_info_manager_message::Group_member_info_manager_message");
  members= new vector<Group_member_info*>();
  members->push_back(member_info);
  DBUG_VOID_RETURN;
}

Group_member_info_manager_message::~Group_member_info_manager_message()
{
  DBUG_ENTER("Group_member_info_manager_message::~Group_member_info_manager_message");
  clear_members();
  delete members;
  DBUG_VOID_RETURN;
}

void Group_member_info_manager_message::clear_members()
{
  DBUG_ENTER("Group_member_info_manager_message::clear_members");
  std::vector<Group_member_info*>::iterator it;
  for(it= members->begin(); it != members->end(); it++)
  {
    delete (*it);
  }
  members->clear();
  DBUG_VOID_RETURN;
}

std::vector<Group_member_info*>*
Group_member_info_manager_message::get_all_members()
{
  DBUG_ENTER("Group_member_info_manager_message::get_all_members");
  vector<Group_member_info*>* all_members= new vector<Group_member_info*>();

  std::vector<Group_member_info*>::iterator it;
  for(it= members->begin(); it != members->end(); it++)
  {
    Group_member_info* member_copy = new Group_member_info(*(*it));
    all_members->push_back(member_copy);
  }

  DBUG_RETURN(all_members);
}

void
Group_member_info_manager_message::encode_payload(std::vector<unsigned char>* buffer) const
{
  DBUG_ENTER("Group_member_info_manager_message::encode_payload");

  uint16 number_of_members= (uint16)members->size();
  encode_payload_item_int2(buffer, PIT_MEMBERS_NUMBER,
                           number_of_members);

  std::vector<Group_member_info*>::iterator it;
  for(it= members->begin(); it != members->end(); it++)
  {
    std::vector<uchar> encoded_member;
    (*it)->encode(&encoded_member);

    encode_payload_item_type_and_length(buffer, PIT_MEMBER_DATA,
                                        encoded_member.size());
    buffer->insert(buffer->end(), encoded_member.begin(), encoded_member.end());
  }

  DBUG_VOID_RETURN;
}

void
Group_member_info_manager_message::decode_payload(const unsigned char* buffer,
                                                  size_t length)
{
  DBUG_ENTER("Group_member_info_manager_message::decode_payload");
  const unsigned char *slider= buffer;
  uint16 payload_item_type= 0;
  unsigned long long payload_item_length= 0;

  uint16 number_of_members= 0;
  decode_payload_item_int2(&slider,
                           &payload_item_type,
                           &number_of_members);

  clear_members();
  for(uint16 i= 0; i < number_of_members; i++)
  {
    decode_payload_item_type_and_length(&slider,
                                        &payload_item_type,
                                        &payload_item_length);
    Group_member_info* member= new Group_member_info(slider,
                                                     payload_item_length);
    members->push_back(member);
    slider+= payload_item_length;
  }

  DBUG_VOID_RETURN;
}
