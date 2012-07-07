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


#include <NdbApi.hpp>

#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "async_common.h"

using namespace v8;


/*  Ndb_cluster_connection(const char * connectstring = 0);
*/
Handle<Value> Ndb_cluster_connection_new_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  
  JsValueConverter<const char *> arg0(args[0]);
  
  Ndb_cluster_connection * c = new Ndb_cluster_connection(arg0.toC());
  
  return JsConstructorThis(args, c);
}


/*   void set_name(const char *name);
     TODO: Is this sync or async?
*/
Handle<Value> Ndb_cluster_connection_set_name(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  
  NativeVoidMethodCall_1_<Ndb_cluster_connection, const char *> mcall(args);
  mcall.method = & Ndb_cluster_connection::set_name;
  mcall.run();
  
  return scope.Close(JS_VOID_RETURN);
}


/* int connect(int no_retries=30, int retry_delay_in_seconds=1, int verbose=0);
*/
Handle<Value> Ndb_cluster_connection_connectSync(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(3);

  NativeMethodCall_3_ <int, Ndb_cluster_connection, int, int, int> mcall(args);
  mcall.method = & Ndb_cluster_connection::connect;
  mcall.run();
      
  return scope.Close(mcall.jsReturnVal());
}


Handle<Value>Ndb_cluster_connection_connectAsync(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(4);
  
  NativeMethodCall_3_ <int, Ndb_cluster_connection, int, int, int> * mcallptr = 
    new NativeMethodCall_3_ <int, Ndb_cluster_connection, int, int, int>(args);

  mcallptr->method = & Ndb_cluster_connection::connect;
  mcallptr->setCallback(args[3]);

  uv_work_t *req = new uv_work_t;
  req->data = (void *) mcallptr;
  uv_queue_work(uv_default_loop(), req, work_thd_run, main_thd_complete);

  return scope.Close(JS_VOID_RETURN);
}


/*   int wait_until_ready(int timeout_for_first_alive,
                          int timeout_after_first_alive,
                          callback);
     ASYNC
*/
Handle<Value> Ndb_cluster_connection_wait_until_ready(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(3);
  
  NativeMethodCall_2_<int, Ndb_cluster_connection, int, int> * mcallptr = 
    new NativeMethodCall_2_<int, Ndb_cluster_connection, int, int>(args);

  mcallptr->method = & Ndb_cluster_connection::wait_until_ready;
  mcallptr->setCallback(args[3]);

  uv_work_t *req = new uv_work_t;
  req->data = (void *) mcallptr;
  uv_queue_work(uv_default_loop(), req, work_thd_run, main_thd_complete);
  
  return scope.Close(JS_VOID_RETURN);
}


/*  unsigned node_id();
    IMMEDIATE
*/
Handle<Value> Ndb_cluster_connection_node_id(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);  
  
  NativeMethodCall_0_<unsigned int, Ndb_cluster_connection> mcall(args);
  mcall.method = & Ndb_cluster_connection::node_id;
  mcall.run();

  return scope.Close(mcall.jsReturnVal());
 }


Handle<Value> Ndb_cluster_connection_delete_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);  
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);

  delete c;
  return scope.Close(JS_VOID_RETURN);
}


void Ndb_cluster_connection_initOnLoad(Handle<Object> target) {
  Local<FunctionTemplate> JSNdb_cluster_connection;

  DEFINE_JS_CLASS(JSNdb_cluster_connection, "Ndb_cluster_connection", 
                  Ndb_cluster_connection_new_wrapper);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "set_name",
                   Ndb_cluster_connection_set_name);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "connectSync",
                   Ndb_cluster_connection_connectSync);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "connectAsync",
                   Ndb_cluster_connection_connectAsync);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "wait_until_ready",
                   Ndb_cluster_connection_wait_until_ready);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "node_id",
                   Ndb_cluster_connection_node_id);
  DEFINE_JS_METHOD(JSNdb_cluster_connection, "delete",
                   Ndb_cluster_connection_delete_wrapper);
  DEFINE_JS_CONSTRUCTOR(target, "Ndb_cluster_connection", JSNdb_cluster_connection);
}
  
