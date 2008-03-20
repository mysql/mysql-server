/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

class Ndb_cluster_connection {

public:

  // NdbFactory.getNdbClusterConnection should be used instead
  Ndb_cluster_connection(const char * connectstring = 0);

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename set_name setName;
  %rename wait_until_ready waitUntilReady;
  %rename set_timeout setTimeout;
#endif

  %ndbnoexception;

  void set_name(const char* name);

  %ndbexception("NdbApiException") {
    $action
      if (result > 0) {
        const char * msg = "Setting timeout failed";
        NDB_exception(NdbApiException,msg);
      }
  }

  voidint set_timeout(int timeout_ms);


  %ndbexception("NdbClusterConnectionPermanentException,"
                "NdbClusterConnectionTemporaryException") {
    $action
      if (result > 0) {
        const char * msg = "Connect to management server failed";
        NDB_exception(NdbClusterConnectionPermanentException,msg);
      } else if (result < 0) {
        const char * msg = "Connect to management server failed";
        NDB_exception(NdbClusterConnectionTemporaryException,msg);
      }
  }
  %typemap(check) int retry_delay_in_seconds {
    if ($1 < 0) {
      NDB_exception(NdbClusterConnectionPermanentException,
                    "Delay must be greater than or equal to zero.");
    }
  }
  int connect(int no_retries=0, int retry_delay_in_seconds=1,
              bool verbose=false);

  %ndbexception("NdbApiException") {
    $action
      if (result) {
        const char * msg = "Cluster was not ready";
        NDB_exception(NdbApiException,msg);
      }
  }

  int wait_until_ready(int timeoutFor_firstAlive,
                       int timeoutAfterFirstAlive);

};

%newobject Ndb_cluster_connection::createNdb;
%typemap(newfree) Ndb * "delete $1;";
%delobject Ndb_cluster_connection::close;

%extend Ndb_cluster_connection {

  %ndbnoexception;

  bool close() {
    delete self;
    return true;
  }

  void deleteAllNdbObjects() {

    const Ndb * tmpNdb = NULL;

    self->lock_ndb_objects();

    tmpNdb = self->get_next_ndb_object(NULL);
    while (tmpNdb != NULL) {
      const Ndb * delNdb = tmpNdb;
      tmpNdb = self->get_next_ndb_object(tmpNdb);
      delete delNdb;
    }

    tmpNdb = NULL;

    // Leave Ndb objects locked until we delete the mutex
    delete self;

  }

public:
  %ndbexception("NdbApiException") createNdb {
    $action
      if (result==NULL) {
        NDB_exception(NdbApiException,"Couldn't allocate an Ndb object");
      }
  }

  %contract createNdb(const char* aCatalogName, const char* aSchemaName,
                      Int32 initThreads) {
require:
    initThreads > 0;
  }
  Ndb* createNdb(const char* aCatalogName="", const char* aSchemaName="def",
                 Int32 initThreads = 4) {
    Ndb * theNdb = new Ndb(self,aCatalogName,aSchemaName);
    if (theNdb!=NULL) {
      int ret = theNdb->init(initThreads);
      if ( ret ) {
        delete theNdb;
        return NULL;
      }
    }
    return theNdb;
  }

  %contract createNdb(const char* aCatalogName, Int32 initThreads) {
require:
    initThreads > 0;
  }
  Ndb* createNdb(const char* aCatalogName, Int32 initThreads = 4) {
    Ndb * theNdb = new Ndb(self,aCatalogName);
    if (theNdb!=NULL) {
      int ret = theNdb->init(initThreads);
      if ( ret ) {
        delete theNdb;
        return NULL;
      }
    }
    return theNdb;
  }

  %ndbexception("NdbApiException") {
    $action
      if (result) {
        const char * msg = "Cluster was not ready";
        NDB_exception(NdbApiException,msg);
      }
  }

  %ndbnoexception;

}
