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

#include <node.h>
#include "JsValueAccess.h"

#define SYMBOL(I, S) NewStringSymbol(I, S)

#define REQUIRE_ARGS_LENGTH(N)                                              \
  if (args.Length() != N) {                                                 \
    args.GetIsolate()->ThrowException(                                      \
        Exception::TypeError(ToString(args, "Requires " #N " arguments"))); \
  }

#define REQUIRE_MIN_ARGS(N)                                     \
  if (args.Length() < N) {                                      \
    args.GetIsolate()->ThrowException(Exception::TypeError(     \
        ToString(args, "Requires at least " #N " arguments"))); \
  }

#define REQUIRE_MAX_ARGS(N)                                         \
  if (args.Length() > N) {                                          \
    args.GetIsolate()->ThrowException(Exception::TypeError(         \
        ToString(args, "Requires no more than " #N " arguments"))); \
  }

#define REQUIRE_CONSTRUCTOR_CALL() assert(args.IsConstructCall())

#define PROHIBIT_CONSTRUCTOR_CALL() assert(!args.IsConstructCall())

#define GET_CONTEXT() v8::Isolate::GetCurrent()->GetCurrentContext()

#define NEW_FN_TEMPLATE(FN) \
  v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), FN)

#define DEFINE_JS_FUNCTION(TARGET, NAME, FN)                               \
  {                                                                        \
    Maybe<bool> result = TARGET->Set(                                      \
        GET_CONTEXT(), NewStringSymbol(TARGET, NAME),                      \
        NEW_FN_TEMPLATE(FN)->GetFunction(GET_CONTEXT()).ToLocalChecked()); \
    result.Check();                                                        \
  }

#define DEFINE_JS_ACCESSOR(isolate, TARGET, property, getter)   \
  (TARGET)                                                      \
      ->SetAccessor((target)->CreationContext(),                \
                    NewStringSymbol(isolate, property), getter) \
      .IsJust()

#define SET_PROPERTY(target, symbol, value, flags)                    \
  (target)                                                            \
      ->DefineOwnProperty((target)->CreationContext(), symbol, value, \
                          static_cast<v8::PropertyAttribute>(flags))  \
      .IsJust()

#define SET_RO_PROPERTY(target, symbol, value) \
  SET_PROPERTY(target, symbol, value, (v8::ReadOnly | v8::DontDelete))

#define DEFINE_JS_INT(TARGET, name, value)               \
  SET_RO_PROPERTY(TARGET, NewStringSymbol(TARGET, name), \
                  v8::Integer::New(v8::Isolate::GetCurrent(), value))

#define DEFINE_JS_CONSTANT(TARGET, constant) \
  DEFINE_JS_INT(TARGET, #constant, constant)
