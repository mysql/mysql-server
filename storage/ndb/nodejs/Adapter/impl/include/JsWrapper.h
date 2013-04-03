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


#pragma once
#include <node.h>
#include "unified_debug.h"
using namespace v8;


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
  const char * classname;               // for debugging output
  Persistent<ObjectTemplate> stencil;   // for creating JavaScript objects
  
  /* Constructor */
  Envelope(const char *name) : magic(0xF00D), classname(name) {
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
 Construct a wrapped object. 
 arg1: pointer to the object to be wrapped.
 arg2: an Envelope reference
 arg3: a reference to a v8 object, which must have already been 
       initialized from a proper ObjectTemplate.
******************************************************************/
template <typename PTR>
void wrapPointerInObject(PTR ptr,
                         Envelope & env,
                         Local<Object> obj) {
  DEBUG_PRINT("Constructor wrapping %s: %p", env.classname, ptr);
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  obj->SetInternalField(0, External::Wrap((void *) & env));
  obj->SetInternalField(1, External::Wrap((void *) ptr));
}


/*****************************************************************
 Unwrap a native pointer from a JavaScript object
 arg1: pointer to the object to be wrapped.
 arg2: an Envelope reference
 arg3: a reference to a v8 object, which must have already been 
       initialized from a proper ObjectTemplate.
******************************************************************/
template <typename PTR> 
PTR unwrapPointer(Local<Object> obj) {
  PTR ptr;
  DEBUG_ASSERT(obj->InternalFieldCount() == 2);
  ptr = static_cast<PTR>(obj->GetPointerFromInternalField(1));
#ifdef UNIFIED_DEBUG
  Envelope * env = static_cast<Envelope *>(obj->GetPointerFromInternalField(0));
  assert(env->magic == 0xF00D);
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

