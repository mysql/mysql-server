/*
 Copyright (c) 2013, 2016,, Oracle and/or its affiliates. All rights
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

#define STRING(I, S) v8::String::NewFromUtf8(I, S)

#define NEW_STRING(S) STRING(v8::Isolate::GetCurrent(), S)

#define SYMBOL(I, S) v8::String::NewFromUtf8(I, S, v8::String::kInternalizedString)

#define NEW_SYMBOL(S) SYMBOL(v8::Isolate::GetCurrent(), S)

#define THROW_ERROR(MESSAGE) \
  ThrowException(Exception::Error(NEW_STRING(MESSAGE))))
  
#define THROW_TYPE_ERROR(MESSAGE) \
  Isolate::GetCurrent()->ThrowException(Exception::TypeError(NEW_STRING(MESSAGE)))
  
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
  }

#define REQUIRE_MAX_ARGS(N) \
  if(args.Length() > N) { \
    THROW_TYPE_ERROR("Requires no more than " #N " arguments"); \
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

#define NEW_FN_TEMPLATE(FN) \
  v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FN)

#define DEFINE_JS_FUNCTION(TARGET, NAME, FN) \
  TARGET->Set(NEW_SYMBOL(NAME), NEW_FN_TEMPLATE(FN)->GetFunction())

/* For SetInternalFieldCount() , see:
 http://groups.google.com/group/v8-users/browse_thread/thread/d8bcb33178a55223
*/  

#define DEFINE_JS_ACCESSOR(TARGET, property, getter)                 \
  (TARGET)->SetAccessor(NEW_SYMBOL(property), getter)

#define DEFINE_JS_INT(TARGET, name, value) \
  (TARGET)->ForceSet(NEW_SYMBOL(name), \
                Integer::New(v8::Isolate::GetCurrent(), value), \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))

#define DEFINE_JS_CONSTANT(TARGET, constant) \
   DEFINE_JS_INT(TARGET, #constant, constant)

   
