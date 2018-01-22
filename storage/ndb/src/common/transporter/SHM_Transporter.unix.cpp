/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
  if (!isServer)
  {
    ndbout_c("Trying to create shared memory segment on the client side");
    return false;
  }
  shmId = shmget(shmKey, shmSize, IPC_CREAT | 960);
  if (shmId == -1)
  {
    perror("shmget: ");
    return false;
  }
  _shmSegCreated = true;
  return true;
}

bool
SHM_Transporter::ndb_shm_get()
{
  shmId = shmget(shmKey, shmSize, 0);
  if (shmId == -1)
  {
    perror("shmget: ");
    return false;
  }
  return true;
}

bool
SHM_Transporter::ndb_shm_attach()
{
  shmBuf = (char *)shmat(shmId, 0, 0);
  if (shmBuf == 0)
  {
    perror("shmat: ");
    shmctl(shmId, IPC_RMID, 0);
    _shmSegCreated = false;
    return false;
  }
  return true;
}

void
SHM_Transporter::ndb_shm_destroy()
{
  /**
   * We have attached to the shared memory segment.
   * The shared memory won't be removed until all
   * attached processes have detached. To ensure
   * that we remove the shared memory segment even
   * after a crash we now remove it immediately.
   * Otherwise the shared memory segment will be
   * left after a crash.
   */
  const int res = shmctl(shmId, IPC_RMID, 0);
  if(res == -1)
  {
    perror("shmctl: ");
    return;
  }
  _shmSegCreated = false;
}

bool
SHM_Transporter::checkConnected()
{
  struct shmid_ds info;
  const int res = shmctl(shmId, IPC_STAT, &info);
  if (res == -1)
  {
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
 
  if (info.shm_nattch != 2)
  {
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
SHM_Transporter::disconnectImpl()
{
  setupBuffersUndone();
  if (_attached)
  {
    struct shmid_ds info;
    const int ret_val = shmctl(shmId, IPC_STAT, &info);
    if (ret_val != -1)
    {
      if (info.shm_nattch == 1)
      {
        remove_mutexes();
      }
    }
    const int res = shmdt(shmBuf);
    if(res == -1)
    {
      perror("shmdelete: ");
      return;
    }
    _attached = false;
  }
  disconnect_socket();
  
  if (isServer && _shmSegCreated)
  {
    /**
     * Normally should not happen.
     */
    assert(false);
    const int res = shmctl(shmId, IPC_RMID, 0);
    if(res == -1)
    {
      char buf[64];
      make_error_info(buf, sizeof(buf));
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;
}
