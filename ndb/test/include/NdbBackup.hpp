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

#ifndef NDBT_BACKUP_HPP
#define NDBT_BACKUP_HPP

#include <mgmapi.h>
#include <Vector.hpp>
#include "NdbConfig.hpp"
#include <NdbRestarter.hpp>

class NdbBackup : public NdbConfig {
public:
  NdbBackup(int _own_id, const char* _addr = 0) 
    : NdbConfig(_own_id, _addr) {};

  int start(unsigned & _backup_id);
  int restore(unsigned _backup_id);

  int NFMaster(NdbRestarter& _restarter);
  int NFMasterAsSlave(NdbRestarter& _restarter);
  int NFSlave(NdbRestarter& _restarter);
  int NF(NdbRestarter& _restarter, int *NFDuringBackup_codes, const int sz, bool onMaster);

  int FailMaster(NdbRestarter& _restarter);
  int FailMasterAsSlave(NdbRestarter& _restarter);
  int FailSlave(NdbRestarter& _restarter);
  int Fail(NdbRestarter& _restarter, int *Fail_codes, const int sz, bool onMaster);

private:

  int execRestore(bool _restore_data,
		  bool _restore_meta,
		  int _node_id,
		  unsigned _backup_id);

  const char * getBackupDataDirForNode(int _node_id);
  
};

#endif
