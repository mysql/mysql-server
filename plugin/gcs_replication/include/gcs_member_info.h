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

#ifndef GCS_MEMBER_INFO_H
#define GCS_MEMBER_INFO_H

/*
  The file contains declarations relevant to Member state and
  its identification by the Protocol Client.
  Classes usage is intented to be within the Corosync binding,
  should it has to be extened a few specific data types would have to be
  generilized.
*/

#include <string>
#include <set>
#include "gcs_payload.h"
#include <corosync/cpg.h>

using std::string;
using std::set;

namespace GCS
{


/**
   The following struct describes GCS internal configuration identifier.
   The identifier is needed when the GCS does not provide Primary Component
   service, so the latter would have to be built on the top of that GCS provides.

   todo: make it generic.
*/
typedef struct cpg_ring_id Corosync_ring_id;

typedef enum en_recovery_status
{
  MEMBER_ONLINE,
  MEMBER_IN_RECOVERY,
  MEMBER_OFFLINE,
  MEMBER_END  // the end of the enum
} Member_recovery_status;

class MessageBuffer;

typedef enum en_client_log_level
{
  GCS_ERROR_LEVEL,
  GCS_WARNING_LEVEL,
  GCS_INFORMATION_LEVEL
} Client_log_level;

typedef int (*Client_logger_func)(Client_log_level level, const char *format, ...);

/*
  Client_info describes the GCS Client property as it relates
  to group joining.
*/
class Client_info
{
private:
  string hostname;
  uint port;
  string uuid;
  Member_recovery_status status;

public:
  Client_info(Client_logger_func f_arg= NULL) :
    status(MEMBER_OFFLINE), logger_func(f_arg) {};
  Client_info(string& uuid_arg) : uuid(uuid_arg), status(MEMBER_OFFLINE),
                                  logger_func(NULL) {};

  /* Decoder constructor */
  Client_info(const uchar* data, size_t len);

  const uchar* encode(MessageBuffer* mbuf_ptr);

  void store(string hostname_arg, uint port_arg, string uuid_arg,
             Member_recovery_status status_arg)
  {
    hostname= hostname_arg;
    port= port_arg;
    uuid= uuid_arg;
    status= status_arg;
  }
  string& get_hostname() { return hostname; }
  uint get_port() { return port; }
  string& get_uuid() { return uuid; }
  Member_recovery_status get_recovery_status() { return status; }
  int (*logger_func)(Client_log_level level, const char *format, ...);
};

class View;
class Member;
class Member_cmp;
typedef set<Member, Member_cmp> Member_set;

/*
  State message "pure" payload structure is as the following:
  [
    View-id
    Conf-id
    #-of-members
    Member-uuid_1
    Member-uuid_2
    ...

    Client_info
  ]
*/
class Member_state : public Serializable
{
public:
  ulonglong view_id;
  Corosync_ring_id conf_id;
  Client_info client_info;
  set<string> member_uuids;
  Member_state(ulonglong, Member_set&, Corosync_ring_id id, Client_info&);
  Payload_code get_code() { return PAYLOAD_STATE_EXCHANGE; }
  const uchar* encode(MessageBuffer* mbuf_ptr);

  Member_state(const uchar* data, size_t len);
};

} // namespace
#endif
