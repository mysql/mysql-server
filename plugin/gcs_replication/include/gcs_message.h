/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_MESSAGE_H
#define GCS_MESSAGE_H

/*
  The file contains declarations relevant to transport facilities such
  as class Message.
*/

#include <set>
#include <string>
#include <vector>
#include "gcs_member_info.h"

using std::vector;

namespace GCS
{

class Message;
// Structure of the GCS message (common) header
struct Message_header
{
  uint64 version;    // (the plugin's) version.
                     // Todo: version handshake (upgrade case) at Group joining
  size_t msg_size;
  uint8  type;

  uint64 micro_time; // Value in microseconds
  uint64 sender_id;  // Member_id, as e.g corosync's process id of 64 bits
  uint64 view_id;
  uint64 local_cnt;  // sender's total sent message counter

  // If any more entity is added the plugin version might need to
  // step. Version compatibility verification must adopt the lowest
  // level everywhere in the cluster.
};

class MessageBuffer
{
private:
  vector<uchar> *buffer;

public:
  MessageBuffer()
  {
    this->buffer= new vector<uchar>();
  }

  MessageBuffer(int capacity)
  {
    this->buffer= new vector<uchar>();
    this->buffer->reserve(capacity);
  }

  ~MessageBuffer()
  {
    delete this->buffer;
  }

  size_t length()
  {
    return this->buffer->size();
  }

  void append(const uchar *s, size_t len)
  {
    this->buffer->insert(this->buffer->end(), s, s+len);
  }

  void append_stdstr(const std::string& str)
  {
    this->buffer->insert(this->buffer->end(), str.c_str(),
                         str.c_str() + (str.length() + 1));
  }

  void append_uint8(uchar val)
  {
    uchar s[sizeof(val)];
    s[0]= val;
    this->buffer->insert(this->buffer->end(), s, s + 1);
  }
  void append_uint16(uint val);
  void append_uint32(ulong val);
  void append_uint64(ulonglong val);

  void reset()
  {
    this->buffer->clear();
  }

  const uchar* data()
  {
    return &this->buffer->front();
  }
};
/**
   The message type identifies the destination layer.
   INTERNAL type corresponds to GCS bindings.
   CLIENT type points the addressee is the Application/Client.
*/
typedef enum enum_message_type
{
  MSG_GCS_INTERNAL,
  MSG_GCS_CLIENT
} Msg_type;

/*
   Quality of Service of message delivery, RELIABLE is supposed to be
   implemented mandatorily by any underlying GCS.
   Other qos:s are optinal.
*/
typedef enum enum_msg_qos { MSGQOS_BEST_EFFORT,
                            MSGQOS_RELIABLE, MSGQOS_UNIFORM } Msg_qos;

/*
  Orthogonal to QOS but related style of message ordering by delivery.
  TOTAL_ORDER must be implemented by underlying GCS.
*/
typedef enum enum_msg_ordering
{
  MSGORD_UNORDERED,
  MSGORD_TOTAL_ORDER
} Msg_ordering;


class Message
{
public:
  static const size_t Msg_type_size= 1; // in bytes
  /*
     Sending time constructor for to be broadcast object to be encoded
     explicitly into MessageBuffer supplied by the instance.
     The caller is to provide the payload type, Msg type, qos & ordering.
     The payload will be finalized by MessageBuffer::append() of the
     caller later.
  */
  Message(Payload_code pcode,
          Msg_type type_arg= MSG_GCS_CLIENT,
          Msg_qos qos_arg= MSGQOS_UNIFORM,
          Msg_ordering ord_arg= MSGORD_TOTAL_ORDER) : data(NULL)
    {
      set_type(type_arg);
      prepare_header();

      Serializable::store_code(pcode, &mbuf);

      assert(mbuf.length() == sizeof(Message_header) + Serializable::Payload_code_size);
    }

  /*
     This constructor is for informational objects subclassed from
     Serializable. Encoded object image goes straight into
     MessageBuffer supplied by the constructor.
     The GCS message type defaults to the main use case which is the client type.
     Qos and ordering will be handly needed to change from the chosen defaults.

     @param info      a pointer to an informational object instance
     @param type_arg  one of the two Msg_type values. Unless it's the binding
                      specific the message is of MSG_GCS_CLIENT.
  */
  Message(Serializable *info, Msg_type type_arg= MSG_GCS_CLIENT,
          Msg_qos qos_arg= MSGQOS_UNIFORM,
          Msg_ordering ord_arg= MSGORD_TOTAL_ORDER) :
    data(NULL), qos(qos_arg), ord(ord_arg)
    {
      /*
         all but the type slot of the header are to be filled at broadcast(),
         see store_header()
      */
      set_type(type_arg);
      prepare_header();
      /* Payload code in filled in by Serializable */
      info->encode(&mbuf);
    }

  /*
     The delivered (being dispatched/received) Message constructor.
     The header and the payload are copied into the instance's private buffer.

     Todo: consider memory root or similar mechanism to work with a session
           life-time reusable storage provided by the consumer.
  */
  Message(const uchar* data_arg, size_t len_arg);

  /*
    Method to be used in combination with "light" impicit sub-serializable
    Message(Payload_code pcode...).
    Informational object data get to the message via this method.
  */
  void append(const uchar *s, size_t len)
  {
    mbuf.append(s, len);
  }

  /* Total size of the Message  */
  size_t get_size()
  {
    if (hdr.msg_size == 0)
      hdr.msg_size= mbuf.length();

    return hdr.msg_size;
  }

  /* returns the first byte of the message which is the 1st byte of the header */
  const uchar* get_data()
  {
    if (!data)
      data= mbuf.data();
    return data;
  }
  /* The size of the net playload */
  size_t get_payload_size() { return get_size() - sizeof(Message_header); }
  const uchar *get_payload() { return get_data() + sizeof(Message_header); }
  Msg_type get_type()
  {
    compile_time_assert(sizeof(hdr.type) <= sizeof(Msg_type));
    return (Msg_type) hdr.type;
  }

  /*
    The method is called to finalize being sent Message header slots
    at immediate sending.
  */
  void store_header(Message_header& hdr_arg)
  {
    Message_header *msg_hdr= (Message_header *) mbuf.data();

    //TODO: convert to network byte order
    msg_hdr->version= hdr_arg.version;
    msg_hdr->msg_size= get_size();
    msg_hdr->sender_id= hdr_arg.sender_id;
    msg_hdr->micro_time= hdr_arg.micro_time;
    msg_hdr->local_cnt= hdr_arg.local_cnt;
    msg_hdr->view_id= hdr_arg.view_id;
  }
  /*
    The function is invoked at being delivered Message constructor
    to gather the Msg type as well as low-level GCS stats
    for benchmarking and debugging.
  */
  void restore_header()
  {
    Message_header *ptr_hdr= (Message_header *) data;

    //TODO: restore from network byte order
    hdr.version= ptr_hdr->version;
    hdr.msg_size= ptr_hdr->msg_size;
    hdr.type= ptr_hdr->type;
    hdr.micro_time= ptr_hdr->micro_time;
    hdr.sender_id= ptr_hdr->sender_id;
    hdr.local_cnt= ptr_hdr->local_cnt;
    hdr.view_id= ptr_hdr->view_id;
  }
  ulonglong get_msg_hdr_view_id() { return hdr.view_id; }
  MessageBuffer& get_buffer() { return mbuf; }

private:

  Msg_type set_type(Msg_type type) { return (Msg_type) (hdr.type= (uint8) type); }

  void prepare_header()
  {
    hdr.msg_size= 0;
    mbuf.append((const uchar*) &hdr, sizeof(Message_header));
  }

  const uchar *data;   // pointer to actual buffer could be left unknown until sending
  struct st_msg_global_id
  {
    ulonglong view_no; // the deliery view id
    ulonglong seq_no;  // sequence number in the view
  } msg_id;
  Msg_qos qos;
  Msg_ordering ord;

  MessageBuffer mbuf;  // sender's storage/facility
  Message_header hdr;
};

} // namespace
#endif
