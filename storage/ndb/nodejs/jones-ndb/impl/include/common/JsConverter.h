/*
 Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
 
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

#ifndef NODEJS_ADAPTER_INCLUDE_JSCONVERTER_H
#define NODEJS_ADAPTER_INCLUDE_JSCONVERTER_H

#include "JsWrapper.h"
#include "node_buffer.h"

typedef Local<v8::Value> jsvalue;
typedef Handle<v8::Object> jsobject;
 

/*****************************************************************
 JsValueConverter 
 Value conversion from JavaScript to C
 
 Some additional NDB-Specific specializations are in NdBJsConverters.h
 
******************************************************************/

/* Generic (for pointers).
   If you get an "invalid static cast from type void *", then the compiler
   may be erroneously falling back on this implementation. 
*/
template <typename T> class JsValueConverter {
public:  
  JsValueConverter(jsvalue v) {
    if(v->IsNull()) {
      native_object = 0;
    } else {
      DEBUG_ASSERT(v->IsObject());
      Local<Object> obj = v->ToObject();
      DEBUG_ASSERT(obj->InternalFieldCount() == 2);
      native_object = unwrapPointer<T>(obj);
    }
  }

  virtual ~JsValueConverter() {}

  virtual T toC() { 
    return native_object;
  }

protected:
  T native_object;  
};


/**** Specializations *****/

template <>
class JsValueConverter <int> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  int toC() { return jsval->Int32Value(); };
};


template <>
class JsValueConverter <uint32_t> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  int toC() { return jsval->Uint32Value(); };
};


template <>
class JsValueConverter <unsigned long> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  int64_t toC() { return jsval->IntegerValue(); };
};


template <>
class JsValueConverter <double> {
public:
  jsvalue jsval;
  
JsValueConverter(jsvalue v) : jsval(v) {};
  double toC() { return jsval->NumberValue(); };
};


template <>
class JsValueConverter <int64_t> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  int64_t toC() { return jsval->IntegerValue(); };
};


template <>
class JsValueConverter <bool> {
public:
  jsvalue jsval;
  
  JsValueConverter(jsvalue v) : jsval(v) {};
  bool toC()  { return jsval->BooleanValue(); };
};


/* const char * is JavaScript String */
template <>
class JsValueConverter <const char *> {
private:
  v8::String::Utf8Value av;

public: 
  JsValueConverter(jsvalue v) : av(v)   {};
  const char * toC()  { return *av;  };
};


/* char * is Node::Buffer */
template <>
class JsValueConverter <char *> {
public: 
  jsvalue jsval;
  JsValueConverter(jsvalue v) : jsval(v)   {};
  char * toC()  { 
    DEBUG_PRINT_DETAIL("Unwrapping Node buffer");
    return node::Buffer::Data(jsval->ToObject());
  };
};

/* Pass through of JavaScript value */
template <>
class JsValueConverter <Handle<v8::Function> > {
public:
  Local<v8::Function> jsval;
  JsValueConverter(Local<Value> v) {
    jsval = Local<v8::Function>::New(v8::Isolate::GetCurrent(), Local<v8::Function>::Cast(v));
  };
  Handle<v8::Function> toC()  { return jsval;  };
};

/*****************************************************************
 toJs functions
 Value Conversion from C to JavaScript
 
 These are called from the ReturnValueHandler for non-pointer types.
 We do not expect them ever to be called with pointer types.

 The generic function treats its argument as a JS Integer.

 SPECIALIZATIONS should be over C PRIMITIVE types only.

 These functions do not declare a HandleScope
******************************************************************/

template <typename T> Local<Value> toJS(Isolate * isolate, T cval) {
  return v8::Integer::New(isolate, cval);
}

// unsigned int
template <>
inline Local<Value> toJS<unsigned int>(Isolate * isolate, unsigned int cval) {
  return v8::Integer::NewFromUnsigned(isolate, cval);
}

// unsigned long long
template <>
inline Local<Value> toJS<unsigned long long>(Isolate * isolate, unsigned long long cval) {
  double d = static_cast<double>(cval);
  return v8::Number::New(isolate, d);
}

// unsigned short
template <>
inline Local<Value> toJS<unsigned short>(Isolate * isolate, unsigned short cval) {
  return v8::Integer::NewFromUnsigned(isolate, cval);
}

// double
template <>
inline Local<Value> toJS<double>(Isolate * isolate, double cval) {
  return v8::Number::New(isolate, cval);
};

// const char *
template <> 
inline Local<Value> toJS<const char *>(Isolate * isolate, const char * cval) {
  return v8::String::NewFromUtf8(isolate, cval);
}

// const bool * 
template <> 
inline Local<Value> toJS<const bool *>(Isolate * isolate, const bool * cbp) {
  // return Local<Value>::New(Boolean::New(isolate, *cbp));
  return *cbp ? True(isolate) : False(isolate);
}

// bool 
template <>
inline Local<Value> toJS<bool>(Isolate * isolate, bool b) {
  // return Local<Value>::New(Boolean::New(isolate, b));
  return b ? True(isolate) : False(isolate);
}

#endif
