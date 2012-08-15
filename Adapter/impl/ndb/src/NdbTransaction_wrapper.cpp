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

#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"

using namespace v8;

Handle<Value> getTCNodeId_wrapper(const Arguments &args);


class NdbTransactionEnvelopeClass : public Envelope {
public:
  NdbTransactionEnvelopeClass() : Envelope("NdbTransaction") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getConnectedNodeId", getTCNodeId_wrapper);    
  }
};

NdbTransactionEnvelopeClass NdbTransactionEnvelope;


/******* immediate wrapper template
Handle<Value>  _wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH( );

  typedef NativeMethodCall_ _< , NdbTransaction, , > NCALL;

  NCALL ncall(args);
  ncall.method = & NdbTransaction:: ;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}
*******/



//////////// IMMEDIATE METHOD WRAPPERS


/* Uint32 getConnectedNodeId(); 
   Get nodeId of TC for this transaction
   IMMEDIATE
*/
Handle<Value> getTCNodeId_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);

  NativeMethodCall_0_<uint32_t, NdbTransaction> ncall(args);
  ncall.method = & NdbTransaction::getConnectedNodeId;
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


//////////// ASYNC METHOD WRAPPERS

