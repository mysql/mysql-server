#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H
/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "buffer.h"
#include <my_list.h>

typedef struct field {
  char *name;
  uint length;
} NAME_WITH_LENGTH;

struct st_net;

int net_send_ok(struct st_net *net, unsigned long connection_id);

int net_send_error(struct st_net *net, unsigned sql_errno);

int net_send_error_323(struct st_net *net, unsigned sql_errno);

int send_fields(struct st_net *net, LIST *fields);

char *net_store_length(char *pkg, uint length);

int store_to_string(Buffer *buf, const char *string, uint *position);

int send_eof(struct st_net *net);

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_PROTOCOL_H */
