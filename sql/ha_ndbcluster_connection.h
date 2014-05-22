/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
int ndbcluster_connect(int (*connect_callback)(void),
                       ulong wait_connected,
                       uint connection_pool_size,
                       bool optimized_node_select,
                       const char* connect_string,
                       uint force_nodeid,
                       uint recv_thread_activation_threshold);

void ndbcluster_disconnect(void);

Ndb_cluster_connection *ndb_get_cluster_connection();
ulonglong ndb_get_latest_trans_gci();
void ndb_set_latest_trans_gci(ulonglong val);
int ndb_has_node_id(uint id);
int ndb_set_recv_thread_activation_threshold(Uint32 threshold);
int ndb_set_recv_thread_cpu(Uint16 *cpuid_array,
                            Uint32 cpuid_array_size);
void ndb_get_connection_stats(Uint64* statsArr);

/* perform random sleep in the range milli_sleep to 2*milli_sleep */
inline void do_retry_sleep(unsigned milli_sleep)
{
  my_sleep(1000*(milli_sleep + 5*(rand()%(milli_sleep/5))));
}
