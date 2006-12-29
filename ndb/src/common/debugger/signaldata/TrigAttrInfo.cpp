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

#include <signaldata/TrigAttrInfo.hpp>

static
const char *
tatype(Uint32 i){
  switch(i){
  case TrigAttrInfo::PRIMARY_KEY:
    return "PK";
    break;
  case TrigAttrInfo::BEFORE_VALUES:
    return "BEFORE";
    break;
  case TrigAttrInfo::AFTER_VALUES:
    return "AFTER";
    break;
  }
  return "UNKNOWN";
}

bool
printTRIG_ATTRINFO(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo)
{
  const TrigAttrInfo * const sig = (TrigAttrInfo *) theData;
  
  fprintf(output, " TriggerId: %d Type: %s ConnectPtr: %x\n",
	  sig->getTriggerId(),
	  tatype(sig->getAttrInfoType()),
	  sig->getConnectionPtr());
  
  Uint32 i = 0;
  while (i < len - TrigAttrInfo::StaticLength)
    fprintf(output, " H\'%.8x", sig->getData()[i++]);
  fprintf(output,"\n");
  
  return true;
}
