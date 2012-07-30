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

#include "Wrapper.h"

using namespace v8;
typedef Local<Value> jsvalue;
typedef Handle<Object> jsobject;



/*****************************************************************
 JsMethodThis and JsConstructorThis
 Wrap C++ "this" in a Javascript object
******************************************************************/

/* JsConstructorThis template; use as return value from constructor 
*/
template <typename P>
jsobject JsConstructorThis(const v8::Arguments &args, P * ptr) {
  Wrapper<P> * wrapper = new Wrapper<P>(ptr);
  return wrapper->Wrap(args.This());
}


/* JsMethodThis.
   Use in a method call wrapper to get the instance.
*/
template <typename T> 
T * JsMethodThis(const v8::Arguments &args) {
  Wrapper<T> * wrapper = Wrapper<T>::Unwrap(args.This());
  return wrapper->object;
}


/*****************************************************************
 JsValueConverter 
 Value conversion from JavaScript to C
******************************************************************/
template <typename T> class JsValueConverter {
public:  
  JsValueConverter(jsvalue v) {  
    assert(v->IsObject());
    Local<Object> obj = v->ToObject();
    assert(obj->InternalFieldCount() > 0);
    native_object = static_cast<T>(obj->GetPointerFromInternalField(0));
  }
  /* If you get an "invalid static cast from type void *" above, then the compiler
     is erroneously falling back on this implementation. */  
  virtual T toC() { 
    return native_object;
  }

protected:
  T native_object;  
};

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
  int toC() { return jsval->IntegerValue(); };
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


template <>
class JsValueConverter <const char *> {
private:
  v8::String::AsciiValue av;

public: 
  JsValueConverter(jsvalue v) : av(v)   {};
  const char * toC()  { return *av;  };
};




/*****************************************************************
 toJs functions
 Value Conversion from C to JavaScript
******************************************************************/

/* If you get an "invalid static cast from type void *" below, 
   then the compiler is erroneously falling back on this implementation. 
*/  
template <typename T> Local<Value> toJS(T cptr) {
  Local<Object> obj = Object::New();
  assert(obj->InternalFieldCount() > 0);
  obj->SetPointerInInternalField(0, static_cast<void *>(cptr));
  return obj;
}


/*****************************************************************
 toJs specializations
 Value Conversion from C to JavaScript
******************************************************************/


// int
template <>
inline Local<Value> toJS<int>(int cval){ 
  return Number::New(cval);
};

// uint32_t
template <>
inline Local<Value> toJS<uint32_t>(uint32_t cval) {
  return v8::Uint32::New(cval);
};

// double
template <>
inline Local<Value> toJS<double>(double cval) {
  return Number::New(cval);
};

// const char *
template <> 
inline Local<Value> toJS<const char *>(const char * cval) {
  return v8::String::New(cval);
}

