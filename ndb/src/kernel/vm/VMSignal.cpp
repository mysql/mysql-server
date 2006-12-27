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

#include "VMSignal.hpp"
#include <string.h>

Signal::Signal(){
  memset(&header, 0, sizeof(header));
  memset(theData, 0, sizeof(theData));
}

void
Signal::garbage_register()
{
  int i;
  theData[0] = 0x13579135;
  header.theLength = 0x13579135;
  header.theSendersBlockRef = 0x13579135;
  for (i = 1; i < 24; i++)
    theData[i] = 0x13579135;
}
