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


#ifndef CLUSTER_CONNECTION_IMPL_HPP
#define CLUSTER_CONNECTION_IMPL_HPP

#include <ndb_cluster_connection.hpp>
#include <Vector.hpp>
#include <NdbMutex.h>

extern NdbMutex *g_ndb_connection_mutex;

class TransporterFacade;
class ConfigRetriever;
class NdbThread;
class ndb_mgm_configuration;

extern "C" {
  void* run_ndb_cluster_connection_connect_thread(void*);
}

class Ndb_cluster_connection_impl : public Ndb_cluster_connection
{
  Ndb_cluster_connection_impl(const char *connectstring);
  ~Ndb_cluster_connection_impl();

  void do_test();

  void init_get_next_node(Ndb_cluster_connection_node_iter &iter);
  Uint32 get_next_node(Ndb_cluster_connection_node_iter &iter);

  inline unsigned get_connect_count() const;
public:
  inline Uint64 *get_latest_trans_gci() { return &m_latest_trans_gci; }

private:
  friend class Ndb;
  friend class NdbImpl;
  friend void* run_ndb_cluster_connection_connect_thread(void*);
  friend class Ndb_cluster_connection;
  friend class NdbEventBuffer;
  
  struct Node
  {
    Node(Uint32 _g= 0, Uint32 _id= 0) : this_group(0),
					next_group(0),
					group(_g),
					id(_id) {};
    Uint32 this_group;
    Uint32 next_group;
    Uint32 group;
    Uint32 id;
  };

  Vector<Node> m_all_nodes;
  int init_nodes_vector(Uint32 nodeid, const ndb_mgm_configuration &config);
  void connect_thread();
  void set_name(const char *name);
  
  TransporterFacade *m_transporter_facade;
  ConfigRetriever *m_config_retriever;
  NdbThread *m_connect_thread;
  int (*m_connect_callback)(void);

  int m_optimized_node_selection;
  char *m_name;
  int m_run_connect_thread;
  NdbMutex *m_event_add_drop_mutex;
  Uint64 m_latest_trans_gci;
};

#endif
