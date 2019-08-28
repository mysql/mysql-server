/*
   Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_TAP_HPP
#define NDB_TAP_HPP

#include <../../../unittest/mytap/tap.h>
#include <../../../unittest/mytap/tap.c>

#ifdef VM_TRACE
#define OK(b) assert(b);
#else
#define OK(b) if (!(b)) abort();
#endif

#define TAPTEST(name)                           \
int name##_test();                              \
int main(int argc, const char** argv){          \
  plan(1);                                      \
  ok(name##_test(), #name);                     \
  return exit_status();                         \
}                                               \
int name##_test()

/* tap.c needs my_print_stacktrace */
#ifdef DONT_DEFINE_VOID
// stacktrace.c turns off VOID redefinition if needed
#undef DONT_DEFINE_VOID
#endif

#ifdef HAVE_GCOV
// __gcov_flush need C linkage
extern "C" void __gcov_flush(void);
#endif

/* stacktrace.c needs min unless MY_MIN is defined */
#if !defined MY_MIN && !defined min
#define min(a, b)    ((a) < (b) ? (a) : (b)) 
#endif

#include <../../../mysys/stacktrace.c>


#endif
