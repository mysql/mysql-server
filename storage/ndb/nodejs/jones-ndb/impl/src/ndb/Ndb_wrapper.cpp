/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#include <NdbApi.hpp>

#include "NativeCFunctionCall.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "Record.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

// FIXME: All of this should be on SessionImpl with Ndb object not directly
// exposed to JavaScript

V8WrapperFn getAutoIncValue, closeNdb, getStatistics, getConnectionStatistics;

class NdbEnvelopeClass : public Envelope {
 public:
  NdbEnvelopeClass() : Envelope("Ndb") {
    EscapableHandleScope scope(v8::Isolate::GetCurrent());
    addMethod("getNdbError", getNdbError<Ndb>);
    addMethod("close", closeNdb);
    addMethod("getStatistics", getStatistics);
    addMethod("getConnectionStatistics", getConnectionStatistics);
  }
};

NdbEnvelopeClass NdbEnvelope;

Local<Value> Ndb_Wrapper(Ndb *ndb) { return NdbEnvelope.wrap(ndb); }

/* Ndb constructor.
   create_ndb(Ndb_cluster_connection, databaseName, callback)
   The constructor is wrapped in a call that also calls ndb->init().
*/
Ndb *async_create_ndb(Ndb_cluster_connection *conn, const char *db) {
  Ndb *ndb = new Ndb(conn, db);
  DEBUG_PRINT("Created Ndb %p", ndb);
  if (ndb) ndb->init();
  return ndb;
}

void create_ndb(const Arguments &args) {
  REQUIRE_ARGS_LENGTH(3);

  typedef NativeCFunctionCall_2_<Ndb *, Ndb_cluster_connection *, const char *>
      MCALL;
  MCALL *mcallptr = new MCALL(&async_create_ndb, args);
  mcallptr->wrapReturnValueAs(&NdbEnvelope);
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

/* getAutoIncrementValue(ndb, table, batch_size, callback);
   We can't map Ndb::getAutoIncrementValue() directly due to in/out param.
   The JS Wrapper function will simply return 0 on error.
*/
Uint64 getAutoInc(Ndb *ndb, const NdbDictionary::Table *table, uint32_t batch) {
  Uint64 autoinc;
  DEBUG_PRINT("getAutoIncrementValue %p", ndb);
  int r = ndb->getAutoIncrementValue(table, autoinc, batch);
  if (r == -1) autoinc = 0;
  return autoinc;
}

void getAutoIncValue(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(4);
  typedef NativeCFunctionCall_3_<Uint64, Ndb *, const NdbDictionary::Table *,
                                 uint32_t>
      MCALL;
  MCALL *mcallptr = new MCALL(&getAutoInc, args);
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

void getStatistics(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  Ndb *ndb = unwrapPointer<Ndb *>(args.Holder());

  Local<Object> stats = Object::New(args.GetIsolate());
  for (int i = 0; i < Ndb::NumClientStatistics; i++) {
    SetProp(stats, ndb->getClientStatName(i),
            Number::New(args.GetIsolate(), ndb->getClientStat(i)));
  }

  args.GetReturnValue().Set(scope.Escape(stats));
}

void getConnectionStatistics(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  Uint64 ndb_stats[Ndb::NumClientStatistics];

  Ndb *ndb = unwrapPointer<Ndb *>(args.Holder());
  Ndb_cluster_connection &c = ndb->get_ndb_cluster_connection();

  c.collect_client_stats(ndb_stats, Ndb::NumClientStatistics);

  Local<Object> stats = Object::New(args.GetIsolate());
  for (int i = 0; i < Ndb::NumClientStatistics; i++) {
    SetProp(stats, ndb->getClientStatName(i),
            Number::New(args.GetIsolate(), ndb_stats[i]));
  }

  args.GetReturnValue().Set(scope.Escape(stats));
}

void closeNdb(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  typedef NativeDestructorCall<Ndb> MCALL;
  MCALL *mcallptr = new MCALL(args);
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

void NdbWrapper_initOnLoad(Local<Object> target) {
  DEFINE_JS_FUNCTION(target, "getAutoIncrementValue", getAutoIncValue);
  DEFINE_JS_FUNCTION(target, "create_ndb", create_ndb);
}
