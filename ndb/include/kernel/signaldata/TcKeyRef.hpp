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

#ifndef TCKEYREF_HPP
#define TCKEYREF_HPP

#include "SignalData.hpp"

class TcKeyRef {

  /**
   * Receiver(s)
   */
  friend class NdbOperation;
  friend class Ndbcntr;
  friend class DbUtil;

  /**
   * Sender(s) / Receiver(s)
   */
  friend class Dbtc;

  /**
   * Sender(s)
   */
  friend class Dblqh;
  
  friend bool printTCKEYREF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 connectPtr;
  Uint32 transId[2];
  Uint32 errorCode;
};

#endif
