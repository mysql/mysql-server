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

#include "NdbWrappers.h"

using v8::Arguments;

class NdbNativeCodeError : public NativeCodeError {
public:
  const NdbError & ndberr;
  NdbNativeCodeError(const NdbError &err) : NativeCodeError(0), ndberr(err)  {}
    
  Local<Value> toJS() {
    HandleScope scope;
    Local<String> JSMsg = String::New(ndberr.message);
    Local<Object> Obj = Exception::Error(JSMsg)->ToObject();
    Obj->Set(String::NewSymbol("ndb_error"), NdbError_Wrapper(ndberr));

    return scope.Close(Obj);
  }
};


template<typename R, typename C> 
NativeCodeError * getNdbErrorIfNull(R return_val, C * ndbapiobject) {
  NativeCodeError *err = 0;
  
  if(return_val == 0) {
    err = new NdbNativeCodeError(ndbapiobject->getNdbError());
  }
  
  return err;
};


template<typename R, typename C> 
NativeCodeError * getNdbErrorIfLessThanZero(R return_val, C * ndbapiobject) {
  NativeCodeError *err = 0;
  
  if(return_val < 0) {
    err = new NdbNativeCodeError(ndbapiobject->getNdbError());
  }
  
  return err;
};


template<typename R, typename C> 
NativeCodeError * getNdbErrorAlways(R return_val, C * ndbApiObject) {

  return new NdbNativeCodeError(ndbApiObject->getNdbError());
};


template<typename C> 
Handle<Value> getNdbError(const Arguments &args) {
  C * ndbApiObject = unwrapPointer<C *>(args.Holder());
  const NdbError & ndberr = ndbApiObject->getNdbError();
  return NdbError_Wrapper(ndberr);
};

