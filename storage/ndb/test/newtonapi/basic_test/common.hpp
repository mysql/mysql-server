/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef COMMON_H
#define COMMON_H

#include <ndb_global.h>

extern "C" {
#include <dba.h>
}

#include <NdbOut.hpp>

typedef struct Employee {
  UInt32_t   EmpNo;
  char       FirstName[24];
  char       LastName[24];
  
  struct Address * EmployeeAddress;
} Employee_t;

typedef struct Address {
  UInt32_t   EmpNo;
  char       StreetName[24];
  UInt32_t   StreetNo;
  char       City[12];
} Address_t;

/**
 * Employee functions
 */
NdbOut & operator << (NdbOut & out, const Employee_t & emp);
bool operator==(const Employee_t & e1, const Employee_t & e2);
void Alter(Employee_t & emp);
void CompareRows(Employee_t * data1, int rows, Employee_t * data2);
void AlterRows(Employee_t * data1, int rows);

/**
 * Address functions
 */
NdbOut & operator << (NdbOut & out, const Address_t & adr);
bool operator==(const Address_t & a1, const Address_t & a2);
void Alter(Address_t & emp);
void CompareRows(Address_t * data1, int rows, Address_t * data2);
void AlterRows(Address_t * data1, int rows);

inline void require(bool test){
  if(!test)
    abort();
}

#endif
