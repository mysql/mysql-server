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


#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "JsWrapper.h"
#include "NdbWrapperErrors.h"
#include "NdbWrappers.h"

using namespace v8;

Handle<Value> startTransaction(const Arguments &);

class NdbEnvelopeClass : public Envelope {
public:
  NdbEnvelopeClass() : Envelope("Ndb") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "startTransaction", startTransaction);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", getNdbError<Ndb>);
  }
  
  Local<Object> wrap(Ndb *ndb) {
    HandleScope scope;    
    Local<Object> wrapper = Envelope::stencil->NewInstance();
    wrapPointerInObject(ndb, *this, wrapper);
    return scope.Close(wrapper);
  }
};

NdbEnvelopeClass NdbEnvelope;


Handle<Value> Ndb_Wrapper(Ndb *ndb) {
  return NdbEnvelope.wrap(ndb);
}


Handle<Value> startTransaction(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(4);
  
  typedef NativeMethodCall_3_<NdbTransaction *, Ndb, 
                              const NdbDictionary::Table *, 
                              const char *, uint32_t> MCALL;

  MCALL * mcallptr = new MCALL(& Ndb::startTransaction, args);
  mcallptr->envelope = & NdbEnvelope;
  mcallptr->errorHandler = getNdbErrorIfNull<NdbTransaction *, Ndb>;
  mcallptr->runAsync();
  
  return scope.Close(JS_VOID_RETURN);
}

