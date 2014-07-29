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
#include "NativeMethodCall.h"

using namespace v8;

Envelope NdbccEnvelope("Ndb_cluster_connection");

/*  Ndb_cluster_connection(const char * connectstring = 0);
*/
Handle<Value> Ndb_cluster_connection_new_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<const char *> arg0(args[0]);
  
  Ndb_cluster_connection * c = new Ndb_cluster_connection(arg0.toC());

  /* We do not expose set_max_adaptive_send_time() to JavaScript nor even
     consider using the default value of 10 ms.
  */
  c->set_max_adaptive_send_time(1);
  
  wrapPointerInObject(c, NdbccEnvelope, args.This());
  freeFromGC(c, args.This());
  return args.This();
}


/*   void set_name(const char *name);
*/
Handle<Value> Ndb_cluster_connection_set_name(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeVoidMethodCall_1_<Ndb_cluster_connection, const char *> MCALL;
  MCALL mcall(& Ndb_cluster_connection::set_name, args);
  mcall.run();
  
  return Undefined();
}

/* int connect(int no_retries=30, int retry_delay_in_seconds=1, int verbose=0);
   3 args SYNC / 4 args ASYNC
*/
Handle<Value> Ndb_cluster_connection_connect(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  Local<Value> ret = Local<Value>(*Undefined());
  
  REQUIRE_MIN_ARGS(3);
  REQUIRE_MAX_ARGS(4);

  typedef NativeMethodCall_3_ <int, Ndb_cluster_connection, int, int, int> MCALL;
  MCALL * mcallptr = new MCALL(& Ndb_cluster_connection::connect, args);

  if(args.Length() == 4) {
    DEBUG_PRINT_DETAIL("async");
    mcallptr->runAsync();
  }
  else {
    DEBUG_PRINT_DETAIL("sync");
    mcallptr->run();
    ret = mcallptr->jsReturnVal();
    delete mcallptr;
  }

  return scope.Close(ret);
}


/*   int wait_until_ready(int timeout_for_first_alive,
                          int timeout_after_first_alive,
                          callback);
     2 args SYNC / 3 args ASYNC
*/
Handle<Value> Ndb_cluster_connection_wait_until_ready(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  Local<Value> ret = Local<Value>(*Undefined());
  
  REQUIRE_MIN_ARGS(2);
  REQUIRE_MAX_ARGS(3);
  
  typedef NativeMethodCall_2_<int, Ndb_cluster_connection, int, int> MCALL;
  MCALL * mcallptr = new MCALL(& Ndb_cluster_connection::wait_until_ready, args);

  if(args.Length() == 3) {
    mcallptr->runAsync();
  }
  else {
    mcallptr->run();
    ret = mcallptr->jsReturnVal();
    delete mcallptr;
  };
  
  return scope.Close(ret);
}


/*  unsigned node_id();
    IMMEDIATE
*/
Handle<Value> Ndb_cluster_connection_node_id(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);  
  
  typedef NativeMethodCall_0_<unsigned int, Ndb_cluster_connection> MCALL;
  MCALL mcall(& Ndb_cluster_connection::node_id, args);
  mcall.run();

  return scope.Close(mcall.jsReturnVal());
 }


Handle<Value> Ndb_cluster_connection_delete_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  typedef NativeDestructorCall<Ndb_cluster_connection> MCALL;
  MCALL * mcallptr = new MCALL(args);
  mcallptr->runAsync();
  return Undefined();
}


Handle<Value> get_latest_error_msg_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);
  
  typedef NativeConstMethodCall_0_<const char *, Ndb_cluster_connection> MCALL;
  MCALL mcall(& Ndb_cluster_connection::get_latest_error_msg, args);
  mcall.run();
  
  return scope.Close(mcall.jsReturnVal());
}


void Ndb_cluster_connection_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER(UDEB_DETAIL);
  Local<FunctionTemplate> JSNdb_cluster_connection;

  DEFINE_JS_CLASS(JSNdb_cluster_connection, "Ndb_cluster_connection", 
                  Ndb_cluster_connection_new_wrapper);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "set_name",
                   Ndb_cluster_connection_set_name);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "connect",
                   Ndb_cluster_connection_connect);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "wait_until_ready",
                   Ndb_cluster_connection_wait_until_ready);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "node_id",
                   Ndb_cluster_connection_node_id);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "get_latest_error_msg",
                   get_latest_error_msg_wrapper);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "delete",
                   Ndb_cluster_connection_delete_wrapper);
  DEFINE_JS_CONSTRUCTOR(target, "Ndb_cluster_connection", JSNdb_cluster_connection);
}
  
