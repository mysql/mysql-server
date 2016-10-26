/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_MSG_QUEUE_H
#define XCOM_MSG_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Helper struct used for putting messages in a queue */
struct msg_link {
  linkage l;
  pax_msg * p;
  node_no to;
};
typedef struct msg_link msg_link;

msg_link *msg_link_new(pax_msg *p, node_no to);
char *dbg_msg_link(msg_link *link);
void empty_link_free_list();
void empty_msg_channel(channel *c);
void empty_msg_list(linkage *l);
void init_link_list();
void msg_link_delete(msg_link **link_p);

#ifdef __cplusplus
}
#endif

#endif

