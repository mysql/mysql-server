/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_GROUP_MEMBER_INFORMATION_INCLUDED
#define GCS_XCOM_GROUP_MEMBER_INFORMATION_INCLUDED

#include <stdint.h>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_connection.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_net.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

/**
  Stores connection information associated with a node.
*/
class Gcs_xcom_node_address {
 public:
  /**
    Gcs_xcom_node_address constructor.
  */

  explicit Gcs_xcom_node_address(std::string member_address);

  virtual ~Gcs_xcom_node_address();

  /**
    Return address using the format ip:port.
  */

  std::string &get_member_address();

  /**
    Return the IP address.
  */

  std::string &get_member_ip();

  /**
    Return the port number.
  */

  xcom_port get_member_port();

  /**
    Return an internal representation.
  */

  std::string *get_member_representation() const;

 private:
  /*
    Member's address.
  */
  std::string m_member_address;

  /*
    Member's IP.
  */
  std::string m_member_ip;

  /*
    Member's port.
  */
  xcom_port m_member_port;
};

/*
  Internal GCS unique identifier.
*/
class Gcs_xcom_uuid {
 public:
  /*
    Create a GCS unique identifier.
  */

  static Gcs_xcom_uuid create_uuid();

  /*
    Copies the internal buffer which is used to store a uuid to an
    external buffer. If the parameters buffer or size point to NULL,
    nothing is returned.

    @param [out] buffer storage buffer
    @param [out] size data size
    @return Whether the data was returned or not.
  */

  bool encode(uchar **buffer, unsigned int *size) const;

  /*
    Copies the external buffer to an internal buffer. If the
    parameter buffer points to NULL, nothing is returned.

    @param [in] buffer storage buffer
    @param [in] size data size
    @return Whether the data was copied or not.
  */

  bool decode(const uchar *buffer, const unsigned int size);

  /*
    Unique identifier which currently only accommodates 64 bits but
    can easily be extended to 128 bits and become a truly UUID in
    the future.
  */

  std::string actual_value;
};

/**
  @class Gcs_xcom_node_information

  It represents a node within a group and is identified by the member
  identifier, unique identifier and node number. Users are responsible
  for guaranteeing that they are related to the same node.

  One should avoid creating this representation from outside the binding,
  since each one might have its own internal representations. Instead
  one should use the Gcs_control_interface::get_local_information method
  to know our own identification within the group.

  Note also that it is possible to use the copy constructor and assignment
  operator and these properties are required by several other classes such
  as the Gcs_xcom_nodes.
 */
class Gcs_xcom_node_information {
 public:
  /**
    Gcs_xcom_node_information constructor.

    @param[in] member_id the member identifier
    @param[in] alive whether the node is alive or not.
  */

  explicit Gcs_xcom_node_information(const std::string &member_id,
                                     bool alive = true);

  /**
    Gcs_xcom_node_information constructor.

    @param[in] member_id the member identifier
    @param[in] uuid the member uuid
    @param[in] node_no the member node number
    @param[in] alive whether the node is alive or not.
  */

  explicit Gcs_xcom_node_information(const std::string &member_id,
                                     const Gcs_xcom_uuid &uuid,
                                     const unsigned int node_no,
                                     const bool alive);

  virtual ~Gcs_xcom_node_information() {}

  /**
    Sets the timestamp to indicate the creation of the suspicion.
  */

  void set_timestamp(uint64_t ts);

  /**
    Gets the timestamp that indicates the creation of the suspicion.
  */

  uint64_t get_timestamp() const;

  /**
    Compares the object's timestamp with the received one, in order
    to check if the suspicion has timed out and the suspect node
    must be removed.

    @param[in] ts Provided timestamp
    @param[in] timeout Time interval for the suspicion to timeout
  */

  bool has_timed_out(uint64_t ts, uint64_t timeout);

  /**
    @return the member identifier
  */

  const Gcs_member_identifier &get_member_id() const;

  /**
    @return the member uuid
  */

  const Gcs_xcom_uuid &get_member_uuid() const;

  /**
    Regenerate the member uuid.
  */

  void regenerate_member_uuid();

  /**
    Set the member node_no.
  */

  void set_node_no(unsigned int);

  /**
    Return member node_no.
  */

  unsigned int get_node_no() const;

  /**
    Get whether the member is alive or not.
  */

  bool is_alive() const;

 private:
  Gcs_member_identifier m_member_id;

  /**
    Member unique identifier.
  */
  Gcs_xcom_uuid m_uuid;

  /**
    Member node_no.
  */
  unsigned int m_node_no;

  /**
    Whether the member is alive or dead.
  */
  bool m_alive;

  /**
    Stores the timestamp of the creation of the suspicion.
  */
  uint64_t m_timestamp;
};

/**
  This class contains information on the configuration, i.e set of nodes
  or simply site definition.

  Users are responsible for guaranteeing that information encapsulated
  by different Gcs_xcom_nodes is properly defined. In the sense that
  the member identifier, the unique identifier and the address uniquely
  identify a node and the same holds for any combination of those three.

  Nodes inserted in this set are copied and stored in a vector object.
  Currently, we don't check whether the same node is inserted twice or
  not and as such duplicated entries are allowed. Users are responsible
  for guaranteeing that duplicated entries are not inserted.
*/
class Gcs_xcom_nodes {
 public:
  /**
    Constructor that reads the site definition and whether a node
    is considered dead or alive to build a list of addresses and
    statuses.
  */

  explicit Gcs_xcom_nodes();

  /**
    Constructor that reads the site definition and whether a node
    is considered dead or alive to build a list of addresses and
    statuses.
  */

  explicit Gcs_xcom_nodes(const site_def *site, node_set &nodes);

  /**
    Destructor for Gcs_xcom_nodes.
  */

  virtual ~Gcs_xcom_nodes();

  /*
    Set the index of the current node (i.e. member);
  */

  void set_node_no(unsigned int node_no);

  /**
    Return the index of the current node (i.e. member).
  */

  unsigned int get_node_no() const;

  /**
    Return with the configuration is valid or not.
  */

  inline bool is_valid() const {
    /*
      Unfortunately a node may get notifications even when its configuration
      inside XCOM is not properly established and this may trigger view
      changes and may lead to problems because the node is not really ready.

      We detect this fact by checking the node identification is valid.
    */
    return m_node_no != VOID_NODE_NO;
  }

  /**
    Return a reference to the addresses' vector.
  */

  const std::vector<Gcs_xcom_node_information> &get_nodes() const;

  /**
    Return a pointer to a node if it exists, otherwise NULL.
  */

  const Gcs_xcom_node_information *get_node(
      const Gcs_member_identifier &member_id) const;

  /**
    Return a pointer to a node if it exists, otherwise NULL.
  */

  const Gcs_xcom_node_information *get_node(const std::string &member_id) const;

  /**
    Return a pointer to a node if it exists, otherwise NULL.
  */

  const Gcs_xcom_node_information *get_node(unsigned int node_no) const;

  /**
    Return a pointer to a node if it exists, otherwise NULL.
  */

  const Gcs_xcom_node_information *get_node(const Gcs_xcom_uuid &uuid) const;

  /**
    Add a node to the set of nodes. Note that the method does not
    verify if the node already exists.
  */

  void add_node(const Gcs_xcom_node_information &node);

  /**
    Remove a node from the set of nodes.

    @param node Node to be removed from the set of nodes.
  */

  void remove_node(const Gcs_xcom_node_information &node);

  /**
    Clear up the current set and add a new set of nodes.

    @param xcom_nodes Set of nodes.
  */

  void add_nodes(const Gcs_xcom_nodes &xcom_nodes);

  /**
    Clear the set of nodes.
  */

  void clear_nodes();

  /**
    Return the number of nodes in the set.
  */

  unsigned int get_size() const;

  /**
    Whether the set of nodes is empty or not.
  */

  bool empty() const;

  /**
    Encode the information on the set of nodes in a format that can be
    interpreted by XCOM to boot, add or remove nodes.
  */

  bool encode(unsigned int *ptr_size, char ***ptr_addrs, blob **ptr_uuids);

 private:
  /*
    Free memory allocated to encode the object.
  */

  void free_encode();

  /*
    Number of the current node which is used as an index to
    the other data structures.
  */
  unsigned int m_node_no;

  /*
    List of nodes known by the group communication.
  */
  std::vector<Gcs_xcom_node_information> m_nodes;

  /*
    The size of the lists.
  */
  unsigned int m_size;

  /*
    Memory allocated to encode addresses.
  */
  char **m_addrs;

  /*
    Memory allocated to encode uuids.
  */
  blob *m_uuids;

 private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_nodes(const Gcs_xcom_nodes &);
  Gcs_xcom_nodes &operator=(const Gcs_xcom_nodes &);
};
#endif  // GCS_XCOM_GROUP_MEMBER_INFORMATION_INCLUDED
