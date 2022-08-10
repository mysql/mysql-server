/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

    static bool
    create_instance
    ( Ndb_cluster_connection * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return ::create_instance(p0, p1, p2, p3);
    }

    static void
    drop_instance
    ( )
    {
        ::drop_instance();
    }

    static Ndb *
    get_ndb_object
    ( Uint32 & p0, const char * p1, const char * p2 )
    {
        return ::get_ndb_object(p0, p1, p2);
    }

    static void
    return_ndb_object
    ( Ndb * p0, Uint32 p1 )
    {
        ::return_ndb_object(p0, p1);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_Ndb.h"

    static int 
    Ndb__getAutoIncrementValue
    ( Ndb & obj , const NdbDictionary::Table * p1, Uint64 & ret, Uint32 batch, Uint64 step, Uint64 start )
    {
        return obj.getAutoIncrementValue(p1, ret, batch, step, start);
    }

    static const char *
    Ndb__getDatabaseName
    ( const Ndb & obj )
    {
        return obj.getDatabaseName();
    }

    static const char *
    Ndb__getDatabaseSchemaName
    ( const Ndb & obj )
    {
        return obj.getDatabaseSchemaName();
    }

    static NdbDictionary::Dictionary *
    Ndb__getDictionary
    ( const Ndb & obj )
    {
        return obj.getDictionary();
    }

    static const NdbError &
    Ndb__getNdbError__0 // disambiguate overloaded function
    ( const Ndb & obj )
    {
        return obj.getNdbError();
    }

    static const char *
    Ndb__getNdbErrorDetail
    ( const Ndb & obj, const NdbError & p0, char * p1, Uint32 p2 )
    {
        return obj.getNdbErrorDetail(p0, p1, p2);
    }

    static int
    Ndb__setDatabaseName
    ( Ndb & obj, const char * p0 )
    {
        return obj.setDatabaseName(p0);
    }

    static int
    Ndb__setDatabaseSchemaName
    ( Ndb & obj, const char * p0 )
    {
        return obj.setDatabaseSchemaName(p0);
    }

    static int
    Ndb__init
    ( Ndb & obj, int p0 )
    {
        return obj.init(p0);
    }

    static NdbEventOperation *
    Ndb__createEventOperation
    ( Ndb & obj, const char * p0 )
    {
        return obj.createEventOperation(p0);
    }

    static int
    Ndb__dropEventOperation
    ( Ndb & obj, NdbEventOperation * p0 )
    {
        return obj.dropEventOperation(p0);
    }

    static int
    Ndb__pollEvents
    ( Ndb & obj, int p0, Uint64 * p1 )
    {
        return obj.pollEvents(p0, p1);
    }

    static NdbEventOperation *
    Ndb__nextEvent
    ( Ndb & obj )
    {
        return obj.nextEvent();
    }

    static bool
    Ndb__isConsistent
    ( Ndb & obj, Uint64 & p0 )
    {
        return obj.isConsistent(p0);
    }

    static bool
    Ndb__isConsistentGCI
    ( Ndb & obj, Uint64 p0 )
    {
        return obj.isConsistentGCI(p0);
    }

    static const NdbEventOperation *
    Ndb__getGCIEventOperations
    ( Ndb & obj, Uint32 * p0, Uint32 * p1 )
    {
        return obj.getGCIEventOperations(p0, p1);
    }

    static NdbTransaction *
    Ndb__startTransaction__0 // disambiguate overloaded function
    ( Ndb & obj, const NdbDictionary::Table * p0, const char * p1, Uint32 p2 )
    {
        return obj.startTransaction(p0, p1, p2);
    }

    static NdbTransaction *
    Ndb__startTransaction__1 // disambiguate overloaded function
    ( Ndb & obj, const NdbDictionary::Table * p0, const Ndb::Key_part_ptr * p1, void * p2, Uint32 p3 )
    {
        return obj.startTransaction(p0, p1, p2, p3);
    }

    static NdbTransaction *
    Ndb__startTransaction
    ( Ndb & obj, const NdbDictionary::Table * p0, Uint32 p1 )
    {
        return obj.startTransaction(p0, p1);
    }

    static int
    Ndb__computeHash
    ( Uint32 * p0, const NdbDictionary::Table * p1, const Ndb::Key_part_ptr * p2, void * p3, Uint32 p4 )
    {
        return Ndb::computeHash(p0, p1, p2, p3, p4);
    }

    static void
    Ndb__closeTransaction
    ( Ndb & obj, NdbTransaction * p0 )
    {
        obj.closeTransaction(p0);
    }

    static const NdbError &
    Ndb__getNdbError__1 // disambiguate overloaded function
    ( Ndb & obj, int p0 )
    {
        return obj.getNdbError(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbBlob.h"

    static const NdbError &
    NdbBlob__getNdbError
    ( const NdbBlob & obj )
    {
        return obj.getNdbError();
    }

    static const NdbOperation *
    NdbBlob__getNdbOperation
    ( const NdbBlob & obj )
    {
        return obj.getNdbOperation();
    }

    static NdbBlob::State
    NdbBlob__getState
    ( NdbBlob & obj )
    {
        return obj.getState();
    }

    static void
    NdbBlob__getVersion
    ( NdbBlob & obj, int & p0 )
    {
        obj.getVersion(p0);
    }

    static int
    NdbBlob__getValue
    ( NdbBlob & obj, void * p0, Uint32 p1 )
    {
        return obj.getValue(p0, p1);
    }

    static int
    NdbBlob__setValue
    ( NdbBlob & obj, const void * p0, Uint32 p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbBlob__getNull
    ( NdbBlob & obj, int & p0 )
    {
        return obj.getNull(p0);
    }

    static int
    NdbBlob__setNull
    ( NdbBlob & obj )
    {
        return obj.setNull();
    }

    static int
    NdbBlob__getLength
    ( NdbBlob & obj, Uint64 & p0 )
    {
        return obj.getLength(p0);
    }

    static int
    NdbBlob__truncate
    ( NdbBlob & obj, Uint64 p0 )
    {
        return obj.truncate(p0);
    }

    static int
    NdbBlob__getPos
    ( NdbBlob & obj, Uint64 & p0 )
    {
        return obj.getPos(p0);
    }

    static int
    NdbBlob__setPos
    ( NdbBlob & obj, Uint64 p0 )
    {
        return obj.setPos(p0);
    }

    static int
    NdbBlob__readData
    ( NdbBlob & obj, void * p0, Uint32 & p1 )
    {
        return obj.readData(p0, p1);
    }

    static int
    NdbBlob__writeData
    ( NdbBlob & obj, const void * p0, Uint32 p1 )
    {
        return obj.writeData(p0, p1);
    }

    static const NdbDictionary::Column *
    NdbBlob__getColumn
    ( NdbBlob & obj )
    {
        return obj.getColumn();
    }

    static int
    NdbBlob__getBlobTableName
    ( char * p0, Ndb * p1, const char * p2, const char * p3 )
    {
        return NdbBlob::getBlobTableName(p0, p1, p2, p3);
    }

    static int
    NdbBlob__getBlobEventName
    ( char * p0, Ndb * p1, const char * p2, const char * p3 )
    {
        return NdbBlob::getBlobEventName(p0, p1, p2, p3);
    }

    static NdbBlob *
    NdbBlob__blobsFirstBlob
    ( NdbBlob & obj )
    {
        return obj.blobsFirstBlob();
    }

    static NdbBlob *
    NdbBlob__blobsNextBlob
    ( NdbBlob & obj )
    {
        return obj.blobsNextBlob();
    }

    static int
    NdbBlob__close
    ( NdbBlob & obj, bool p0 )
    {
        return obj.close(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary.h"

    static NdbDictionary::RecordType
    NdbDictionary__getRecordType
    ( const NdbRecord * p0 )
    {
        return NdbDictionary::getRecordType(p0);
    }

    static const char *
    NdbDictionary__getRecordTableName
    ( const NdbRecord * p0 )
    {
        return NdbDictionary::getRecordTableName(p0);
    }

    static const char *
    NdbDictionary__getRecordIndexName
    ( const NdbRecord * p0 )
    {
        return NdbDictionary::getRecordIndexName(p0);
    }

    static bool
    NdbDictionary__getFirstAttrId
    ( const NdbRecord * p0, Uint32 & p1 )
    {
        return NdbDictionary::getFirstAttrId(p0, p1);
    }

    static bool
    NdbDictionary__getNextAttrId
    ( const NdbRecord * p0, Uint32 & p1 )
    {
        return NdbDictionary::getNextAttrId(p0, p1);
    }

    static bool
    NdbDictionary__getOffset
    ( const NdbRecord * p0, Uint32 p1, Uint32 & p2 )
    {
        return NdbDictionary::getOffset(p0, p1, p2);
    }

    static bool
    NdbDictionary__getNullBitOffset
    ( const NdbRecord * p0, Uint32 p1, Uint32 & p2, Uint32 & p3 )
    {
        return NdbDictionary::getNullBitOffset(p0, p1, p2, p3);
    }

    static const char *
    NdbDictionary__getValuePtr
    ( const NdbRecord * p0, const char * p1, Uint32 p2 )
    {
        return NdbDictionary::getValuePtr(p0, p1, p2);
    }

    static bool
    NdbDictionary__isNull
    ( const NdbRecord * p0, const char * p1, Uint32 p2 )
    {
        return NdbDictionary::isNull(p0, p1, p2);
    }

    static int
    NdbDictionary__setNull
    ( const NdbRecord * p0, char * p1, Uint32 p2, bool p3 )
    {
        return NdbDictionary::setNull(p0, p1, p2, p3);
    }

    static Uint32
    NdbDictionary__getRecordRowLength
    ( const NdbRecord * p0 )
    {
        return NdbDictionary::getRecordRowLength(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Column.h"

    static bool
    NdbDictionary__Column__getAutoIncrement
    ( const NdbDictionary::Column & obj )
    {
        return obj.getAutoIncrement();
    }

    static const char *
    NdbDictionary__Column__getName
    ( const NdbDictionary::Column & obj )
    {
        return obj.getName();
    }

    static bool
    NdbDictionary__Column__getNullable
    ( const NdbDictionary::Column & obj )
    {
        return obj.getNullable();
    }

    static bool
    NdbDictionary__Column__getPrimaryKey
    ( const NdbDictionary::Column & obj )
    {
        return obj.getPrimaryKey();
    }

    static int
    NdbDictionary__Column__getColumnNo
    ( const NdbDictionary::Column & obj )
    {
        return obj.getColumnNo();
    }

    static bool
    NdbDictionary__Column__equal
    ( const NdbDictionary::Column & obj, const NdbDictionary::Column & p0 )
    {
        return obj.equal(p0);
    }

    static NdbDictionary::Column::Type
    NdbDictionary__Column__getType
    ( const NdbDictionary::Column & obj )
    {
        return obj.getType();
    }

    static int
    NdbDictionary__Column__getPrecision
    ( const NdbDictionary::Column & obj )
    {
        return obj.getPrecision();
    }

    static int
    NdbDictionary__Column__getScale
    ( const NdbDictionary::Column & obj )
    {
        return obj.getScale();
    }

    static int
    NdbDictionary__Column__getLength
    ( const NdbDictionary::Column & obj )
    {
        return obj.getLength();
    }

    static int
    NdbDictionary__Column__getCharsetNumber
    ( const NdbDictionary::Column & obj )
    {
        return obj.getCharsetNumber();
    }

    static int
    NdbDictionary__Column__getInlineSize
    ( const NdbDictionary::Column & obj )
    {
        return obj.getInlineSize();
    }

    static int
    NdbDictionary__Column__getPartSize
    ( const NdbDictionary::Column & obj )
    {
        return obj.getPartSize();
    }

    static int
    NdbDictionary__Column__getStripeSize
    ( const NdbDictionary::Column & obj )
    {
        return obj.getStripeSize();
    }

    static int
    NdbDictionary__Column__getSize
    ( const NdbDictionary::Column & obj )
    {
        return obj.getSize();
    }

    static bool
    NdbDictionary__Column__getPartitionKey
    ( const NdbDictionary::Column & obj )
    {
        return obj.getPartitionKey();
    }

    static NdbDictionary::Column::ArrayType
    NdbDictionary__Column__getArrayType
    ( const NdbDictionary::Column & obj )
    {
        return obj.getArrayType();
    }

    static NdbDictionary::Column::StorageType
    NdbDictionary__Column__getStorageType
    ( const NdbDictionary::Column & obj )
    {
        return obj.getStorageType();
    }

    static bool
    NdbDictionary__Column__getDynamic
    ( const NdbDictionary::Column & obj )
    {
        return obj.getDynamic();
    }

    static bool
    NdbDictionary__Column__getIndexSourced
    ( const NdbDictionary::Column & obj )
    {
        return obj.getIndexSourced();
    }

    static int
    NdbDictionary__Column__setName
    ( NdbDictionary::Column & obj, const char * p0 )
    {
        return obj.setName(p0);
    }

    static void
    NdbDictionary__Column__setNullable
    ( NdbDictionary::Column & obj, bool p0 )
    {
        obj.setNullable(p0);
    }

    static void
    NdbDictionary__Column__setPrimaryKey
    ( NdbDictionary::Column & obj, bool p0 )
    {
        obj.setPrimaryKey(p0);
    }

    static void
    NdbDictionary__Column__setType
    ( NdbDictionary::Column & obj, NdbDictionary::Column::Type p0 )
    {
        obj.setType(p0);
    }

    static void
    NdbDictionary__Column__setPrecision
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setPrecision(p0);
    }

    static void
    NdbDictionary__Column__setScale
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setScale(p0);
    }

    static void
    NdbDictionary__Column__setLength
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setLength(p0);
    }

    static void
    NdbDictionary__Column__setInlineSize
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setInlineSize(p0);
    }

    static void
    NdbDictionary__Column__setPartSize
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setPartSize(p0);
    }

    static void
    NdbDictionary__Column__setStripeSize
    ( NdbDictionary::Column & obj, int p0 )
    {
        obj.setStripeSize(p0);
    }

    static void
    NdbDictionary__Column__setPartitionKey
    ( NdbDictionary::Column & obj, bool p0 )
    {
        obj.setPartitionKey(p0);
    }

    static void
    NdbDictionary__Column__setArrayType
    ( NdbDictionary::Column & obj, NdbDictionary::Column::ArrayType p0 )
    {
        obj.setArrayType(p0);
    }

    static void
    NdbDictionary__Column__setStorageType
    ( NdbDictionary::Column & obj, NdbDictionary::Column::StorageType p0 )
    {
        obj.setStorageType(p0);
    }

    static void
    NdbDictionary__Column__setDynamic
    ( NdbDictionary::Column & obj, bool p0 )
    {
        obj.setDynamic(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile.h"

    static const char *
    NdbDictionary__Datafile__getPath
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getPath();
    }

    static Uint64
    NdbDictionary__Datafile__getSize
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getSize();
    }

    static Uint64
    NdbDictionary__Datafile__getFree
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getFree();
    }

    static const char *
    NdbDictionary__Datafile__getTablespace
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getTablespace();
    }

    static void
    NdbDictionary__Datafile__getTablespaceId
    ( const NdbDictionary::Datafile & obj, NdbDictionary::ObjectId * p0 )
    {
        obj.getTablespaceId(p0);
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Datafile__getObjectStatus
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__Datafile__getObjectVersion
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__Datafile__getObjectId
    ( const NdbDictionary::Datafile & obj )
    {
        return obj.getObjectId();
    }

    static void
    NdbDictionary__Datafile__setPath
    ( NdbDictionary::Datafile & obj, const char * p0 )
    {
        obj.setPath(p0);
    }

    static void
    NdbDictionary__Datafile__setSize
    ( NdbDictionary::Datafile & obj, Uint64 p0 )
    {
        obj.setSize(p0);
    }

    static int
    NdbDictionary__Datafile__setTablespace__0 // disambiguate overloaded function
    ( NdbDictionary::Datafile & obj, const char * p0 )
    {
        return obj.setTablespace(p0);
    }

    static int
    NdbDictionary__Datafile__setTablespace__1 // disambiguate overloaded function
    ( NdbDictionary::Datafile & obj, const NdbDictionary::Tablespace & p0 )
    {
        return obj.setTablespace(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary.h"

    static int
    NdbDictionary__Dictionary__listObjects__0 // disambiguate overloaded const/non-const function
    ( const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & p0, NdbDictionary::Object::Type p1 )
    {
        return obj.listObjects(p0, p1);
    }

    static const NdbError &
    NdbDictionary__Dictionary__getNdbError
    ( const NdbDictionary::Dictionary & obj )
    {
        return obj.getNdbError();
    }

    static const NdbDictionary::Table *
    NdbDictionary__Dictionary__getTable
    ( const NdbDictionary::Dictionary & obj, const char * p0 )
    {
        return obj.getTable(p0);
    }

    static const NdbDictionary::Index *
    NdbDictionary__Dictionary__getIndex
    ( const NdbDictionary::Dictionary & obj, const char * p0, const char * p1 )
    {
        return obj.getIndex(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__listIndexes__0 // disambiguate overloaded const/non-const function
    ( const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & p0, const char * p1 )
    {
        return obj.listIndexes(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__listEvents__0 // disambiguate overloaded const/non-const function
    ( const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & p0 )
    {
        return obj.listEvents(p0);
    }

    static int
    NdbDictionary__Dictionary__createEvent
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Event & p0 )
    {
        return obj.createEvent(p0);
    }

    static int
    NdbDictionary__Dictionary__dropEvent
    ( NdbDictionary::Dictionary & obj, const char * p0, int p1 )
    {
        return obj.dropEvent(p0, p1);
    }

    static const NdbDictionary::Event *
    NdbDictionary__Dictionary__getEvent
    ( NdbDictionary::Dictionary & obj, const char * p0 )
    {
        return obj.getEvent(p0);
    }

    static int
    NdbDictionary__Dictionary__createTable
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Table & p0 )
    {
        return obj.createTable(p0);
    }

    static int
    NdbDictionary__Dictionary__optimizeTable
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Table & p0, NdbDictionary::OptimizeTableHandle & p1 )
    {
        return obj.optimizeTable(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__optimizeIndex
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Index & p0, NdbDictionary::OptimizeIndexHandle & p1 )
    {
        return obj.optimizeIndex(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__dropTable__0 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, NdbDictionary::Table & p0 )
    {
        return obj.dropTable(p0);
    }

    static int
    NdbDictionary__Dictionary__dropTable__1 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const char * p0 )
    {
        return obj.dropTable(p0);
    }

    static bool
    NdbDictionary__Dictionary__supportedAlterTable
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Table & p0, const NdbDictionary::Table & p1 )
    {
        return obj.supportedAlterTable(p0, p1);
    }

    static void
    NdbDictionary__Dictionary__removeCachedTable__0 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const char * p0 )
    {
        obj.removeCachedTable(p0);
    }

    static void
    NdbDictionary__Dictionary__removeCachedIndex__1 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const char * p0, const char * p1 )
    {
        obj.removeCachedIndex(p0, p1);
    }

    static void
    NdbDictionary__Dictionary__invalidateTable__0 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const char * p0 )
    {
        obj.invalidateTable(p0);
    }

    static void
    NdbDictionary__Dictionary__invalidateIndex__1 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const char * p0, const char * p1 )
    {
        obj.invalidateIndex(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__createIndex__0 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Index & p0, bool p1 )
    {
        return obj.createIndex(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__createIndex__1 // disambiguate overloaded function
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Index & p0, const NdbDictionary::Table & p1, bool p2 )
    {
        return obj.createIndex(p0, p1, p2);
    }

    static int
    NdbDictionary__Dictionary__dropIndex
    ( NdbDictionary::Dictionary & obj, const char * p0, const char * p1 )
    {
        return obj.dropIndex(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__createLogfileGroup
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::LogfileGroup & p0, NdbDictionary::ObjectId * p1 )
    {
        return obj.createLogfileGroup(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__dropLogfileGroup
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::LogfileGroup & p0 )
    {
        return obj.dropLogfileGroup(p0);
    }

    static int
    NdbDictionary__Dictionary__createTablespace
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Tablespace & p0, NdbDictionary::ObjectId * p1 )
    {
        return obj.createTablespace(p0, p1);
    }

    static int
    NdbDictionary__Dictionary__dropTablespace
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Tablespace & p0 )
    {
        return obj.dropTablespace(p0);
    }

    static int
    NdbDictionary__Dictionary__createDatafile
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Datafile & p0, bool p1, NdbDictionary::ObjectId * p2 )
    {
        return obj.createDatafile(p0, p1, p2);
    }

    static int
    NdbDictionary__Dictionary__dropDatafile
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Datafile & p0 )
    {
        return obj.dropDatafile(p0);
    }

    static int
    NdbDictionary__Dictionary__createUndofile
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Undofile & p0, bool p1, NdbDictionary::ObjectId * p2 )
    {
        return obj.createUndofile(p0, p1, p2);
    }

    static int
    NdbDictionary__Dictionary__dropUndofile
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Undofile & p0 )
    {
        return obj.dropUndofile(p0);
    }

    static NdbRecord *
    NdbDictionary__Dictionary__createRecord__0
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Table * p0, const NdbDictionary::RecordSpecification * p1, Uint32 p2, Uint32 p3, Uint32 p4 )
    {
        return obj.createRecord(p0, p1, p2, p3, p4);
    }

    static NdbRecord *
    NdbDictionary__Dictionary__createRecord__1
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Index * p0, const NdbDictionary::Table * p1, const NdbDictionary::RecordSpecification * p2, Uint32 p3, Uint32 p4, Uint32 p5 )
    {
        return obj.createRecord(p0, p1, p2, p3, p4, p5);
    }

    static NdbRecord *
    NdbDictionary__Dictionary__createRecord__2
    ( NdbDictionary::Dictionary & obj, const NdbDictionary::Index * p0, const NdbDictionary::RecordSpecification * p1, Uint32 p2, Uint32 p3, Uint32 p4 )
    {
        return obj.createRecord(p0, p1, p2, p3, p4);
    }

    static void
    NdbDictionary__Dictionary__releaseRecord
    ( NdbDictionary::Dictionary & obj, NdbRecord * p0 )
    {
        obj.releaseRecord(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Event.h"

    static const char *
    NdbDictionary__Event__getName
    ( const NdbDictionary::Event & obj )
    {
        return obj.getName();
    }

    static const NdbDictionary::Table *
    NdbDictionary__Event__getTable
    ( const NdbDictionary::Event & obj )
    {
        return obj.getTable();
    }

    static const char *
    NdbDictionary__Event__getTableName
    ( const NdbDictionary::Event & obj )
    {
        return obj.getTableName();
    }

    static bool
    NdbDictionary__Event__getTableEvent
    ( const NdbDictionary::Event & obj, NdbDictionary::Event::TableEvent p0 )
    {
        return obj.getTableEvent(p0);
    }

    static NdbDictionary::Event::EventDurability
    NdbDictionary__Event__getDurability
    ( const NdbDictionary::Event & obj )
    {
        return obj.getDurability();
    }

    static NdbDictionary::Event::EventReport
    NdbDictionary__Event__getReport
    ( const NdbDictionary::Event & obj )
    {
        return obj.getReport();
    }

    static int
    NdbDictionary__Event__getNoOfEventColumns
    ( const NdbDictionary::Event & obj )
    {
        return obj.getNoOfEventColumns();
    }

    static const NdbDictionary::Column *
    NdbDictionary__Event__getEventColumn
    ( const NdbDictionary::Event & obj, unsigned int p0 )
    {
        return obj.getEventColumn(p0);
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Event__getObjectStatus
    ( const NdbDictionary::Event & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__Event__getObjectVersion
    ( const NdbDictionary::Event & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__Event__getObjectId
    ( const NdbDictionary::Event & obj )
    {
        return obj.getObjectId();
    }

    static int
    NdbDictionary__Event__setName
    ( NdbDictionary::Event & obj, const char * p0 )
    {
        return obj.setName(p0);
    }

    static void
    NdbDictionary__Event__setTable__0 // disambiguate overloaded function
    ( NdbDictionary::Event & obj, const NdbDictionary::Table & p0 )
    {
        obj.setTable(p0);
    }

    static int
    NdbDictionary__Event__setTable__1 // disambiguate overloaded function
    ( NdbDictionary::Event & obj, const char * p0 )
    {
        return obj.setTable(p0);
    }

    static void
    NdbDictionary__Event__addTableEvent
    ( NdbDictionary::Event & obj, NdbDictionary::Event::TableEvent p0 )
    {
        obj.addTableEvent(p0);
    }

    static void
    NdbDictionary__Event__setDurability
    ( NdbDictionary::Event & obj, NdbDictionary::Event::EventDurability p0 )
    {
        obj.setDurability(p0);
    }

    static void
    NdbDictionary__Event__setReport
    ( NdbDictionary::Event & obj, NdbDictionary::Event::EventReport p0 )
    {
        obj.setReport(p0);
    }

    static void
    NdbDictionary__Event__addEventColumn__0 // disambiguate overloaded function
    ( NdbDictionary::Event & obj, unsigned int p0 )
    {
        obj.addEventColumn(p0);
    }

    static void
    NdbDictionary__Event__addEventColumn__1 // disambiguate overloaded function
    ( NdbDictionary::Event & obj, const char * p0 )
    {
        obj.addEventColumn(p0);
    }

    static void
    NdbDictionary__Event__addEventColumns
    ( NdbDictionary::Event & obj, int p0, const char * * p1 )
    {
        obj.addEventColumns(p0, p1);
    }

    static void
    NdbDictionary__Event__mergeEvents
    ( NdbDictionary::Event & obj, bool p0 )
    {
        obj.mergeEvents(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Index.h"

    static const char *
    NdbDictionary__Index__getName
    ( const NdbDictionary::Index & obj )
    {
        return obj.getName();
    }

    static const char *
    NdbDictionary__Index__getTable
    ( const NdbDictionary::Index & obj )
    {
        return obj.getTable();
    }

    static unsigned int
    NdbDictionary__Index__getNoOfColumns
    ( const NdbDictionary::Index & obj )
    {
        return obj.getNoOfColumns();
    }

    static const NdbDictionary::Column *
    NdbDictionary__Index__getColumn
    ( const NdbDictionary::Index & obj, unsigned int p0 )
    {
        return obj.getColumn(p0);
    }

    static NdbDictionary::Index::Type
    NdbDictionary__Index__getType
    ( const NdbDictionary::Index & obj )
    {
        return obj.getType();
    }

    static bool
    NdbDictionary__Index__getLogging
    ( const NdbDictionary::Index & obj )
    {
        return obj.getLogging();
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Index__getObjectStatus
    ( const NdbDictionary::Index & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__Index__getObjectVersion
    ( const NdbDictionary::Index & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__Index__getObjectId
    ( const NdbDictionary::Index & obj )
    {
        return obj.getObjectId();
    }

    static const NdbRecord *
    NdbDictionary__Index__getDefaultRecord
    ( const NdbDictionary::Index & obj )
    {
        return obj.getDefaultRecord();
    }

    static int
    NdbDictionary__Index__setName
    ( NdbDictionary::Index & obj, const char * p0 )
    {
        return obj.setName(p0);
    }

    static int
    NdbDictionary__Index__setTable
    ( NdbDictionary::Index & obj, const char * p0 )
    {
        return obj.setTable(p0);
    }

    static int
    NdbDictionary__Index__addColumn
    ( NdbDictionary::Index & obj, const NdbDictionary::Column & p0 )
    {
        return obj.addColumn(p0);
    }

    static int
    NdbDictionary__Index__addColumnName
    ( NdbDictionary::Index & obj, const char * p0 )
    {
        return obj.addColumnName(p0);
    }

    static int
    NdbDictionary__Index__addColumnNames
    ( NdbDictionary::Index & obj, unsigned int p0, const char * * p1 )
    {
        return obj.addColumnNames(p0, p1);
    }

    static void
    NdbDictionary__Index__setType
    ( NdbDictionary::Index & obj, NdbDictionary::Index::Type p0 )
    {
        obj.setType(p0);
    }

    static void
    NdbDictionary__Index__setLogging
    ( NdbDictionary::Index & obj, bool p0 )
    {
        obj.setLogging(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup.h"

    static const char *
    NdbDictionary__LogfileGroup__getName
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getName();
    }

    static Uint32
    NdbDictionary__LogfileGroup__getUndoBufferSize
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getUndoBufferSize();
    }

    static const NdbDictionary::AutoGrowSpecification &
    NdbDictionary__LogfileGroup__getAutoGrowSpecification
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getAutoGrowSpecification();
    }

    static Uint64
    NdbDictionary__LogfileGroup__getUndoFreeWords
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getUndoFreeWords();
    }

    static NdbDictionary::Object::Status
    NdbDictionary__LogfileGroup__getObjectStatus
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__LogfileGroup__getObjectVersion
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__LogfileGroup__getObjectId
    ( const NdbDictionary::LogfileGroup & obj )
    {
        return obj.getObjectId();
    }

    static void
    NdbDictionary__LogfileGroup__setName
    ( NdbDictionary::LogfileGroup & obj, const char * p0 )
    {
        obj.setName(p0);
    }

    static void
    NdbDictionary__LogfileGroup__setUndoBufferSize
    ( NdbDictionary::LogfileGroup & obj, Uint32 p0 )
    {
        obj.setUndoBufferSize(p0);
    }

    static void
    NdbDictionary__LogfileGroup__setAutoGrowSpecification
    ( NdbDictionary::LogfileGroup & obj, const NdbDictionary::AutoGrowSpecification & p0 )
    {
        obj.setAutoGrowSpecification(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Object.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId.h"

    static NdbDictionary::Object::Status
    NdbDictionary__ObjectId__getObjectStatus
    ( const NdbDictionary::ObjectId & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__ObjectId__getObjectVersion
    ( const NdbDictionary::ObjectId & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__ObjectId__getObjectId
    ( const NdbDictionary::ObjectId & obj )
    {
        return obj.getObjectId();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle.h"

    static int
    NdbDictionary__OptimizeIndexHandle__next
    ( NdbDictionary::OptimizeIndexHandle & obj )
    {
        return obj.next();
    }

    static int
    NdbDictionary__OptimizeIndexHandle__close
    ( NdbDictionary::OptimizeIndexHandle & obj )
    {
        return obj.close();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle.h"

    static int
    NdbDictionary__OptimizeTableHandle__next
    ( NdbDictionary::OptimizeTableHandle & obj )
    {
        return obj.next();
    }

    static int
    NdbDictionary__OptimizeTableHandle__close
    ( NdbDictionary::OptimizeTableHandle & obj )
    {
        return obj.close();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification.h"

    static Uint32
    NdbDictionary__RecordSpecification__size
    ( )
    {
        return NdbDictionary::RecordSpecification::size();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Table.h"

    static const char *
    NdbDictionary__Table__getName
    ( const NdbDictionary::Table & obj )
    {
        return obj.getName();
    }

    static int
    NdbDictionary__Table__getTableId
    ( const NdbDictionary::Table & obj )
    {
        return obj.getTableId();
    }

    static const NdbDictionary::Column *
    NdbDictionary__Table__getColumn__0 // disambiguate overloaded const/non-const function
    ( const NdbDictionary::Table & obj, const char * p0 )
    {
        return obj.getColumn(p0);
    }

    static const NdbDictionary::Column *
    NdbDictionary__Table__getColumn__1 // disambiguate overloaded const/non-const function
    ( const NdbDictionary::Table & obj, int p0 )
    {
        return obj.getColumn(p0);
    }

    static bool
    NdbDictionary__Table__getLogging
    ( const NdbDictionary::Table & obj )
    {
        return obj.getLogging();
    }

    static NdbDictionary::Object::FragmentType
    NdbDictionary__Table__getFragmentType
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFragmentType();
    }

    static int
    NdbDictionary__Table__getKValue
    ( const NdbDictionary::Table & obj )
    {
        return obj.getKValue();
    }

    static int
    NdbDictionary__Table__getMinLoadFactor
    ( const NdbDictionary::Table & obj )
    {
        return obj.getMinLoadFactor();
    }

    static int
    NdbDictionary__Table__getMaxLoadFactor
    ( const NdbDictionary::Table & obj )
    {
        return obj.getMaxLoadFactor();
    }

    static int
    NdbDictionary__Table__getNoOfColumns
    ( const NdbDictionary::Table & obj )
    {
        return obj.getNoOfColumns();
    }

    static int
    NdbDictionary__Table__getNoOfPrimaryKeys
    ( const NdbDictionary::Table & obj )
    {
        return obj.getNoOfPrimaryKeys();
    }

    static const char *
    NdbDictionary__Table__getPrimaryKey
    ( const NdbDictionary::Table & obj, int p0 )
    {
        return obj.getPrimaryKey(p0);
    }

    static bool
    NdbDictionary__Table__equal
    ( const NdbDictionary::Table & obj, const NdbDictionary::Table & p0 )
    {
        return obj.equal(p0);
    }

    static const void *
    NdbDictionary__Table__getFrmData
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFrmData();
    }

    static Uint32
    NdbDictionary__Table__getFrmLength
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFrmLength();
    }

    static const Uint32 *
    NdbDictionary__Table__getFragmentData
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFragmentData();
    }

    static Uint32
    NdbDictionary__Table__getFragmentDataLen
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFragmentDataLen();
    }

    static const Int32 *
    NdbDictionary__Table__getRangeListData
    ( const NdbDictionary::Table & obj )
    {
        return obj.getRangeListData();
    }

    static Uint32
    NdbDictionary__Table__getRangeListDataLen
    ( const NdbDictionary::Table & obj )
    {
        return obj.getRangeListDataLen();
    }

    static const NdbRecord *
    NdbDictionary__Table__getDefaultRecord
    ( const NdbDictionary::Table & obj )
    {
        return obj.getDefaultRecord();
    }

    static bool
    NdbDictionary__Table__getLinearFlag
    ( const NdbDictionary::Table & obj )
    {
        return obj.getLinearFlag();
    }

    static Uint32
    NdbDictionary__Table__getFragmentCount
    ( const NdbDictionary::Table & obj )
    {
        return obj.getFragmentCount();
    }

    static const char *
    NdbDictionary__Table__getTablespaceName
    ( const NdbDictionary::Table & obj )
    {
        return obj.getTablespaceName();
    }

    static bool
    NdbDictionary__Table__getTablespace
    ( const NdbDictionary::Table & obj, Uint32 * p0, Uint32 * p1 )
    {
        return obj.getTablespace(p0, p1);
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Table__getObjectStatus
    ( const NdbDictionary::Table & obj )
    {
        return obj.getObjectStatus();
    }

    static void
    NdbDictionary__Table__setStatusInvalid
    ( const NdbDictionary::Table & obj )
    {
        obj.setStatusInvalid();
    }

    static int
    NdbDictionary__Table__getObjectVersion
    ( const NdbDictionary::Table & obj )
    {
        return obj.getObjectVersion();
    }

    static Uint32
    NdbDictionary__Table__getDefaultNoPartitionsFlag
    ( const NdbDictionary::Table & obj )
    {
        return obj.getDefaultNoPartitionsFlag();
    }

    static int
    NdbDictionary__Table__getObjectId
    ( const NdbDictionary::Table & obj )
    {
        return obj.getObjectId();
    }

    static Uint64
    NdbDictionary__Table__getMaxRows
    ( const NdbDictionary::Table & obj )
    {
        return obj.getMaxRows();
    }

    static Uint64
    NdbDictionary__Table__getMinRows
    ( const NdbDictionary::Table & obj )
    {
        return obj.getMinRows();
    }

    static NdbDictionary::Table::SingleUserMode
    NdbDictionary__Table__getSingleUserMode
    ( const NdbDictionary::Table & obj )
    {
        return obj.getSingleUserMode();
    }

    static bool
    NdbDictionary__Table__getRowGCIIndicator
    ( const NdbDictionary::Table & obj )
    {
        return obj.getRowGCIIndicator();
    }

    static bool
    NdbDictionary__Table__getRowChecksumIndicator
    ( const NdbDictionary::Table & obj )
    {
        return obj.getRowChecksumIndicator();
    }

    static Uint32
    NdbDictionary__Table__getPartitionId
    ( const NdbDictionary::Table & obj, Uint32 p0 )
    {
        return obj.getPartitionId(p0);
    }

    static NdbDictionary::Column *
    NdbDictionary__Table__getColumn__2 // disambiguate overloaded const/non-const function
    ( NdbDictionary::Table & obj, int p0 )
    {
        return obj.getColumn(p0);
    }

    static NdbDictionary::Column *
    NdbDictionary__Table__getColumn__3 // disambiguate overloaded const/non-const function
    ( NdbDictionary::Table & obj, const char * p0 )
    {
        return obj.getColumn(p0);
    }

    static int
    NdbDictionary__Table__setName
    ( NdbDictionary::Table & obj, const char * p0 )
    {
        return obj.setName(p0);
    }

    static int
    NdbDictionary__Table__addColumn
    ( NdbDictionary::Table & obj, const NdbDictionary::Column & p0 )
    {
        return obj.addColumn(p0);
    }

    static void
    NdbDictionary__Table__setLogging
    ( NdbDictionary::Table & obj, bool p0 )
    {
        obj.setLogging(p0);
    }

    static void
    NdbDictionary__Table__setLinearFlag
    ( NdbDictionary::Table & obj, Uint32 p0 )
    {
        obj.setLinearFlag(p0);
    }

    static void
    NdbDictionary__Table__setFragmentCount
    ( NdbDictionary::Table & obj, Uint32 p0 )
    {
        obj.setFragmentCount(p0);
    }

    static void
    NdbDictionary__Table__setFragmentType
    ( NdbDictionary::Table & obj, NdbDictionary::Object::FragmentType p0 )
    {
        obj.setFragmentType(p0);
    }

    static void
    NdbDictionary__Table__setKValue
    ( NdbDictionary::Table & obj, int p0 )
    {
        obj.setKValue(p0);
    }

    static void
    NdbDictionary__Table__setMinLoadFactor
    ( NdbDictionary::Table & obj, int p0 )
    {
        obj.setMinLoadFactor(p0);
    }

    static void
    NdbDictionary__Table__setMaxLoadFactor
    ( NdbDictionary::Table & obj, int p0 )
    {
        obj.setMaxLoadFactor(p0);
    }

    static int
    NdbDictionary__Table__setTablespaceName
    ( NdbDictionary::Table & obj, const char * p0 )
    {
        return obj.setTablespaceName(p0);
    }

    static int
    NdbDictionary__Table__setTablespace
    ( NdbDictionary::Table & obj, const NdbDictionary::Tablespace & p0 )
    {
        return obj.setTablespace(p0);
    }

    static void
    NdbDictionary__Table__setDefaultNoPartitionsFlag
    ( NdbDictionary::Table & obj, Uint32 p0 )
    {
        obj.setDefaultNoPartitionsFlag(p0);
    }

    static int
    NdbDictionary__Table__setFrm
    ( NdbDictionary::Table & obj, const void * p0, Uint32 p1 )
    {
        return obj.setFrm(p0, p1);
    }

    static int
    NdbDictionary__Table__setFragmentData
    ( NdbDictionary::Table & obj, const Uint32 * p0, Uint32 p1 )
    {
        return obj.setFragmentData(p0, p1);
    }

    static int
    NdbDictionary__Table__setRangeListData
    ( NdbDictionary::Table & obj, const Int32 * p0, Uint32 p1 )
    {
        return obj.setRangeListData(p0, p1);
    }

    static void
    NdbDictionary__Table__setMaxRows
    ( NdbDictionary::Table & obj, Uint64 p0 )
    {
        obj.setMaxRows(p0);
    }

    static void
    NdbDictionary__Table__setMinRows
    ( NdbDictionary::Table & obj, Uint64 p0 )
    {
        obj.setMinRows(p0);
    }

    static void
    NdbDictionary__Table__setSingleUserMode
    ( NdbDictionary::Table & obj, NdbDictionary::Table::SingleUserMode p0 )
    {
        obj.setSingleUserMode(p0);
    }

    static void
    NdbDictionary__Table__setRowGCIIndicator
    ( NdbDictionary::Table & obj, bool p0 )
    {
        obj.setRowGCIIndicator(p0);
    }

    static void
    NdbDictionary__Table__setRowChecksumIndicator
    ( NdbDictionary::Table & obj, bool p0 )
    {
        obj.setRowChecksumIndicator(p0);
    }

    static int
    NdbDictionary__Table__aggregate
    ( NdbDictionary::Table & obj, NdbError & p0 )
    {
        return obj.aggregate(p0);
    }

    static int
    NdbDictionary__Table__validate
    ( NdbDictionary::Table & obj, NdbError & p0 )
    {
        return obj.validate(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace.h"

    static const char *
    NdbDictionary__Tablespace__getName
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getName();
    }

    static Uint32
    NdbDictionary__Tablespace__getExtentSize
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getExtentSize();
    }

    static const NdbDictionary::AutoGrowSpecification &
    NdbDictionary__Tablespace__getAutoGrowSpecification
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getAutoGrowSpecification();
    }

    static const char *
    NdbDictionary__Tablespace__getDefaultLogfileGroup
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getDefaultLogfileGroup();
    }

    static Uint32
    NdbDictionary__Tablespace__getDefaultLogfileGroupId
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getDefaultLogfileGroupId();
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Tablespace__getObjectStatus
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__Tablespace__getObjectVersion
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__Tablespace__getObjectId
    ( const NdbDictionary::Tablespace & obj )
    {
        return obj.getObjectId();
    }

    static void
    NdbDictionary__Tablespace__setName
    ( NdbDictionary::Tablespace & obj, const char * p0 )
    {
        obj.setName(p0);
    }

    static void
    NdbDictionary__Tablespace__setExtentSize
    ( NdbDictionary::Tablespace & obj, Uint32 p0 )
    {
        obj.setExtentSize(p0);
    }

    static void
    NdbDictionary__Tablespace__setAutoGrowSpecification
    ( NdbDictionary::Tablespace & obj, const NdbDictionary::AutoGrowSpecification & p0 )
    {
        obj.setAutoGrowSpecification(p0);
    }

    static void
    NdbDictionary__Tablespace__setDefaultLogfileGroup__0 // disambiguate overloaded function
    ( NdbDictionary::Tablespace & obj, const char * p0 )
    {
        obj.setDefaultLogfileGroup(p0);
    }

    static void
    NdbDictionary__Tablespace__setDefaultLogfileGroup__1 // disambiguate overloaded function
    ( NdbDictionary::Tablespace & obj, const NdbDictionary::LogfileGroup & p0 )
    {
        obj.setDefaultLogfileGroup(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile.h"

    static const char *
    NdbDictionary__Undofile__getPath
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getPath();
    }

    static Uint64
    NdbDictionary__Undofile__getSize
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getSize();
    }

    static const char *
    NdbDictionary__Undofile__getLogfileGroup
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getLogfileGroup();
    }

    static void
    NdbDictionary__Undofile__getLogfileGroupId
    ( const NdbDictionary::Undofile & obj, NdbDictionary::ObjectId * p0 )
    {
        obj.getLogfileGroupId(p0);
    }

    static NdbDictionary::Object::Status
    NdbDictionary__Undofile__getObjectStatus
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getObjectStatus();
    }

    static int
    NdbDictionary__Undofile__getObjectVersion
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getObjectVersion();
    }

    static int
    NdbDictionary__Undofile__getObjectId
    ( const NdbDictionary::Undofile & obj )
    {
        return obj.getObjectId();
    }

    static void
    NdbDictionary__Undofile__setPath
    ( NdbDictionary::Undofile & obj, const char * p0 )
    {
        obj.setPath(p0);
    }

    static void
    NdbDictionary__Undofile__setSize
    ( NdbDictionary::Undofile & obj, Uint64 p0 )
    {
        obj.setSize(p0);
    }

    static void
    NdbDictionary__Undofile__setLogfileGroup__0 // disambiguate overloaded function
    ( NdbDictionary::Undofile & obj, const char * p0 )
    {
        obj.setLogfileGroup(p0);
    }

    static void
    NdbDictionary__Undofile__setLogfileGroup__1 // disambiguate overloaded function
    ( NdbDictionary::Undofile & obj, const NdbDictionary::LogfileGroup & p0 )
    {
        obj.setLogfileGroup(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbError.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbEventOperation.h"

    static int
    NdbEventOperation__isOverrun
    ( const NdbEventOperation & obj )
    {
        return obj.isOverrun();
    }

    static bool
    NdbEventOperation__isConsistent
    ( const NdbEventOperation & obj )
    {
        return obj.isConsistent();
    }

    static NdbDictionary::Event::TableEvent
    NdbEventOperation__getEventType
    ( const NdbEventOperation & obj )
    {
        return obj.getEventType();
    }

    static bool
    NdbEventOperation__tableNameChanged
    ( const NdbEventOperation & obj )
    {
        return obj.tableNameChanged();
    }

    static bool
    NdbEventOperation__tableFrmChanged
    ( const NdbEventOperation & obj )
    {
        return obj.tableFrmChanged();
    }
    static bool
    NdbEventOperation__tableFragmentationChanged
    ( const NdbEventOperation & obj )
    {
        return obj.tableFragmentationChanged();
    }
    static bool
    NdbEventOperation__tableRangeListChanged
    ( const NdbEventOperation & obj )
    {
        return obj.tableRangeListChanged();
    }

    static Uint64
    NdbEventOperation__getGCI
    ( const NdbEventOperation & obj )
    {
        return obj.getGCI();
    }

    static Uint32
    NdbEventOperation__getAnyValue
    ( const NdbEventOperation & obj )
    {
        return obj.getAnyValue();
    }

    static Uint64
    NdbEventOperation__getLatestGCI
    ( const NdbEventOperation & obj )
    {
        return obj.getLatestGCI();
    }

    static const NdbError &
    NdbEventOperation__getNdbError
    ( const NdbEventOperation & obj )
    {
        return obj.getNdbError();
    }

    static NdbEventOperation::State
    NdbEventOperation__getState
    ( NdbEventOperation & obj )
    {
        return obj.getState();
    }

    static void
    NdbEventOperation__mergeEvents
    ( NdbEventOperation & obj, bool p0 )
    {
        obj.mergeEvents(p0);
    }

    static int
    NdbEventOperation__execute
    ( NdbEventOperation & obj )
    {
        return obj.execute();
    }

    static NdbRecAttr *
    NdbEventOperation__getValue
    ( NdbEventOperation & obj, const char * p0, char * p1 )
    {
        return obj.getValue(p0, p1);
    }

    static NdbRecAttr *
    NdbEventOperation__getPreValue
    ( NdbEventOperation & obj, const char * p0, char * p1 )
    {
        return obj.getPreValue(p0, p1);
    }

    static NdbBlob *
    NdbEventOperation__getBlobHandle
    ( NdbEventOperation & obj, const char * p0 )
    {
        return obj.getBlobHandle(p0);
    }

    static NdbBlob *
    NdbEventOperation__getPreBlobHandle
    ( NdbEventOperation & obj, const char * p0 )
    {
        return obj.getPreBlobHandle(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexOperation.h"

    static const NdbDictionary::Index *
    NdbIndexOperation__getIndex
    ( const NdbIndexOperation & obj )
    {
        return obj.getIndex();
    }

    static int
    NdbIndexOperation__insertTuple
    ( NdbIndexOperation & obj )
    {
        return obj.insertTuple();
    }

    static int
    NdbIndexOperation__readTuple
    ( NdbIndexOperation & obj, NdbOperation::LockMode p0 )
    {
        return obj.readTuple(p0);
    }

    static int
    NdbIndexOperation__updateTuple
    ( NdbIndexOperation & obj )
    {
        return obj.updateTuple();
    }

    static int
    NdbIndexOperation__deleteTuple
    ( NdbIndexOperation & obj )
    {
        return obj.deleteTuple();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation.h"

    static bool
    NdbIndexScanOperation__getSorted
    ( const NdbIndexScanOperation & obj )
    {
        return obj.getSorted();
    }

    static bool
    NdbIndexScanOperation__getDescending
    ( const NdbIndexScanOperation & obj )
    {
        return obj.getDescending();
    }

    static int
    NdbIndexScanOperation__readTuples
    ( NdbIndexScanOperation & obj, NdbOperation::LockMode p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.readTuples(p0, p1, p2, p3);
    }

    static int
    NdbIndexScanOperation__setBound__0 // disambiguate overloaded function
    ( NdbIndexScanOperation & obj, const char * p0, int p1, const void * p2 )
    {
        return obj.setBound(p0, p1, p2);
    }

    static int
    NdbIndexScanOperation__setBound__1 // disambiguate overloaded function
    ( NdbIndexScanOperation & obj, Uint32 p0, int p1, const void * p2 )
    {
        return obj.setBound(p0, p1, p2);
    }

    static int
    NdbIndexScanOperation__end_of_bound
    ( NdbIndexScanOperation & obj, Uint32 p0 )
    {
        return obj.end_of_bound(p0);
    }

    static int
    NdbIndexScanOperation__get_range_no
    ( NdbIndexScanOperation & obj )
    {
        return obj.get_range_no();
    }

    static int
    NdbIndexScanOperation__setBound__2 // disambiguate overloaded function
    ( NdbIndexScanOperation & obj, const NdbRecord * p0, const NdbIndexScanOperation::IndexBound & p1 )
    {
        return obj.setBound(p0, p1);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbInterpretedCode.h"

    static const NdbDictionary::Table *
    NdbInterpretedCode__getTable
    ( const NdbInterpretedCode & obj )
    {
        return obj.getTable();
    }

    static const NdbError &
    NdbInterpretedCode__getNdbError
    ( const NdbInterpretedCode & obj )
    {
        return obj.getNdbError();
    }

    static Uint32
    NdbInterpretedCode__getWordsUsed
    ( const NdbInterpretedCode & obj )
    {
        return obj.getWordsUsed();
    }

    static int
    NdbInterpretedCode__load_const_null
    ( NdbInterpretedCode & obj, Uint32 p0 )
    {
        return obj.load_const_null(p0);
    }

    static int
    NdbInterpretedCode__load_const_u16
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.load_const_u16(p0, p1);
    }

    static int
    NdbInterpretedCode__load_const_u32
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.load_const_u32(p0, p1);
    }

    static int
    NdbInterpretedCode__load_const_u64
    ( NdbInterpretedCode & obj, Uint32 p0, Uint64 p1 )
    {
        return obj.load_const_u64(p0, p1);
    }

    static int
    NdbInterpretedCode__read_attr__0 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.read_attr(p0, p1);
    }

    static int
    NdbInterpretedCode__read_attr__1 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, const NdbDictionary::Column * p1 )
    {
        return obj.read_attr(p0, p1);
    }

    static int
    NdbInterpretedCode__write_attr__0 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.write_attr(p0, p1);
    }

    static int
    NdbInterpretedCode__write_attr__1 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, const NdbDictionary::Column * p0, Uint32 p1 )
    {
        return obj.write_attr(p0, p1);
    }

    static int
    NdbInterpretedCode__add_reg
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.add_reg(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__sub_reg
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.sub_reg(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__def_label
    ( NdbInterpretedCode & obj, int p0 )
    {
        return obj.def_label(p0);
    }

    static int
    NdbInterpretedCode__branch_label
    ( NdbInterpretedCode & obj, Uint32 p0 )
    {
        return obj.branch_label(p0);
    }

    static int
    NdbInterpretedCode__branch_ge
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_ge(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_gt
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_gt(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_le
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_le(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_lt
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_lt(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_eq
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_eq(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_ne
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1, Uint32 p2 )
    {
        return obj.branch_ne(p0, p1, p2);
    }

    static int
    NdbInterpretedCode__branch_ne_null
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.branch_ne_null(p0, p1);
    }

    static int
    NdbInterpretedCode__branch_eq_null
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.branch_eq_null(p0, p1);
    }

    static int
    NdbInterpretedCode__branch_col_eq
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_eq(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_ne
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_ne(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_lt
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_lt(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_le
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_le(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_gt
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_gt(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_ge
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_ge(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_eq_null
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.branch_col_eq_null(p0, p1);
    }

    static int
    NdbInterpretedCode__branch_col_ne_null
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.branch_col_ne_null(p0, p1);
    }

    static int
    NdbInterpretedCode__branch_col_like
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_like(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__branch_col_notlike
    ( NdbInterpretedCode & obj, const void * p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.branch_col_notlike(p0, p1, p2, p3);
    }

    static int
    NdbInterpretedCode__interpret_exit_ok
    ( NdbInterpretedCode & obj )
    {
        return obj.interpret_exit_ok();
    }

    static int
    NdbInterpretedCode__interpret_exit_nok__0 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0 )
    {
        return obj.interpret_exit_nok(p0);
    }

    static int
    NdbInterpretedCode__interpret_exit_nok__1 // disambiguate overloaded function
    ( NdbInterpretedCode & obj )
    {
        return obj.interpret_exit_nok();
    }

    static int
    NdbInterpretedCode__interpret_exit_last_row
    ( NdbInterpretedCode & obj )
    {
        return obj.interpret_exit_last_row();
    }

    static int
    NdbInterpretedCode__add_val__0 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.add_val(p0, p1);
    }

    static int
    NdbInterpretedCode__add_val__1 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint64 p1 )
    {
        return obj.add_val(p0, p1);
    }

    static int
    NdbInterpretedCode__sub_val__0 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint32 p1 )
    {
        return obj.sub_val(p0, p1);
    }

    static int
    NdbInterpretedCode__sub_val__1 // disambiguate overloaded function
    ( NdbInterpretedCode & obj, Uint32 p0, Uint64 p1 )
    {
        return obj.sub_val(p0, p1);
    }

    static int
    NdbInterpretedCode__def_sub
    ( NdbInterpretedCode & obj, Uint32 p0 )
    {
        return obj.def_sub(p0);
    }

    static int
    NdbInterpretedCode__call_sub
    ( NdbInterpretedCode & obj, Uint32 p0 )
    {
        return obj.call_sub(p0);
    }

    static int
    NdbInterpretedCode__ret_sub
    ( NdbInterpretedCode & obj )
    {
        return obj.ret_sub();
    }

    static int
    NdbInterpretedCode__finalise
    ( NdbInterpretedCode & obj )
    {
        return obj.finalise();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation.h"

    static NdbBlob *
    NdbOperation__getBlobHandle__0 // disambiguate overloaded const/non-const function
    ( const NdbOperation & obj, const char * p0 )
    {
        return obj.getBlobHandle(p0);
    }

    static NdbBlob *
    NdbOperation__getBlobHandle__1 // disambiguate overloaded const/non-const function
    ( const NdbOperation & obj, Uint32 p0 )
    {
        return obj.getBlobHandle(p0);
    }

    static const NdbError &
    NdbOperation__getNdbError
    ( const NdbOperation & obj )
    {
        return obj.getNdbError();
    }

    static int
    NdbOperation__getNdbErrorLine
    ( const NdbOperation & obj )
    {
        return obj.getNdbErrorLine();
    }

    static const char *
    NdbOperation__getTableName
    ( const NdbOperation & obj )
    {
        return obj.getTableName();
    }

    static const NdbDictionary::Table *
    NdbOperation__getTable
    ( const NdbOperation & obj )
    {
        return obj.getTable();
    }

    static NdbOperation::Type
    NdbOperation__getType
    ( const NdbOperation & obj )
    {
        return obj.getType();
    }

    static NdbOperation::LockMode
    NdbOperation__getLockMode
    ( const NdbOperation & obj )
    {
        return obj.getLockMode();
    }

    static NdbOperation::AbortOption
    NdbOperation__getAbortOption
    ( const NdbOperation & obj )
    {
        return obj.getAbortOption();
    }

    static NdbTransaction *
    NdbOperation__getNdbTransaction
    ( const NdbOperation & obj )
    {
        return obj.getNdbTransaction();
    }

    static const NdbLockHandle *
    NdbOperation__getLockHandle__0 // disambiguate overloaded const/non-const function
    ( const NdbOperation & obj )
    {
        return obj.getLockHandle();
    }

    static const NdbLockHandle *
    NdbOperation__getLockHandle__1 // disambiguate overloaded const/non-const function
    ( NdbOperation & obj )
    {
        return obj.getLockHandle();
    }

    static int
    NdbOperation__insertTuple
    ( NdbOperation & obj )
    {
        return obj.insertTuple();
    }

    static int
    NdbOperation__updateTuple
    ( NdbOperation & obj )
    {
        return obj.updateTuple();
    }

    static int
    NdbOperation__writeTuple
    ( NdbOperation & obj )
    {
        return obj.writeTuple();
    }

    static int
    NdbOperation__deleteTuple
    ( NdbOperation & obj )
    {
        return obj.deleteTuple();
    }

    static int
    NdbOperation__readTuple
    ( NdbOperation & obj, NdbOperation::LockMode p0 )
    {
        return obj.readTuple(p0);
    }

    static int
    NdbOperation__equal__0 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, const char * p1 )
    {
        return obj.equal(p0, p1);
    }

    static int
    NdbOperation__equal__1 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, Int32 p1 )
    {
        return obj.equal(p0, p1);
    }

    static int
    NdbOperation__equal__2 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, Int64 p1 )
    {
        return obj.equal(p0, p1);
    }

    static int
    NdbOperation__equal__3 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, const char * p1 )
    {
        return obj.equal(p0, p1);
    }

    static int
    NdbOperation__equal__4 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, Int32 p1 )
    {
        return obj.equal(p0, p1);
    }

    static int
    NdbOperation__equal__5 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, Int64 p1 )
    {
        return obj.equal(p0, p1);
    }

    static NdbRecAttr *
    NdbOperation__getValue__0 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, char * p1 )
    {
        return obj.getValue(p0, p1);
    }

    static NdbRecAttr *
    NdbOperation__getValue__1 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, char * p1 )
    {
        return obj.getValue(p0, p1);
    }

    static NdbRecAttr *
    NdbOperation__getValue__2 // disambiguate overloaded function
    ( NdbOperation & obj, const NdbDictionary::Column * p0, char * p1 )
    {
        return obj.getValue(p0, p1);
    }

    static int
    NdbOperation__setValue__0 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, const char * p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__1 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, Int32 p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__2 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, Int64 p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__3 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, float p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__4 // disambiguate overloaded function
    ( NdbOperation & obj, const char * p0, double p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__5 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, const char * p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__6 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, Int32 p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__7 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, Int64 p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__8 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, float p1 )
    {
        return obj.setValue(p0, p1);
    }

    static int
    NdbOperation__setValue__9 // disambiguate overloaded function
    ( NdbOperation & obj, Uint32 p0, double p1 )
    {
        return obj.setValue(p0, p1);
    }

    static NdbBlob *
    NdbOperation__getBlobHandle__2 // disambiguate overloaded const/non-const function
    ( NdbOperation & obj, const char * p0 )
    {
        return obj.getBlobHandle(p0);
    }

    static NdbBlob *
    NdbOperation__getBlobHandle__3 // disambiguate overloaded const/non-const function
    ( NdbOperation & obj, Uint32 p0 )
    {
        return obj.getBlobHandle(p0);
    }

    static int
    NdbOperation__setAbortOption
    ( NdbOperation & obj, NdbOperation::AbortOption p0 )
    {
        return obj.setAbortOption(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions.h"

    static Uint32
    NdbOperation__OperationOptions__size
    ( )
    {
        return NdbOperation::OperationOptions::size();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbRecAttr.h"

    static const NdbDictionary::Column *
    NdbRecAttr__getColumn
    ( const NdbRecAttr & obj )
    {
        return obj.getColumn();
    }

    static NdbDictionary::Column::Type
    NdbRecAttr__getType
    ( const NdbRecAttr & obj )
    {
        return obj.getType();
    }

    static Uint32
    NdbRecAttr__get_size_in_bytes
    ( const NdbRecAttr & obj )
    {
        return obj.get_size_in_bytes();
    }

    static int
    NdbRecAttr__isNULL
    ( const NdbRecAttr & obj )
    {
        return obj.isNULL();
    }

    static Int64
    NdbRecAttr__int64_value
    ( const NdbRecAttr & obj )
    {
        return obj.int64_value();
    }

    static Int32
    NdbRecAttr__int32_value
    ( const NdbRecAttr & obj )
    {
        return obj.int32_value();
    }

    static Int32
    NdbRecAttr__medium_value
    ( const NdbRecAttr & obj )
    {
        return obj.medium_value();
    }

    static short
    NdbRecAttr__short_value
    ( const NdbRecAttr & obj )
    {
        return obj.short_value();
    }

    static char
    NdbRecAttr__char_value
    ( const NdbRecAttr & obj )
    {
        return obj.char_value();
    }

    static Int8
    NdbRecAttr__int8_value
    ( const NdbRecAttr & obj )
    {
        return obj.int8_value();
    }

    static Uint64
    NdbRecAttr__u_64_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_64_value();
    }

    static Uint32
    NdbRecAttr__u_32_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_32_value();
    }

    static Uint32
    NdbRecAttr__u_medium_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_medium_value();
    }

    static Uint16
    NdbRecAttr__u_short_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_short_value();
    }

    static Uint8
    NdbRecAttr__u_char_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_char_value();
    }

    static Uint8
    NdbRecAttr__u_8_value
    ( const NdbRecAttr & obj )
    {
        return obj.u_8_value();
    }

    static float
    NdbRecAttr__float_value
    ( const NdbRecAttr & obj )
    {
        return obj.float_value();
    }

    static double
    NdbRecAttr__double_value
    ( const NdbRecAttr & obj )
    {
        return obj.double_value();
    }

    static char *
    NdbRecAttr__aRef
    ( const NdbRecAttr & obj )
    {
        return obj.aRef();
    }

    static NdbRecAttr *
    NdbRecAttr__clone
    ( const NdbRecAttr & obj )
    {
        return obj.clone();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbScanFilter.h"

    static const NdbError &
    NdbScanFilter__getNdbError
    ( const NdbScanFilter & obj )
    {
        return obj.getNdbError();
    }

    static const NdbInterpretedCode *
    NdbScanFilter__getInterpretedCode
    ( const NdbScanFilter & obj )
    {
        return obj.getInterpretedCode();
    }

    static NdbOperation *
    NdbScanFilter__getNdbOperation
    ( const NdbScanFilter & obj )
    {
        return obj.getNdbOperation();
    }

    static int
    NdbScanFilter__begin
    ( NdbScanFilter & obj, NdbScanFilter::Group p0 )
    {
        return obj.begin(p0);
    }

    static int
    NdbScanFilter__end
    ( NdbScanFilter & obj )
    {
        return obj.end();
    }

    static int
    NdbScanFilter__istrue
    ( NdbScanFilter & obj )
    {
        return obj.istrue();
    }

    static int
    NdbScanFilter__isfalse
    ( NdbScanFilter & obj )
    {
        return obj.isfalse();
    }

    static int
    NdbScanFilter__cmp
    ( NdbScanFilter & obj, NdbScanFilter::BinaryCondition p0, int p1, const void * p2, Uint32 p3 )
    {
        return obj.cmp(p0, p1, p2, p3);
    }

    static int
    NdbScanFilter__eq__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.eq(p0, p1);
    }

    static int
    NdbScanFilter__ne__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.ne(p0, p1);
    }

    static int
    NdbScanFilter__lt__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.lt(p0, p1);
    }

    static int
    NdbScanFilter__le__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.le(p0, p1);
    }

    static int
    NdbScanFilter__gt__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.gt(p0, p1);
    }

    static int
    NdbScanFilter__ge__0 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint32 p1 )
    {
        return obj.ge(p0, p1);
    }

    static int
    NdbScanFilter__eq__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.eq(p0, p1);
    }

    static int
    NdbScanFilter__ne__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.ne(p0, p1);
    }

    static int
    NdbScanFilter__lt__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.lt(p0, p1);
    }

    static int
    NdbScanFilter__le__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.le(p0, p1);
    }

    static int
    NdbScanFilter__gt__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.gt(p0, p1);
    }

    static int
    NdbScanFilter__ge__1 // disambiguate overloaded function
    ( NdbScanFilter & obj, int p0, Uint64 p1 )
    {
        return obj.ge(p0, p1);
    }

    static int
    NdbScanFilter__isnull
    ( NdbScanFilter & obj, int p0 )
    {
        return obj.isnull(p0);
    }

    static int
    NdbScanFilter__isnotnull
    ( NdbScanFilter & obj, int p0 )
    {
        return obj.isnotnull(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbScanOperation.h"

    static NdbTransaction *
    NdbScanOperation__getNdbTransaction
    ( const NdbScanOperation & obj )
    {
        return obj.getNdbTransaction();
    }

    static int
    NdbScanOperation__readTuples
    ( NdbScanOperation & obj, NdbOperation::LockMode p0, Uint32 p1, Uint32 p2, Uint32 p3 )
    {
        return obj.readTuples(p0, p1, p2, p3);
    }

    static int
    NdbScanOperation__nextResult
    ( NdbScanOperation & obj, bool p0, bool p1 )
    {
        return obj.nextResult(p0, p1);
    }

    static int
    NdbScanOperation__nextResultCopyOut
    ( NdbScanOperation & obj, char * p0, bool p1, bool p2 )
    {
        return obj.nextResultCopyOut(p0, p1, p2);
    }

    static void
    NdbScanOperation__close
    ( NdbScanOperation & obj, bool p0, bool p1 )
    {
        obj.close(p0, p1);
    }

    static NdbOperation *
    NdbScanOperation__lockCurrentTuple__0 // disambiguate overloaded function
    ( NdbScanOperation & obj )
    {
        return obj.lockCurrentTuple();
    }

    static NdbOperation *
    NdbScanOperation__lockCurrentTuple__1 // disambiguate overloaded function
    ( NdbScanOperation & obj, NdbTransaction * p0 )
    {
        return obj.lockCurrentTuple(p0);
    }

    static NdbOperation *
    NdbScanOperation__updateCurrentTuple__0 // disambiguate overloaded function
    ( NdbScanOperation & obj )
    {
        return obj.updateCurrentTuple();
    }

    static NdbOperation *
    NdbScanOperation__updateCurrentTuple__1 // disambiguate overloaded function
    ( NdbScanOperation & obj, NdbTransaction * p0 )
    {
        return obj.updateCurrentTuple(p0);
    }

    static int
    NdbScanOperation__deleteCurrentTuple__0 // disambiguate overloaded function
    ( NdbScanOperation & obj )
    {
        return obj.deleteCurrentTuple();
    }

    static int
    NdbScanOperation__deleteCurrentTuple__1 // disambiguate overloaded function
    ( NdbScanOperation & obj, NdbTransaction * p0 )
    {
        return obj.deleteCurrentTuple(p0);
    }

    static const NdbOperation *
    NdbScanOperation__lockCurrentTuple
    ( NdbScanOperation & obj, NdbTransaction * p0, const NdbRecord * p1, char * p2, const Uint8 * p3, const NdbOperation::OperationOptions * p4, Uint32 p5 )
    {
        return obj.lockCurrentTuple(p0, p1, p2, p3, p4, p5);
    }

    static const NdbOperation *
    NdbScanOperation__updateCurrentTuple
    ( NdbScanOperation & obj, NdbTransaction * p0, const NdbRecord * p1, const char * p2, const Uint8 * p3, const NdbOperation::OperationOptions * p4, Uint32 p5 )
    {
        return obj.updateCurrentTuple(p0, p1, p2, p3, p4, p5);
    }

    static const NdbOperation *
    NdbScanOperation__deleteCurrentTuple
    ( NdbScanOperation & obj, NdbTransaction * p0, const NdbRecord * p1, char * p2, const Uint8 * p3, const NdbOperation::OperationOptions * p4, Uint32 p5 )
    {
        return obj.deleteCurrentTuple(p0, p1, p2, p3, p4, p5);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions.h"

    static Uint32
    NdbScanOperation__ScanOptions__size
    ( )
    {
        return NdbScanOperation::ScanOptions::size();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_NdbTransaction.h"

    static const NdbError &
    NdbTransaction__getNdbError
    ( const NdbTransaction & obj )
    {
        return obj.getNdbError();
    }

    static const NdbOperation *
    NdbTransaction__getNdbErrorOperation__0 // disambiguate overloaded const/non-const function
    ( const NdbTransaction & obj )
    {
        return obj.getNdbErrorOperation();
    }

    static const NdbOperation *
    NdbTransaction__getNextCompletedOperation
    ( const NdbTransaction & obj, const NdbOperation * p0 )
    {
        return obj.getNextCompletedOperation(p0);
    }

    static NdbOperation *
    NdbTransaction__getNdbOperation
    ( NdbTransaction & obj, const NdbDictionary::Table * p0 )
    {
        return obj.getNdbOperation(p0);
    }

    static NdbScanOperation *
    NdbTransaction__getNdbScanOperation
    ( NdbTransaction & obj, const NdbDictionary::Table * p0 )
    {
        return obj.getNdbScanOperation(p0);
    }

    static NdbIndexScanOperation *
    NdbTransaction__getNdbIndexScanOperation
    ( NdbTransaction & obj, const NdbDictionary::Index * p0 )
    {
        return obj.getNdbIndexScanOperation(p0);
    }

    static NdbIndexOperation *
    NdbTransaction__getNdbIndexOperation
    ( NdbTransaction & obj, const NdbDictionary::Index * p0 )
    {
        return obj.getNdbIndexOperation(p0);
    }

    static int
    NdbTransaction__execute
    ( NdbTransaction & obj, NdbTransaction::ExecType p0, NdbOperation::AbortOption p1, int p2 )
    {
        return obj.execute(p0, p1, p2);
    }

    static int
    NdbTransaction__refresh
    ( NdbTransaction & obj )
    {
        return obj.refresh();
    }

    static void
    NdbTransaction__close
    ( NdbTransaction & obj )
    {
        obj.close();
    }

    static int
    NdbTransaction__getGCI
    ( NdbTransaction & obj, Uint64 * p0 )
    {
        return obj.getGCI(p0);
    }

    static Uint64
    NdbTransaction__getTransactionId
    ( NdbTransaction & obj )
    {
        return obj.getTransactionId();
    }

    static NdbTransaction::CommitStatusType
    NdbTransaction__commitStatus
    ( NdbTransaction & obj )
    {
        return obj.commitStatus();
    }

    static int
    NdbTransaction__getNdbErrorLine
    ( NdbTransaction & obj )
    {
        return obj.getNdbErrorLine();
    }

    static const NdbOperation *
    NdbTransaction__readTuple
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const NdbRecord * p2, char * p3, NdbOperation::LockMode p4, const Uint8 * p5, const NdbOperation::OperationOptions * p6, Uint32 p7 )
    {
        return obj.readTuple(p0, p1, p2, p3, p4, p5, p6, p7);
    }

    static const NdbOperation *
    NdbTransaction__insertTuple__0
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const NdbRecord * p2, const char * p3, const Uint8 * p4, const NdbOperation::OperationOptions * p5, Uint32 p6 )
    {
        return obj.insertTuple(p0, p1, p2, p3, p4, p5, p6);
    }

    static const NdbOperation *
    NdbTransaction__insertTuple__1
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const Uint8 * p2, const NdbOperation::OperationOptions * p3, Uint32 p4 )
    {
        return obj.insertTuple(p0, p1, p2, p3, p4);
    }

    static const NdbOperation *
    NdbTransaction__updateTuple
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const NdbRecord * p2, const char * p3, const Uint8 * p4, const NdbOperation::OperationOptions * p5, Uint32 p6 )
    {
        return obj.updateTuple(p0, p1, p2, p3, p4, p5, p6);
    }

    static const NdbOperation *
    NdbTransaction__writeTuple
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const NdbRecord * p2, const char * p3, const Uint8 * p4, const NdbOperation::OperationOptions * p5, Uint32 p6 )
    {
        return obj.writeTuple(p0, p1, p2, p3, p4, p5, p6);
    }

    static const NdbOperation *
    NdbTransaction__deleteTuple
    ( NdbTransaction & obj, const NdbRecord * p0, const char * p1, const NdbRecord * p2, char * p3, const Uint8 * p4, const NdbOperation::OperationOptions * p5, Uint32 p6 )
    {
        return obj.deleteTuple(p0, p1, p2, p3, p4, p5, p6);
    }

    static NdbScanOperation *
    NdbTransaction__scanTable
    ( NdbTransaction & obj, const NdbRecord * p0, NdbOperation::LockMode p1, const Uint8 * p2, const NdbScanOperation::ScanOptions * p3, Uint32 p4 )
    {
        return obj.scanTable(p0, p1, p2, p3, p4);
    }

    static NdbIndexScanOperation *
    NdbTransaction__scanIndex
    ( NdbTransaction & obj, const NdbRecord * p0, const NdbRecord * p1, NdbOperation::LockMode p2, const Uint8 * p3, const NdbIndexScanOperation::IndexBound * p4, const NdbScanOperation::ScanOptions * p5, Uint32 p6 )
    {
        return obj.scanIndex(p0, p1, p2, p3, p4, p5, p6);
    }

    static const NdbOperation *
    NdbTransaction__unlock
    ( NdbTransaction & obj, const NdbLockHandle * p0, NdbOperation::AbortOption p1 )
    {
        return obj.unlock(p0, p1);
    }

    static int
    NdbTransaction__releaseLockHandle
    ( NdbTransaction & obj, const NdbLockHandle * p0 )
    {
        return obj.releaseLockHandle(p0);
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr.h"

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_Ndb_PartitionSpec.h"

    static Uint32
    Ndb__PartitionSpec__size
    ( )
    {
        return Ndb::PartitionSpec::size();
    }

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection.h"

    static int
    Ndb_cluster_connection__get_latest_error
    ( const Ndb_cluster_connection & obj )
    {
        return obj.get_latest_error();
    }

    static const char *
    Ndb_cluster_connection__get_latest_error_msg
    ( const Ndb_cluster_connection & obj )
    {
        return obj.get_latest_error_msg();
    }

    static void
    Ndb_cluster_connection__set_name
    ( Ndb_cluster_connection & obj, const char * p0 )
    {
        obj.set_name(p0);
    }

    static void
    Ndb_cluster_connection__set_service_uri
    ( Ndb_cluster_connection & obj, const char * p0, const char * p1, int p2, const char * p3)
    {
        obj.set_service_uri(p0, p1, p2, p3);
    }

    static int
    Ndb_cluster_connection__set_timeout
    ( Ndb_cluster_connection & obj, int p0 )
    {
        return obj.set_timeout(p0);
    }

    static int
    Ndb_cluster_connection__connect
    ( Ndb_cluster_connection & obj, int p0, int p1, int p2 )
    {
        return obj.connect(p0, p1, p2);
    }

    static int
    Ndb_cluster_connection__wait_until_ready
    ( Ndb_cluster_connection & obj, int p0, int p1 )
    {
        return obj.wait_until_ready(p0, p1);
    }

    static void
    Ndb_cluster_connection__lock_ndb_objects
    ( Ndb_cluster_connection & obj )
    {
        obj.lock_ndb_objects();
    }

    static void
    Ndb_cluster_connection__unlock_ndb_objects
    ( Ndb_cluster_connection & obj )
    {
        obj.unlock_ndb_objects();
    }

    static int
    Ndb_cluster_connection__set_recv_thread_activation_threshold
    ( Ndb_cluster_connection & obj, int p0 )
    {
        return obj.set_recv_thread_activation_threshold(p0);
    }

    static int
    Ndb_cluster_connection__get_recv_thread_activation_threshold
    ( Ndb_cluster_connection & obj )
    {
        return obj.get_recv_thread_activation_threshold();
    }

    static int
    Ndb_cluster_connection__set_recv_thread_cpu
    ( Ndb_cluster_connection & obj, short p0 )
    {
        return obj.set_recv_thread_cpu(p0);
    }

    static int
    Ndb_cluster_connection__unset_recv_thread_cpu
    ( Ndb_cluster_connection & obj )
    {
        return obj.unset_recv_thread_cpu(0);
    }

    static const Ndb *
    Ndb_cluster_connection__get_next_ndb_object
    ( Ndb_cluster_connection & obj, const Ndb * p0 )
    {
        return obj.get_next_ndb_object(p0);
    }

    static const char *
    Ndb_cluster_connection__get_system_name
    ( const Ndb_cluster_connection & obj )
    {
        return obj.get_system_name();
    }

// ---------------------------------------------------------------------------

};

#endif // NdbApiWrapper_hpp
