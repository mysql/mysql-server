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

#define THROW_ERROR(MESSAGE) \
  ThrowException(Exception::Error(String::New(MESSAGE)))
  
#define THROW_TYPE_ERROR(MESSAGE) \
  ThrowException(Exception::TypeError(String::New(MESSAGE)))
  
/*#define REQUIRE_ARGS_LENGTH(N) \
  if(args.Length() != N) { \
    THROW_TYPE_ERROR("Requires " #N " arguments"); \
    return scope.Close(Undefined()); \
  }
*/
#define REQUIRE_ARGS_LENGTH(N) assert(args.Length() == N);


#define REQUIRE_MIN_ARGS(N) \
  if(args.Length() < N) { \
    THROW_TYPE_ERROR("Requires at least " #N " arguments"); \
    return scope.Close(Undefined()); \
  }

#define REQUIRE_MAX_ARGS(N) \
  if(args.Length() > N) { \
    THROW_TYPE_ERROR("Requires no more than " #N " arguments"); \
    return scope.Close(Undefined()); \
  }

/* #define REQUIRE_CONSTRUCTOR_CALL() \
  if(! args.IsConstructCall()) { \
    THROW_ERROR("Must be called as a Constructor call"); \
    return scope.Close(Undefined()); \
  }
*/
#define REQUIRE_CONSTRUCTOR_CALL() assert(args.IsConstructCall()) 


/* #define PROHIBIT_CONSTRUCTOR_CALL() \
  if(args.IsConstructCall()) { \
    THROW_ERROR("May not be used as a Constructor call"); \
    return scope.Close(Undefined()); \
  }
*/
#define PROHIBIT_CONSTRUCTOR_CALL() assert(! args.IsConstructCall())

#define DEFINE_JS_FUNCTION(TARGET, NAME, FN) \
  TARGET->Set(String::NewSymbol(NAME), FunctionTemplate::New(FN)->GetFunction())

/* For SetInternalFieldCount() , see:
 http://groups.google.com/group/v8-users/browse_thread/thread/d8bcb33178a55223
*/  

#define DEFINE_JS_CLASS(JSCLASS, NAME, FN) \
  JSCLASS = FunctionTemplate::New(FN); \
  JSCLASS->SetClassName(String::NewSymbol(NAME)); \
  JSCLASS->InstanceTemplate()->SetInternalFieldCount(2);

/* This could be replaced with NODE_SET_PROTOTYPE_METHOD from node.h */
#define DEFINE_JS_METHOD(CLASS, NAME, FN) \
  DEFINE_JS_FUNCTION(CLASS->PrototypeTemplate(), NAME, FN);

#define DEFINE_JS_CONSTRUCTOR(TARGET, NAME, JSCLASS) \
  TARGET->Set(String::NewSymbol(NAME), \
    Persistent<Function>::New(JSCLASS->GetFunction()));

#define DEFINE_JS_ACCESSOR(TARGET, property, getter)                 \
  (TARGET)->SetAccessor(String::NewSymbol(property), getter)

#define DEFINE_JS_INT(TARGET, name, value) \
  (TARGET)->Set(String::NewSymbol(name), \
                Integer::New(value), \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))

#define DEFINE_JS_CONSTANT(TARGET, constant) \
   DEFINE_JS_INT(TARGET, #constant, constant)

   
