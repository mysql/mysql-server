/* Copyright (C) 2000-2003 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  These functions are shared with ndb_restore so that the creating of
  tables through ndb_restore is syncronized correctly with the mysqld's

  The lock/unlock functions use the BACKUP_SEQUENCE row in SYSTAB_0
*/
static NdbTransaction *
ndbcluster_global_schema_lock_ext(Ndb *ndb, NdbError &ndb_error)
{
  ndb->setDatabaseName("sys");
  ndb->setDatabaseSchemaName("def");
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, "SYSTAB_0");
  const NdbDictionary::Table *ndbtab;
  NdbOperation *op;
  NdbTransaction *trans= NULL;
  int retries= 100;
  int retry_sleep= 50; /* 50 milliseconds, transaction */
  if (!(ndbtab= ndbtab_g.get_table()))
  {
    ndb_error= dict->getNdbError();
    goto error_handler;
  }
  while (1)
  {
    trans= ndb->startTransaction();
    if (trans == NULL)
    {
      ndb_error= ndb->getNdbError();
      goto error_handler;
    }

    op= trans->getNdbOperation(ndbtab);
    op->readTuple(NdbOperation::LM_Exclusive);
    op->equal("SYSKEY_0", NDB_BACKUP_SEQUENCE);

    if (trans->execute(NdbTransaction::NoCommit) == 0)
      break;

    if (trans->getNdbError().status == NdbError::TemporaryError)
    {
      if (retries--)
      {
        ndb->closeTransaction(trans);
        trans= NULL;
        do_retry_sleep(retry_sleep);
        continue; // retry
      }
    }
    ndb_error= trans->getNdbError();
    goto error_handler;
  }
  return trans;

 error_handler:
  if (trans)
  {
    ndb->closeTransaction(trans);
  }
  return NULL;
}

static int
ndbcluster_global_schema_unlock_ext(Ndb *ndb, NdbTransaction *trans,
                                    NdbError &ndb_error)
{
  if (trans->execute(NdbTransaction::Commit))
  {
    ndb_error= trans->getNdbError();
    ndb->closeTransaction(trans);
    return -1;
  }
  ndb->closeTransaction(trans);
  return 0;
}
