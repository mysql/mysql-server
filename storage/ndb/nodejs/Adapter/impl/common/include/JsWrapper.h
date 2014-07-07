/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

#ifndef NODEJS_ADAPTER_INCLUDE_JSWRAPPER_H
#define NODEJS_ADAPTER_INCLUDE_JSWRAPPER_H

#include <node.h>
#include "adapter_global.h"
#include "unified_debug.h"

using v8::Persistent;
using v8::ObjectTemplate;
using v8::HandleScope;
using v8::Handle;
using v8::Local;
using v8::Object;
using v8::Value;
using v8::Exception;
using v8::String;


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
#define TYPE_CHECK_T(x) const char * x
#define SET_CLASS_ID(env, PTR) env.class_id = typeid(PTR).name()
#define CHECK_CLASS_ID(env, PTR) check_class_id(env->class_id, typeid(PTR).name()) 
#else
#define TYPE_CHECK_T(x)
#define SET_CLASS_ID(env, PTR)
#define CHECK_CLASS_ID(env, PTR)
#endif


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
  TYPE_CHECK_T(class_id);               // for checking type of wrapped object
  const char * classname;               // for debugging output
  Persistent<ObjectTemplate> stencil;   // for creating JavaScript objects
  
  /* Constructor */
  Envelope(const char *name) : 
    magic(0xF00D), 
    classname(name)
  {
    HandleScope scope; 
    Handle<ObjectTemplate> proto = ObjectTemplate::New();
    proto->SetInternalFieldCount(2);
    stencil = Persistent<ObjectTemplate>::New(proto);
  }

  /* Instance Method */
  Local<Object> newWrapper() { 
    return stencil->NewInstance();
  }
};


/*****************************************************************
 Create a weak handle for a wrapped object.
 Use it to delete the wrapped object when the GC wants to reclaim the handle.
 For safety, the compiler will not let you use this on any "const PTR" type;
 (if you hold a const pointer to something, you probably don't own its
 memory allocation).
 If the underlying pointer has already been freed and zeroed, just dispose
 of the JavaScript reference.
******************************************************************/
template<typename PTR> 
void onGcReclaim(Persistent<Value> notifier, void * param) {
  PTR ptr = static_cast<PTR>(param);
  if(ptr) delete ptr;
  notifier.Dispose();
}

template<typename PTR> 
void freeFromGC(PTR ptr, Handle<Object> obj) {
  Persistent<Object> notifier = Persistent<Object>::New(obj);
  notifier.MarkIndependent();
  notifier.MakeWeak((void *) ptr, onGcReclaim<PTR>);
}


/*****************************************************************
 Construct a wrapped object. 
 arg0: pointer to the object to be wrapped.
 arg1: an Envelope reference
 arg2: a reference to a v8 object, which must have already been 
       initialized from a proper ObjectTemplate.
******************************************************************/
template <typename PTR>
void wrapPointerInObject(PTR ptr,
                         Envelope & env,
                         Handle<Object> obj) {
  DEBUG_PRINT("Constructor wrapping %s: %p", env.classname, ptr);
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  SET_CLASS_ID(env, PTR);
  obj->SetPointerInInternalField(0, (void *) & env);
  obj->SetPointerInInternalField(1, (void *) ptr);
  // obj->SetInternalField(0, v8::External::Wrap((void *) & env));
  // obj->SetInternalField(1, v8::External::Wrap((void *) ptr));
}

/* Specializations for non-pointers reduce gcc warnings.
   Only specialize over primitive types. */
template <> inline void wrapPointerInObject(int, Envelope &, Handle<Object>) {
  assert(0);
}
template <> inline void wrapPointerInObject(unsigned long long int, Envelope &, Handle<Object>) {
  assert(0);
}
template <> inline void wrapPointerInObject(unsigned int, Envelope &, Handle<Object>) {
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
PTR unwrapPointer(Handle<Object> obj) {
  PTR ptr;
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  ptr = static_cast<PTR>(obj->GetPointerFromInternalField(1));
#ifdef UNIFIED_DEBUG
  Envelope * env = static_cast<Envelope *>(obj->GetPointerFromInternalField(0));
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
    HandleScope scope;
    return scope.Close(Exception::Error(String::New(message)));
  }
};


#endif

