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

#include <node.h>

#include "DBSessionImpl.h"
#include "adapter_global.h"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "unified_debug.h"
#include "NdbWrappers.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Envelope NdbSessionImplEnv("NdbSessionImpl");

/* ndb_session_new()
   UV_WORKER_THREAD
   This is the background thread part of NewDBSessionImpl
*/
ndb_session * ndb_session_new(Ndb_cluster_connection *conn, const char *db) {
  DEBUG_MARKER(UDEB_DETAIL);
  ndb_session * sess = new ndb_session;
    
  sess->ndb = new Ndb(conn, db);
  sess->dbname = db;
  sess->ndb->init();
  sess->dict = sess->ndb->getDictionary();    // get Dictionary
  
  return sess;
}


Handle<Value> getNdb(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(1);
  ndb_session * sess = unwrapPointer<ndb_session *>(args[0]->ToObject());
  return Ndb_Wrapper(sess->ndb);
}


/* NewDBSessionImpl()
   SYNC (2 args) / ASYNC (3 args)
   arg0: Ndb_cluster_connection
   arg1: Name of database
   arg2: callback
   Callback will receive an ndb_session *.
*/
Handle<Value>NewDBSessionImpl(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  Local<Value> ret;
  
  REQUIRE_MIN_ARGS(2);
  REQUIRE_MAX_ARGS(3);
  
  typedef NativeCFunctionCall_2_<ndb_session *, Ndb_cluster_connection *, 
                                 const char *> NCALL;
  NCALL *ncallptr = new NCALL(& ndb_session_new, args);
  ncallptr->wrapReturnValueAs(& NdbSessionImplEnv);

  if(args.Length() == 3) {
    DEBUG_PRINT("async (%d arguments)", args.Length());
    ncallptr->runAsync();
    ret = JS_VOID_RETURN;
  }
  else { 
    DEBUG_PRINT("sync (%d arguments)", args.Length());
    ncallptr->run();
    ret = ncallptr->jsReturnVal();
    delete ncallptr;
  }

  return scope.Close(ret);
}


Handle<Value>DeleteDBSessionImpl(const Arguments &args) {
  HandleScope scope;
  
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(1);
  ndb_session * sess = unwrapPointer<ndb_session *>(args[0]->ToObject());

  delete sess->ndb;
  delete sess;
  
  return scope.Close(JS_VOID_RETURN);
}


void DBSessionImpl_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  Persistent<Object> dbsession_obj = Persistent<Object>(Object::New());
  
  DEFINE_JS_FUNCTION(dbsession_obj, "create", NewDBSessionImpl);
  DEFINE_JS_FUNCTION(dbsession_obj, "getNdb", getNdb);
  DEFINE_JS_FUNCTION(dbsession_obj, "destroy", DeleteDBSessionImpl);
  
  target->Set(Persistent<String>(String::NewSymbol("DBSession")), dbsession_obj);
}


