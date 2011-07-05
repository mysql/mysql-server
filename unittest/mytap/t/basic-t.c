/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_config.h"

#include <stdlib.h>
#include "../tap.h"

int main() {
  plan(5);
  ok(1 == 1, "testing basic functions");
  ok(2 == 2, " ");
  ok1(3 == 3);
  if (1 == 1)
    skip(2, "Sensa fragoli");
  else {
    ok(1 == 2, "Should not be run at all");
    ok(1, "This one neither");
  }
  return exit_status();
}
