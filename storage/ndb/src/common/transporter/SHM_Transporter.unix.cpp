/*
   Copyright (C) 2003-2006 MySQL AB
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


#include <ndb_global.h>

#include "SHM_Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <TransporterCallback.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>

#include <sys/ipc.h>
#include <sys/shm.h>

void SHM_Transporter::make_error_info(char info[], int sz)
{
  snprintf(info,sz,"Shm key=%d sz=%d id=%d",
	   shmKey, shmSize, shmId);
}

bool
SHM_Transporter::ndb_shm_create()
{
  shmId = shmget(shmKey, shmSize, IPC_CREAT | 960);
  if(shmId == -1) {
    perror("shmget: ");
    return false;
  }
  return true;
}

bool
SHM_Transporter::ndb_shm_get()
{
  shmId = shmget(shmKey, shmSize, 0);
  if(shmId == -1) {
    perror("shmget: ");
    return false;
  }
  return true;
}

bool
SHM_Transporter::ndb_shm_attach()
{
  shmBuf = (char *)shmat(shmId, 0, 0);
  if(shmBuf == 0) {
    perror("shmat: ");
    return false;
  }
  return true;
}

bool
SHM_Transporter::checkConnected(){
  struct shmid_ds info;
  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    char buf[128];
    int r= snprintf(buf, sizeof(buf),
		    "shmctl(%d, IPC_STAT) errno: %d(%s). ", shmId,
		    errno, strerror(errno));
    make_error_info(buf+r, sizeof(buf)-r);
    DBUG_PRINT("error",("%s", buf));
    switch (errno)
    {
    case EACCES:
      report_error(TE_SHM_IPC_PERMANENT, buf);
      break;
    default:
      report_error(TE_SHM_IPC_STAT, buf);
      break;
    }
    return false;
  }
 
  if(info.shm_nattch != 2){
    char buf[128];
    make_error_info(buf, sizeof(buf));
    report_error(TE_SHM_DISCONNECT);
    DBUG_PRINT("error", ("Already connected to node %d",
                remoteNodeId));
    return false;
  }
  return true;
}

void
SHM_Transporter::disconnectImpl(){
  if(_attached){
    const int res = shmdt(shmBuf);
    if(res == -1){
      perror("shmdelete: ");
      return;   
    }
    _attached = false;
    if(!isServer && _shmSegCreated)
      _shmSegCreated = false;
  }
  
  if(isServer && _shmSegCreated){
    const int res = shmctl(shmId, IPC_RMID, 0);
    if(res == -1){
      char buf[64];
      make_error_info(buf, sizeof(buf));
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;
}
