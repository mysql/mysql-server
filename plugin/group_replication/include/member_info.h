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

#ifndef MEMBER_INFO_INCLUDE
#define MEMBER_INFO_INCLUDE

/*
  The file contains declarations relevant to Member state and
  its identification by the Protocol Client.
*/

#include <string>
#include <map>
#include <vector>
#include <set>

#include "gcs_member_identifier.h"

/*
  Since this file is used on unit tests includes must set here and
  not through plugin_server_include.h.
*/
#include <my_global.h>
#include <my_sys.h>

#define PORT_ENCODED_LENGTH 2

/*
  @class Group_member_info

  Describes all the properties of a group member
*/
class Group_member_info
{

public:

  /*
   @enum Member_recovery_status

   This enumeration describes all the states that a member can assume while in a
   group.
   */
  typedef enum
  {
    MEMBER_ONLINE= 1,
    MEMBER_OFFLINE,
    MEMBER_IN_RECOVERY,
    MEMBER_END  // the end of the enum
  } Group_member_status;

public:
  /**
    Group_member_info constructor

    @param[in] hostname_arg       member hostname
    @param[in] port_arg           member port
    @param[in] uuid_arg           member uuid
    @param[in] gcs_member_id_arg  member GCS member identifier
    @param[in] status_arg         member Recovery status
   */
  Group_member_info(char* hostname_arg,
                    uint port_arg,
                    char* uuid_arg,
                    Gcs_member_identifier* gcs_member_id_arg,
                    Group_member_status status_arg);

  /**
    Copy constructor

    @param other source of the copy
   */
  Group_member_info(Group_member_info& other );

  /**
   * Group_member_info raw data constructor
   *
   * @param[in] data raw data
   * @param[in] len raw data length
   */
  Group_member_info(const uchar* data, size_t len);

  /**
    Destructor
   */
  ~Group_member_info();

  /**
    Encodes this object to send over the network

    @param[out] mbuf_ptr  parameter where the data is written
    @return the amount of data written
   */
  uint encode(std::vector<uchar>* mbuf_ptr);

  /**
    @return the member hostname
   */
  std::string* get_hostname();

  /**
    @return the member port
   */
  uint get_port();

  /**
    @return the member uuid
   */
  std::string* get_uuid();

  /**
    @return the member identifier in the GCS layer
   */
  Gcs_member_identifier* get_gcs_member_id();

  /**
    @return the member recovery status
   */
  Group_member_status get_recovery_status();

  /**
    Updates this object recovery status

    @param[in] new_status the status to set
   */
  void update_recovery_status(Group_member_status new_status);

  /**
    Returns a textual representation of this object

    @return an std::string with the representation
   */
  std::string get_textual_representation();

  /**
   Redefinition of operate == and <. They operate upon the uuid
   */
  bool operator ==(Group_member_info& other);

  bool operator <(Group_member_info& other);

private:
  std::string hostname;
  uint port;
  std::string uuid;
  Group_member_status status;
  Gcs_member_identifier* gcs_member_id;
};

/*
  @interface Group_member_info_manager_interface

  Defines the set of operations that a Group_member_info_manager should provide.
  This is a component that lies on top of the GCS, on the application level,
  providing richer and relevant information to the plugin.
 */
class Group_member_info_manager_interface
{
public:
  virtual ~Group_member_info_manager_interface(){};

  virtual int get_number_of_members()= 0;

  /**
    Retrieves a registered Group member by its uuid

    @param[in] uuid uuid to retrieve
    @return reference to a copy of Group_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Group_member_info* get_group_member_info(std::string uuid)= 0;

  /**
    Retrieves a registered Group member by an index function.
    One is free to determine the index function. Nevertheless, it should have
    the same result regardless of the member of the group where it is called

    @param[in] idx the index
    @return reference to a Group_member_info. NULL if not managed
   */
  virtual Group_member_info* get_group_member_info_by_index(int idx)= 0;

  /**
    Retrieves a registered Group member by its backbone GCS identifier

    @param[in] idx the GCS identifier
    @return reference to a copy of Group_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Group_member_info*
  get_group_member_info_by_member_id(Gcs_member_identifier idx)= 0;

  /**
    Retrieves all Group members managed by this site

    @return a vector with all managed Group_member_info
   */
  virtual std::vector<Group_member_info*>* get_all_members()= 0;

  /**
    Adds a new member to be managed by this Group manager

    @param[in] new_member new group member
   */
  virtual void add(Group_member_info* new_member)= 0;

  /**
    Updates all members of the group. Typically used after a view change.

    @param[in] new_members new Group members
   */
  virtual void update(std::vector<Group_member_info*>* new_members)= 0;

  /**
    Updates the status of a single member

    @param[in] uuid        member uuid
    @param[in] new_status  status to change to
   */
  virtual void
  update_member_status(std::string uuid,
                       Group_member_info::Group_member_status new_status)= 0;

  /**
    Encodes this object to send via the network

    @param[out] to_encode out parameter to receive the encoded data
   */
  virtual void encode(std::vector<uchar>* to_encode)= 0;

  /**
    Decodes the raw format of this object

    @param[out] to_decode raw encoded data
    @return a vector of Group_member_info references
   */
  virtual std::vector<Group_member_info*>* decode(uchar* to_decode)= 0;

  /**
    Reference to this object in a serialized format

    @return a vector of serialized data
   */
  virtual std::vector<uchar>* get_exchangeable_format()= 0;
};

/**
  @class Group_member_info_manager

  Implementation of the interface Group_member_info_manager_interface
 */
class Group_member_info_manager: public Group_member_info_manager_interface
{
public:
  Group_member_info_manager(Group_member_info* local_member_info);

  virtual ~Group_member_info_manager();

  int get_number_of_members();

  Group_member_info* get_group_member_info(std::string uuid);

  Group_member_info* get_group_member_info_by_index(int idx);

  Group_member_info*
  get_group_member_info_by_member_id(Gcs_member_identifier idx);

  std::vector<Group_member_info*>* get_all_members();

  void add(Group_member_info* new_member);

  void update(std::vector<Group_member_info*>* new_members);

  void
  update_member_status(std::string uuid,
                       Group_member_info::Group_member_status new_status);

  void encode(std::vector<uchar>* to_encode);

  std::vector<Group_member_info*>* decode(uchar* to_decode);

  std::vector<uchar>* get_exchangeable_format();

private:
  void clear_members();

  std::map<std::string, Group_member_info*> *members;

  /*
   This field exists in order to provide a permanent reference to Exchangeable
   Data, since this is the data that is exchanged when a member joins and when
   a view changes. As such it should always be up-to-date.
   */
  std::vector<uchar> *serialized_format;
  Group_member_info* local_member_info;

  mysql_mutex_t update_lock;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key group_info_manager_key_mutex;
#endif
};

#endif /* MEMBER_INFO_INCLUDE */
