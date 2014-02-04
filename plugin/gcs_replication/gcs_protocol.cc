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

#include <gcs_protocol.h>
#include "gcs_plugin_utils.h"

namespace GCS
{

void Protocol::set_client_info(Client_info& info_arg)
{
  local_client_info= info_arg;
}

string& Protocol::get_client_uuid()
{
  return local_client_info.get_uuid();
}

/*
  Constraints to buffer size reasoned by limitted capacity
  of log_warnings system buffers.
*/
static const uint log_buf_size= 1024;
static const uint m_id_buf_size= 256;

/* Auxiliary function to serve to log_view_change */
static void describe_members(Member_set& members, char *buf, uint buf_size)
{
  if (members.size() == 0)
    sprintf(buf, "*empty*");

  Member_set::iterator it;
  int remained= buf_size, it_done;
  for (it= members.begin(); it != members.end() && remained > 0;
       ++it, remained -= it_done, buf += it_done)
  {
    Member m_curr= *it;
    uchar buf_member_id[m_id_buf_size];
    const char* fmt= remained == (int) buf_size ?
      "'%s %s'" : ", '%s %s'";
    it_done= snprintf(buf, remained, fmt, m_curr.get_uuid().c_str(),
                      m_curr.describe_member_id(buf_member_id, sizeof(buf_member_id)));
  }
}

void log_view_change(ulonglong view_id, Member_set& total,
                     Member_set& left, Member_set& joined)
{
  char buf_totl[log_buf_size]= {'0'};
  char buf_left[log_buf_size]= {'0'};
  char buf_join[log_buf_size]= {'0'};

  describe_members(total,  buf_totl, sizeof(buf_totl));
  describe_members(left,   buf_left, sizeof(buf_left));
  describe_members(joined, buf_join, sizeof(buf_join));
  log_message(MY_INFORMATION_LEVEL,
              "A view change was detected. View-id %llu; "
              "current number of members: "
              "%d, left %d, joined %d, quorate %s; "
              "Content of sets, "
              "Total: %s, "
              "Left: %s, "
              "Joined: %s",
              view_id, (int)total.size(), (int)left.size(), (int)joined.size(),
              view_id > 0 ? "true" : "false",
              buf_totl, buf_left, buf_join);
}

} // end of namespace

