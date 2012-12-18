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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <v8.h>

#include "js_wrapper_macros.h"
#include "adapter_global.h"

/* Undefine UNIFIED_DEBUG here so macros are not expanded 
   and uni_debug is not declared as an extern
*/
int uni_debug        = 0;
int udeb_level       = 0;
int udeb_initialized = 0;

#undef UNIFIED_DEBUG
#include "unified_debug.h"

using namespace v8;

Persistent<Function> JSLoggerFunction;


/////  Internal Utility Functions

inline const char * udeb_basename(const char *path) {
  const char * last_sep = 0;
  if(path) {  
    const char * s = path;
    last_sep = s;
  
    for(; *s ; s++) 
      if(*s == '/') 
         last_sep = s;
    if(last_sep > path && last_sep < s) // point just past the separator
      last_sep += 1;
  }
  return last_sep;
}


// Bernstein hash
inline int udeb_hash(const char *name) {
  const unsigned char *p;
  unsigned int h = 5381;
  
  for (p = (const unsigned char *) name ; *p != '\0' ; p++) 
      h = ((h << 5) + h) + *p;

  h = h % UDEB_SOURCE_FILE_BITMASK_BITS;

  return h;
}


////// The Logging API for C:  udeb_print() and udeb_enter() 

void udeb_enter(int level, const char *src_path, const char *fn, int ln) {
  udeb_print(src_path, level, "Enter: %27s - line %d", fn, ln);
}


#define SEND_MESSAGES_TO_JAVASCRIPT 0

/* udeb_print() is used by macros in the public API 
*/ 
void udeb_print(const char *src_path, int level, const char *fmt, ...) {
  int sz = 0;
  char message[UDEB_MSG_BUF];
  va_list args;
  va_start(args, fmt);
  
  const char * src_file = udeb_basename(src_path);

  /* Construct the message */
  sz += snprintf(message, UDEB_MSG_BUF, "%s ", src_file);
  sz += vsnprintf(message + sz, UDEB_MSG_BUF - sz, fmt, args);

  if(udeb_initialized && udeb_level >= level) {
#if SEND_MESSAGES_TO_JAVASCRIPT
    HandleScope scope;
    Handle<Value> jsArgs[3];
    jsArgs[0] = Number::New(level);
    jsArgs[1] = String::New(src_file);
    jsArgs[2] = String::New(message, sz);
    JSLoggerFunction->Call(Context::GetCurrent()->Global(), 3, jsArgs);
#else
    sprintf(message + sz, "\n");
    fputs(message, stderr);
    va_end(args);
    return;
#endif
  }

  va_end(args);
}


/************************* The JavaScript API ***********************
 * setLevel():   JS tells C the global state and level.
 * setLogger():  JS introduces itself to C and provides a logging function
 ***************/
/* libc's basename(3) is not thread-safe, so we implement a version here.
   This one is essentially a strlen() function that also remembers the final
   path separator
*/

Handle<Value> udeb_setLogger(const Arguments &args) {
  HandleScope scope;
  
  Local<Function> f = Function::Cast(* (args[0]));
  JSLoggerFunction = Persistent<Function>::New(f);
  udeb_initialized = 1;

  Handle<Value> argv[3];
  argv[0] = Number::New(UDEB_INFO);
  argv[1] = String::New("unified_debug.cpp");
  argv[2] = String::New("unified_debug.cpp C++ unified_debug enabled");

  JSLoggerFunction->Call(Context::GetCurrent()->Global(), 3, argv);
  return scope.Close(True());
}


Handle<Value> udeb_setLevel(const Arguments &args) {
  HandleScope scope;
  
  udeb_level = args[0]->Int32Value();
  // C code cannot log below UDEB_INFO
  uni_debug = (udeb_level > UDEB_NOTICE) ? 1 : 0;  
  
  // leave uni_debug off until stack corruption in udeb_print() is fixed
  //uni_debug = 0;

  
  return scope.Close(True());
}


void udebug_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "setLogger", udeb_setLogger);
  DEFINE_JS_FUNCTION(target, "setLevel" , udeb_setLevel );
}

