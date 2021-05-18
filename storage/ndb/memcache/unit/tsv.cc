/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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
#include "my_config.h"
#include <stdio.h>
#include <assert.h>

#include "TabSeparatedValues.h"

#include "all_tests.h"


int run_tsv_test(QueryPlan *, Ndb *, int v) {
  {
    TabSeparatedValues t1("frodo.xxx", 4, 5);
    require(t1.getLength() == 5);
    require(t1.advance() == 0);
    detail(v, "tsv test 1 OK\n");
  }
  
  {
    char sam[16];

    const char *v2 = "sam\tjessie";   // null-terminated
    TabSeparatedValues t2(v2, 4, strlen(v2));
    strncpy(sam, t2.getPointer(), t2.getLength());
    require(! strcmp(sam, "sam"));
    require(t2.getLength() == 3);
    
    require(  t2.advance() == 1);

    require(* t2.getPointer() == 'j');
    require(t2.getLength() == 6);
    detail(v, "tsv test 2 OK\n");
  }
  
  {
    char jes[16];
    const char *v3 = "sam\tjessie......";  // no null terminator
    TabSeparatedValues t3(v3, 4, 10);

    require(t3.advance() == 1);
    require(t3.getLength() == 6);
    strncpy(jes, t3.getPointer(), t3.getLength());
    require(strncmp(jes,"jessie", t3.getLength()) == 0);
    detail(v, "tsv test 3 OK\n");
  }

  {
    const char *v4 = "\tabc";  // 2 values
    TabSeparatedValues t4(v4, 4, strlen(v4));
    
    /* First value is null */
    require(t4.getLength() == 0);
    
    /* Second value */
    require(t4.advance() == 1);
    require(* t4.getPointer() == 'a');
    require(  t4.getLength() == 3);
    
    /* No more */
    require(t4.advance() == 0);
    detail(v, "tsv test 4 OK\n");
  }
  
  {
    const char *v5 = "\t\tabc"; // 3 values
    TabSeparatedValues t5(v5, 4, strlen(v5));

    /* First value is null */
    require(t5.getLength() == 0);
    
    /* Second value */
    require(t5.advance() == 1);
    require(t5.getLength() == 0);

    /* Third value */
    require(t5.advance() == 1);
    require(* t5.getPointer() == 'a');
    require(  t5.getLength() == 3);
    
    /* No more */
    require(t5.advance() == 0);
    detail(v, "tsv test 5 OK\n");
  }

  {
    const char *v6 = "\t\tabc\t\t"; // 5 values with null terminator
    TabSeparatedValues t6(v6, 6, strlen(v6));
    
    /* First value is null */
    require(t6.getLength() == 0);
    
    /* Second value is null */
    require(t6.advance() == 1);
    require(t6.getLength() == 0);
    
    /* Third value is abc */
    require(t6.advance() == 1);
    require(  t6.getLength() == 3);

    /* 4th value is null */
    require(t6.advance() == 1);
    require(t6.getLength() == 0);

    /* 5th value is null */
    require(t6.advance() == 1);
    require(t6.getLength() == 0);
    
    /* No more */
    require(t6.advance() == 0);
    detail(v, "tsv test 6 OK\n");
  }
  
  {
    const char *v7 = "\t\tabc\t__"; // 4 values, no null
    TabSeparatedValues t7(v7, 4, strlen(v7) - 2);
    
    /* First value is null */
    require(t7.getLength() == 0);
    
    /* Second value is null */
    require(t7.advance() == 1);
    require(t7.getLength() == 0);
    
    /* Third value is abc */
    require(t7.advance() == 1);
    require(t7.getLength() == 3);
    
    /* 4th value is null */
    require(t7.advance() == 1);
    require(t7.getLength() == 0);
    
    /* No more */
    require(t7.advance() == 0);
    detail(v, "tsv test 7 OK\n");
  }
  
  
  pass;
}

