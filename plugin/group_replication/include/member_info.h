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

#ifndef GCS_MEMBER_INFO_H
#define GCS_MEMBER_INFO_H

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
  not through gcs_plugin_server_include.h.
*/
#include <my_global.h>
#include <my_sys.h>

#define PORT_ENCODED_LENGTH 2

/*
  @class Cluster_member_info

  Describes all the properties of a cluster member
*/
class Cluster_member_info
{

public:

  /*
   @enum Member_recovery_status

   This enumeration describes all the states that a node can assume while in a
   cluster.
   */
  typedef enum
  {
    MEMBER_ONLINE= 1,
    MEMBER_OFFLINE,
    MEMBER_IN_RECOVERY,
    MEMBER_END  // the end of the enum
  } Cluster_member_status;

public:
  /**
    Cluster_member_info constructor

    @param[in] hostname_arg node hostname
    @param[in] port_arg node port
    @param[in] uuid_arg node uuid
    @param[in] gcs_member_id_arg node GCS member identifier
    @param[in] status_arg node Recovery status
   */
  Cluster_member_info(char* hostname_arg,
                      uint port_arg,
                      char* uuid_arg,
                      Gcs_member_identifier* gcs_member_id_arg,
                      Cluster_member_status status_arg);

  /**
    Copy constructor

    @param other source of the copy
   */
  Cluster_member_info( Cluster_member_info& other );

  /**
   * Cluster_member_info raw data constructor
   *
   * @param[in] data raw data
   * @param[in] len raw data length
   */
  Cluster_member_info(const uchar* data, size_t len);

  /**
    Destructor
   */
  ~Cluster_member_info();

  /**
    Encodes this object to send over the network

    @param[out] mbuf_ptr  parameter where the data is written
    @return the amount of data written
   */
  uint encode(std::vector<uchar>* mbuf_ptr);

  /**
    @return the node hostname
   */
  std::string* get_hostname();

  /**
    @return the node port
   */
  uint get_port();

  /**
    @return the node uuid
   */
  std::string* get_uuid();

  /**
    @return the node identifier in the GCS layer
   */
  Gcs_member_identifier* get_gcs_member_id();

  /**
    @return the node recovery status
   */
  Cluster_member_status get_recovery_status();

  /**
    Updates this object recovery status

    @param[in] new_status the status to set
   */
  void update_recovery_status(Cluster_member_status new_status);

  /**
    Returns a textual representation of this object

    @return an std::string with the representation
   */
  std::string get_textual_representation();

  /**
   Redefinition of operate == and <. They operate upon the uuid
   */
  bool operator ==(Cluster_member_info& other);

  bool operator <(Cluster_member_info& other);

private:
  std::string hostname;
  uint port;
  std::string uuid;
  Cluster_member_status status;
  Gcs_member_identifier* gcs_member_id;
};

/*
  @interface Cluster_member_info_manager_interface

  Defines the set of operations that a Cluster_member_info_manager should
  provide. This is a component that lies on top of the GCS, on the application
  level, providing richer and relevant information to the plugin.
 */
class Cluster_member_info_manager_interface
{
public:
  virtual ~Cluster_member_info_manager_interface(){};

  virtual int get_number_of_members()= 0;

  /**
    Retrieves a registered Cluster member by its uuid

    @param[in] uuid uuid to retrieve
    @return reference to a copy of Cluster_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Cluster_member_info* get_cluster_member_info(std::string uuid)= 0;

  /**
    Retrieves a registered Cluster member by an index function.
    One is free to determine the index function. Nevertheless, it should have
    the same result regardless of the node of the cluster where it is called

    @param[in] idx the index
    @return reference to a Cluster_member_info. NULL if not managed
   */
  virtual Cluster_member_info* get_cluster_member_info_by_index(int idx)= 0;

  /**
    Retrieves a registered Cluster member by its backbone GCS identifier

    @param[in] idx the GCS identifier
    @return reference to a copy of Cluster_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Cluster_member_info*
             get_cluster_member_info_by_member_id(Gcs_member_identifier idx)= 0;

  /**
    Retrieves all Cluster members managed by this site

    @return a vector with all managed Cluster_member_info
   */
  virtual std::vector<Cluster_member_info*>* get_all_members()= 0;

  /**
    Adds a new member to be managed by this Cluster manager

    @param[in] new_member new cluster member
   */
  virtual void add(Cluster_member_info* new_member)= 0;

  /**
    Updates all members of the cluster. Typically used after a view change.

    @param[in] new_members new Cluster members
   */
  virtual void update(std::vector<Cluster_member_info*>* new_members)= 0;

  /**
    Updates the status of a single member

    @param[in] uuid member uuid
    @param[in] new_status status to change to
   */
  virtual void update_member_status(std::string uuid,
                                    Cluster_member_info::Cluster_member_status
                                                                 new_status)= 0;

  /**
    Encodes this object to send via the network

    @param[out] to_encode out parameter to receive the encoded data
   */
  virtual void encode(std::vector<uchar>* to_encode)= 0;

  /**
    Decodes the raw format of this object

    @param[out] to_decode raw encoded data
    @return a vector of Cluster_member_info references
   */
  virtual std::vector<Cluster_member_info*>* decode(uchar* to_decode)= 0;

  /**
    Reference to this object in a serialized format

    @return a vector of serialized data
   */
  virtual std::vector<uchar>* get_exchangeable_format()= 0;
};

/**
  @class Cluster_member_info_manager

  Implementation of the interface Cluster_member_info_manager_interface
 */
class Cluster_member_info_manager: public Cluster_member_info_manager_interface
{
public:
  Cluster_member_info_manager(Cluster_member_info* local_node);

  virtual ~Cluster_member_info_manager();

  int get_number_of_members();

  Cluster_member_info* get_cluster_member_info(std::string uuid);

  Cluster_member_info* get_cluster_member_info_by_index(int idx);

  Cluster_member_info*
              get_cluster_member_info_by_member_id(Gcs_member_identifier idx);

  std::vector<Cluster_member_info*>* get_all_members();

  void add(Cluster_member_info* new_member);

  void update(std::vector<Cluster_member_info*>* new_members);

  void update_member_status(std::string uuid,
                            Cluster_member_info::Cluster_member_status
                                                                   new_status);

  void encode(std::vector<uchar>* to_encode);

  std::vector<Cluster_member_info*>* decode(uchar* to_decode);

  std::vector<uchar>* get_exchangeable_format();

private:
  void clear_members();

  std::map<std::string, Cluster_member_info*> *members;

  /*
   This field exists in order to provide a permanent reference to Exchangeable
   Data, since this is the data that is exchanged when a node joins and when
   a view changes. As such it should always be up-to-date.
   */
  std::vector<uchar> *serialized_format;
  Cluster_member_info* local_node;

  mysql_mutex_t update_lock;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key cluster_info_manager_key_mutex;
#endif
};

#endif
