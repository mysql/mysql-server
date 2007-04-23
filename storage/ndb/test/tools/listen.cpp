/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>
#include <HugoTransactions.hpp>
#include <getarg.h>


#define BATCH_SIZE 128
struct Table_info
{
  Uint32 id;
};

struct Trans_arg
{
  Ndb *ndb;
  NdbTransaction *trans;
  Uint32 bytes_batched;
};

Vector< Vector<NdbRecAttr*> > event_values;
Vector< Vector<NdbRecAttr*> > event_pre_values;
Vector<struct Table_info> table_infos;

static void do_begin(Ndb *ndb, struct Trans_arg &trans_arg)
{
  trans_arg.ndb =  ndb;
  trans_arg.trans = ndb->startTransaction();
  trans_arg.bytes_batched = 0;
}

static void do_equal(NdbOperation *op,
                     NdbEventOperation *pOp)
{
  struct Table_info *ti = (struct Table_info *)pOp->getCustomData();
  Vector<NdbRecAttr*> &ev = event_values[ti->id];
  const NdbDictionary::Table *tab= pOp->getTable();
  unsigned i, n_columns = tab->getNoOfColumns();
  for (i= 0; i < n_columns; i++)
  {
    if (tab->getColumn(i)->getPrimaryKey() &&
        op->equal(i, ev[i]->aRef()))
    {
      abort();
    }
  }
}

static void do_set_value(NdbOperation *op,
                         NdbEventOperation *pOp)
{
  struct Table_info *ti = (struct Table_info *)pOp->getCustomData();
  Vector<NdbRecAttr*> &ev = event_values[ti->id];
  const NdbDictionary::Table *tab= pOp->getTable();
  unsigned i, n_columns = tab->getNoOfColumns();
  for (i= 0; i < n_columns; i++)
  {
    if (!tab->getColumn(i)->getPrimaryKey() &&
        op->setValue(i, ev[i]->aRef()))
    {
      abort();
    }
  }
}

static void do_insert(struct Trans_arg &trans_arg, NdbEventOperation *pOp)
{
  if (!trans_arg.trans)
    return;

  NdbOperation *op =
    trans_arg.trans->getNdbOperation(pOp->getEvent()->getTableName());
  op->writeTuple();

  do_equal(op, pOp);
  do_set_value(op, pOp);

  trans_arg.bytes_batched++;
  if (trans_arg.bytes_batched > BATCH_SIZE)
  {
    trans_arg.trans->execute(NdbTransaction::NoCommit);
    trans_arg.bytes_batched = 0; 
  }
}
static void do_update(struct Trans_arg &trans_arg, NdbEventOperation *pOp)
{
  if (!trans_arg.trans)
    return;

  NdbOperation *op =
    trans_arg.trans->getNdbOperation(pOp->getEvent()->getTableName());
  op->writeTuple();

  do_equal(op, pOp);
  do_set_value(op, pOp);

  trans_arg.bytes_batched++;
  if (trans_arg.bytes_batched > BATCH_SIZE)
  {
    trans_arg.trans->execute(NdbTransaction::NoCommit);
    trans_arg.bytes_batched = 0; 
  }
}
static void do_delete(struct Trans_arg &trans_arg, NdbEventOperation *pOp)
{
  if (!trans_arg.trans)
    return;

  NdbOperation *op =
    trans_arg.trans->getNdbOperation(pOp->getEvent()->getTableName());
  op->deleteTuple();

  do_equal(op, pOp);

  trans_arg.bytes_batched++;
  if (trans_arg.bytes_batched > BATCH_SIZE)
  {
    trans_arg.trans->execute(NdbTransaction::NoCommit);
    trans_arg.bytes_batched = 0; 
  }
}
static void do_commit(struct Trans_arg &trans_arg)
{
  if (!trans_arg.trans)
    return;
  trans_arg.trans->execute(NdbTransaction::Commit);
  trans_arg.ndb->closeTransaction(trans_arg.trans);
}

int 
main(int argc, const char** argv){
  ndb_init();

  
  int _help = 0;
  const char* db = 0;
  const char* connectstring1 = 0;
  const char* connectstring2 = 0;

  struct getargs args[] = {
    { "connectstring1", 'c',
      arg_string, &connectstring1, "connectstring1", "" },
    { "connectstring2", 'C',
      arg_string, &connectstring2, "connectstring2", "" },
    { "database", 'd', arg_string, &db, "Database", "" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0, i;
  char desc[] = 
    "<tabname>+ \nThis program listen to events on specified tables\n";
  
  if(getarg(args, num_args, argc, argv, &optind) ||
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  // Connect to Ndb
  Ndb_cluster_connection con(connectstring1);
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb( &con, db ? db : "TEST_DB" );

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;

  Ndb_cluster_connection *con2 = NULL;
  Ndb *ndb2 =  NULL;
  if (connectstring2)
  {
    con2 = new Ndb_cluster_connection(connectstring2);

    if(con2->connect(12, 5, 1) != 0)
    {
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    ndb2 = new Ndb( con2, db ? db : "TEST_DB" );

    if(ndb2->init() != 0){
      ERR(ndb2->getNdbError());
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    // Connect to Ndb and wait for it to become ready
    while(ndb2->waitUntilReady() != 0)
      ndbout << "Waiting for ndb to become ready..." << endl;
  }

  int result = 0;
  
  NdbDictionary::Dictionary *myDict = MyNdb.getDictionary();
  Vector<NdbDictionary::Event*> events;
  Vector<NdbEventOperation*> event_ops;
  int sz = 0;
  for(i= optind; i<argc; i++)
  {
    const NdbDictionary::Table* table= myDict->getTable(argv[i]);
    if(!table)
    {
      ndbout_c("Could not find table: %s, skipping", argv[i]);
      continue;
    }

    BaseString name;
    name.appfmt("EV-%s", argv[i]);
    NdbDictionary::Event *myEvent= new NdbDictionary::Event(name.c_str());
    myEvent->setTable(table->getName());
    myEvent->addTableEvent(NdbDictionary::Event::TE_ALL); 
    for(int a = 0; a < table->getNoOfColumns(); a++){
      myEvent->addEventColumn(a);
    }

    if (myDict->createEvent(* myEvent))
    {
      if(myDict->getNdbError().classification == NdbError::SchemaObjectExists) 
      {
	g_info << "Event creation failed event exists. Removing...\n";
	if (myDict->dropEvent(name.c_str()))
	{
	  g_err << "Failed to drop event: " << myDict->getNdbError() << endl;
	  result = 1;
	  goto end;
	}
	// try again
	if (myDict->createEvent(* myEvent)) 
	{
	  g_err << "Failed to create event: " << myDict->getNdbError() << endl;
	  result = 1;
	  goto end;
	}
      }
      else
      {
	g_err << "Failed to create event: " << myDict->getNdbError() << endl;
	result = 1;
	goto end;
      }
    }
    
    events.push_back(myEvent);

    NdbEventOperation* pOp = MyNdb.createEventOperation(name.c_str());
    if ( pOp == NULL ) {
      g_err << "Event operation creation failed" << endl;
      result = 1;
      goto end;
    }

    event_values.push_back(Vector<NdbRecAttr *>());
    event_pre_values.push_back(Vector<NdbRecAttr *>());
    for (int a = 0; a < table->getNoOfColumns(); a++) 
    {
      event_values[sz].
        push_back(pOp->getValue(table->getColumn(a)->getName()));
      event_pre_values[sz].
        push_back(pOp->getPreValue(table->getColumn(a)->getName()));
    }
    event_ops.push_back(pOp);
    {
      struct Table_info ti;
      ti.id = sz;
      table_infos.push_back(ti);
    }
    pOp->setCustomData((void *)&table_infos[sz]);
    sz++;
  }

  for(i= 0; i<(int)event_ops.size(); i++)
  {
    if (event_ops[i]->execute())
    { 
      g_err << "operation execution failed: " << event_ops[i]->getNdbError()
	    << endl;
      result = 1;
      goto end;
    }
  }

  struct Trans_arg trans_arg;
  while(true)
  {
    while(MyNdb.pollEvents(100) == 0);
    
    NdbEventOperation* pOp= MyNdb.nextEvent();
    while(pOp)
    {
      Uint64 gci= pOp->getGCI();
      Uint64 cnt_i= 0, cnt_u= 0, cnt_d= 0;
      if (ndb2)
        do_begin(ndb2, trans_arg);
      do
      {
	switch(pOp->getEventType())
	{
	case NdbDictionary::Event::TE_INSERT:
	  cnt_i++;
          if (ndb2)
            do_insert(trans_arg, pOp);
	  break;
	case NdbDictionary::Event::TE_DELETE:
	  cnt_d++;
          if (ndb2)
            do_delete(trans_arg, pOp);
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	  cnt_u++;
          if (ndb2)
            do_update(trans_arg, pOp);
	  break;
	case NdbDictionary::Event::TE_CLUSTER_FAILURE:
	  break;
	case NdbDictionary::Event::TE_ALTER:
	  break;
	case NdbDictionary::Event::TE_DROP:
	  break;
	case NdbDictionary::Event::TE_NODE_FAILURE:
	  break;
	case NdbDictionary::Event::TE_SUBSCRIBE:
	case NdbDictionary::Event::TE_UNSUBSCRIBE:
	  break;
	default:
	  /* We should REALLY never get here. */
	  ndbout_c("Error: unknown event type: %u", 
		   (Uint32)pOp->getEventType());
	  abort();
	}
      } while ((pOp= MyNdb.nextEvent()) && gci == pOp->getGCI());
      if (ndb2)
        do_commit(trans_arg);
      ndbout_c("GCI: %lld events: %lld(I) %lld(U) %lld(D)", gci, cnt_i, cnt_u, cnt_d);
    }
  }
end:
  for(i= 0; i<(int)event_ops.size(); i++)
    MyNdb.dropEventOperation(event_ops[i]);

  if (ndb2)
    delete ndb2;
  if (con2)
    delete con2;
  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<struct Table_info>;
template class Vector<NdbRecAttr*>;
template class Vector< Vector<NdbRecAttr*> >;
template class Vector<NdbDictionary::Event*>;
template class Vector<NdbEventOperation*>;
