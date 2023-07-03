/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
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

#ifndef NODEJS_ADAPTER_INCLUDE_JSWRAPPER_H
#define NODEJS_ADAPTER_INCLUDE_JSWRAPPER_H

#include <node.h>
#include "unified_debug.h"

using v8::Isolate;
using v8::Persistent;
using v8::Eternal;
using v8::ObjectTemplate;
using v8::HandleScope;
using v8::EscapableHandleScope;
using v8::Local;
using v8::Value;
using v8::Object;
using v8::Array;
using v8::Exception;
using v8::String;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::PropertyCallbackInfo;
using v8::TryCatch;
using v8::Context;
using v8::WeakCallbackInfo;

/* Signature of a V8 function wrapper
*/
typedef FunctionCallbackInfo<Value> Arguments;
typedef void V8WrapperFn(const Arguments &);

/* Signatures for property setters & getters
*/
typedef PropertyCallbackInfo<Value> AccessorInfo;  // for getter
typedef PropertyCallbackInfo<void>  SetterInfo;
typedef void (*Getter) (Local<String>, const AccessorInfo &);
typedef void (*Setter) (Local<String>, Local<Value>, const SetterInfo &);

/*****************************************************************
 Code to confirm that C++ types wrapped as JavaScript values
 are unwrapped back to the original type. This can be disabled.
 ENABLE_WRAPPER_TYPE_CHECKS is defined in adapter_global.h
 ******************************************************************/

#if ENABLE_WRAPPER_TYPE_CHECKS
#include <typeinfo>
inline void check_class_id(const char *a, const char *b) {
  if(a != b) {
    fprintf(stderr, " !!! Expected %s but unwrapped %s !!!\n", b, a);
    assert(a == b);
  }
}
#define TYPE_CHECK_T(x) const char * x;
#define SET_CLASS_ID(env, PTR) env.class_id = typeid(PTR).name()
#define SET_THIS_CLASS_ID(PTR) class_id = typeid(PTR).name()
#define CHECK_CLASS_ID(env, PTR) check_class_id(env->class_id, typeid(PTR).name())
#else
#define TYPE_CHECK_T(x)  /* TYPE_CHECK_T */
#define SET_CLASS_ID(env, PTR) /* SET_CLASS_ID */
#define SET_THIS_CLASS_ID(PTR) /* SET_THIS_CLASS_ID */
#define CHECK_CLASS_ID(env, PTR) /* CHECK_CLASS_ID */
#endif

/*  Delete a native C++ object when the Garbage Collector reclaims its
    JavaScript handle.
    The GcReclaimer holds the pointer to be deleted, its classname for
    debugging output, and the weak JS reference (which should be Reset).
*/
template<typename CPP_OBJECT> class GcReclaimer;  // forward declaration

template<typename CPP_OBJECT>
void onGcReclaim(const WeakCallbackInfo< GcReclaimer<CPP_OBJECT> > & data) {
  GcReclaimer<CPP_OBJECT> * reclaimer = data.GetParameter();
  reclaimer->reclaim();
  delete reclaimer;
}

template<typename CPP_OBJECT> class GcReclaimer {
public:
  GcReclaimer(const char * cls, CPP_OBJECT * p) : classname(cls), ptr(p)   { }

  void SetWeakReference(Isolate *isolate, Local<Value> obj) {
    notifier.Reset(isolate, obj);
    notifier.SetWeak(this, onGcReclaim<CPP_OBJECT>,
                     v8::WeakCallbackType::kParameter);
  }

  void reclaim() {
    delete ptr;
    DEBUG_PRINT_DETAIL("GC Reclaim %s %p", classname, ptr);
    notifier.Reset();
  }

private:
  const char * classname;
  CPP_OBJECT * ptr;
  Persistent<Value> notifier;
};


/*****************************************************************
 An Envelope is a simple structure providing some safety 
 & convenience for wrapped classes.

 All objects are wrapped using two internal fields.
 The first points to the envelope; the second to the object itself.
 ******************************************************************/
class Envelope {
public:
  /* Instance variables */
  int magic;                            // for safety when unwrapping 
  TYPE_CHECK_T(class_id)                // for checking type of wrapped object
  const char * classname;               // for debugging output
  Eternal<ObjectTemplate> stencil;      // for creating JavaScript objects
  Isolate * isolate;
  bool isVO;

  /* Constructor */
  Envelope(const char *name) :
    magic(0xF00D), 
    classname(name),
    isolate(Isolate::GetCurrent()),
    isVO(false)
  {
    EscapableHandleScope scope(isolate);
    Local<ObjectTemplate> proto = ObjectTemplate::New(isolate);
    proto->SetInternalFieldCount(2);
    stencil.Set(isolate, proto);
  }

  /* Instance Methods */
  Local<Object> newWrapper() { 
    auto obj = stencil.Get(isolate)->NewInstance(isolate->GetCurrentContext());
    return obj.ToLocalChecked();
  }

  void addMethod(const char *name, V8WrapperFn wrapper) {
    stencil.Get(isolate)->Set(
      String::NewFromUtf8(isolate, name, v8::NewStringType::kInternalized).ToLocalChecked(),
      FunctionTemplate::New(isolate, wrapper)
    );
  }

  template<typename T>
  Local<Value> wrap(const T* ptr) { return wrap(const_cast<T*>(ptr)); }

  template<typename PTR>
  Local<Value> wrap(PTR ptr) {
    if(ptr) {
      DEBUG_PRINT("Envelope wrapping %s: %p", classname, ptr);
      SET_THIS_CLASS_ID(PTR);
      Local<Object> wrapper = newWrapper();
      wrapper->SetAlignedPointerInInternalField(0, (void *) this);
      wrapper->SetAlignedPointerInInternalField(1, (void *) ptr);
      return wrapper;
    }

    /* But if ptr was null, return a JavaScript null: */
    return Null(isolate);
  }

  /* An overloaded wrap() method for the special case of converting a
     const char * to a JS String.
  */
  Local<Value> wrap(const char * str) {
    return String::NewFromUtf8(isolate, str, v8::NewStringType::kNormal).ToLocalChecked();
  }

  void addAccessor(const char *name, Getter accessor) {
    stencil.Get(isolate)->SetNativeDataProperty(
      String::NewFromUtf8(isolate, name, v8::NewStringType::kInternalized).ToLocalChecked(),
      accessor
    );
  }

  void addAccessor(Local<String> name, Getter getter,
                   Setter setter = 0,
                   Local<Value> data = Local<Value>()) {
    stencil.Get(isolate)->SetNativeDataProperty(name, getter, setter, data,
                                                v8::DontDelete,
                                                Local<v8::AccessorSignature>(),
                                                v8::DEFAULT);
  }


  /*****************************************************************
   Create a weak handle for a wrapped object.
   Use it to delete the wrapped object when the GC wants to reclaim the handle.

   Don't do this if ptr is null (and wrapper object is therefore JS Null).

   For safety, the compiler will not let you use this on any "const PTR" type;
   (if you hold a const pointer to something, you probably don't own its
   memory allocation).
   ******************************************************************/
  template<typename P>
  void freeFromGC(P * ptr, Local<Value> obj) {
    if(ptr) {
      GcReclaimer<P> * reclaimer = new GcReclaimer<P>(classname, ptr);
      reclaimer->SetWeakReference(isolate, obj);
    }
  }
};

/*****************************************************************
 Construct a wrapped object. 
 arg0: pointer to the object to be wrapped.
 arg1: an Envelope reference
 arg2: a reference to a v8 object, which must have already been 
       initialized from a proper ObjectTemplate.
 The *usual* case is to use Envelope.wrap() instead of this.
******************************************************************/
template <typename PTR>
void wrapPointerInObject(PTR ptr,
                         Envelope & env,
                         Local<Object> obj) {
  DEBUG_PRINT("wrapPointerInObject for %s: %p", env.classname, ptr);
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  SET_CLASS_ID(env, PTR);
  obj->SetAlignedPointerInInternalField(0, (void *) & env);
  obj->SetAlignedPointerInInternalField(1, (void *) ptr);
}

/* Specialization for const pointers; cast away const before wrapping
*/
template <typename T>
void wrapPointerInObject(const T* ptr, Envelope &env, Local<Object> obj) {
  wrapPointerInObject(const_cast<T*>(ptr), env, obj);
}

/* Specializations for non-pointers reduce gcc warnings.
   Only specialize over primitive types. */
template <> inline void wrapPointerInObject(int, Envelope &, Local<Object>) {
  assert(0);
}
template <> inline void wrapPointerInObject(unsigned long long int, Envelope &,
  Local<Object>) {
    assert(0);
  }
template <> inline void wrapPointerInObject(unsigned int, Envelope &, Local<Object>) {
  assert(0);
}
template <> inline void wrapPointerInObject(double, Envelope &, Local<Object>) {
  assert(0);
}

/*****************************************************************
 Unwrap a native pointer from a JavaScript object
 arg0: a reference to a v8 object, which must have already been 
       initialized from a proper ObjectTemplate.
TODO: Find a way to prevent wrapping a pointer as one
      type and unwrapping it as another.
******************************************************************/
template <typename PTR> 
PTR unwrapPointer(Local<Object> obj) {
  PTR ptr;
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  ptr = static_cast<PTR>(obj->GetAlignedPointerFromInternalField(1));
#ifdef UNIFIED_DEBUG
  Envelope * env = static_cast<Envelope *>(obj->GetAlignedPointerFromInternalField(0));
  assert(env->magic == 0xF00D);
  CHECK_CLASS_ID(env, PTR);
  DEBUG_PRINT_DETAIL("Unwrapping %s: %p", env->classname, ptr);
#endif
  return ptr;
}


/*****************************************************************
 Capture an error message from a C++ routine 
 Provide a method to run later (in the v8 main JavaScript thread) 
 and generate a JavaScript Error object from the message
******************************************************************/
class NativeCodeError {
public:
  const char * message;

  NativeCodeError(const char * msg) : message(msg) {}
  virtual ~NativeCodeError() {}
  
  virtual Local<Value> toJS() {
    EscapableHandleScope scope(Isolate::GetCurrent());
    return scope.Escape(Exception::Error(
      String::NewFromUtf8(Isolate::GetCurrent(),
                          message,
                          v8::NewStringType::kNormal).ToLocalChecked()));
  }
};


#endif

