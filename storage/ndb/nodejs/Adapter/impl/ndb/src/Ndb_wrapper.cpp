/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NativeMethodCall.h"
#include "NativeCFunctionCall.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Handle<Value> getAutoIncValue(const Arguments &);
Handle<Value> closeNdb(const Arguments &);
Handle<Value> getStatistics(const Arguments &);
Handle<Value> getConnectionStatistics(const Arguments &);

class NdbEnvelopeClass : public Envelope {
public:
  NdbEnvelopeClass() : Envelope("Ndb") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", getNdbError<Ndb>);
    DEFINE_JS_FUNCTION(Envelope::stencil, "close", closeNdb);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getStatistics", getStatistics);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getConnectionStatistics", getConnectionStatistics);
  }
  
  Local<Object> wrap(Ndb *ndb) {
    HandleScope scope;    
    Local<Object> wrapper = Envelope::stencil->NewInstance();
    wrapPointerInObject(ndb, *this, wrapper);
    return scope.Close(wrapper);
  }
};

NdbEnvelopeClass NdbEnvelope;

Handle<Value> Ndb_Wrapper(Ndb *ndb) {
  return NdbEnvelope.wrap(ndb);
}

/* Ndb constructor.
   create_ndb(Ndb_cluster_connection, databaseName, callback)
   The constructor is wrapped in a call that also calls ndb->init().
*/
Ndb * async_create_ndb(Ndb_cluster_connection *conn, const char *db) {
  Ndb *ndb = new Ndb(conn, db);
  DEBUG_PRINT("Created Ndb %p", ndb);  
  if(ndb) ndb->init();
  return ndb;
}

Handle<Value> create_ndb(const Arguments &args) {
  REQUIRE_ARGS_LENGTH(3);  

  typedef NativeCFunctionCall_2_<Ndb *, Ndb_cluster_connection *, const char *> MCALL;
  MCALL * mcallptr = new MCALL(& async_create_ndb, args);
  mcallptr->wrapReturnValueAs(& NdbEnvelope);
  mcallptr->runAsync();
  return Undefined();
}


/* getAutoIncrementValue(ndb, table, batch_size, callback);
   We can't map Ndb::getAutoIncrementValue() directly due to in/out param.
   The JS Wrapper function will simply return 0 on error.
*/
Uint64 getAutoInc(Ndb *ndb, const NdbDictionary::Table * table, uint32_t batch) {
  Uint64 autoinc;
  DEBUG_PRINT("getAutoIncrementValue %p", ndb);
  int r = ndb->getAutoIncrementValue(table, autoinc, batch);
  if(r == -1) autoinc = 0;
  return autoinc;
}

Handle<Value> getAutoIncValue(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(4);  
  typedef NativeCFunctionCall_3_<Uint64, Ndb *, const NdbDictionary::Table *,
                                 uint32_t> MCALL;
  MCALL * mcallptr = new MCALL(& getAutoInc, args);
  mcallptr->runAsync();
  return Undefined();
}


Handle<Value> getStatistics(const Arguments &args) {
  HandleScope scope;
  Ndb *ndb = unwrapPointer<Ndb *>(args.Holder());
  Local<Object> stats = Object::New();
  for(int i = 0 ; i < Ndb::NumClientStatistics ; i ++) {
    stats->Set(String::NewSymbol(ndb->getClientStatName(i)),
               Number::New(ndb->getClientStat(i)),
               ReadOnly);
  }
  return scope.Close(stats);
}


Handle<Value> getConnectionStatistics(const Arguments &args) {
  HandleScope scope;
  Uint64 ndb_stats[Ndb::NumClientStatistics];

  Ndb *ndb = unwrapPointer<Ndb *>(args.Holder());
  Ndb_cluster_connection & c = ndb->get_ndb_cluster_connection();

  c.collect_client_stats(ndb_stats, Ndb::NumClientStatistics);

  Local<Object> stats = Object::New();
  for(int i = 0 ; i < Ndb::NumClientStatistics ; i ++) {
    stats->Set(String::NewSymbol(ndb->getClientStatName(i)),
               Number::New(ndb_stats[i]),
               ReadOnly);
  }
  return scope.Close(stats);
}

Handle<Value> closeNdb(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  typedef NativeDestructorCall<Ndb> MCALL;
  MCALL * mcallptr = new MCALL(args);
  mcallptr->runAsync();
  return Undefined();
}


void NdbWrapper_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "getAutoIncrementValue", getAutoIncValue);
  DEFINE_JS_FUNCTION(target, "create_ndb", create_ndb);
}
