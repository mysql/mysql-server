/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
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
#include <assert.h>
#include <string.h>


#include "all_tests.h"

#include "LookupTable.h"

struct blah {
  int i;
  char c[20];
};

typedef struct blah blah;

int run_lookup_test(QueryPlan *, Ndb *, int v) {
  
  LookupTable<blah> btab;
  
  blah * b = new blah;
  b->i = 110;
  strcpy(b->c, "newsboy!");
  
  const char * my_name = "feederica";
  
  btab.insert(my_name, b);
  
  blah *c = btab.find(my_name);
  
  require(c->i == 110);
  detail(v, "c->i: %d", c->i);
  
  blah *d = btab.find("guacamole");
  require(d == 0);
  
  
  pass;
}

