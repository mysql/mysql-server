/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"

#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_proxy.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"

Gcs_xcom_node_address::Gcs_xcom_node_address(std::string member_address)
    : m_member_address(member_address), m_member_ip(), m_member_port(0) {
  char address[IP_MAX_SIZE];
  xcom_port port;

  int error = get_ip_and_port(const_cast<char *>(member_address.c_str()),
                              address, &port);
  if (!error) {
    m_member_ip.append(address);
    m_member_port = port;
  }
}

std::string &Gcs_xcom_node_address::get_member_address() {
  return m_member_address;
}

std::string &Gcs_xcom_node_address::get_member_ip() { return m_member_ip; }

xcom_port Gcs_xcom_node_address::get_member_port() { return m_member_port; }

std::string *Gcs_xcom_node_address::get_member_representation() const {
  return new std::string(m_member_address);
}

bool Gcs_xcom_node_address::is_valid() const {
  return !m_member_ip.empty() && m_member_port != 0;
}

Gcs_xcom_node_address::~Gcs_xcom_node_address() {}

Gcs_xcom_node_information::Gcs_xcom_node_information(
    const std::string &member_id, bool alive)
    : m_member_id(member_id),
      m_uuid(Gcs_xcom_uuid::create_uuid()),
      m_node_no(VOID_NODE_NO),
      m_alive(alive),
      m_member(false),
      m_suspicion_creation_timestamp(0),
      m_lost_messages(false),
      m_max_synode(null_synode) {}

Gcs_xcom_node_information::Gcs_xcom_node_information(
    const std::string &member_id, const Gcs_xcom_uuid &uuid,
    const unsigned int node_no, const bool alive)
    : m_member_id(member_id),
      m_uuid(uuid),
      m_node_no(node_no),
      m_alive(alive),
      m_member(false),
      m_suspicion_creation_timestamp(0),
      m_lost_messages(false),
      m_max_synode(null_synode) {}

void Gcs_xcom_node_information::set_suspicion_creation_timestamp(uint64_t ts) {
  m_suspicion_creation_timestamp = ts;
}

/* purecov: begin tested */
uint64_t Gcs_xcom_node_information::get_suspicion_creation_timestamp() const {
  return m_suspicion_creation_timestamp;
}
/* purecov: end */

const Gcs_member_identifier &Gcs_xcom_node_information::get_member_id() const {
  return m_member_id;
}

const Gcs_xcom_uuid &Gcs_xcom_node_information::get_member_uuid() const {
  return m_uuid;
}

void Gcs_xcom_node_information::regenerate_member_uuid() {
  m_uuid = Gcs_xcom_uuid::create_uuid();
}

/* purecov: begin tested */
void Gcs_xcom_node_information::set_node_no(unsigned int node_no) {
  m_node_no = node_no;
}
/* purecov: end */

unsigned int Gcs_xcom_node_information::get_node_no() const {
  return m_node_no;
}

bool Gcs_xcom_node_information::is_alive() const { return m_alive; }

bool Gcs_xcom_node_information::is_member() const { return m_member; }

void Gcs_xcom_node_information::set_member(bool m) { m_member = m; }

std::pair<bool, node_address *> Gcs_xcom_node_information::make_xcom_identity(
    Gcs_xcom_proxy &xcom_proxy) const {
  bool constexpr kError = true;
  bool constexpr kSuccess = false;

  bool error_code = kError;
  node_address *xcom_identity = nullptr;

  /* Get our unique XCom identifier to pass it along to XCom. */
  // Address.
  const std::string &address_str = get_member_id().get_member_id();
  char *address[] = {const_cast<char *>(address_str.c_str())};
  // Incarnation.
  bool error_creating_blob;
  blob incarnation_blob;
  std::tie(error_creating_blob, incarnation_blob) =
      get_member_uuid().make_xcom_blob();

  if (!error_creating_blob) {
    blob incarnation[] = {incarnation_blob};
    xcom_identity = xcom_proxy.new_node_address_uuid(1, address, incarnation);
    std::free(incarnation_blob.data.data_val);
    error_code = kSuccess;
  }

  return {error_code, xcom_identity};
}

bool Gcs_xcom_node_information::has_timed_out(uint64_t now_ts,
                                              uint64_t timeout) {
  return (m_suspicion_creation_timestamp + timeout) < now_ts;
}

bool Gcs_xcom_node_information::has_lost_messages() const {
  return m_lost_messages;
}

void Gcs_xcom_node_information::set_lost_messages(bool lost_msgs) {
  m_lost_messages = lost_msgs;
}

synode_no Gcs_xcom_node_information::get_max_synode() const {
  return m_max_synode;
}

void Gcs_xcom_node_information::set_max_synode(synode_no synode) {
  m_max_synode = synode;
}

Gcs_xcom_uuid Gcs_xcom_uuid::create_uuid() {
  Gcs_xcom_uuid uuid;
  std::ostringstream ss;
  /* Although it is possible to have collisions if different nodes create
     the same UUID, this is not a problem because the UUID is only used to
     distinguish two situations:

       . whether someone is trying to remove a newer node's incarnation.

       . whether a new node's incarnation is trying to rejoin a group when
         there are still references to its old incarnation.

     So although there might be collissions, this is not a problem because
     the actual node's identification is the combination of address and
     UUID.

     Note that, whatever the UUID is, we have to guarantee that successive
     node's incarnation don't have the same UUID.

     Our current solution uses a simple timestamp which is safe because it
     is very unlikely that the same node will be able to join, fail/leave
     and rejoin again and will keep the same uuid.

     In the future, we can start generating real UUIDs if we need them for
     any reason. The server already has the code to do it, so we could make
     this an option and pass the information to GCS.
  */
  uint64_t value = My_xp_util::getsystime();

  ss << value;
  uuid.actual_value = ss.str();

  return uuid;
}

bool Gcs_xcom_uuid::encode(uchar **buffer, unsigned int *size) const {
  if (buffer == NULL || *buffer == NULL || size == NULL) {
    /* purecov: begin tested */
    return false;
    /* purecov: end */
  }

  memcpy(*buffer, actual_value.c_str(), actual_value.size());
  *size = actual_value.size();

  return true;
}

bool Gcs_xcom_uuid::decode(const uchar *buffer, const unsigned int size) {
  if (buffer == NULL) {
    /* purecov: begin tested */
    return false;
    /* purecov: end */
  }

  actual_value = std::string(reinterpret_cast<const char *>(buffer),
                             static_cast<size_t>(size));

  return true;
}

std::pair<bool, blob> Gcs_xcom_uuid::make_xcom_blob() const {
  bool constexpr kError = true;
  bool constexpr kSuccess = false;
  bool error_code = kError;

  blob incarnation;
  incarnation.data.data_len = actual_value.size();
  incarnation.data.data_val =
      reinterpret_cast<char *>(std::malloc(incarnation.data.data_len));
  if (incarnation.data.data_val == nullptr) goto end;

  encode(reinterpret_cast<uchar **>(&incarnation.data.data_val),
         &incarnation.data.data_len);
  error_code = kSuccess;

end:
  return {error_code, incarnation};
}

Gcs_xcom_nodes::Gcs_xcom_nodes()
    : m_node_no(VOID_NODE_NO),
      m_nodes(),
      m_size(0),
      m_addrs(NULL),
      m_uuids(NULL) {}

Gcs_xcom_nodes::Gcs_xcom_nodes(const site_def *site, node_set &nodes)
    : m_node_no(site->nodeno),
      m_nodes(),
      m_size(nodes.node_set_len),
      m_addrs(NULL),
      m_uuids(NULL) {
  Gcs_xcom_uuid uuid;

  for (unsigned int i = 0; i < nodes.node_set_len; ++i) {
    /* Get member address and save it. */
    std::string address(site->nodes.node_list_val[i].address);

    /* Get member uuid and save it. */
    uuid.decode(reinterpret_cast<uchar *>(
                    site->nodes.node_list_val[i].uuid.data.data_val),
                site->nodes.node_list_val[i].uuid.data.data_len);

    /* Get member status and save it */
    bool alive = nodes.node_set_val[i] ? true : false;

    Gcs_xcom_node_information node(address, uuid, i, alive);

    m_nodes.push_back(node);
  }
  assert(m_size == m_nodes.size());
}

Gcs_xcom_nodes::~Gcs_xcom_nodes() { free_encode(); }

/* purecov: begin tested */
void Gcs_xcom_nodes::set_node_no(unsigned int node_no) { m_node_no = node_no; }
/* purecov: end */

/* purecov: begin tested */
unsigned int Gcs_xcom_nodes::get_node_no() const { return m_node_no; }
/* purecov: end */

const std::vector<Gcs_xcom_node_information> &Gcs_xcom_nodes::get_nodes()
    const {
  return m_nodes;
}

const Gcs_xcom_node_information *Gcs_xcom_nodes::get_node(
    const Gcs_member_identifier &member_id) const {
  return get_node(member_id.get_member_id());
}

const Gcs_xcom_node_information *Gcs_xcom_nodes::get_node(
    const std::string &member_id) const {
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
  for (nodes_it = m_nodes.begin(); nodes_it != m_nodes.end(); ++nodes_it) {
    if ((*nodes_it).get_member_id().get_member_id() == member_id)
      return &(*nodes_it);
  }

  return NULL;
}

const Gcs_xcom_node_information *Gcs_xcom_nodes::get_node(
    unsigned int node_no) const {
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
  for (nodes_it = m_nodes.begin(); nodes_it != m_nodes.end(); ++nodes_it) {
    if ((*nodes_it).get_node_no() == node_no) return &(*nodes_it);
  }

  return NULL; /* purecov: tested */
}

/* purecov: begin tested */
const Gcs_xcom_node_information *Gcs_xcom_nodes::get_node(
    const Gcs_xcom_uuid &uuid) const {
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
  for (nodes_it = m_nodes.begin(); nodes_it != m_nodes.end(); ++nodes_it) {
    if ((*nodes_it).get_member_uuid().actual_value == uuid.actual_value)
      return &(*nodes_it);
  }

  return NULL;
}
/* purecov: end */

unsigned int Gcs_xcom_nodes::get_size() const { return m_size; }

bool Gcs_xcom_nodes::empty() const { return m_size == 0; }

void Gcs_xcom_nodes::add_node(const Gcs_xcom_node_information &node) {
  m_nodes.push_back(node);
  m_size++;
}

void Gcs_xcom_nodes::remove_node(const Gcs_xcom_node_information &node) {
  std::vector<Gcs_xcom_node_information>::iterator nodes_it;

  for (nodes_it = m_nodes.begin(); nodes_it != m_nodes.end(); ++nodes_it) {
    if ((*nodes_it).get_member_id() == node.get_member_id()) {
      m_size--;
      m_nodes.erase(nodes_it);
      return;
    }
  }
}

void Gcs_xcom_nodes::add_nodes(const Gcs_xcom_nodes &xcom_nodes) {
  const std::vector<Gcs_xcom_node_information> &nodes = xcom_nodes.get_nodes();
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;

  clear_nodes();
  for (nodes_it = nodes.begin(); nodes_it != nodes.end(); ++nodes_it) {
    add_node(*(nodes_it));
  }
}

void Gcs_xcom_nodes::clear_nodes() {
  m_nodes.clear();
  m_size = 0;
}

bool Gcs_xcom_nodes::encode(unsigned int *ptr_size, char ***ptr_addrs,
                            blob **ptr_uuids) {
  /*
    If there is information already encoded, free it first.
  */
  if (m_addrs != NULL || m_uuids != NULL) {
    /* purecov: begin tested */
    free_encode();
    /* purecov: end */
  }

  m_addrs = static_cast<char **>(malloc(m_size * sizeof(char *)));
  m_uuids = static_cast<blob *>(malloc(m_size * sizeof(blob)));

  /*
    If memory was not successfuly allocated, an error is
    reported.
  */
  if ((m_addrs == NULL) || (m_uuids == NULL)) {
    /* purecov: begin deadcode */
    free_encode();
    return false;
    /* purecov: end */
  }

  unsigned int i = 0;
  size_t uuid_size = 0;
  std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
  for (nodes_it = m_nodes.begin(); nodes_it != m_nodes.end(); i++, ++nodes_it) {
    m_addrs[i] =
        const_cast<char *>((*nodes_it).get_member_id().get_member_id().c_str());
    uuid_size = (*nodes_it).get_member_uuid().actual_value.size();
    m_uuids[i].data.data_val = static_cast<char *>(malloc(uuid_size));
    (*nodes_it).get_member_uuid().encode(
        reinterpret_cast<uchar **>(&m_uuids[i].data.data_val),
        &m_uuids[i].data.data_len);
    assert(m_uuids[i].data.data_len == uuid_size);

    MYSQL_GCS_LOG_TRACE("Node[%d]=(address=%s), (uuid=%s)", i, m_addrs[i],
                        (*nodes_it).get_member_uuid().actual_value.c_str());
  }

  *ptr_size = m_size;
  *ptr_addrs = m_addrs;
  *ptr_uuids = m_uuids;

  return true;
}

void Gcs_xcom_nodes::free_encode() {
  unsigned int i = 0;

  if (m_uuids != NULL) {
    for (; i < m_size; i++) {
      free(m_uuids[i].data.data_val);
    }
  }

  free(m_addrs);
  free(m_uuids);

  m_addrs = NULL;
  m_uuids = NULL;
}
