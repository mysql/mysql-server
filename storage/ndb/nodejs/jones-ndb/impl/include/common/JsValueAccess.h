/*
Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef NODEJS_ADAPTER_INCLUDE_JSVALUEACCESS_H
#define NODEJS_ADAPTER_INCLUDE_JSVALUEACCESS_H

#include <cstddef>              // size_t

using v8::String;
using v8::Number;
using v8::NewStringType;
using v8::Local;
using v8::Object;
using v8::Isolate;
using v8::Value;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Boolean;
using v8::Integer;
using v8::Number;
using v8::Date;
using v8::Name;
using v8::TryCatch;
using v8::Message;

typedef v8::FunctionCallbackInfo<Value> Arguments;

#include "node_buffer.h"

// HasProperty
template <typename KEY>
inline bool HasProperty(Isolate *isolate, Local<Object> obj, KEY key) {
  return obj->Has(isolate->GetCurrentContext(), key).ToChecked();
}

template <typename KEY>
inline bool HasProperty(const Arguments &args, Local<Object> obj, KEY key) {
  return HasProperty(args.GetIsolate(), obj, key);
}

// Get
template <typename CONTAINER, typename KEY>
inline Local<Value> Get(Isolate *isolate, CONTAINER obj, KEY key) {
  return obj->Get(isolate->GetCurrentContext(), key).ToLocalChecked();
}

template <typename T>
inline Local<Value> Get(const Arguments &args, Local<Object> obj, T key) {
  return Get(args.GetIsolate(), obj, key);
}

template <typename T>
inline Local<Value> Get(Local<Object> obj, T key) {
  return Get(obj->GetIsolate(), obj, key);
}

// Create New JavaScript Strings
inline Local<String> NewUtf8String(Isolate *isolate, const char *str,
                                   size_t len)
{
  return  String::NewFromUtf8(isolate, str, NewStringType::kNormal, len).
    ToLocalChecked();
}

inline Local<String> NewUtf8String(Isolate *isolate, const char *str,
                                   NewStringType type = NewStringType::kNormal)
{
  return String::NewFromUtf8(isolate, str, type).ToLocalChecked();
}

inline Local<String> NewStringSymbol(Isolate *isolate, const char *str) {
  return NewUtf8String(isolate, str, NewStringType::kInternalized);
}

inline Local<String> NewStringSymbol(Local<Object> obj, const char *str) {
  return NewStringSymbol(obj->GetIsolate(), str);
}

template <typename EXT>
inline Local<String> NewExternalOneByteString(Isolate *isolate, EXT ext) {
  return String::NewExternalOneByte(isolate, ext).ToLocalChecked();
}

template <typename EXT>
inline Local<String> NewExternalTwoByteString(Isolate *isolate, EXT ext) {
  return String::NewExternalTwoByte(isolate, ext).ToLocalChecked();
}


inline Local<String> ToString(const Arguments &args, const char *str) {
  return NewUtf8String(args.GetIsolate(), str);
}

inline Local<String> ToString(const char *str) {
  return NewUtf8String(Isolate::GetCurrent(), str);
}

// JS Value to String
inline Local<String> ToString(Isolate *isolate, Local<Value> val) {
  return val->ToString(isolate->GetCurrentContext()).ToLocalChecked();
}

inline Local<String> ArgToString(const Arguments &args, int i) {
  return ToString(args.GetIsolate(), args[i]);
}

inline Local<String> ElementToString(Local<Object> array, int idx) {
  return ToString(array->GetIsolate(), Get(array->GetIsolate(), array, idx));
}

// ToObject
inline Local<Object> ToObject(Isolate *isolate, Local<Value> val) {
  return val->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
}

inline Local<Object> ToObject(const Arguments &args, Local<Value> val) {
  return ToObject(args.GetIsolate(), val);
}

inline Local<Object> ArgToObject(const Arguments &args, int i) {
  return ToObject(args, args[i]);
}

inline Local<Object> ToObject(Local<Value> val) {
  return ToObject(Isolate::GetCurrent(), val);
}

template <typename T>
inline Local<Object> ElementToObject(Local<Object> obj, T key)
{
  return ToObject(obj->GetIsolate(), Get(obj->GetIsolate(), obj, key));
}


// SetProp
template <typename T>
inline void SetProp(Isolate *isolate, T obj, const char * key, Local<Value> value) {
  Maybe<bool> r = obj->Set(isolate->GetCurrentContext(),
                           NewStringSymbol(isolate, key), value);
  r.Check();
}

template <typename T>
inline void SetProp(Isolate *isolate, T obj, int i, Local<Value> value) {
  Maybe<bool> r = obj->Set(isolate->GetCurrentContext(), i, value);
  r.Check();
}

inline void SetProp(Isolate *isolate, Local<Object> obj,
                    Local<String> key, Local<Value> value) {
  Maybe<bool> r = obj->Set(isolate->GetCurrentContext(), key, value);
  r.Check();
}

inline void SetProp(Local<Object> obj, Local<String> key, Local<Value> value) {
  return SetProp(obj->GetIsolate(), obj, key, value);
}

template <typename CONTAINER, typename KEY>
inline void SetProp(Isolate *isolate, CONTAINER obj, KEY key, const char *str) {
  return SetProp(isolate, obj, key, NewUtf8String(isolate, str));
}

template <typename CONTAINER, typename KEY>
inline void SetProp(Isolate *isolate, CONTAINER obj, KEY key, int i) {
  return SetProp(isolate, obj, key, v8::Int32::New(isolate, i));
}

template <typename KEY>
inline void SetProp(Local<Object> obj, KEY key, Local<Value> value) {
  SetProp(obj->GetIsolate(), obj, key, value);
}

// Node::Buffer
inline Local<Object> CopyToJsBuffer(Isolate* i, const char* data, size_t len) {
  return node::Buffer::Copy(i, data, len).ToLocalChecked();
}

inline Local<Object> NewJsBuffer(Isolate* iso, char* data, size_t len) {
  return node::Buffer::New(iso, data, len).ToLocalChecked();
}

inline Local<Object> NewJsBuffer(Isolate* isolate, char* data, size_t len,
                                 node::Buffer::FreeCallback cb) {
  /* nullptr is hint */
  return node::Buffer::New(isolate, data, len, cb, nullptr).ToLocalChecked();
}

inline Local<Object> NewJsBuffer(Isolate *isolate, Local<String> str) {
  return node::Buffer::New(isolate, str).ToLocalChecked();
}

inline Local<Object> NewJsBuffer(Isolate* iso, size_t len) {
  return node::Buffer::New(iso, len).ToLocalChecked();
}

inline size_t GetBufferLength(Local<Object> obj) {
  return node::Buffer::Length(obj);
}

inline char * GetBufferData(Local<Object> obj) {
  return node::Buffer::Data(obj);
}

inline bool IsJsBuffer(Local<Value> value) {
  return node::Buffer::HasInstance(value);
}

// Int32
inline int GetInt32Value(Isolate *isolate, Local<Value> val) {
  return val->Int32Value(isolate->GetCurrentContext()).ToChecked();
}

inline int GetInt32Value(const Arguments &args, Local<Value> val) {
  return GetInt32Value(args.GetIsolate(), val);
}

inline int GetInt32Value(Local<Value> val) {
  return GetInt32Value(Isolate::GetCurrent(), val);
}

template <typename T>
inline int GetInt32Property(Isolate *isolate, Local<Object> obj, T key) {
  return GetInt32Value(isolate, Get(isolate, obj, key));
}

template <typename T>
inline int GetInt32Property(Local<Object> obj, T key) {
  return GetInt32Property(obj->GetIsolate(), obj, key);
}

inline int GetInt32Arg(const Arguments &args, int i) {
  return GetInt32Value(args.GetIsolate(), args[i]);
}

// Uint32
inline uint32_t GetUint32Value(Isolate *isolate, Local<Value> val) {
  return val->Uint32Value(isolate->GetCurrentContext()).ToChecked();
}

inline uint32_t GetUint32Value(const Arguments &args, Local<Value> val) {
  return GetUint32Value(args.GetIsolate(), val);
}

inline uint32_t GetUint32Value(Local<Value> val) {
  return GetUint32Value(Isolate::GetCurrent(), val);
}

inline uint32_t GetUint32Arg(const Arguments &args, int i) {
  return GetUint32Value(args.GetIsolate(), args[i]);
}

template <typename T>
inline uint32_t GetUint32Property(Local<Object> obj, T key) {
  return GetUint32Value(obj->GetIsolate(), Get(obj, key));
}

// Number
inline double ToNumber(Isolate *isolate, Local<Value> val) {
  return val->ToNumber(isolate->GetCurrentContext()).ToLocalChecked()->Value();
}

inline double ToNumber(Local<Value> val) {
  return ToNumber(Isolate::GetCurrent(), val);
}

// Integer
inline int64_t GetIntegerValue(Isolate *isolate, Local<Value> val) {
  return val->IntegerValue(isolate->GetCurrentContext()).ToChecked();
}

inline int64_t GetIntegerValue(Local<Value> val) {
  return GetIntegerValue(Isolate::GetCurrent(), val);
}

// Bool
inline bool GetBoolValue(Isolate *isolate, Local<Value> val) {
  return val->BooleanValue(isolate);
}

inline bool GetBoolValue(const Arguments &args, Local<Value> val) {
  return GetBoolValue(args.GetIsolate(), val);
}

inline bool GetBoolValue(Local<Value> val) {
  return GetBoolValue(Isolate::GetCurrent(), val);
}

template <typename T>
inline bool GetBoolProperty(Isolate *isolate, Local<Object> obj, T key) {
  return GetBoolValue(isolate, Get(isolate, obj, key));
}

template <typename T>
inline bool GetBoolProperty(Local<Object> obj, T key) {
  return GetBoolProperty(obj->GetIsolate(), obj, key);
}

// StackTrace
inline Local<Value> GetStackTrace(Isolate *isolate, TryCatch *err) {
  return ToString(isolate,
    err->StackTrace(isolate->GetCurrentContext()).ToLocalChecked());
}

#endif
