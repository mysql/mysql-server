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


#include "common.hpp"

NdbOut &
operator << (NdbOut & out, const Employee_t & emp){
  out << emp.EmpNo << " \"" << emp.FirstName << "\" \"" 
      << emp.LastName << "\"";
  return out;
}

bool
operator==(const Employee_t & e1, const Employee_t & e2){
  if(e1.EmpNo != e2.EmpNo)
    return false;
  if(strcmp(e1.FirstName, e2.FirstName) != 0)
    return false;
  return strcmp(e1.LastName, e2.LastName) == 0;
}

void
Alter(Employee_t & emp){
  static int updown = 0;
  if(updown == 0){
    for(int i = 0; i<strlen(emp.FirstName); i++)
      toupper(emp.FirstName[i]);
    
    for(int i = 0; i<strlen(emp.LastName); i++)
      toupper(emp.LastName[i]);
  } else {
    for(int i = 0; i<strlen(emp.FirstName); i++)
      tolower(emp.FirstName[i]);
    
    for(int i = 0; i<strlen(emp.LastName); i++)
      tolower(emp.LastName[i]);
  }
  updown = 1 - updown;
}

void
CompareRows(Employee_t * data1,
	    int rows,  
	    Employee_t * data2){
  for(int i = 0; i<rows; i++){
    if(!(data1[i] == data2[i])){
      ndbout << data1[i] << endl
	     << data2[i] << endl;
    }
  }
}

void
AlterRows(Employee_t * data1, int rows){
  for(int i = 0; i<rows; i++){
    Alter(data1[i]);
  }
}

inline
NdbOut &
operator << (NdbOut & out, const Address_t & adr){
  out << adr.EmpNo << " \"" << adr.StreetName << "\" " 
      << adr.StreetNo << " \"" << adr.City << "\"";
  return out;
}

inline
bool
operator==(const Address_t & a1, const Address_t & a2){
  if(a1.EmpNo != a2.EmpNo)
    return false;
  if(a1.StreetNo != a2.StreetNo)
    return false;
  if(strcmp(a1.StreetName, a2.StreetName) != 0)
    return false;
  return strcmp(a1.City, a2.City) == 0;
}

inline
void
Alter(Address_t & emp){
  static int updown = 0;
  if(updown == 0){
    for(int i = 0; i<strlen(emp.StreetName); i++)
      toupper(emp.StreetName[i]);
    
    for(int i = 0; i<strlen(emp.City); i++)
      toupper(emp.City[i]);
  } else {
    for(int i = 0; i<strlen(emp.StreetName); i++)
      tolower(emp.StreetName[i]);
    
    for(int i = 0; i<strlen(emp.City); i++)
      tolower(emp.City[i]);
  }
  emp.StreetNo *= emp.EmpNo;
  updown = 1 - updown;
}

void
CompareRows(Address_t * data1,
	    int rows,  
	    Address_t * data2){
  for(int i = 0; i<rows; i++){
    if(!(data1[i] == data2[i])){
      ndbout << data1[i] << endl
	     << data2[i] << endl;
    }
  }
}

void
AlterRows(Address_t * data1, int rows){
  for(int i = 0; i<rows; i++){
    Alter(data1[i]);
  }
}

