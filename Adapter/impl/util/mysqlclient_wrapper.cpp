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

#include <mysql.h>

#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NativeCFunctionCall.h"

using namespace v8;

Handle<Value> mysql_init_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);  // there is one arg but we supply it here
  
  NativeCFunctionCall_1_<MYSQL *, MYSQL *> ncall(args);
  ncall.arg0 = 0; 
  ncall.function = & mysql_init;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> mysql_close_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);
  
  NativeCVoidFunctionCall_1_<MYSQL *> ncall(args);
  ncall.function = & mysql_close;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> mysql_real_connect_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(8);
  
  NativeCFunctionCall_8_<MYSQL *, MYSQL *, const char *, const char *, const char *,
                         const char *, unsigned int, const char *, unsigned long> 
                         ncall(args);
  ncall.function = & mysql_real_connect;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> mysql_error_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  
  NativeCFunctionCall_1_<const char *, MYSQL *> ncall(args);
  ncall.function = & mysql_error;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> mysql_query_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);
  
  NativeCFunctionCall_2_<int, MYSQL *, const char *> ncall(args);
  ncall.function = & mysql_query;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


void mysqlclient_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "mysql_init", mysql_init_wrapper);
  DEFINE_JS_FUNCTION(target, "mysql_close", mysql_close_wrapper);
  DEFINE_JS_FUNCTION(target, "mysql_real_connect", mysql_real_connect_wrapper);
  DEFINE_JS_FUNCTION(target, "mysql_error", mysql_error_wrapper);
  DEFINE_JS_FUNCTION(target, "myqsl_query", mysql_query_wrapper);  
}

V8BINDER_LOADABLE_MODULE(mysqlclient, mysqlclient_initOnLoad)



  
  