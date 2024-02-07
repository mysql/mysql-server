/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"

#include <algorithm>  // std::min
#include <cassert>
#include <ctime>
#include <iterator>
#include <limits>
#include <unordered_set>  // std::unordered_set

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"  // gcs_protocol_to_mysql_version

Xcom_member_state::Xcom_member_state(const Gcs_xcom_view_identifier &view_id,
                                     synode_no configuration_id,
                                     Gcs_protocol_version version,
                                     const Gcs_xcom_synode_set &snapshot,
                                     const uchar *data, uint64_t data_size)
    : m_view_id(nullptr),
      m_configuration_id(configuration_id),
      m_data(nullptr),
      m_data_size(0),
      m_snapshot(snapshot),
      m_version(version) {
  m_view_id = new Gcs_xcom_view_identifier(view_id.get_fixed_part(),
                                           view_id.get_monotonic_part());

  if (data_size != 0) {
    m_data_size = data_size;
    m_data = static_cast<uchar *>(malloc(sizeof(uchar) * m_data_size));
    memcpy(m_data, data, m_data_size);
  }
}

Xcom_member_state::Xcom_member_state(Gcs_protocol_version version,
                                     const uchar *data, uint64_t data_size)
    : m_view_id(nullptr),
      m_configuration_id(null_synode),
      m_data(nullptr),
      m_data_size(0),
      m_version(version) {
  decode(data, data_size);
}

Xcom_member_state::~Xcom_member_state() {
  delete m_view_id;
  free(m_data);
}

uint64_t Xcom_member_state::get_encode_payload_size() const {
  return m_data_size;
}

uint64_t Xcom_member_state::get_encode_snapshot_size() const {
  uint64_t snapshot_size = 0;

  if (m_version == Gcs_protocol_version::V1) {
    snapshot_size = 0;
  } else if (m_version >= Gcs_protocol_version::V2) {
    snapshot_size = get_encode_snapshot_elem_size() * m_snapshot.size() +
                    WIRE_XCOM_SNAPSHOT_NR_ELEMS_SIZE;
  }

  return snapshot_size;
}
extern uint32_t get_my_xcom_id();

bool Xcom_member_state::encode_header(uchar *buffer,
                                      uint64_t *buffer_len) const {
  uint64_t fixed_view_id = 0;
  uint32_t monotonic_view_id = 0;
  uint32_t group_id = 0;
  uint64_t msg_no = 0;
  uint32_t node_no = 0;
  uint64_t encoded_size = get_encode_header_size();
  unsigned char *slider = buffer;

  MYSQL_GCS_LOG_TRACE("xcom_id %x Encoding header for exchangeable data.",
                      get_my_xcom_id())

  if (buffer == nullptr || buffer_len == nullptr) {
    MYSQL_GCS_LOG_ERROR(
        "Buffer to return information on encoded data or encoded data "
        "size is not properly configured.");
    return true;
  }

  if (*buffer_len < encoded_size) {
    MYSQL_GCS_LOG_ERROR("Buffer reserved capacity is "
                        << *buffer_len
                        << " but it has "
                           "been requested to add data whose size is "
                        << encoded_size);
    return true;
  }

  *buffer_len = encoded_size;

  if (m_view_id != nullptr) {
    fixed_view_id = htole64(m_view_id->get_fixed_part());
    monotonic_view_id = htole32(m_view_id->get_monotonic_part());
  }
  memcpy(slider, &fixed_view_id, WIRE_XCOM_VARIABLE_VIEW_ID_SIZE);
  slider += WIRE_XCOM_VARIABLE_VIEW_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  memcpy(slider, &monotonic_view_id, WIRE_XCOM_VIEW_ID_SIZE);
  slider += WIRE_XCOM_VIEW_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  group_id = htole32(m_configuration_id.group_id);
  memcpy(slider, &group_id, WIRE_XCOM_GROUP_ID_SIZE);
  slider += WIRE_XCOM_GROUP_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  msg_no = htole64(m_configuration_id.msgno);
  memcpy(slider, &msg_no, WIRE_XCOM_MSG_ID_SIZE);
  slider += WIRE_XCOM_MSG_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  node_no = htole32(m_configuration_id.node);
  memcpy(slider, &node_no, WIRE_XCOM_NODE_ID_SIZE);
  slider += WIRE_XCOM_NODE_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) == encoded_size);

  MYSQL_GCS_LOG_TRACE(
      "xcom_id %x Encoded header for exchangeable data: (header)=%llu view_id "
      "%s",
      get_my_xcom_id(), static_cast<long long unsigned>(encoded_size),
      m_view_id->get_representation().c_str());

  return false;
}

bool Xcom_member_state::encode_snapshot(uchar *buffer,
                                        uint64_t *buffer_len) const {
  uint64_t encoded_size = get_encode_snapshot_size();
  unsigned char *slider = buffer;
  uint64_t nr_synods = 0;

  /* There is no snapshot information on protocol V1. */
  if (m_version == Gcs_protocol_version::V1) goto end;

  MYSQL_GCS_LOG_TRACE("xcom_id %x Encoding snapshot for exchangeable data.",
                      get_my_xcom_id())

  if (buffer == nullptr || buffer_len == nullptr) {
    MYSQL_GCS_LOG_ERROR(
        "Buffer to return information on encoded data or encoded data "
        "size is not properly configured.");
    return true;
  }

  if (*buffer_len < encoded_size) {
    MYSQL_GCS_LOG_ERROR("Buffer reserved capacity is "
                        << *buffer_len
                        << " but it has "
                           "been requested to add data whose size is "
                        << encoded_size);
    return true;
  }

  *buffer_len = encoded_size;

  for (auto const &gcs_synod : m_snapshot) {
    uint64_t msgno = htole64(gcs_synod.get_synod().msgno);
    std::memcpy(slider, &msgno, WIRE_XCOM_MSG_ID_SIZE);
    slider += WIRE_XCOM_MSG_ID_SIZE;

    uint32_t nodeno = htole32(gcs_synod.get_synod().node);
    std::memcpy(slider, &nodeno, WIRE_XCOM_NODE_ID_SIZE);
    slider += WIRE_XCOM_NODE_ID_SIZE;
  }

  nr_synods = htole64(m_snapshot.size());
  std::memcpy(slider, &nr_synods, WIRE_XCOM_SNAPSHOT_NR_ELEMS_SIZE);

end:
  return false;
}

bool Xcom_member_state::decode_header(const uchar *buffer, uint64_t) {
  uint64_t fixed_view_id = 0;
  uint32_t monotonic_view_id = 0;
  uint32_t group_id = 0;
  uint64_t msg_no = 0;
  uint32_t node_no = 0;

  const uchar *slider = buffer;

  memcpy(&fixed_view_id, slider, WIRE_XCOM_VARIABLE_VIEW_ID_SIZE);
  fixed_view_id = le64toh(fixed_view_id);
  slider += WIRE_XCOM_VARIABLE_VIEW_ID_SIZE;

  memcpy(&monotonic_view_id, slider, WIRE_XCOM_VIEW_ID_SIZE);
  monotonic_view_id = le32toh(monotonic_view_id);
  slider += WIRE_XCOM_VIEW_ID_SIZE;

  m_view_id = new Gcs_xcom_view_identifier(fixed_view_id, monotonic_view_id);

  memcpy(&group_id, slider, WIRE_XCOM_GROUP_ID_SIZE);
  m_configuration_id.group_id = le32toh(group_id);
  slider += WIRE_XCOM_GROUP_ID_SIZE;

  memcpy(&msg_no, slider, WIRE_XCOM_MSG_ID_SIZE);
  m_configuration_id.msgno = le64toh(msg_no);
  slider += WIRE_XCOM_MSG_ID_SIZE;

  memcpy(&node_no, slider, WIRE_XCOM_NODE_ID_SIZE);
  m_configuration_id.node = le32toh(node_no);

  return true;
}

bool Xcom_member_state::decode_snapshot(const uchar *buffer,
                                        uint64_t buffer_size) {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  bool result = ERROR;

  if (m_version == Gcs_protocol_version::V1) {
    // This message doesn't contain a snapshot
    result = OK;
  } else if (m_version >= Gcs_protocol_version::V2) {
    // End of the buffer.
    const uchar *slider = (buffer + buffer_size);

    slider -= WIRE_XCOM_SNAPSHOT_NR_ELEMS_SIZE;
    uint64_t nr_synods = 0;
    std::memcpy(&nr_synods, slider, WIRE_XCOM_SNAPSHOT_NR_ELEMS_SIZE);
    nr_synods = le64toh(nr_synods);

    for (uint64_t i = 0; i < nr_synods; i++) {
      slider -= WIRE_XCOM_NODE_ID_SIZE;
      uint32_t node = 0;
      std::memcpy(&node, slider, WIRE_XCOM_NODE_ID_SIZE);
      node = le32toh(node);

      slider -= WIRE_XCOM_MSG_ID_SIZE;
      uint64_t msgno = 0;
      memcpy(&msgno, slider, WIRE_XCOM_MSG_ID_SIZE);
      msgno = le64toh(msgno);

      synode_no synod;
      synod.group_id = m_configuration_id.group_id;
      synod.msgno = msgno;
      synod.node = node;
      m_snapshot.insert(Gcs_xcom_synode(synod));
    }

    result = OK;
  }

  return result;
}

bool Xcom_member_state::decode(const uchar *data, uint64_t data_size) {
  const uchar *slider = data;
  decode_header(slider, data_size);
  uint64_t exchangeable_header_size = get_encode_header_size();
  slider += exchangeable_header_size;

  decode_snapshot(data, data_size);
  uint64_t snapshot_size = get_encode_snapshot_size();

  uint64_t exchangeable_data_size =
      data_size - exchangeable_header_size - snapshot_size;

  if (exchangeable_data_size != 0) {
    m_data_size = exchangeable_data_size;
    m_data = static_cast<uchar *>(malloc(sizeof(uchar) * m_data_size));
    memcpy(m_data, slider, m_data_size);
  }

  MYSQL_GCS_LOG_TRACE(
      "Decoded header, snapshot and payload for exchageable data: "
      "(header)=%llu (payload)=%llu (snapshot)=%llu",
      static_cast<long long unsigned>(exchangeable_header_size),
      static_cast<long long unsigned>(exchangeable_data_size),
      static_cast<long long unsigned>(snapshot_size));

  return false;
}

Gcs_xcom_state_exchange::Gcs_xcom_state_exchange(
    Gcs_communication_interface *comm)
    : m_broadcaster(comm),
      m_awaited_vector(),
      m_recover_vector(),
      m_ms_total(),
      m_ms_left(),
      m_ms_joined(),
      m_member_states(),
      m_member_versions(),
      m_member_max_versions(),
      m_group_name(nullptr),
      m_local_information("none"),
      m_configuration_id(null_synode),
      m_ms_xcom_nodes() {}

Gcs_xcom_state_exchange::~Gcs_xcom_state_exchange() {
  Gcs_xcom_communication_interface *binding_broadcaster =
      static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  binding_broadcaster->cleanup_buffered_packets();

  reset();
}

void Gcs_xcom_state_exchange::init() {}

void Gcs_xcom_state_exchange::reset_with_flush() {
  Gcs_xcom_communication_interface *binding_broadcaster =
      static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  /*
    If the state exchange is restarted, this possibly mean that a new
    global view was delivered by XCOM. If the current node is joining
    the cluster, any buffered message must be discarded. On the other
    hand, nodes that are already part of the group must have any
    buffered message delivered.

    The idea here is to guarantee that messages from an old view are
    are not delivered in a new view. Note however that the concept of
    view here is loose due to the asynchronous behavior introduced by
    the global view message in XCOM.
  */
  if (is_joining()) {
    binding_broadcaster->cleanup_buffered_packets();
  } else {
    binding_broadcaster->deliver_buffered_packets();
  }

  reset();
}

void Gcs_xcom_state_exchange::reset() {
  assert(static_cast<Gcs_xcom_communication_interface *>(m_broadcaster)
             ->number_buffered_packets() == 0);

  m_configuration_id = null_synode;

  std::set<Gcs_member_identifier *>::iterator member_it;

  for (member_it = m_ms_total.begin(); member_it != m_ms_total.end();
       member_it++)
    delete (*member_it);
  m_ms_total.clear();

  for (member_it = m_ms_left.begin(); member_it != m_ms_left.end(); member_it++)
    delete (*member_it);
  m_ms_left.clear();

  for (member_it = m_ms_joined.begin(); member_it != m_ms_joined.end();
       member_it++)
    delete (*member_it);
  m_ms_joined.clear();

  std::map<Gcs_member_identifier, Xcom_member_state *>::iterator state_it;
  for (state_it = m_member_states.begin(); state_it != m_member_states.end();
       state_it++)
    delete (*state_it).second;
  m_member_states.clear();

  m_member_versions.clear();
  m_member_max_versions.clear();

  m_awaited_vector.clear();

  delete m_group_name;
  m_group_name = nullptr;

  m_ms_xcom_nodes.clear_nodes();
}

void Gcs_xcom_state_exchange::end() {
  Gcs_xcom_communication_interface *binding_broadcaster =
      static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  binding_broadcaster->deliver_buffered_packets();

  reset();
}

bool Gcs_xcom_state_exchange::state_exchange(
    synode_no configuration_id, std::vector<Gcs_member_identifier *> &total,
    std::vector<Gcs_member_identifier *> &left,
    std::vector<Gcs_member_identifier *> &joined,
    std::vector<std::unique_ptr<Gcs_message_data>> &exchangeable_data,
    Gcs_view *current_view, std::string *group,
    const Gcs_member_identifier &local_info, const Gcs_xcom_nodes &xcom_nodes) {
  uint64_t fixed_part = 0;
  uint32_t monotonic_part = 0;

  /* Keep track of when the view was internally delivered. */
  m_configuration_id = configuration_id;

  /* Store member state for later broadcast */
  m_local_information = local_info;

  /*
   If the view shall be installed, the communication system has to find out the
   nodes that belong to it in order to build the map to reconstruct possible
   sliced messages.

   Note this is the only point in the code where this information should be
   conveyed to the communication system because state exchange messages may
   only be compressed and don't go through any other communication stage in
   the communication pipeline.
   */
  update_communication_channel(xcom_nodes);

  if (m_group_name == nullptr) m_group_name = new std::string(*group);

  if (current_view != nullptr) {
    /*
      If a view has been already installed, disseminate this information
      to other members so that a member that is joining may learn about
      it. Please, check ::get_new_view_id to find out how the view is
      chosen.
    */
    const Gcs_xcom_view_identifier &xcom_view_id =
        static_cast<const Gcs_xcom_view_identifier &>(
            current_view->get_view_id());
    fixed_part = xcom_view_id.get_fixed_part();
    monotonic_part = xcom_view_id.get_monotonic_part();
  } else {
    /*
      The member has not installed any view yet and is joining the group.
      In this case, a random view is always chosen regardless whether the
      group has been bootstrapped already or not.

      If the member is bootstrapping the group, the random view will be
      chosen and will be used for future views. Otherwise, the member will
      eventually learn about the current view installed by older members
      and will use it.

      Please, check ::get_new_view_id to find out how the view is chosen.

      Note that in some (old) platforms that do not have high resolution
      timers we default to rand.
    */
    uint64_t ts = My_xp_util::getsystime();
    fixed_part = ((ts == 0) ? static_cast<uint64_t>(rand())
                            : (ts + static_cast<uint64_t>((rand()) % 1000)));
    monotonic_part = 0;
  }
  Gcs_xcom_view_identifier proposed_view(fixed_part, monotonic_part);

  fill_member_set(total, m_ms_total);
  fill_member_set(joined, m_ms_joined);
  fill_member_set(left, m_ms_left);
  m_ms_xcom_nodes.add_nodes(xcom_nodes);

  /*
    Calculate if i am leaving...
    If so, SE will be interrupted and it will return true...
  */
  bool leaving = is_leaving();

  if (!leaving) {
    update_awaited_vector();
    broadcast_state(proposed_view, exchangeable_data);
  }

  return leaving;
}

/* purecov: begin deadcode */
bool Gcs_xcom_state_exchange::is_joining() {
  bool is_joining = false;

  std::set<Gcs_member_identifier *>::iterator it;

  for (it = m_ms_joined.begin(); it != m_ms_joined.end() && !is_joining; it++)
    is_joining = (*(*it) == m_local_information);

  return is_joining;
}
/* purecov: end */

bool Gcs_xcom_state_exchange::is_leaving() {
  bool is_leaving = false;

  std::set<Gcs_member_identifier *>::iterator it;

  for (it = m_ms_left.begin(); it != m_ms_left.end() && !is_leaving; it++)
    is_leaving = (*(*it) == m_local_information);

  return is_leaving;
}

enum_gcs_error Gcs_xcom_state_exchange::broadcast_state(
    const Gcs_xcom_view_identifier &proposed_view,
    std::vector<std::unique_ptr<Gcs_message_data>> &exchangeable_data) {
  uchar *buffer = nullptr;
  uchar *slider = nullptr;
  uint64_t buffer_len = 0;
  uint64_t exchangeable_header_len = 0;
  uint64_t exchangeable_data_len = 0;
  uint64_t exchangeable_snapshot_len = 0;

  Gcs_xcom_communication_interface *xcom_communication =
      static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  Gcs_xcom_synode_set snapshot =
      xcom_communication->get_msg_pipeline().get_snapshot();
  Xcom_member_state member_state(proposed_view, m_configuration_id,
                                 Gcs_protocol_version::HIGHEST_KNOWN, snapshot,
                                 nullptr, 0);

  /*
    The exchangeable_data may have a list with Gcs_message_data
    and the following code gets the size of the data that will
    be sent to peers.

    This will be changed in the future so that there will be
    only a single piece of data.
  */
  auto it_ends = exchangeable_data.end();
  for (auto it = exchangeable_data.begin(); it != it_ends; ++it) {
    auto &msg_data = (*it);
    exchangeable_data_len += msg_data ? msg_data->get_encode_size() : 0;
  }
  /*
    This returns the size of the header that will compose the
    message.
  */
  exchangeable_header_len = member_state.get_encode_header_size();

  /*
    This returns the size of snapshot information that will
    compose the message.
   */
  exchangeable_snapshot_len = member_state.get_encode_snapshot_size();

  /*
    Allocate a buffer that will contain the header, the data, and the packet
    recovery snapshot.
  */
  MYSQL_GCS_LOG_TRACE(
      "Allocating buffer to carry exchangeable data: (header)=%llu "
      "(payload)=%llu (snapshot)=%llu",
      static_cast<long long unsigned>(exchangeable_header_len),
      static_cast<long long unsigned>(exchangeable_data_len),
      static_cast<long long unsigned>(exchangeable_snapshot_len));
  buffer_len = exchangeable_header_len + exchangeable_data_len +
               exchangeable_snapshot_len;
  buffer = slider = static_cast<uchar *>(malloc(buffer_len * sizeof(uchar)));
  if (buffer == nullptr) {
    MYSQL_GCS_LOG_ERROR("Error allocating buffer to carry exchangeable data")
    return GCS_NOK;
  }

  /*
   Serialize the state exchange message.

   Its wire format is:

       +--------+------------------+----------+
       | header | upper-layer data | snapshot |
       +--------+------------------+----------+

   For more context, see Xcom_member_state.
   */
  member_state.encode_header(slider, &exchangeable_header_len);
  slider += exchangeable_header_len;

  /*
   Note that the size of the list may be empty and this means that the node has
   nothing to exchange during a view change.
   However, it will send an empty message anyway.
   */
  if (exchangeable_data_len > 0) {
    uint64_t slider_len = 0;
    for (auto it = exchangeable_data.begin(); it != it_ends; ++it) {
      auto &msg_data = (*it);

      if (msg_data != nullptr) {
        slider_len = msg_data->get_encode_size();
        MYSQL_GCS_LOG_TRACE(
            "Populating payload for exchangeable data: (payload)=%llu",
            static_cast<long long unsigned>(slider_len));
        msg_data->encode(slider, &slider_len);
        slider += slider_len;
      }
    }
  }

  member_state.encode_snapshot(slider, &exchangeable_snapshot_len);

  /*
    There is another copy here but we cannot avoid this right now
    since the other other stacks further down that are expecting
    this.
  */
  MYSQL_GCS_LOG_TRACE(
      "Creating message to carry exchangeable data: (payload)=%llu",
      static_cast<long long unsigned>(buffer_len));
  Gcs_message_data *message_data = new Gcs_message_data(0, buffer_len);
  message_data->append_to_payload(buffer, buffer_len);
  free(buffer);
  buffer = nullptr;

  Gcs_group_identifier group_id(*m_group_name);
  Gcs_message message(m_local_information, group_id, message_data);

  unsigned long long message_length = 0;
  return xcom_communication->do_send_message(
      message, &message_length, Cargo_type::CT_INTERNAL_STATE_EXCHANGE);
}

void Gcs_xcom_state_exchange::update_awaited_vector() {
  std::set<Gcs_member_identifier *>::iterator it;
  Gcs_member_identifier *p_id;

  it = m_ms_total.begin();
  while (it != m_ms_total.end()) {
    p_id = *it;
    m_awaited_vector[*p_id]++;
    ++it;
  }

  it = m_ms_left.begin();
  while (it != m_ms_left.end()) {
    p_id = *it;
    m_awaited_vector.erase(*p_id);
    ++it;
  }
}

void Gcs_xcom_state_exchange::update_communication_channel(
    const Gcs_xcom_nodes &xcom_nodes) {
  Gcs_xcom_communication_interface *xcom_communication =
      static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  xcom_communication->update_members_information(m_local_information,
                                                 xcom_nodes);
}

bool Gcs_xcom_state_exchange::process_member_state(
    Xcom_member_state *ms_info, const Gcs_member_identifier &p_id,
    Gcs_protocol_version maximum_supported_protocol_version,
    Gcs_protocol_version used_protocol_version) {
  /*
    A state exchange message just arrived and we will only consider it
    if its configuration identifier matches the one expected by the
    current state exchange phase. If this does not happen the message
    is discarded.
  */
  if (!synode_eq(ms_info->get_configuration_id(), m_configuration_id)) {
    MYSQL_GCS_DEBUG_EXECUTE(
        synode_no configuration_id = ms_info->get_configuration_id();
        MYSQL_GCS_LOG_DEBUG(
            "Ignoring exchangeable data because its from a previous state "
            "exchange phase. Message is from group_id(%d), msg_no(%llu), "
            "node_no(%d) but current phase is group_id(%d), msg_no(%llu), "
            "node_no(%d). ",
            configuration_id.group_id,
            static_cast<long long unsigned>(configuration_id.msgno),
            configuration_id.node, m_configuration_id.group_id,
            static_cast<long long unsigned>(m_configuration_id.msgno),
            m_configuration_id.node));
    /*
     * ms_info will leak if we don't delete it here.
     * If this branch is not taken, m_member_states takes ownership of the
     * pointer below.
     */
    delete ms_info;
    return false;
  }

  /*
   Save the protocol version in use and state exchange per member.
   */
  save_member_state(ms_info, p_id, maximum_supported_protocol_version,
                    used_protocol_version);

  /*
    The rule of updating the awaited_vector at receiving is simply to
    decrement the counter in the right index. When the value drops to
    zero the index is discarded from the vector.

    Installation goes into terminal phase when all expected state
    messages have arrived which is indicated by the empty vector.
  */
  if (m_awaited_vector.find(p_id) != m_awaited_vector.end()) {
    m_awaited_vector.erase(p_id);
  }

  bool can_install_view = (m_awaited_vector.size() == 0);

  return can_install_view;
}

std::vector<Gcs_xcom_node_information>
Gcs_xcom_state_exchange::compute_incompatible_members() {
  std::vector<Gcs_xcom_node_information> incompatible_members;

  auto &me = m_local_information;

  /* Check if I am joining the group. */
  auto const it = std::find_if(
      m_ms_joined.begin(), m_ms_joined.end(),
      [&me](Gcs_member_identifier const *joining) { return *joining == me; });

  bool const i_am_joining = (it != m_ms_joined.end());

  /*
   If I am an existing member, identify the joining nodes with incompatible
   protocol versions.

   If I am joining, proactively check whether any of the other nodes has a
   different protocol version.
   If so, remove myself from the group.
   If the group is comprised of older nodes, none of them will remove me, so I
   have to do it myself.
   */
  if (!i_am_joining) {
    incompatible_members = compute_incompatible_joiners();

  } else if (incompatible_with_group()) {
    incompatible_members.push_back(*m_ms_xcom_nodes.get_node(me));
  }

  return incompatible_members;
}

std::pair<bool, Gcs_protocol_version>
Gcs_xcom_state_exchange::members_announce_same_version() const {
  /* Validate preconditions. */
  assert(m_member_versions.size() > 1);

  bool constexpr SAME_VERSION = true;
  bool constexpr DIFFERENT_VERSIONS = false;
  std::pair<bool, Gcs_protocol_version> result =
      std::make_pair(DIFFERENT_VERSIONS, Gcs_protocol_version::UNKNOWN);

  /* Get the protocol announced by the first node that is not me. */
  auto const &me = m_local_information;
  auto is_me_predicate = [&me](auto &pair) { return pair.first == me; };
  auto it = std::find_if_not(m_member_versions.begin(), m_member_versions.end(),
                             is_me_predicate);
  auto const &group_version = it->second;

  /*
   Check whether all the other members, myself excluded, announce the same
   protocol.
  */
  for (it = std::find_if_not(++it, m_member_versions.end(), is_me_predicate);
       it != m_member_versions.end();
       it = std::find_if_not(++it, m_member_versions.end(), is_me_predicate)) {
    auto const &member_version = it->second;
    bool const different_version = (group_version != member_version);
    if (different_version) goto end;
  }

  result = std::make_pair(SAME_VERSION, group_version);

end:
  return result;
}

bool Gcs_xcom_state_exchange::incompatible_with_group() const {
  bool constexpr INCOMPATIBLE = true;
  bool constexpr COMPATIBLE = false;
  bool result = INCOMPATIBLE;

  Gcs_xcom_communication &comm =
      static_cast<Gcs_xcom_communication &>(*m_broadcaster);
  Gcs_message_pipeline &pipeline = comm.get_msg_pipeline();

  /*
   Confirm we are trying to join while the group is in a quiescent state, i.e.
   everyone is announcing the same protocol.

   Here is the reasoning for why the code below is correct.
   A protocol change occurs in the same logical instant in all the group's
   members. A membership change also occurs in the same logical instant in all
   the group's member. This means that there is a total order between a protocol
   change and a membership change, even if they are concurrent.
   Denote a "protocol change to x" event by P(x) and a "membership change where
   this node joins" event by M.
   The possibilities are:

   a. P(x) < M, such that this node supports x.
      In this situation, we support the group's protocol, x. If every group
      member announces x, then can join successfully by adapting to protocol x.
      However, it is possible for the protocols announced by all members to be
      different. This can happen if two, or more, nodes join the group in the
      same membership change event because a node that joins announces the
      highest protocol it knows. Therefore, the existing group members will
      announce the group's current protocol, and joining members will announce
      the highest protocol they know. If these protocols do not match, we have
      different nodes announcing different protocols.
      A joining node does not have a previous membership information to compare
      against, so from the joining node's point of view it cannot distinguish
      existing members from other joining members, besides itself. For this
      reason, we can only join safely if every node announces the same protocol.

   b. P(y) < M, such that this node does *not* support y.
      In this situation the joining node expels itself immediately, because the
      group is potentially using unsupported functionality.

   If we are the sole member, then by definition the group is in a quiescent
   state.
  */
  bool const we_are_sole_member = (m_member_versions.size() == 1);
  if (!we_are_sole_member) {
    /* Get the group's protocol version. */
    bool same_version;
    Gcs_protocol_version group_version;
    std::tie(same_version, group_version) = members_announce_same_version();

    if (!same_version) {
      MYSQL_GCS_LOG_WARN(
          "This server could not adjust its communication protocol to match "
          "the group's. This server will be expelled from the group. This "
          "could be due to two or more servers joining simultaneously. Please "
          "ensure that this server joins the group in isolation and try "
          "again.");
      goto end;
    } else {
      assert(group_version != Gcs_protocol_version::UNKNOWN);
    }

    /*
     Every member announced the same protocol, so set our protocol to match the
     group's if we support it.
    */
    bool const supports_protocol =
        (group_version <= Gcs_protocol_version::HIGHEST_KNOWN);
    if (supports_protocol) {
#ifndef NDEBUG
      bool const failed =
#endif
          pipeline.set_version(group_version);
      assert(!failed && "Setting the pipeline version should not have failed");
      MYSQL_GCS_LOG_INFO("This server adjusted its communication protocol to "
                         << gcs_protocol_to_mysql_version(group_version)
                         << " in order to join the group.");
    } else {
      MYSQL_GCS_LOG_WARN(
          "This server does not support the group's newer communication "
          "protocol "
          << gcs_protocol_to_mysql_version(group_version)
          << ". This server will be expelled from the group.");
      goto end;
    }
  } else {
    assert(m_member_versions.begin()->first == m_local_information);
  }

  result = COMPATIBLE;

end:
  return result;
}

std::vector<Gcs_xcom_node_information>
Gcs_xcom_state_exchange::compute_incompatible_joiners() {
  std::vector<Gcs_xcom_node_information> incompatible_joiners;

  /* Get the protocol version that is in use. */
  Gcs_xcom_communication &comm =
      static_cast<Gcs_xcom_communication &>(*m_broadcaster);
  Gcs_message_pipeline &pipeline = comm.get_msg_pipeline();
  Gcs_protocol_version const protocol_version = pipeline.get_version();

  /* Compute the set of incompatible joiners. */
  for (Gcs_member_identifier const *joiner_id : m_ms_joined) {
    assert(m_member_versions.find(*joiner_id) != m_member_versions.end());
    Gcs_protocol_version const &joiner_version = m_member_versions[*joiner_id];

    assert(m_member_max_versions.find(*joiner_id) !=
           m_member_max_versions.end());
    Gcs_protocol_version const &joiner_max_version =
        m_member_max_versions[*joiner_id];

    bool const joiner_has_wrong_protocol = (joiner_version != protocol_version);
    bool const joiner_doesnt_expel_itself =
        (joiner_max_version == Gcs_protocol_version::V1);
    bool const incompatible_joiner =
        (joiner_has_wrong_protocol && joiner_doesnt_expel_itself);

    if (incompatible_joiner) {
      incompatible_joiners.push_back(*m_ms_xcom_nodes.get_node(*joiner_id));

      auto my_protocol = gcs_protocol_to_mysql_version(protocol_version);
      auto joiner_protocol = gcs_protocol_to_mysql_version(joiner_version);
      auto &joiner = joiner_id->get_member_id();

      MYSQL_GCS_LOG_WARN("The server "
                         << joiner
                         << ", which is attempting to join the group, only "
                            "supports communication protocol "
                         << joiner_protocol
                         << ", which is incompatible with the group's ("
                         << my_protocol << "). The server " << joiner
                         << " will be expelled from the group.");

    } else {
      MYSQL_GCS_LOG_TRACE(
          "compute_incompatible_joiners: compatible joiner=%s with protocol "
          "version=%d = %d (joiner_has_wrong_protocol=%d, "
          "joiner_expels_itself=%d)",
          joiner_id->get_member_id().c_str(),
          static_cast<unsigned int>(joiner_version),
          static_cast<unsigned int>(protocol_version),
          joiner_has_wrong_protocol, !joiner_doesnt_expel_itself);
    }
  }

  return incompatible_joiners;
}

void Gcs_xcom_state_exchange::compute_maximum_supported_protocol_version() {
  Gcs_xcom_communication &comm =
      static_cast<Gcs_xcom_communication &>(*m_broadcaster);

  /* Compute the maximum common protocol supported by the group. */
  Gcs_protocol_version max_supported_version =
      Gcs_protocol_version::HIGHEST_KNOWN;
  for (auto const &pair : m_member_max_versions) {
    auto max_member_version = pair.second;

    MYSQL_GCS_LOG_TRACE(
        "compute_maximum_supported_protocol_version: Member=%s supports up to "
        "version=%d",
        pair.first.get_member_id().c_str(),
        static_cast<unsigned short>(max_member_version));

    max_supported_version = std::min(max_member_version, max_supported_version);
  }

  comm.set_maximum_supported_protocol_version(max_supported_version);
}

bool Gcs_xcom_state_exchange::process_recovery_state() {
  bool successful = false;
  Gcs_xcom_synode_set synodes_needed;
  bool need_recovery = false;

  /*
   If I am the only one that participated in the state exchange, it means I am
   alone in the group, so there is nothing to recover.
   */
  bool const only_i_exist = (m_member_states.size() == 1);
  if (only_i_exist) {
    assert(m_member_states.begin()->first == m_local_information);
    successful = true;
    goto end;
  }

  /*
   Since the state exchange is part of the state machine, all existing group
   members will send the same synode recovery set.
   The optimal approach would be to simply use the synode recovery set of one of
   the existing group members.

   However, other nodes may be joining "together" with us.
   Those nodes, ourselves included, sent an empty synode recovery set.
   We cannot distinguish between who is joining and who is already a member.

   So we merge the recovery synodes sets of all nodes into a single set.
   This simplifies the code by not having to deal with corner cases.
   */
  for (auto const &pair : m_member_states) {
    auto const member_synodes = pair.second->get_snapshot();
    synodes_needed.insert(member_synodes.begin(), member_synodes.end());
  }

  need_recovery = (is_joining() && synodes_needed.size() != 0);
  if (need_recovery) {
    auto *const comm =
        static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);
    successful = comm->recover_packets(synodes_needed);
  } else {
    successful = true;
  }

end:
  return successful;
}

void Gcs_xcom_state_exchange::fill_member_set(
    std::vector<Gcs_member_identifier *> &in,
    std::set<Gcs_member_identifier *> &pset) {
  std::copy(in.begin(), in.end(), std::inserter(pset, pset.begin()));
}

void Gcs_xcom_state_exchange::save_member_state(
    Xcom_member_state *ms_info, const Gcs_member_identifier &p_id,
    Gcs_protocol_version maximum_supported_protocol_version,
    Gcs_protocol_version used_protocol_version) {
  m_member_max_versions[p_id] = maximum_supported_protocol_version;
  m_member_versions[p_id] = used_protocol_version;
  /* m_member_states[p_id] may already exist. In that case we delete the
   * existing pointer, otherwise it leaks. */
  auto member_state_it = m_member_states.find(p_id);
  bool const state_already_exists = (member_state_it != m_member_states.end());
  if (state_already_exists) delete member_state_it->second;
  m_member_states[p_id] = ms_info;
}

Gcs_xcom_view_identifier *Gcs_xcom_state_exchange::get_new_view_id() {
  /*
    This method is responsible for retrieving the correct view among
    the set of state exchange messages that the member has received.
    The view must be deterministically chosen and for that we rely
    on the assumption that all platforms will deterministically
    iterate through all elements in a standard library map object for
    the same set of input values.

    The algorithm picks the view in the first message that has a view
    with a monotonic part that is not zero or the view in the last
    ordered message if all views have the monotonic part equal to zero.
    Note that the following assertions must be true:

      . Views that have a monotonic part that is not zero must have
        the same value.

      . If a view with a monotonic part equals to zero is chosen, this
        means that all views have the monotonic part equal to zero.
  */
  Gcs_xcom_view_identifier *view_id = nullptr;

  std::map<Gcs_member_identifier, Xcom_member_state *>::iterator state_it;
  for (state_it = m_member_states.begin(); state_it != m_member_states.end();
       state_it++) {
    Xcom_member_state *member_state = (*state_it).second;
    view_id = member_state->get_view_id();
    if (view_id->get_monotonic_part() != 0) break;
  }

  assert(view_id != nullptr);
  MYSQL_GCS_DEBUG_EXECUTE(for (state_it = m_member_states.begin();
                               state_it != m_member_states.end(); state_it++) {
    Gcs_xcom_view_identifier member_state_view =
        *(static_cast<Xcom_member_state *>((*state_it).second)->get_view_id());
    /*
      Views that have a monotonic part that is not zero must have
      the same value.
    */
    if (member_state_view.get_monotonic_part() != 0) {
      if ((*view_id) != member_state_view) return nullptr;
    }
  });

  MYSQL_GCS_LOG_TRACE("get_new_view_id returns view_id %s",
                      view_id->get_representation().c_str());
  return view_id;
}

Gcs_xcom_view_change_control::Gcs_xcom_view_change_control()
    : m_view_changing(false),
      m_leaving(false),
      m_joining(false),
      m_wait_for_view_cond(),
      m_wait_for_view_mutex(),
      m_joining_leaving_mutex(),
      m_current_view(nullptr),
      m_current_view_mutex(),
      m_belongs_to_group(false),
      m_finalized(false) {
  m_wait_for_view_cond.init(
      key_GCS_COND_Gcs_xcom_view_change_control_m_wait_for_view_cond);
  m_wait_for_view_mutex.init(
      key_GCS_MUTEX_Gcs_xcom_view_change_control_m_wait_for_view_mutex,
      nullptr);
  m_joining_leaving_mutex.init(
      key_GCS_MUTEX_Gcs_xcom_view_change_control_m_joining_leaving_mutex,
      nullptr);
  m_current_view_mutex.init(
      key_GCS_MUTEX_Gcs_xcom_view_change_control_m_current_view_mutex, nullptr);
}

Gcs_xcom_view_change_control::~Gcs_xcom_view_change_control() {
  m_wait_for_view_mutex.destroy();
  m_wait_for_view_cond.destroy();
  m_joining_leaving_mutex.destroy();
  m_current_view_mutex.destroy();
}

void Gcs_xcom_view_change_control::set_current_view(Gcs_view *view) {
  m_current_view_mutex.lock();
  delete m_current_view;
  m_current_view = view;
  m_current_view_mutex.unlock();
}

/* purecov: begin deadcode */
void Gcs_xcom_view_change_control::set_unsafe_current_view(Gcs_view *view) {
  delete m_current_view;
  m_current_view = view;
}
/* purecov: end */

Gcs_view *Gcs_xcom_view_change_control::get_current_view() {
  Gcs_view *ret = nullptr;

  m_current_view_mutex.lock();
  if (m_current_view != nullptr) ret = new Gcs_view(*m_current_view);
  m_current_view_mutex.unlock();

  return ret;
}

Gcs_view *Gcs_xcom_view_change_control::get_unsafe_current_view() {
  return m_current_view;
}

bool Gcs_xcom_view_change_control::belongs_to_group() {
  return m_belongs_to_group;
}

void Gcs_xcom_view_change_control::set_belongs_to_group(bool belong) {
  m_belongs_to_group = belong;
}

void Gcs_xcom_view_change_control::start_view_exchange() {
  m_wait_for_view_mutex.lock();
  m_view_changing = true;
  m_wait_for_view_mutex.unlock();
}

void Gcs_xcom_view_change_control::end_view_exchange() {
  m_wait_for_view_mutex.lock();
  m_view_changing = false;
  m_wait_for_view_cond.broadcast();
  m_wait_for_view_mutex.unlock();
}

bool Gcs_xcom_view_change_control::is_view_changing() {
  bool retval;
  m_wait_for_view_mutex.lock();
  retval = m_view_changing;
  m_wait_for_view_mutex.unlock();

  return retval;
}

void Gcs_xcom_view_change_control::wait_for_view_change_end() {
  m_wait_for_view_mutex.lock();

  while (m_view_changing)
    m_wait_for_view_cond.wait(m_wait_for_view_mutex.get_native_mutex());

  m_wait_for_view_mutex.unlock();
}

bool Gcs_xcom_view_change_control::start_leave() {
  bool retval = false;

  m_joining_leaving_mutex.lock();
  retval = m_joining || m_leaving;
  if (!retval) m_leaving = true;
  m_joining_leaving_mutex.unlock();

  return !retval;
}

void Gcs_xcom_view_change_control::end_leave() {
  m_joining_leaving_mutex.lock();
  m_leaving = false;
  m_joining_leaving_mutex.unlock();
}

bool Gcs_xcom_view_change_control::is_leaving() {
  bool retval;

  m_joining_leaving_mutex.lock();
  retval = m_leaving;
  m_joining_leaving_mutex.unlock();

  return retval;
}

bool Gcs_xcom_view_change_control::start_join() {
  bool retval = false;

  m_joining_leaving_mutex.lock();
  retval = m_joining || m_leaving;
  if (!retval) m_joining = true;
  m_joining_leaving_mutex.unlock();

  return !retval;
}

void Gcs_xcom_view_change_control::end_join() {
  m_joining_leaving_mutex.lock();
  m_joining = false;
  m_joining_leaving_mutex.unlock();
}

bool Gcs_xcom_view_change_control::is_joining() {
  bool retval;

  m_joining_leaving_mutex.lock();
  retval = m_joining;
  m_joining_leaving_mutex.unlock();

  return retval;
}

void Gcs_xcom_view_change_control::finalize() { m_finalized.store(true); }

bool Gcs_xcom_view_change_control::is_finalized() { return m_finalized.load(); }
