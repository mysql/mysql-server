/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
/*
 * helpers.hpp
 */

#ifndef helpers_hpp
#define helpers_hpp

//#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// using namespace std;
using std::cout;
using std::endl;
using std::flush;

/************************************************************
 * Helper Macros & Functions
 ************************************************************/

// JNI crashes with gcc & operator<<(ostream &, long/int)
// so, we use C99's __func__ and also convert to string using sprintf()
#define ABORT_ERROR(message)                                                  \
  do {                                                                        \
    char l[1024];                                                             \
    sprintf(l, "%d", __LINE__);                                               \
    cout << "!!! error, file: " << (__FILE__) << ", function: " << (__func__) \
         << ", line: " << l << ", msg: " << message << "." << endl;           \
    exit(-1);                                                                 \
  } while (0)

// an output stream for debug messages
#if DEBUG
#define CDBG \
  if (0)     \
    ;        \
  else       \
    cout
#else
#define CDBG \
  if (1)     \
    ;        \
  else       \
    cout
#endif

// some macros for tracing
#define ENTER(name) CDBG << "--> " << name << endl;

#define LEAVE(name) CDBG << "<-- " << name << endl;

#define TRACE(name) Tracer _tracer_(name);

// use as:
// myfunction() {
//   TRACE("myfunction()");
//   ...
// }
class Tracer {
  const char *const name;

 public:
  Tracer(const char *name) : name(name) { ENTER(name); }

  ~Tracer() { LEAVE(name); }
};

#endif  // helpers_hpp
