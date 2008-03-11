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

%newobject NdbFactory::createNdbClusterConnection;
%typemap(newfree) Ndb_cluster_connection * "delete $1;";

%{

  class NdbFactory
  {

  public:
    static Ndb_cluster_connection * createNdbClusterConnection(const char * connectString = 0)
      {
        Ndb_cluster_connection * theConnection = NULL;
        if (connectString == 0) {
          theConnection = new Ndb_cluster_connection();
        } else {
          theConnection = new Ndb_cluster_connection(connectString);
        }
        return theConnection;
      }

    static Ndb * createNdb(Ndb_cluster_connection * theConn,const char* aCatalogName="", const char* aSchemaName="def") {
      return new Ndb(theConn,aCatalogName,aSchemaName);
    }

    static NdbTransaction * createTransaction(Ndb * theNdb, const NdbDictTable *table= 0,
                                              const char  *keyData = 0,
                                              Uint32       keyLen = 0) {
      return theNdb->startTransaction(table,keyData,keyLen);
    }

    static NdbTransaction* createTransaction(Ndb * theNdb,
                                             const char* aTableName,
                                             const char *keyData) {
      const NdbDictDictionary *myDict = theNdb->getDictionary();
      const NdbDictTable *myTable = myDict->getTable(aTableName);
      return theNdb->startTransaction(myTable,keyData);
    }
    static NdbTransaction* createTransaction(Ndb * theNdb,
                                             const char* aTableName,
                                             int keyData) {
      const NdbDictDictionary *myDict = theNdb->getDictionary();
      const NdbDictTable *myTable = myDict->getTable(aTableName);
      return theNdb->startTransaction(myTable,(const char *) &keyData);
    }


  };

  %}


class NdbFactory
{
  // We list these here as private so that SWIG doesnt generate them
  NdbFactory();
  ~NdbFactory();
public:

  %ndbexception("NdbApiException") {

    $action
      if (result==NULL) {
        NDB_exception(NdbApiException,"Couldn't allocate object");
      }
  }
  static Ndb_cluster_connection * createNdbClusterConnection(const char * connectString = 0);
  static Ndb * createNdb(Ndb_cluster_connection * theConn,const char* aCatalogName="", const char* aSchemaName="def");
  static NdbTransaction * createTransaction(Ndb * theNdb, const NdbDictTable *table= 0,
                                            const char  *keyData = 0,
                                            Uint32       keyLen = 0);
  static NdbTransaction* createTransaction(Ndb * theNdb,
                                           const char* aTableName,
                                           const char *keyData);
  static NdbTransaction* createTransaction(Ndb * theNdb,
                                           const char* aTableName,
                                           int keyData);

};
