/* Copyright (C) 2003 MySQL AB

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

#ifndef Ndb_mgmclient_h
#define Ndb_mgmclient_h

#ifdef __cplusplus
extern "C" {
#endif

typedef void* Ndb_mgmclient_handle;
Ndb_mgmclient_handle ndb_mgmclient_handle_create(const char *connect_string);
int ndb_mgmclient_execute(Ndb_mgmclient_handle, int argc, char** argv);
int ndb_mgmclient_handle_destroy(Ndb_mgmclient_handle);

#ifdef __cplusplus
}
#endif

#endif /* Ndb_mgmclient_h */
