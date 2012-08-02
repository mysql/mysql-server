/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

#include <v8.h>

#include "DBSessionImpl.h"
#include "NativeCFunctionCall.h"
#include "js_wrapper_macros.h"
#include "unified_debug.h"

using namespace v8;

Envelope NdbSessionImplEnv("NdbSessionImpl");

/* ndb_session_new()
   UV_WORKER_THREAD
   This is the background thread part of NewDBSessionImpl
*/
ndb_session * ndb_session_new(Ndb_cluster_connection *conn, const char *db) {
  DEBUG_ENTER();
  ndb_session * sess = new ndb_session;
  
  sess->ndb = new Ndb(conn);

  DEBUG_TRACE();
  sess->ndb->init();

  DEBUG_TRACE();
  sess->ndb->setDatabaseName(db);  

  DEBUG_TRACE();
  sess->dict = sess->ndb->getDictionary();    // get Dictionary
  sess->err = sess->ndb->getNdbError();        // get NdbError
  
  DEBUG_TRACE();
  return sess;
}


/* NewDBSessionImpl()
   ASYNC
   arg0: Ndb_cluster_connection
   arg1: Name of database
   arg2: callback
   Callback will receive an ndb_session *.
*/
Handle<Value>NewDBSessionImpl(const Arguments &args) {
  DEBUG_MARKER();
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(3);
  
  typedef NativeCFunctionCall_2_<ndb_session *, Ndb_cluster_connection *, 
                                 const char *> NCALL;
  NCALL *ncallptr = new NCALL(args);

  DEBUG_PRINT(UDEB_DEBUG, "IN NewDbSessionImpl, UNWRAPPED NdbCC: %p", ncallptr->arg0);

  ncallptr->function = & ndb_session_new;
  ncallptr->setCallback(args[2]);
  ncallptr->runAsync();

  return scope.Close(JS_VOID_RETURN);
}



void DBSessionImpl_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER();
  DEFINE_JS_FUNCTION(target, "NewDBSessionImpl", NewDBSessionImpl);
}


