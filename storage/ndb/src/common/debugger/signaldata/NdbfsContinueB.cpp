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


#include <signaldata/NdbfsContinueB.hpp>

bool
printCONTINUEB_NDBFS(FILE * output, const Uint32 * theData,
		     Uint32 len, Uint16 not_used){

  (void)not_used;

  switch (theData[0]) {
  case NdbfsContinueB::ZSCAN_MEMORYCHANNEL_10MS_DELAY:
    fprintf(output, " Scanning the memory channel every 10ms\n");
    return true;
    break;
  case NdbfsContinueB::ZSCAN_MEMORYCHANNEL_NO_DELAY:
    fprintf(output, " Scanning the memory channel again with no delay\n");
    return true;
    break;
  default:
    fprintf(output, " Default system error lab...\n");
    return false;
    break;
  }//switch
  return false;
}
