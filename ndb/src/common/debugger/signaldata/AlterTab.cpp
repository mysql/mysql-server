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

#include <signaldata/AlterTab.hpp>

bool printALTER_TAB_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
//  const AlterTabReq * const sig = (AlterTabReq *) theData;
  
  return false;
}

bool printALTER_TAB_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
//  const AlterTabConf * const sig = (AlterTabConf *) theData;
  
  return false;
}

bool printALTER_TAB_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
//  const AlterTabRef * const sig = (AlterTabRef *) theData;
    
  return false;
}
