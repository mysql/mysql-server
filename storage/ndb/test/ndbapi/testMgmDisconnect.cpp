/* 
   Copyright (c) 2008, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdlib.h>

#include <mgmapi.h>
#include <NdbSleep.h>

int main()
{
  NdbMgmHandle handle= ndb_mgm_create_handle();

  while(1==1){
    if (ndb_mgm_connect(handle,0,0,0) == -1){
      printf("connect failed, error: '%d: %s'\n",
             ndb_mgm_get_latest_error(handle),
             ndb_mgm_get_latest_error_desc(handle));
      NdbSleep_SecSleep(1);
      continue;
    }

    while(ndb_mgm_is_connected(handle) != 0){
      struct ndb_mgm_cluster_state *state= ndb_mgm_get_status(handle);

      if(state==NULL){
        printf("ndb_mgm_get_status failed, error: '%d: %s', line: %d\n",
               ndb_mgm_get_latest_error(handle),
               ndb_mgm_get_latest_error_desc(handle),
               ndb_mgm_get_latest_error_line(handle));
        continue;
      }

      int i= 0;
      for(i=0; i < state->no_of_nodes; i++)
      {
        struct ndb_mgm_node_state *node_state= &state->node_states[i];
        printf("node with ID=%d ", node_state->node_id);

        if(node_state->version != 0)
          printf("connected\n");
        else
          printf("not connected\n");
      }
      free((void*)state);
    }
  }

  ndb_mgm_destroy_handle(&handle);
  return 1;
}
