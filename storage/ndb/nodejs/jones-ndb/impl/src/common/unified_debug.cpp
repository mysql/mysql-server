/*
 Copyright (c) 2012, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <node.h>

#include "JsValueAccess.h"
#include "JsWrapper.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

/* Undefine UNIFIED_DEBUG here so macros are not expanded
   and uni_debug is not declared as an extern
*/
int uni_debug = 0;
int udeb_level = 0;
int udeb_initialized = 0;
int udeb_per_file = 0;

#undef UNIFIED_DEBUG
#include "unified_debug.h"

Persistent<Object> JSLoggerFunction;

/* Initialized to all zeros? */
unsigned char bit_index[UDEB_SOURCE_FILE_BITMASK_BYTES];

/////  Internal Utility Functions

/* libc's basename(3) is not thread-safe, so we implement a version here.
   This one is essentially a strlen() function that also remembers the final
   path separator
*/
inline const char *udeb_basename(const char *path) {
  const char *last_sep = 0;
  if (path) {
    const char *s = path;
    last_sep = s;

    for (; *s; s++)
      if (*s == '/') last_sep = s;
    if (last_sep > path && last_sep < s)  // point just past the separator
      last_sep += 1;
  }
  return last_sep;
}

// Bernstein hash
inline unsigned short udeb_hash(const char *name) {
  const unsigned char *p;
  unsigned int h = 5381;

  for (p = (const unsigned char *)name; *p != '\0'; p++)
    h = ((h << 5) + h) + *p;

  h = h % UDEB_SOURCE_FILE_BITMASK_BITS;

  return (unsigned short)h;
}

inline int index_read(unsigned short bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char mask = static_cast<unsigned char>(1U << (bit_number % 8));
  return bit_index[byte] & mask;
}

inline void index_set(unsigned short bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char mask = static_cast<unsigned char>(1U << (bit_number % 8));
  bit_index[byte] |= mask;
}

inline void index_clear(unsigned short bit_number) {
  unsigned short byte = bit_number / 8;
  unsigned char mask = static_cast<unsigned char>(1 << (bit_number % 8)) ^ 0xFF;
  bit_index[byte] &= mask;
}

inline int log_level(const char *path) {
  return index_read(udeb_hash(path)) ? UDEB_DETAIL : udeb_level;
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

  const char *src_file = udeb_basename(src_path);

  /* Construct the message */
  va_list args;
  va_start(args, fmt);
  sz += snprintf(message, UDEB_MSG_BUF, "%s ", src_file);
  sz += vsnprintf(message + sz, UDEB_MSG_BUF - sz, fmt, args);
  va_end(args);

  if (udeb_initialized && log_level(src_file) >= level) {
#if SEND_MESSAGES_TO_JAVASCRIPT
//    HandleScope scope;
//    Handle<Value> jsArgs[3];
//    jsArgs[0] = Number::New(level);
//    jsArgs[1] = String::New(src_file);
//    jsArgs[2] = String::New(message, sz);
//    JSLoggerFunction->Call(Context::GetCurrent()->Global(), 3, jsArgs);
#else
    sprintf(message + sz, "\n");
    fputs(message, stderr);
#endif
  }
  return;
}

/************************* The JavaScript API ***********************
 * setLevel():   JS tells C the global state and level.
 * setLogger():  JS introduces itself to C and provides a logging function
 ***************/

void udeb_setLogger(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());

  if (!udeb_initialized) {
    Isolate *iso = args.GetIsolate();
    Local<Context> ctx = iso->GetCurrentContext();
    JSLoggerFunction.Reset(iso, args[0]->ToObject(ctx).ToLocalChecked());
    // JSLoggerFunction.Reset(Function::Cast(* (args[0])));
    // Local<Function> f = Function::Cast(* (args[0]));
    // JSLoggerFunction = Persistent<Function>::New(f);
    udeb_initialized = 1;

    udeb_print("unified_debug.cpp", UDEB_DEBUG,
               "unified_debug.cpp C++ unified_debug enabled");
  }
  args.GetReturnValue().Set(true);
}

void udeb_setLevel(const Arguments &args) {
  udeb_level = GetInt32Arg(args, 0);
  // C code cannot log below UDEB_INFO
  uni_debug = (udeb_per_file || (udeb_level > UDEB_NOTICE)) ? 1 : 0;

  // leave uni_debug off until stack corruption in udeb_print() is fixed
  // uni_debug = 0;

  args.GetReturnValue().Set(true);
}

void udeb_setFileLevel(const Arguments &args) {
  unsigned char filename[250];
  Isolate *iso = args.GetIsolate();
  Local<Context> ctx = iso->GetCurrentContext();
  args[0]->ToString(ctx).ToLocalChecked()->WriteOneByte(iso, filename, 0, 250);
  index_set(udeb_hash(udeb_basename((const char *)filename)));
  uni_debug = udeb_per_file = 1;

  args.GetReturnValue().Set(true);
}

void udebug_initOnLoad(Local<Object> target) {
  DEFINE_JS_FUNCTION(target, "setLogger", udeb_setLogger);
  DEFINE_JS_FUNCTION(target, "setLevel", udeb_setLevel);
  DEFINE_JS_FUNCTION(target, "setFileLevel", udeb_setFileLevel);
}
