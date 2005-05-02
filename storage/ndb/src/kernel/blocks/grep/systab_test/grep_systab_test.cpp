/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
 *  Unit Test for GrepSystemTable
 */

#include "../GrepSystemTable.hpp"
#include <SimulatedBlock.hpp>

#define EXEC(X)  ( ndbout << endl, ndbout_c(#X), X )

int
main () {
  GrepSystemTable st;

  Uint32 f, l;

  ndbout_c("*************************************");
  ndbout_c("* GrepSystemTable Unit Test Program *");
  ndbout_c("*************************************");

  ndbout_c("--------------------------------------------------------");
  ndbout_c("Test 1: Clear");
  ndbout_c("--------------------------------------------------------");

  EXEC(st.set(GrepSystemTable::PS, 22, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);

  EXEC(st.clear(GrepSystemTable::PS, 20, 24));
  st.print();
  st.require(GrepSystemTable::PS, 25, 26);

  EXEC(st.clear(GrepSystemTable::PS, 0, 100));
  st.print();
  st.require(GrepSystemTable::PS, 1, 0);

  EXEC(st.set(GrepSystemTable::PS, 22, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);

  EXEC(st.clear(GrepSystemTable::PS, 24, 28));
  st.print();
  st.require(GrepSystemTable::PS, 22, 23);

  EXEC(st.clear(GrepSystemTable::PS, 0, 100));
  st.print();
  st.require(GrepSystemTable::PS, 1, 0);

  EXEC(st.set(GrepSystemTable::PS, 22, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);
  
  EXEC(st.clear(GrepSystemTable::PS, 24, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 23);

  EXEC(st.clear(GrepSystemTable::PS, 0, 100));
  st.print();
  st.require(GrepSystemTable::PS, 1, 0);

  EXEC(st.set(GrepSystemTable::PS, 22, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);

  EXEC(st.clear(GrepSystemTable::PS, 22, 24));
  st.print();
  st.require(GrepSystemTable::PS, 25, 26);

  ndbout_c("--------------------------------------------------------");
  ndbout_c("Test 2: PS --> SSreq");
  ndbout_c("--------------------------------------------------------");
  
  EXEC(st.set(GrepSystemTable::PS, 22, 26));
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);
  st.require(GrepSystemTable::SSReq, 1, 0);

  if (!EXEC(st.copy(GrepSystemTable::PS, GrepSystemTable::SSReq, 3, &f, &l)))
    ndbout_c("%s:%d: Illegal copy!", __FILE__, __FILE__);
  ndbout_c("f=%d, l=%d", f, l); 
  st.print();
  st.require(GrepSystemTable::PS, 22, 26);
  st.require(GrepSystemTable::SSReq, 22, 24);

  EXEC(st.clear(GrepSystemTable::PS, 22, 22));
  st.print();
  st.require(GrepSystemTable::PS, 23, 26);
  st.require(GrepSystemTable::SSReq, 22, 24);

  if (!EXEC(st.copy(GrepSystemTable::PS, GrepSystemTable::SSReq, 2, &f, &l)))
    ndbout_c("%s:%d: Illegal copy!", __FILE__, __LINE__);
  ndbout_c("f=%d, l=%d", f, l); 
  st.print();
  st.require(GrepSystemTable::PS, 23, 26);
  st.require(GrepSystemTable::SSReq, 22, 26);

  st.set(GrepSystemTable::SS, 7, 9);  
  st.set(GrepSystemTable::InsReq, 7, 9);  
  if (EXEC(st.movable(GrepSystemTable::SS, GrepSystemTable::InsReq))) 
    ndbout_c("%s:%d: Illegal move!", __FILE__, __LINE__);
  st.print();
  st.require(GrepSystemTable::SS, 7, 9);
  st.require(GrepSystemTable::InsReq, 7, 9);

  EXEC(st.intervalMinus(7, 9, 7, 7, &f, &l)); 
  ndbout_c("f=%d, l=%d", f, l); 

  st.clear(GrepSystemTable::InsReq, 8, 9);  
  st.require(GrepSystemTable::SS, 7, 9);
  st.require(GrepSystemTable::InsReq, 7, 7);
  if (EXEC(st.movable(GrepSystemTable::SS, GrepSystemTable::InsReq)) != 2)
    ndbout_c("%s:%d: Illegal move!", __FILE__, __LINE__);
  st.print();

  EXEC(st.copy(GrepSystemTable::SS, GrepSystemTable::InsReq, &f));
  st.print();
  st.require(GrepSystemTable::SS, 7, 9);
  st.require(GrepSystemTable::InsReq, 7, 8);

  ndbout_c("--------------------------------------------------------");
  ndbout_c("Test completed");
  ndbout_c("--------------------------------------------------------");
}
