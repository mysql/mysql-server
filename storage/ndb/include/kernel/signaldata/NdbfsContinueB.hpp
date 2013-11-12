/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBFS_CONTINUEB_H
#define NDBFS_CONTINUEB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 130


class NdbfsContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Ndbfs;
  friend class VoidFs;
  friend bool printCONTINUEB_NDBFS(FILE * output, const Uint32 * theData,
				   Uint32 len, Uint16);
private:
  enum {
    ZSCAN_MEMORYCHANNEL_10MS_DELAY  =  0,
    ZSCAN_MEMORYCHANNEL_NO_DELAY    =  1
  };
};


#undef JAM_FILE_ID

#endif
