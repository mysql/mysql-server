/* Copyright (C) 2008 MySQL AB
   Use is subject to license terms

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

#ifndef NDB_BACKUP_PROXY_HPP
#define NDB_BACKUP_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/UtilSequence.hpp>

class BackupProxy : public LocalProxy {
public:
  BackupProxy(Block_context& ctx);
  virtual ~BackupProxy();
  BLOCK_DEFINES(BackupProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_STTOR
  virtual void callSTTOR(Signal*);
  void sendUTIL_SEQUENCE_REQ(Signal*);
  void execUTIL_SEQUENCE_CONF(Signal*);
  void execUTIL_SEQUENCE_REF(Signal*);
};

#endif
