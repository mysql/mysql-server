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

#include <assert.h>
#include <string.h>

#include <NdbApi.hpp>

#include "ndb_util/CharsetMap.hpp"

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "JsConverter.h"

using namespace v8;

/* JavaScript API for recode():

  RecodeIn recodes from a String *in* to a Buffer.
  RecodeOut recodes *out* of a Buffer into a String.

  RecodeIn:
    arg0:  source string
    arg1:  destination charset number
    arg2:  destination buffer
    arg3:  offset in buffer
    arg4:  max writable length in buffer
    Return value: an object:
       { status    : RecodeStatusCode,
         lengthIn  : number of bytes consumed from source 
         lengthOut : number of bytes written to destination
       }
*/

Handle<Value> CharsetMap_recode_in(const Arguments &args) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  
  REQUIRE_ARGS_LENGTH(5);

  JsValueConverter<CharsetMap *> converter(args.Holder());
  CharsetMap * csmap = converter.toC();
  
  int32_t lengths[2]; 
  enum { SOURCE = 0 , DEST = 1 };  // for lengths[]
  int status = CharsetMap::RECODE_OK;
  int copyLen;
  
  Local<Object> NodeBuffer = args[2]->ToObject();
  Local<String> sourceStr  = args[0]->ToString();
  int32_t cs_to            = args[1]->Int32Value();
  char * buffer            = node::Buffer::Data(NodeBuffer);
  uint32_t offset          = args[3]->Uint32Value();
  lengths[DEST]            = args[4]->Int32Value();

  /* Source string length and charset */
  int32_t cs_from          = csmap->getUTF16CharsetNumber();
  lengths[SOURCE]          = sourceStr->Length();

  /* Special case: if the destination is 2-byte unicode, just copy directly. 
     sourceStr->Write(uint16_t * ...) might face alignment issues, so we use
     memcpy().
  */
  if(cs_to == cs_from) {
    if(lengths[DEST] >= lengths[SOURCE]) {
      copyLen = lengths[SOURCE];
    }
    else {
      copyLen = lengths[DEST];
      status = csmap->RECODE_BUFF_TOO_SMALL;
    }
    DEBUG_PRINT("recodeIn() optimized path UTF16 -> UTF16 using memcpy");
    String::Value source(sourceStr);
    memcpy(buffer + offset, *source, copyLen);
    lengths[DEST] = copyLen;
  }
  
  /* Special case: if the destination is UTF-8, let V8 do the copying. 
     copyLen will receive the length written in characters.
     The return value is the length written in bytes.
  */
  else if(cs_to == csmap->getUTF8CharsetNumber()) {
    lengths[DEST] = sourceStr->WriteUtf8(buffer + offset, lengths[DEST], 
                                         &copyLen, 1);
    if(copyLen < sourceStr->Length()) {
      status = CharsetMap::RECODE_BUFF_TOO_SMALL;
    }                                         
    DEBUG_PRINT("recodeIn() UTF16 -> UTF8 using v8 WriteUtf8(): %s",
                buffer + offset);
  }
  
  /* General case: use CharsetMap::recode() */
  else {
    String::Value source(sourceStr);
    status = csmap->recode(lengths, cs_from, cs_to, *source, buffer + offset);
    DEBUG_PRINT("recodeIn() UTF16 -> X using recode(%c%c%c%c...): %s", 
                (*source)[1], (*source)[3], (*source)[5], (*source)[7],
                buffer + offset);
  }

  /* Build the return value */
  Local<Object> returnVal = Object::New();
  returnVal->Set(String::NewSymbol("status"), Integer::New(status));
  returnVal->Set(String::NewSymbol("lengthIn"), Integer::New(lengths[SOURCE]));
  returnVal->Set(String::NewSymbol("lengthOut"), Integer::New(lengths[DEST]));
  returnVal->Set(String::NewSymbol("charset"), args[1]);
  returnVal->Set(String::NewSymbol("offset"), Integer::New(offset));
  
  return scope.Close(returnVal);
}


/*
  RecodeOut:
    arg0:  source buffer
    arg1:  offset in buffer
    arg2:  length of string data in buffer (in bytes)
    arg3:  source charset number
    arg4:  (OUT) an object where obj->status will be set to RecodeStatusCode
    Return value: a JavaScript string
*/

Handle<Value> CharsetMap_recode_out(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(5);
 
  JsValueConverter<CharsetMap *> converter(args.Holder());
  CharsetMap * csmap = converter.toC();

  int32_t lengths[2]; 
  enum { SOURCE = 0 , DEST = 1 };  // for lengths[]
  int status = CharsetMap::RECODE_OK;

  Local<Value> result = String::Empty();
  
  Local<Object> NodeBuffer = args[0]->ToObject();
  char * buffer            = node::Buffer::Data(NodeBuffer);
  uint32_t offset          = args[1]->Uint32Value();
  lengths[SOURCE]          = args[2]->Int32Value();
  int32_t cs_from          = args[3]->Int32Value();
  Local<Object> statusObj  = args[4]->ToObject();
  
  /* Destination charset */
  int32_t cs_to            = csmap->getUTF16CharsetNumber();

  /* Special case: source string is UTF-8 */
  if(cs_from == csmap->getUTF8CharsetNumber()) {
    result = String::New(buffer + offset, lengths[SOURCE]);
  }

  /* General case */
  else {
    lengths[DEST] = lengths[SOURCE] * 2;  
    char * target = new char[lengths[DEST]];
    status = csmap->recode(lengths, cs_from, cs_to, buffer + offset, target);
    if(status == CharsetMap::RECODE_OK) {
      result = String::New(target, lengths[DEST]);
    }
	delete[] target;
  }

  // statusObj->Set(String::NewSymbol("status"), Integer::New(status));
  return scope.Close(result);
}


/* NOTES
   -----
   
    - In recode_out, if the source is 2-byte Unicode, we might like to copy 
   directly, but alignment issues prevent us from casting the buffer to 
   a uint16_t *.  Since we cannot avoid an intermediate copy, we let csmap 
   do the work. 

    - There are multiple UTF-8 Charsets in MySQL (utf8mb3 and utf8mb4), 
   and this only recognizes one of them.  CharsetMap could be extended with 
   a function "bool isUTF8(cs_num)".  

    - There is a subtle difference between UTF-16 and UCS-2 which may be
   important, and, as with UTF-8 above, if both of these are present in the 
   mysql build, only one of them is recognized as UTF-16. 

    - More efficient handling of ASCII would be possible if CharsetMap 
   gave us "bool isASCII(cs_num)."
*/

