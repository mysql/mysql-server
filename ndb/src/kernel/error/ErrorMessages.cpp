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

#include "ErrorMessages.hpp"

struct ErrStruct {
   int fauldId;
   const char* text;
};

const ErrStruct errArray[] = {

   {2301, "Assertion, probably a programming error"},
   {2302, "Own Node Id not a NDB node, configuration error"},
   {2303, "System error"},
   {2304, "Index too large"},
   {2305, "Arbitrator shutdown"},
   {2306, "Pointer too large"},
   {2307, "Internal program error"},
   {2308, "Node failed during system restart"},
   {2309, "Node state conflict"},
   {2310, "Error while reading the REDO log"},
   {2311, "Conflict when selecting restart type"},
   {2312, "No more free UNDO log"},
   {2313, "Error while reading the datapages and UNDO log"},
   {2327, "Memory allocation failure"},
   {2334, "Job buffer congestion"},
   {2335, "Error in short time queue"},
   {2336, "Error in long time queue"},
   {2337, "Error in time queue, too long delay"},
   {2338, "Time queue index out of range"},
   {2339, "Send signal error"},
   {2340, "Wrong prio level when sending signal"},
   {2341, "Internal program error (failed ndbrequire)"},
   {2342, "Error insert executed" },
   {2350, "Invalid Configuration fetched from Management Server" },

   // Ndbfs error messages
   {2801, "No file system path"},
   {2802, "Channel is full"},
   {2803, "No more threads"},
   {2804, "Bad parameter"},
   {2805, "Illegal file system path"},
   {2806, "Max number of open files exceeded"},
   {2807, "File has already been opened"},

   // Sentinel
   {0, "No message slogan found"}

};

const unsigned short NO_OF_ERROR_MESSAGES = sizeof(errArray)/sizeof(ErrStruct);

const char* lookupErrorMessage(int faultId)
{
  int i = 0; 
  while (errArray[i].fauldId != faultId && errArray[i].fauldId != 0)
    i++;
  return errArray[i].text;
}


