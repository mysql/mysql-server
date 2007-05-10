/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H

#include "buffer.h"

#include <my_list.h>

/* default field length to be used in various field-realted functions */
enum { DEFAULT_FIELD_LENGTH= 20 };

struct st_net;

int net_send_ok(struct st_net *net, unsigned long connection_id,
                const char *message);

int net_send_error(struct st_net *net, unsigned sql_errno);

int net_send_error_323(struct st_net *net, unsigned sql_errno);

int send_fields(struct st_net *net, LIST *fields);

char *net_store_length(char *pkg, uint length);

int store_to_protocol_packet(Buffer *buf, const char *string,
                             size_t *position);

int store_to_protocol_packet(Buffer *buf, const char *string, size_t *position,
                             size_t string_len);

int send_eof(struct st_net *net);

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H */
