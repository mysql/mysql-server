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
class ConfigRetriever;

class Ndb_cluster_connection {
public:
  Ndb_cluster_connection(const char * connect_string = 0);
  ~Ndb_cluster_connection();
  int connect();
private:
  char *m_connect_string;
  TransporterFacade *m_facade;
  ConfigRetriever *m_config_retriever;
};

#endif
