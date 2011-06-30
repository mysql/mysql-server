/*
   Copyright (c) 2008 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_TAP_HPP
#define NDB_TAP_HPP

#include <../../../unittest/mytap/tap.h>
#include <../../../unittest/mytap/tap.c>

#define TAPTEST(name)                           \
int name##_test();                              \
int main(int argc, const char** argv){          \
  plan(1);                                      \
  ok(name##_test(), #name);                     \
  return exit_status();                         \
}                                               \
int name##_test()


#endif
