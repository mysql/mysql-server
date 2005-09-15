/* Copyright (C) 2003 MySQL AB

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


#ifndef CLUSTER_CONNECTION_IMPL_HPP
#define CLUSTER_CONNECTION_IMPL_HPP

#include <ndb_cluster_connection.hpp>
#include <Vector.hpp>

class TransporterFacade;
class ConfigRetriever;
class NdbThread;
class ndb_mgm_configuration;

struct Ndb_cluster_connection_node_iter {
  Ndb_cluster_connection_node_iter() : scan_state(~0),
				       init_pos(0),
				       cur_pos(0) {};
  Uint8 scan_state;
  Uint8 init_pos;
  Uint8 cur_pos;
};

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
  void init_nodes_vector(Uint32 nodeid, const ndb_mgm_configuration &config);
  void connect_thread();
  
  TransporterFacade *m_transporter_facade;
  ConfigRetriever *m_config_retriever;
  NdbThread *m_connect_thread;
  int (*m_connect_callback)(void);

  int m_optimized_node_selection;
};

#endif
