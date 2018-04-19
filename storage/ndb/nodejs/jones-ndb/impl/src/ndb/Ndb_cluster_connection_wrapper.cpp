/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
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


#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"

using namespace v8;

V8WrapperFn Ndb_cluster_connection_set_name;
V8WrapperFn Ndb_cluster_connection_connect;
V8WrapperFn Ndb_cluster_connection_wait_until_ready;
V8WrapperFn Ndb_cluster_connection_node_id;
V8WrapperFn get_latest_error_msg_wrapper;
V8WrapperFn Ndb_cluster_connection_delete_wrapper;


class NdbccEnvelopeClass : public Envelope {
public:
  NdbccEnvelopeClass() : Envelope("Ndb_cluster_connection") {
    addMethod("set_name", Ndb_cluster_connection_set_name);
    addMethod("connect", Ndb_cluster_connection_connect);
    addMethod("wait_until_ready", Ndb_cluster_connection_wait_until_ready);
    addMethod("node_id", Ndb_cluster_connection_node_id);
    addMethod("get_latest_error_msg", get_latest_error_msg_wrapper);
    addMethod("delete", Ndb_cluster_connection_delete_wrapper);
  }
};

NdbccEnvelopeClass NdbccEnvelope;
Envelope ErrorMessageEnvelope("Error Message from const char *");

/*  Ndb_cluster_connection(const char * connectstring = 0);
*/
void Ndb_cluster_connection_new_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<const char *> arg0(args[0]);
  
  Ndb_cluster_connection * c = new Ndb_cluster_connection(arg0.toC());

  /* We do not expose set_max_adaptive_send_time() to JavaScript nor even
     consider using the default value of 10 ms.
  */
  c->set_max_adaptive_send_time(1);

  Local<Value> wrapper = NdbccEnvelope.wrap(c);
  NdbccEnvelope.freeFromGC(c, wrapper);

  args.GetReturnValue().Set(wrapper);
}


/*   void set_name(const char *name);
*/
void Ndb_cluster_connection_set_name(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeVoidMethodCall_1_<Ndb_cluster_connection, const char *> MCALL;
  MCALL mcall(& Ndb_cluster_connection::set_name, args);
  mcall.run();
  
  args.GetReturnValue().SetUndefined();
}

/* int connect(int no_retries=30, int retry_delay_in_seconds=1, int verbose=0);
   3 args SYNC / 4 args ASYNC
*/
void Ndb_cluster_connection_connect(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());

  args.GetReturnValue().SetUndefined();
  REQUIRE_MIN_ARGS(3);
  REQUIRE_MAX_ARGS(4);

  typedef NativeMethodCall_3_ <int, Ndb_cluster_connection, int, int, int> MCALL;

  if(args.Length() == 4) {
    DEBUG_PRINT_DETAIL("async");
    MCALL * mcallptr = new MCALL(& Ndb_cluster_connection::connect, args);
    mcallptr->runAsync();
  }
  else {
    DEBUG_PRINT_DETAIL("sync");
    MCALL mcall(& Ndb_cluster_connection::connect, args);
    mcall.run();
    args.GetReturnValue().Set(mcall.jsReturnVal());
  }
}


/*   int wait_until_ready(int timeout_for_first_alive,
                          int timeout_after_first_alive,
                          callback);
     2 args SYNC / 3 args ASYNC
*/
void Ndb_cluster_connection_wait_until_ready(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());

  args.GetReturnValue().SetUndefined();
  REQUIRE_MIN_ARGS(2);
  REQUIRE_MAX_ARGS(3);
  
  typedef NativeMethodCall_2_<int, Ndb_cluster_connection, int, int> MCALL;

  if(args.Length() == 3) {
    MCALL * mcallptr = new MCALL(& Ndb_cluster_connection::wait_until_ready, args);
    mcallptr->runAsync();
  }
  else {
    MCALL mcall(& Ndb_cluster_connection::wait_until_ready, args);
    mcall.run();
    args.GetReturnValue().Set(mcall.jsReturnVal());
  };
}


/*  unsigned node_id();
    IMMEDIATE
*/
void Ndb_cluster_connection_node_id(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(0);  
  
  typedef NativeMethodCall_0_<unsigned int, Ndb_cluster_connection> MCALL;
  MCALL mcall(& Ndb_cluster_connection::node_id, args);
  mcall.run();
  args.GetReturnValue().Set(mcall.jsReturnVal());
 }


void Ndb_cluster_connection_delete_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeDestructorCall<Ndb_cluster_connection> MCALL;
  MCALL * mcallptr = new MCALL(args);
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}


void get_latest_error_msg_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(0);
  
  typedef NativeConstMethodCall_0_<const char *, Ndb_cluster_connection> MCALL;
  MCALL mcall(& Ndb_cluster_connection::get_latest_error_msg, args);
  mcall.wrapReturnValueAs(& ErrorMessageEnvelope);
  mcall.run();
  
  args.GetReturnValue().Set(mcall.jsReturnVal());
}


void Ndb_cluster_connection_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "Ndb_cluster_connection", Ndb_cluster_connection_new_wrapper);
}
  
