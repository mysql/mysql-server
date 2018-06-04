/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gcs_xcom_state_exchange.h"
#include "gcs_xcom_communication_interface.h"
#include "gcs_logging.h"
#include "synode_no.h"

#include <time.h>
#include <xplatform/byteorder.h>

#ifdef _WIN32
#include<iterator>
#endif


Xcom_member_state::Xcom_member_state(const Gcs_xcom_view_identifier &view_id,
                                     synode_no configuration_id,
                                     const uchar *data, uint64_t data_size)
  :m_view_id(NULL), m_configuration_id(configuration_id),
   m_data(NULL), m_data_size(0)
{
  m_view_id=
    new Gcs_xcom_view_identifier(view_id.get_fixed_part(),
                                 view_id.get_monotonic_part());

  if (data_size != 0)
  {
    m_data_size= data_size;
    m_data= static_cast<uchar *>(malloc(sizeof(uchar) * m_data_size));
    memcpy(m_data, data, m_data_size);
  }
}


Xcom_member_state::Xcom_member_state(const uchar *data,
                                     uint64_t data_size)
  :m_view_id(NULL), m_configuration_id(null_synode), m_data(NULL),
   m_data_size(0)
{
  uint64_t fixed_view_id= 0;
  uint32_t monotonic_view_id= 0;
  uint32_t group_id= 0;
  uint64_t msg_no= 0;
  uint32_t node_no= 0;

  const uchar *slider= data;
  uint64_t exchangeable_header_size= get_encode_header_size();
  uint64_t exchangeable_data_size= data_size - exchangeable_header_size;

  memcpy(&fixed_view_id, slider, WIRE_XCOM_VARIABLE_VIEW_ID_SIZE);
  fixed_view_id= le64toh(fixed_view_id);
  slider+= WIRE_XCOM_VARIABLE_VIEW_ID_SIZE;

  memcpy(&monotonic_view_id, slider, WIRE_XCOM_VIEW_ID_SIZE);
  monotonic_view_id= le32toh(monotonic_view_id);
  slider+= WIRE_XCOM_VIEW_ID_SIZE;

  m_view_id= new Gcs_xcom_view_identifier(fixed_view_id, monotonic_view_id);

  memcpy(&group_id, slider, WIRE_XCOM_GROUP_ID_SIZE);
  m_configuration_id.group_id= le32toh(group_id);
  slider += WIRE_XCOM_GROUP_ID_SIZE;

  memcpy(&msg_no, slider, WIRE_XCOM_MSG_ID_SIZE);
  m_configuration_id.msgno= le64toh(msg_no);
  slider += WIRE_XCOM_MSG_ID_SIZE;

  memcpy(&node_no, slider, WIRE_XCOM_NODE_ID_SIZE);
  m_configuration_id.node= le32toh(node_no);
  slider += WIRE_XCOM_NODE_ID_SIZE;

  if (exchangeable_data_size != 0)
  {
    m_data_size= exchangeable_data_size;
    m_data= static_cast<uchar *>(malloc(sizeof(uchar) * m_data_size));
    memcpy(m_data, slider, m_data_size);
  }

  MYSQL_GCS_LOG_TRACE(
    "Decoded header and payload for exchageable data: (header)=" <<
    exchangeable_header_size << "(payload)=" << exchangeable_data_size
  );
}


Xcom_member_state::~Xcom_member_state()
{
  delete m_view_id;
  free(m_data);
}


uint64_t Xcom_member_state::get_encode_size() const
{
  return get_encode_header_size() + get_encode_payload_size();
}


uint64_t Xcom_member_state::get_encode_payload_size() const
{
   return m_data_size;
}


uint64_t Xcom_member_state::get_encode_header_size()
{
  return
    WIRE_XCOM_VARIABLE_VIEW_ID_SIZE + WIRE_XCOM_VIEW_ID_SIZE +
    WIRE_XCOM_GROUP_ID_SIZE + WIRE_XCOM_MSG_ID_SIZE +
    WIRE_XCOM_NODE_ID_SIZE;
}


bool Xcom_member_state::encode_header(uchar *buffer, uint64_t *buffer_len)
{
  uint64_t fixed_view_id= 0;
  uint32_t monotonic_view_id= 0;
  uint32_t group_id= 0;
  uint64_t msg_no= 0;
  uint32_t node_no= 0;
  uint64_t encoded_size= get_encode_header_size();
  unsigned char *slider= buffer;

  MYSQL_GCS_LOG_TRACE("Encoding header for exchangeable data.")

  if (buffer == NULL || buffer_len == NULL)
  {
    MYSQL_GCS_LOG_ERROR(
      "Buffer to return information on encoded data or encoded data "
      "size is not properly configured."
    );
    return true;
  }

  if (*buffer_len < encoded_size)
  {
    MYSQL_GCS_LOG_ERROR(
      "Buffer reserved capacity is " << *buffer_len << " but it has "
      "been requested to add data whose size is " << encoded_size
    );
    return true;
  }

  *buffer_len= encoded_size;

  if (m_view_id != NULL)
  {
    fixed_view_id=     htole64(m_view_id->get_fixed_part());
    monotonic_view_id= htole32(m_view_id->get_monotonic_part());
  }
  memcpy(slider, &fixed_view_id, WIRE_XCOM_VARIABLE_VIEW_ID_SIZE);
  slider += WIRE_XCOM_VARIABLE_VIEW_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  memcpy(slider, &monotonic_view_id, WIRE_XCOM_VIEW_ID_SIZE);
  slider += WIRE_XCOM_VIEW_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  group_id= htole32(m_configuration_id.group_id);
  memcpy(slider, &group_id, WIRE_XCOM_GROUP_ID_SIZE);
  slider += WIRE_XCOM_GROUP_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  msg_no= htole64(m_configuration_id.msgno);
  memcpy(slider, &msg_no, WIRE_XCOM_MSG_ID_SIZE);
  slider += WIRE_XCOM_MSG_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  node_no= htole32(m_configuration_id.node);
  memcpy(slider, &node_no, WIRE_XCOM_NODE_ID_SIZE);
  slider += WIRE_XCOM_NODE_ID_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) == encoded_size);

  MYSQL_GCS_LOG_TRACE(
    "Encoded header for exchangeable data: (header)=" <<
    encoded_size
  );

  return false;
}


bool Xcom_member_state::encode(uchar *buffer, uint64_t *buffer_len)
{
  unsigned char *slider= buffer;
  uint64_t encoded_size= get_encode_size();
  uint64_t encoded_header_size= get_encode_header_size();

  MYSQL_GCS_LOG_TRACE("Encoding header and payload for exchangeable data")

  if (buffer == NULL || buffer_len == NULL)
  {
    MYSQL_GCS_LOG_ERROR(
      "Buffer to return information on encoded data or encoded data "
      "size is not properly configured."
    );
    return true;
  }

  if (*buffer_len < encoded_size)
  {
    MYSQL_GCS_LOG_ERROR(
      "Buffer reserved capacity is " << *buffer_len << " but it has "
      "been requested to add data whose size is " << encoded_size
    );
    return true;
  }

  *buffer_len= encoded_size;

  /*
    Copy the header information to the buffer.
  */
  encode_header(slider, &encoded_header_size);
  slider += encoded_header_size;
  assert(static_cast<uint64_t>(slider - buffer) <= encoded_size);

  /*
    Copy the payload information to the buffer.
  */
  memcpy(slider, m_data, m_data_size);
  slider += m_data_size;
  assert(static_cast<uint64_t>(slider - buffer) == encoded_size);

  MYSQL_GCS_LOG_TRACE(
    "Encoded header and payload for exchageable data: (header)=" <<
    encoded_header_size << "(payload)=" << m_data_size
  );

  return false;
}


Gcs_xcom_state_exchange::
Gcs_xcom_state_exchange(Gcs_communication_interface *comm)
  :m_broadcaster(comm), m_awaited_vector(), m_ms_total(),
   m_ms_left(), m_ms_joined(), m_member_states(),
   m_group_name(NULL), m_local_information(NULL),
   m_configuration_id(null_synode)
{}


Gcs_xcom_state_exchange::~Gcs_xcom_state_exchange()
{
  Gcs_xcom_communication_interface *binding_broadcaster=
    static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  binding_broadcaster->cleanup_buffered_messages();

  reset();
}


void Gcs_xcom_state_exchange::init()
{

}

void Gcs_xcom_state_exchange::reset_with_flush()
{
  Gcs_xcom_communication_interface *binding_broadcaster=
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
  if (is_joining())
  {
    binding_broadcaster->cleanup_buffered_messages();
  }
  else
  {
    binding_broadcaster->deliver_buffered_messages();
  }

  reset();
}

void Gcs_xcom_state_exchange::reset()
{
  Gcs_xcom_communication_interface *binding_broadcaster=
    static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);
  assert(binding_broadcaster->number_buffered_messages() == 0);

  m_configuration_id= null_synode;

  std::set<Gcs_member_identifier *>::iterator member_it;

  for (member_it= m_ms_total.begin(); member_it != m_ms_total.end(); member_it++)
    delete (*member_it);
  m_ms_total.clear();

  for (member_it= m_ms_left.begin(); member_it != m_ms_left.end(); member_it++)
    delete (*member_it);
  m_ms_left.clear();

  for (member_it= m_ms_joined.begin(); member_it != m_ms_joined.end();
       member_it++)
    delete (*member_it);
  m_ms_joined.clear();

  std::map<Gcs_member_identifier, Xcom_member_state *>::iterator state_it;
  for (state_it= m_member_states.begin(); state_it != m_member_states.end();
       state_it++)
    delete (*state_it).second;
  m_member_states.clear();

  m_awaited_vector.clear();

  delete m_group_name;
  m_group_name= NULL;
}

void Gcs_xcom_state_exchange::end()
{
  Gcs_xcom_communication_interface *binding_broadcaster=
    static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  binding_broadcaster->deliver_buffered_messages();

  reset();
}

bool Gcs_xcom_state_exchange::
state_exchange(synode_no configuration_id,
               std::vector<Gcs_member_identifier *> &total,
               std::vector<Gcs_member_identifier *> &left,
               std::vector<Gcs_member_identifier *> &joined,
               std::vector<Gcs_message_data *> &exchangeable_data,
               Gcs_view *current_view,
               std::string *group,
               Gcs_member_identifier *local_info)
{
  uint64_t fixed_part= 0;
  int monotonic_part= 0;

  /* Keep track of when the view was internally delivered. */
  m_configuration_id= configuration_id;

  /* Store member state for later broadcast */
  m_local_information= local_info;

  if (m_group_name == NULL)
    m_group_name= new std::string(*group);

  if (current_view != NULL)
  {
    /*
      If a view has been already installed, disseminate this information
      to other members so that a member that is joining may learn about
      it. Please, check ::get_new_view_id to find out how the view is
      chosen.
    */
    Gcs_xcom_view_identifier &xcom_view_id=
      (Gcs_xcom_view_identifier &)current_view->get_view_id();
    fixed_part= xcom_view_id.get_fixed_part();
    monotonic_part= xcom_view_id.get_monotonic_part();
  }
  else
  {
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
    uint64_t ts= My_xp_util::getsystime();
    fixed_part= (ts == 0) ? rand() : (ts + (rand() % 1000));
    monotonic_part= 0;
  }
  Gcs_xcom_view_identifier proposed_view(fixed_part, monotonic_part);

  fill_member_set(total, m_ms_total);
  fill_member_set(joined, m_ms_joined);
  fill_member_set(left, m_ms_left);

  /*
    Calculate if i am leaving...
    If so, SE will be interrupted and it will return true...
  */
  bool leaving= is_leaving();

  if (!leaving)
  {
    update_awaited_vector();
    broadcast_state(proposed_view, exchangeable_data);
  }

  return leaving;
}


bool Gcs_xcom_state_exchange::is_joining()
{
  bool is_joining= false;

  std::set<Gcs_member_identifier *>::iterator it;

  for (it= m_ms_joined.begin(); it != m_ms_joined.end() && !is_joining; it++)
    is_joining= (*(*it) == *m_local_information);

  return is_joining;
}


bool Gcs_xcom_state_exchange::is_leaving()
{
  bool is_leaving= false;

  std::set<Gcs_member_identifier *>::iterator it;

  for (it= m_ms_left.begin(); it != m_ms_left.end() && !is_leaving; it++)
    is_leaving= (*(*it) == *m_local_information);

  return is_leaving;
}


enum_gcs_error Gcs_xcom_state_exchange::broadcast_state(
  const Gcs_xcom_view_identifier &proposed_view,
  std::vector<Gcs_message_data *> &exchangeable_data)
{
  uchar *buffer= NULL;
  uchar *slider= NULL;
  uint64_t buffer_len= 0;
  uint64_t exchangeable_header_len= 0;
  uint64_t exchangeable_data_len= 0;
  std::vector<Gcs_message_data *>::const_iterator it;
  std::vector<Gcs_message_data *>::const_iterator it_ends;
  Gcs_message_data *msg_data= NULL;

  /*
    The exchangeable_data may have a list with Gcs_message_data
    and the following code gets the size of the data that will
    be sent to peers.

    This will be changed in the future so that there will be
    only a single piece of data.
  */
  it_ends= exchangeable_data.end();
  for (it=exchangeable_data.begin(); it != it_ends; ++it)
  {
    msg_data= (*it);
    exchangeable_data_len += msg_data ? msg_data->get_encode_size() : 0;
  }
  /*
    This returns the size of the header that will compose the
    message along with the previous data.
  */
  exchangeable_header_len= Xcom_member_state::get_encode_header_size();

  /*
    Allocate a buffer that will contain both the header and
    the data.
  */
  MYSQL_GCS_LOG_TRACE(
    "Allocating buffer to carry exchangeable data: (header)=" <<
    exchangeable_header_len << " (payload)=" << exchangeable_data_len
  );
  buffer_len= exchangeable_header_len + exchangeable_data_len;
  buffer= slider= static_cast<uchar *>(malloc(buffer_len * sizeof(uchar)));
  if (buffer == NULL)
  {
    MYSQL_GCS_LOG_ERROR("Error allocating buffer to carry exchangeable data")
    return GCS_NOK;
  }

  MYSQL_GCS_LOG_TRACE(
    "Populating header for exchangeable data: (header)=" <<
    exchangeable_header_len
  );
  Xcom_member_state member_state(proposed_view, m_configuration_id, NULL, 0);
  member_state.encode_header(slider, &exchangeable_header_len);
  slider += exchangeable_header_len;
  assert(static_cast<uint64_t>(slider - buffer) <= buffer_len);

  /*
    Note that the size of the list may be empty and this means
    that the node has nothing to exchange during a view change.
    However, it will send an empty message anyway.
  */
  if (exchangeable_data_len > 0)
  {
    uint64_t slider_total_len= 0;
    uint64_t slider_len= 0;
    for (it=exchangeable_data.begin(); it != it_ends; ++it)
    {
      msg_data= (*it);

      if (msg_data != NULL)
      {
        slider_len= msg_data->get_encode_size();
        MYSQL_GCS_LOG_TRACE(
          "Populating payload for exchangeable data: (payload)=" <<
          slider_len
        );
        msg_data->encode(slider, &slider_len);
        slider += slider_len;
        slider_total_len += slider_len;
        delete msg_data;
      }
    }
    assert(slider_total_len == exchangeable_data_len);
  }
  assert(static_cast<uint64_t>(slider - buffer) == buffer_len);

  /*
    There is another copy here but we cannot avoid this right now
    since the other other stacks further down that are expecting
    this.
  */
  MYSQL_GCS_LOG_TRACE(
    "Creating message to carry exchangeable data: (payload)=" << buffer_len
  );
  Gcs_message_data *message_data= new Gcs_message_data(0, buffer_len);
  message_data->append_to_payload(buffer, buffer_len);
  free(buffer);
  buffer= NULL;

  Gcs_group_identifier group_id(*m_group_name);
  Gcs_message message(*m_local_information, group_id, message_data);

  Gcs_xcom_communication_interface *binding_broadcaster=
    static_cast<Gcs_xcom_communication_interface *>(m_broadcaster);

  unsigned long long message_length= 0;
  return binding_broadcaster->send_binding_message(
    message, &message_length, Gcs_internal_message_header::CT_INTERNAL_STATE_EXCHANGE
  );
}


void Gcs_xcom_state_exchange::update_awaited_vector()
{
  std::set<Gcs_member_identifier *>::iterator it;
  Gcs_member_identifier *p_id;

  it= m_ms_total.begin();
  while (it != m_ms_total.end())
  {
    p_id= *it;
    m_awaited_vector[*p_id]++;
    ++it;
  }

  it= m_ms_left.begin();
  while (it != m_ms_left.end())
  {
    p_id= *it;
    m_awaited_vector.erase(*p_id);
    ++it;
  }
}


bool Gcs_xcom_state_exchange::
process_member_state(Xcom_member_state *ms_info,
                     const Gcs_member_identifier &p_id)
{
  /*
    A state exchange message just arrived and we will only consider it
    if its configuration identifier matches the one expected by the
    current state exchange phase. If this does not happen the message
    is discarded.
  */
  if(!synode_eq(ms_info->get_configuration_id(), m_configuration_id))
  {
    MYSQL_GCS_DEBUG_EXECUTE(
      synode_no configuration_id= ms_info->get_configuration_id();
      MYSQL_GCS_LOG_DEBUG(
        "Ignoring exchangeable data because its from a previous state "
        "exchange phase. Message is from group_id("
        << configuration_id.group_id << "), msg_no( "
        << configuration_id.msgno << "), node_no("
        << configuration_id.node << ") but current phase is "
        << m_configuration_id.group_id << "), msg_no( "
        << m_configuration_id.msgno << "), node_no("
        << m_configuration_id.node << ")."
      )
    );
    /*
     * ms_info will leak if we don't delete it here.
     * If this branch is not taken, m_member_states takes ownership of the
     * pointer below.
     */
    delete ms_info;
    return false;
  }

  m_member_states[p_id]= ms_info;

  /*
    The rule of updating the awaited_vector at receiving is simply to
    decrement the counter in the right index. When the value drops to
    zero the index is discarded from the vector.

    Installation goes into terminal phase when all expected state
    messages have arrived which is indicated by the empty vector.
  */
  if (m_awaited_vector.find(p_id) != m_awaited_vector.end())
  {
    m_awaited_vector.erase(p_id);
  }

  bool can_install_view= (m_awaited_vector.size() == 0);

  return can_install_view;
}


void
Gcs_xcom_state_exchange::
fill_member_set(std::vector<Gcs_member_identifier *> &in,
                std::set<Gcs_member_identifier *>&pset)
{
  std::copy(in.begin(), in.end(), std::inserter(pset, pset.begin()));
}


Gcs_xcom_view_identifier *
Gcs_xcom_state_exchange::get_new_view_id()
{
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
  Gcs_xcom_view_identifier* view_id= NULL;

  std::map<Gcs_member_identifier, Xcom_member_state *>::iterator state_it;
  for (state_it= m_member_states.begin(); state_it != m_member_states.end();
       state_it++)
  {
    Xcom_member_state *member_state= (*state_it).second;
    view_id= member_state->get_view_id();
    if (view_id->get_monotonic_part() != 0)
      break;
  }

  assert(view_id != NULL);
  MYSQL_GCS_DEBUG_EXECUTE(
    uint64_t fixed_view_id= 0;
    int monotonic_view_id= 0;
    for (state_it= m_member_states.begin(); state_it != m_member_states.end();
         state_it++)
    {
      Xcom_member_state *member_state= (*state_it).second;
      monotonic_view_id= member_state->get_view_id()->get_monotonic_part();
      fixed_view_id= member_state->get_view_id()->get_fixed_part();
      /*
        Views that have a monotonic part that is not zero must have
        the same value.
      */
      if (monotonic_view_id != 0)
      {
        if ((view_id->get_monotonic_part() != monotonic_view_id) ||
            (view_id->get_fixed_part() != fixed_view_id))
          return NULL;
      }
    }
  );

  return view_id;
}


Gcs_xcom_view_change_control::Gcs_xcom_view_change_control()
  :m_view_changing(false), m_leaving(false), m_joining(false), m_wait_for_view_cond(),
   m_wait_for_view_mutex(), m_joining_leaving_mutex(), m_current_view(NULL),
   m_current_view_mutex(), m_belongs_to_group(false)
{
  m_wait_for_view_cond.init();
  m_wait_for_view_mutex.init(NULL);
  m_joining_leaving_mutex.init(NULL);
  m_current_view_mutex.init(NULL);
}


Gcs_xcom_view_change_control::~Gcs_xcom_view_change_control()
{
  m_wait_for_view_mutex.destroy();
  m_wait_for_view_cond.destroy();
  m_joining_leaving_mutex.destroy();
  m_current_view_mutex.destroy();
}


void Gcs_xcom_view_change_control::set_current_view(Gcs_view *view)
{
  m_current_view_mutex.lock();
  delete m_current_view;
  m_current_view= view;
  m_current_view_mutex.unlock();
}

/* purecov: begin deadcode */
void Gcs_xcom_view_change_control::set_unsafe_current_view(Gcs_view *view)
{
  delete m_current_view;
  m_current_view= view;
}
/* purecov: end */
Gcs_view*
Gcs_xcom_view_change_control::get_current_view()
{
  Gcs_view *ret= NULL;

  m_current_view_mutex.lock();
  if (m_current_view != NULL)
    ret= new Gcs_view(*m_current_view);
  m_current_view_mutex.unlock();

  return ret;
}

Gcs_view*
Gcs_xcom_view_change_control::get_unsafe_current_view()
{
  return m_current_view;
}


bool Gcs_xcom_view_change_control::belongs_to_group()
{
  return m_belongs_to_group;
}


void Gcs_xcom_view_change_control::set_belongs_to_group(bool belong)
{
  m_belongs_to_group= belong;
}


void Gcs_xcom_view_change_control::start_view_exchange()
{
  m_wait_for_view_mutex.lock();
  m_view_changing= true;
  m_wait_for_view_mutex.unlock();
}


void Gcs_xcom_view_change_control::end_view_exchange()
{
  m_wait_for_view_mutex.lock();
  m_view_changing= false;
  m_wait_for_view_cond.broadcast();
  m_wait_for_view_mutex.unlock();
}


bool Gcs_xcom_view_change_control::is_view_changing()
{
  bool retval;
  m_wait_for_view_mutex.lock();
  retval= m_view_changing;
  m_wait_for_view_mutex.unlock();

  return retval;
}


void Gcs_xcom_view_change_control::wait_for_view_change_end()
{
  m_wait_for_view_mutex.lock();

  while (m_view_changing)
    m_wait_for_view_cond.wait(m_wait_for_view_mutex.get_native_mutex());

  m_wait_for_view_mutex.unlock();
}


bool
Gcs_xcom_view_change_control::start_leave()
{
  bool retval= false;

  m_joining_leaving_mutex.lock();
  retval= m_joining || m_leaving;
  if(!retval)
    m_leaving= true;
  m_joining_leaving_mutex.unlock();

  return !retval;
}


void
Gcs_xcom_view_change_control::end_leave()
{
  m_joining_leaving_mutex.lock();
  m_leaving= false;
  m_joining_leaving_mutex.unlock();
}


bool
Gcs_xcom_view_change_control::is_leaving()
{
  bool retval;

  m_joining_leaving_mutex.lock();
  retval= m_leaving;
  m_joining_leaving_mutex.unlock();

  return retval;
}


bool
Gcs_xcom_view_change_control::start_join()
{
  bool retval= false;

  m_joining_leaving_mutex.lock();
  retval= m_joining || m_leaving;
  if(!retval)
    m_joining= true;
  m_joining_leaving_mutex.unlock();

  return !retval;
}


void
Gcs_xcom_view_change_control::end_join()
{
  m_joining_leaving_mutex.lock();
  m_joining= false;
  m_joining_leaving_mutex.unlock();
}


bool
Gcs_xcom_view_change_control::is_joining()
{
  bool retval;

  m_joining_leaving_mutex.lock();
  retval= m_joining;
  m_joining_leaving_mutex.unlock();

  return retval;
}
