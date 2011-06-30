/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef HOSTNAME_INCLUDED
#define HOSTNAME_INCLUDED

#include "my_global.h"                          /* uint */

bool ip_to_hostname(struct sockaddr_storage *ip_storage,
                    const char *ip_string,
                    char **hostname, uint *connect_errors);
void inc_host_errors(const char *ip_string);
void reset_host_errors(const char *ip_string);
bool hostname_cache_init();
void hostname_cache_free();
void hostname_cache_refresh(void);

#endif /* HOSTNAME_INCLUDED */
