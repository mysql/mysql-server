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

typedef v8::Local<v8::Value> jsvalue;
typedef v8::Handle<v8::Object> jsobject;

/* toJS Template: 
  convert a C value to Javascript 
*/
template <typename T>
jsvalue toJS(T cValue) { 
  return v8::Number::New(cValue);
};


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


/* JsValueConverter Template (stub) 
*/
template <typename T> class JsValueConverter {
public:
  JsValueConverter(jsvalue v);
  ~JsValueConverter();
  T toC();
};


/* JsValueConverter Specializations */


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
  double toC() { return jsval->IntegerValue(); };
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


template <typename P> 
class JsValueConverter <P *> {
public:
  Wrapper<P> * wrapper;
  
  JsValueConverter(jsvalue v) {  wrapper = Wrapper<P>::Unwrap(v->ToObject()); };  
  P * toC() { return wrapper->object; };
};
