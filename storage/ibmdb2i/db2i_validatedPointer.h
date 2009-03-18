/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#ifndef DB2I_VALIDATEDPOINTER_H
#define DB2I_VALIDATEDPOINTER_H

#include "db2i_ileBridge.h"

/**
    @class ValidatedPointer
    @brief Encapsulates a pointer registered for usage by the QMYSE APIs
    
    @details As a performance optimization, to prevent pointer validation each
    time a particular pointer is thunked across to ILE, QMYSE allows us to 
    "register" a pointer such that it is validated once and then subsequently 
    referenced on QMYSE APIs by means of a handle value. This class should be
    used to manage memory allocation/registration/unregistration of these
    pointers. Using the alloc function guarantees that the resulting storage is
    16-byte aligned, a requirement for many pointers passed to QMYSE.
*/
template <class T>
class ValidatedPointer
{
public:
  ValidatedPointer<T>() : address(NULL), handle(NULL) {;}
    
  ValidatedPointer<T>(size_t size)
  {
    alloc(size);
  }

  ValidatedPointer<T>(T* ptr)
  {
    assign(ptr);
  }
  
  operator T*()
  {
    return address;
  };

  operator T*() const 
  {
    return address;
  };
  
  operator void*()
  {
    return address;
  };

  operator ILEMemHandle()
  {
    return handle;
  }
  
  void alloc(size_t size)
  {
    address = (T*)malloc_aligned(size);
    if (address)
      db2i_ileBridge::registerPtr(address, &handle);
    mallocedHere = 1;
  }
  
  void assign(T* ptr)
  {
    address = ptr;
    db2i_ileBridge::registerPtr((void*)ptr, &handle);    
    mallocedHere = 0;
  }
  
  void realloc(size_t size)
  {
    dealloc();    
    alloc(size);
  }
  
  void reassign(T* ptr)
  {
    dealloc();    
    assign(ptr);
  }
  
  void dealloc()
  {
    if (address)
    {
      db2i_ileBridge::unregisterPtr(handle);

      if (mallocedHere)
        free_aligned((void*)address);
    }
    address = NULL;
    handle = 0;
  }
  
  ~ValidatedPointer()
  {
    dealloc();
  }
    
private:
  // Disable copy ctor and assignment operator, as these would break
  // the registration guarantees provided by the class.
  ValidatedPointer& operator= (const ValidatedPointer newVal);
  ValidatedPointer(ValidatedPointer& newCopy);

  ILEMemHandle handle;
  T*  address;
  char mallocedHere;
};


/**
    @class ValidatedObject
    @brief This class allows users to instantiate and register a particular 
    object in a single step.
*/
template<class T>
class ValidatedObject : public ValidatedPointer<T>
{
  public:
  ValidatedObject<T>() : ValidatedPointer<T>(&value) {;}
  
  T& operator= (const T newVal) { value = newVal; return value; }
  
  private:
  T value;
};
#endif
