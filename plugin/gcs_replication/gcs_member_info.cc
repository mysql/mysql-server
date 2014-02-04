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

#include "gcs_member_info.h"
#include "gcs_protocol.h" // View
#include "gcs_message.h"

// TODO: work it around
#include "my_byteorder.h"

namespace GCS
{

Client_info::Client_info(const uchar* data, size_t len)
{
  assert(data); // msg must be delivered

  const char *slider= (char*) data;

  hostname= string(slider);
  slider += hostname.length() + 1;
  port= uint2korr(slider);
  slider += 2;
  uuid= string(slider);
  slider += uuid.length() + 1;
  status= (Member_recovery_status) * (uchar*) slider;
  slider +=1;

  assert((uchar*) slider == len +  (uchar*) data);
}

const uchar* Client_info::encode(MessageBuffer* mbuf_ptr)
{
  char local_buf[2];

  mbuf_ptr->append((const uchar*) hostname.c_str(), hostname.length() + 1);
  int2store(local_buf, port);
  mbuf_ptr->append((const uchar*) local_buf, sizeof(local_buf));
  mbuf_ptr->append((const uchar*) uuid.c_str(), uuid.length() + 1);

  compile_time_assert(MEMBER_END < 256);
  mbuf_ptr->append_uint8((const uchar) status);

  return mbuf_ptr->data();
}

/*
  Member state instantiation for sending.
*/
  Member_state::Member_state(ulonglong view_id_arg, Member_set& last_prim_comp_arg,
                             Corosync_ring_id ring_id_arg, Client_info& info_arg)
    : view_id(view_id_arg),
      conf_id(ring_id_arg),
      client_info(info_arg)
{
  Member_set::iterator it_m;
  for (it_m= last_prim_comp_arg.begin(); it_m != last_prim_comp_arg.end();
       ++it_m)
  {
    Member mbr= *it_m;
    member_uuids.insert(mbr.get_uuid());
  }
}

const uchar* Member_state::encode(MessageBuffer* mbuf_ptr)
{
  compile_time_assert(Payload_code_size == 2);
  assert(mbuf_ptr->length() == sizeof(Message_header));

  mbuf_ptr->append_uint16((uint16) get_code());
  mbuf_ptr->append_uint64(view_id);
  mbuf_ptr->append_uint32(conf_id.nodeid);
  mbuf_ptr->append_uint64(conf_id.seq);
  mbuf_ptr->append_uint16(member_uuids.size());
  set<string>::iterator it_s;
  for (it_s= member_uuids.begin(); it_s != member_uuids.end(); ++it_s)
  {
    mbuf_ptr->append_stdstr(*it_s);
  }
  /* Local client info streamed last */
  client_info.encode(mbuf_ptr);
  return mbuf_ptr->data();
}

Member_state::Member_state(const uchar* data, size_t len)
{
  const uchar* slider= data;
  view_id= uint8korr(slider);
  slider += 8;
  conf_id.nodeid= uint4korr(slider);
  slider += 4;
  conf_id.seq= uint8korr(slider);
  slider += 8;

  uint n_members= uint2korr(slider);
  slider += 2;
  for (uint i= 0; i < n_members; i++)
  {
    string mbr_id((const char*) slider);
    size_t read_size= mbr_id.length() + 1;
    member_uuids.insert(mbr_id);
    slider += read_size;

    assert(slider < data + len);
  }

  client_info= Client_info(slider, len - (slider - data));
}

} // namespace
