/*
 Copyright (c) 2013 Oracle and/or its affiliates. All rights reserved.

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

#ifndef NdbWaitGroup_H
#define NdbWaitGroup_H

class Ndb_cluster_connection;
class Ndb;

/* This is a stub include file for NdbWaitGroup */

class NdbWaitGroup {

private:
  NdbWaitGroup(Ndb_cluster_connection *conn, int max_ndb_objects);
  ~NdbWaitGroup();

public:
  void wakeup();

#ifdef USE_OLD_MULTIWAIT_API

  bool addNdb(Ndb *);
  int wait(Ndb ** & arrayHead, Uint32 timeout_millis, int min_ready = 1 );  

#else 

  int push(Ndb *ndb);
  int wait(Uint32 timeout_millis, int pct_ready = 50); 
  Ndb * pop();

#endif
};


#endif

