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


#ifndef CLUSTER_CONNECTION_HPP
#define CLUSTER_CONNECTION_HPP

class TransporterFacade;
class LocalConfig;
class ConfigRetriever;
class NdbThread;

extern "C" {
  void* run_ndb_cluster_connection_connect_thread(void*);
}

class Ndb_cluster_connection {
public:
  Ndb_cluster_connection(const char * connect_string = 0);
  ~Ndb_cluster_connection();
  int connect(int reconnect= 0);
  int start_connect_thread(int (*connect_callback)(void)= 0);
private:
  friend void* run_ndb_cluster_connection_connect_thread(void*);
  void connect_thread();
  char *m_connect_string;
  TransporterFacade *m_facade;
  LocalConfig *m_local_config;
  ConfigRetriever *m_config_retriever;
  NdbThread *m_connect_thread;
  int (*m_connect_callback)(void);
};

#endif
