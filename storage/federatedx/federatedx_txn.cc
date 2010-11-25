/* 
Copyright (c) 2007, Antony T Curtis
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Neither the name of FederatedX nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*#define MYSQL_SERVER 1*/
#include "mysql_priv.h"
#include <mysql/plugin.h>

#include "ha_federatedx.h"

#include "m_string.h"

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation                          // gcc: Class implementation
#endif


federatedx_txn::federatedx_txn()
  : txn_list(0), savepoint_level(0), savepoint_stmt(0), savepoint_next(0)
{
  DBUG_ENTER("federatedx_txn::federatedx_txn");
  DBUG_VOID_RETURN;
}

federatedx_txn::~federatedx_txn()
{
  DBUG_ENTER("federatedx_txn::~federatedx_txn");
  DBUG_ASSERT(!txn_list);
  DBUG_VOID_RETURN;
}


void federatedx_txn::close(FEDERATEDX_SERVER *server)
{
  uint count= 0;
  federatedx_io *io, **iop;
  DBUG_ENTER("federatedx_txn::close");
  
  DBUG_ASSERT(!server->use_count);
  DBUG_PRINT("info",("use count: %u  connections: %u", 
                     server->use_count, server->io_count));

  for (iop= &txn_list; (io= *iop);)
  {
    if (io->server != server)
      iop= &io->txn_next;
    else
    {
      *iop= io->txn_next;
      io->txn_next= NULL;
      io->busy= FALSE;

      io->idle_next= server->idle_list;
      server->idle_list= io;
    }
  }

  while ((io= server->idle_list))
  {
    server->idle_list= io->idle_next;
    delete io;
    count++;
  }
  
  DBUG_PRINT("info",("closed %u connections,  txn_list: %s", count,
                     txn_list ? "active":  "empty"));
  DBUG_VOID_RETURN;
}


int federatedx_txn::acquire(FEDERATEDX_SHARE *share, bool readonly,
                            federatedx_io **ioptr)
{
  federatedx_io *io;
  FEDERATEDX_SERVER *server= share->s;
  DBUG_ENTER("federatedx_txn::acquire");
  DBUG_ASSERT(ioptr && server);

  if (!(io= *ioptr))
  {
    /* check to see if we have an available IO connection */
    for (io= txn_list; io; io= io->txn_next)
      if (io->server == server)
	break;

    if (!io)
    {
      /* check to see if there are any unowned IO connections */
      pthread_mutex_lock(&server->mutex);
      if ((io= server->idle_list))
      {
	server->idle_list= io->idle_next;
	io->idle_next= NULL;
      }
      else
	io= federatedx_io::construct(&server->mem_root, server);

      io->txn_next= txn_list;
      txn_list= io;

      pthread_mutex_unlock(&server->mutex);
    }

    if (io->busy)
      *io->owner_ptr= NULL;
    
    io->busy= TRUE;
    io->owner_ptr= ioptr;
  }
  
  DBUG_ASSERT(io->busy && io->server == server);
  
  io->readonly&= readonly;

  DBUG_RETURN((*ioptr= io) ? 0 : -1);
}


void federatedx_txn::release(federatedx_io **ioptr)
{
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::release");
  DBUG_ASSERT(ioptr);

  if ((io= *ioptr))
  {
    /* mark as available for reuse in this transaction */
    io->busy= FALSE;
    *ioptr= NULL;
  
    DBUG_PRINT("info", ("active: %d autocommit: %d", 
                	io->active, io->is_autocommit()));

    if (io->is_autocommit())
      io->active= FALSE;
  }

  release_scan();

  DBUG_VOID_RETURN;
}


void federatedx_txn::release_scan()
{
  uint count= 0, returned= 0;
  federatedx_io *io, **pio;
  DBUG_ENTER("federatedx_txn::release_scan");

  /* return any inactive and idle connections to the server */  
  for (pio= &txn_list; (io= *pio); count++)
  {
    if (io->active || io->busy)
      pio= &io->txn_next;
    else
    {
      FEDERATEDX_SERVER *server= io->server;

      /* unlink from list of connections bound to the transaction */
      *pio= io->txn_next; 
      io->txn_next= NULL;

      /* reset some values */
      io->readonly= TRUE;

      pthread_mutex_lock(&server->mutex);
      io->idle_next= server->idle_list;
      server->idle_list= io;
      pthread_mutex_unlock(&server->mutex);
      returned++;
    }
  }
  DBUG_PRINT("info",("returned %u of %u connections(s)", returned, count));

  DBUG_VOID_RETURN;
}


bool federatedx_txn::txn_begin()
{
  ulong level= 0;
  DBUG_ENTER("federatedx_txn::txn_begin");

  if (savepoint_next == 0)
  {
    savepoint_next++;
    savepoint_level= savepoint_stmt= 0;
    sp_acquire(&level);
  }

  DBUG_RETURN(level == 1);
}


int federatedx_txn::txn_commit()
{
  int error= 0;
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::txn_commit");

  if (savepoint_next)
  {
    DBUG_ASSERT(savepoint_stmt != 1);

    for (io= txn_list; io; io= io->txn_next)
    {
      int rc= 0;

      if (io->active)
	rc= io->commit();
      else
	io->rollback();

      if (io->active && rc)
	error= -1;

      io->reset();
    }

    release_scan();

    savepoint_next= savepoint_stmt= savepoint_level= 0;
  }
    
  DBUG_RETURN(error);
}


int federatedx_txn::txn_rollback()
{
  int error= 0;
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::txn_commit");

  if (savepoint_next)
  {
    DBUG_ASSERT(savepoint_stmt != 1);

    for (io= txn_list; io; io= io->txn_next)
    {
      int rc= io->rollback();

      if (io->active && rc)
	error= -1;

      io->reset();
    }

    release_scan();

    savepoint_next= savepoint_stmt= savepoint_level= 0;
  }
    
  DBUG_RETURN(error);
}


bool federatedx_txn::sp_acquire(ulong *sp)
{
  bool rc= FALSE;
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::sp_acquire");
  DBUG_ASSERT(sp && savepoint_next);
  
  *sp= savepoint_level= savepoint_next++;
    
  for (io= txn_list; io; io= io->txn_next)
  {
    if (io->readonly)
      continue;

    io->savepoint_set(savepoint_level);
    rc= TRUE;
  }

  DBUG_RETURN(rc);
}


int federatedx_txn::sp_rollback(ulong *sp)
{
  ulong level, new_level= savepoint_level;
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::sp_rollback");
  DBUG_ASSERT(sp && savepoint_next && *sp && *sp <= savepoint_level);
  
  for (io= txn_list; io; io= io->txn_next)
  {
    if (io->readonly)
      continue;

    if ((level= io->savepoint_rollback(*sp)) < new_level)
      new_level= level;
  } 
  
  savepoint_level= new_level;
  
  DBUG_RETURN(0);
}


int federatedx_txn::sp_release(ulong *sp)
{
  ulong level, new_level= savepoint_level;
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::sp_release");
  DBUG_ASSERT(sp && savepoint_next && *sp && *sp <= savepoint_level);
  
  for (io= txn_list; io; io= io->txn_next)
  {
    if (io->readonly)
      continue;

    if ((level= io->savepoint_release(*sp)) < new_level)
      new_level= level;
  }

  savepoint_level= new_level;
  *sp= 0;

  DBUG_RETURN(0);
}


bool federatedx_txn::stmt_begin()
{
  bool result= FALSE;
  DBUG_ENTER("federatedx_txn::stmt_begin");

  if (!savepoint_stmt)
  {
    if (!savepoint_next)
    {
      savepoint_next++;
      savepoint_level= savepoint_stmt= 0;
    }
    result= sp_acquire(&savepoint_stmt);
  }

  DBUG_RETURN(result);
}


int federatedx_txn::stmt_commit()
{ 
  int result= 0;
  DBUG_ENTER("federatedx_txn::stmt_commit");
  
  if (savepoint_stmt == 1)
  {
    savepoint_stmt= 0;
    result= txn_commit();
  }
  else  
  if (savepoint_stmt)
    result= sp_release(&savepoint_stmt);

  DBUG_RETURN(result);
}


int federatedx_txn::stmt_rollback()
{
  int result= 0;
  DBUG_ENTER("federated:txn::stmt_rollback");

  if (savepoint_stmt == 1)
  {
    savepoint_stmt= 0;
    result= txn_rollback();
  }
  else
  if (savepoint_stmt)
  {
    result= sp_rollback(&savepoint_stmt);
    sp_release(&savepoint_stmt);
  }
  
  DBUG_RETURN(result);
}


void federatedx_txn::stmt_autocommit()
{
  federatedx_io *io;
  DBUG_ENTER("federatedx_txn::stmt_autocommit");

  for (io= txn_list; savepoint_stmt && io; io= io->txn_next)
  {
    if (io->readonly)
      continue;

    io->savepoint_restrict(savepoint_stmt);
  }

  DBUG_VOID_RETURN;  
}


