/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
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

#ifndef SR_FRAGIDCONF_HPP
#define SR_FRAGIDCONF_HPP

#include "SignalData.hpp"

class SrFragidConf {
  /**
   * Sender(s)
   */
  friend class Dbacc;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
public:
  STATIC_CONST( SignalLength = 10 );

private:
  Uint32 lcpPtr;
  Uint32 accPtr;
  Uint32 noLocFrag;
  Uint32 fragId[4];
  Uint32 fragPtr[2];
  Uint32 hashCheckBit;
};
#endif
