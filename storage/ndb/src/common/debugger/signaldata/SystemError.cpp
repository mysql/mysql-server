/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <kernel_types.h>
#include <BlockNumbers.h>
#include <signaldata/SystemError.hpp>

bool
printSYSTEM_ERROR(FILE * output, const Uint32 * theData, Uint32 len, 
		  Uint16 receiverBlockNo){

  const SystemError * const sig = (SystemError *) theData;

  fprintf(output, "errorRef: H\'%.8x\n", 
	  sig->errorRef);   
  fprintf(output, "errorCode: %d\n", 
	  sig->errorCode);  
  if (len >= 2)
  {
    for (Uint32 i = 0; i<len - 2; i++)
    {
      fprintf(output, "data[%u]: H\'%.8x\n", i, sig->data[i]);
    }
  }
  return true;
}


