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
#include <uv.h>

#include <NdbApi.hpp>

#include "js_wrapper_macros.h"
#include "JsConverter.h"
#include "AsyncMethodCall.h"
#include "async_common.h"

using namespace v8;


/* Async_Ndb_cluster_connection_connect */
class Async_Ndb_cluster_connection_connect : public AsyncMethodCall {
public:
  Async_Ndb_cluster_connection_connect(Ndb_cluster_connection *c,
                                       int a1, int a2, int a3) :
                                       obj(c), arg1(a1), arg2(a2), arg3(a3)
                                       {};
  void work_thd_run(void);
  void main_thd_complete(v8::Local<v8::Object>);

private:
  Ndb_cluster_connection *obj;
  int arg1, arg2, arg3;
  int rval;
};


void Async_Ndb_cluster_connection_connect::work_thd_run() {
  rval = obj->connect(arg1, arg2, arg3);
}


void Async_Ndb_cluster_connection_connect::main_thd_complete(Local<Object> context) {

  Handle<Value> cb_args[1];   //todo: actually 2 arguments
  
  cb_args[0] = toJS<int>(rval);  // return value
  
  callback->Call(context, 1, cb_args);
}



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
*/
Handle<Value> Ndb_cluster_connection_set_name(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);

  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);
  
  JsValueConverter<const char *> arg0(args[0]);
  c->set_name(arg0.toC());
  
  //FIXME: How to map function returning void?
  return scope.Close(toJS<int>(0));
}


/* int connect(int no_retries=30, int retry_delay_in_seconds=1, int verbose=0);
*/
Handle<Value> Ndb_cluster_connection_connectSync(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(3);
  
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);
  JsValueConverter<int> arg0(args[0]);
  JsValueConverter<int> arg1(args[1]);
  JsValueConverter<int> arg2(args[2]);

  int r = c->connect(arg0.toC(), arg1.toC(), arg2.toC());
  
  return scope.Close(toJS<int>(r));
}


Handle<Value>Ndb_cluster_connection_connectAsync(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(4);
  
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);
  JsValueConverter<int> arg0(args[0]);
  JsValueConverter<int> arg1(args[1]);
  JsValueConverter<int> arg2(args[2]);

  Async_Ndb_cluster_connection_connect *mcall = 
    new Async_Ndb_cluster_connection_connect(c, arg0.toC(), arg1.toC(), arg2.toC());

  uv_work_t *req = new uv_work_t;
  req->data = (void *) mcall;
  
  mcall->callback = Persistent<Function>::New(Local<Function>::Cast(args[3]));

  uv_queue_work(uv_default_loop(), req, work_thd_run, main_thd_complete);

  
  return scope.Close(toJS<int>(0));
}


/*   int wait_until_ready(int timeout_for_first_alive,
                          int timeout_after_first_alive);
*/
Handle<Value> Ndb_cluster_connection_wait_until_ready(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);
  
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);
  JsValueConverter<int> arg0(args[0]);
  JsValueConverter<int> arg1(args[1]);
  
  int r = c->wait_until_ready(arg0.toC(), arg1.toC());
  return scope.Close(toJS<int>(r));
}


/*  unsigned node_id();
*/
Handle<Value> Ndb_cluster_connection_node_id(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);
  return scope.Close(toJS<int>(c->node_id()));
}



Handle<Value> Ndb_cluster_connection_delete_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);  
  Ndb_cluster_connection *c = JsMethodThis<Ndb_cluster_connection>(args);

  delete c;
  return scope.Close(toJS<int>(0));
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
  
