/* Copyright 2008-2010 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef DBINFO_H
#define DBINFO_H

#include <SimulatedBlock.hpp>

class Dbinfo : public SimulatedBlock
{
public:
  Dbinfo(Block_context& ctx);
  virtual ~Dbinfo();
  BLOCK_DEFINES(Dbinfo);

protected:
  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal*);
  void execDUMP_STATE_ORD(Signal* signal);

  Uint32 find_next_block(Uint32 block) const;
  bool find_next(Ndbinfo::ScanCursor* cursor) const;
  void execDBINFO_SCANREQ(Signal *signal);
  void execDBINFO_SCANCONF(Signal *signal);

  void execINCL_NODEREQ(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
};

#endif
