/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * NdbApiWrapper.hpp
 */

#ifndef NdbApiWrapper_hpp
#define NdbApiWrapper_hpp

// API to wrap
#include "NdbApi.hpp"
#include "NdbError.hpp"

struct NdbApiWrapper {
  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NDBAPI.h"

  static bool create_instance(Ndb_cluster_connection *p0, Uint32 p1, Uint32 p2,
                              Uint32 p3) {
    return ::create_instance(p0, p1, p2, p3);
  }

  static void drop_instance() { ::drop_instance(); }

  static Ndb *get_ndb_object(Uint32 &p0, const char *p1, const char *p2) {
    return ::get_ndb_object(p0, p1, p2);
  }

  static void return_ndb_object(Ndb *p0, Uint32 p1) {
    ::return_ndb_object(p0, p1);
  }

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_Ndb.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbBlob.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary.h"

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary.h"

  static int
      NdbDictionary__Dictionary__listObjects__0  // disambiguate overloaded
                                                 // const/non-const function
      (const NdbDictionary::Dictionary &obj,
       NdbDictionary::Dictionary::List &p0, NdbDictionary::Object::Type p1) {
    return obj.listObjects(p0, p1);
  }

  static int
      NdbDictionary__Dictionary__listIndexes__0  // disambiguate overloaded
                                                 // const/non-const function
      (const NdbDictionary::Dictionary &obj,
       NdbDictionary::Dictionary::List &p0, const char *p1) {
    return obj.listIndexes(p0, p1);
  }

  static int
      NdbDictionary__Dictionary__listEvents__0  // disambiguate overloaded
                                                // const/non-const function
      (const NdbDictionary::Dictionary &obj,
       NdbDictionary::Dictionary::List &p0) {
    return obj.listEvents(p0);
  }

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List.h"

  // ---------------------------------------------------------------------------

  // mapped by
  // "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Event.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Index.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Object.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Table.h"

  static const NdbDictionary::Column
      *NdbDictionary__Table__getColumn__0  // disambiguate overloaded
                                           // const/non-const function
      (const NdbDictionary::Table &obj, const char *p0) {
    return obj.getColumn(p0);
  }

  static const NdbDictionary::Column
      *NdbDictionary__Table__getColumn__1  // disambiguate overloaded
                                           // const/non-const function
      (const NdbDictionary::Table &obj, int p0) {
    return obj.getColumn(p0);
  }

  static NdbDictionary::Column
      *NdbDictionary__Table__getColumn__2  // disambiguate overloaded
                                           // const/non-const function
      (NdbDictionary::Table &obj, int p0) {
    return obj.getColumn(p0);
  }

  static NdbDictionary::Column
      *NdbDictionary__Table__getColumn__3  // disambiguate overloaded
                                           // const/non-const function
      (NdbDictionary::Table &obj, const char *p0) {
    return obj.getColumn(p0);
  }

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace.h"
  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbError.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbEventOperation.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexOperation.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbInterpretedCode.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation.h"

  static NdbBlob *NdbOperation__getBlobHandle__0  // disambiguate overloaded
                                                  // const/non-const function
      (const NdbOperation &obj, const char *p0) {
    return obj.getBlobHandle(p0);
  }

  static NdbBlob *NdbOperation__getBlobHandle__1  // disambiguate overloaded
                                                  // const/non-const function
      (const NdbOperation &obj, Uint32 p0) {
    return obj.getBlobHandle(p0);
  }

  static int NdbOperation__getNdbErrorLine(const NdbOperation &obj) {
    return obj.getNdbErrorLine();
  }

  static const NdbLockHandle
      *NdbOperation__getLockHandle__0  // disambiguate overloaded
                                       // const/non-const function
      (const NdbOperation &obj) {
    return obj.getLockHandle();
  }

  static const NdbLockHandle
      *NdbOperation__getLockHandle__1  // disambiguate overloaded
                                       // const/non-const function
      (NdbOperation &obj) {
    return obj.getLockHandle();
  }

  static NdbBlob *NdbOperation__getBlobHandle__2  // disambiguate overloaded
                                                  // const/non-const function
      (NdbOperation &obj, const char *p0) {
    return obj.getBlobHandle(p0);
  }

  static NdbBlob *NdbOperation__getBlobHandle__3  // disambiguate overloaded
                                                  // const/non-const function
      (NdbOperation &obj, Uint32 p0) {
    return obj.getBlobHandle(p0);
  }

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbRecAttr.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbScanFilter.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbScanOperation.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_NdbTransaction.h"

  static const NdbOperation
      *NdbTransaction__getNdbErrorOperation__0  // disambiguate overloaded
                                                // const/non-const function
      (const NdbTransaction &obj) {
    return obj.getNdbErrorOperation();
  }

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_Ndb_PartitionSpec.h"

  // ---------------------------------------------------------------------------

  // mapped by "com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection.h"

  // ---------------------------------------------------------------------------
};

#endif  // NdbApiWrapper_hpp
