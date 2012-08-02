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


#pragma once;
#include "unified_debug.h"
#include "v8.h"
using namespace v8;


/*****************************************************************
 An Envelope is a simple structure providing some safety 
 & convenience for wrapped classes.
 All objects are wrapped using two internal fields.
 The first points to the envelope; the second to the object itself.
******************************************************************/
class Envelope {
public:
  int magic;                       // for safety when unwrapping 
  const char * classname;          // for debugging output
  Handle<ObjectTemplate> stencil;  // for instance construction when needed
  
  Envelope(const char *name) : magic(0xF00D), classname(name), stencil() {}
  void buildStencil() {
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
  Envelope * env = static_cast<Envelope *>(obj->GetPointerFromInternalField(0));
  ptr = static_cast<PTR>(obj->GetPointerFromInternalField(1));
  DEBUG_PRINT("Unwrapping %s: %p", env->classname, ptr);
  return ptr;
}

