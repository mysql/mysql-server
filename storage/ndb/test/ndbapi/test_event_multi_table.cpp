/* Copyright (C) 2005 MySQL AB

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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <TestNdbEventOperation.hpp>

static void usage()
{
  ndb_std_print_version();
}

static int start_transaction(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->startTransaction(ndb) != NDBT_OK)
    return -1;
  NdbTransaction * t= ops[0]->getTransaction();
  for (int i= ops.size()-1; i > 0; i--)
  {
    ops[i]->setTransaction(t);
  }
  return 0;
}

static int close_transaction(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->closeTransaction(ndb) != NDBT_OK)
    return -1;
  for (int i= ops.size()-1; i > 0; i--)
  {
    ops[i]->setTransaction(NULL);
  }
  return 0;
}

static int execute_commit(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->execute_Commit(ndb) != NDBT_OK)
    return -1;
  return 0;
}

static int copy_events(Ndb *ndb,
		       Vector<NdbEventOperation *> &ops,
		       Vector<const NdbDictionary::Table *> &tabs,
		       Vector<Vector<NdbRecAttr *> > &values)
{
  DBUG_ENTER("copy_events");
  int r= 0;
  while (1)
  {
    int res= ndb->pollEvents(1000); // wait for event or 1000 ms
    DBUG_PRINT("info", ("pollEvents res=%d", r));
    if (res <= 0)
    {
      break;
    }
    for (unsigned i_ops= 0; i_ops < ops.size(); i_ops++)
    {
      NdbEventOperation *pOp= ops[i_ops];
      const NdbDictionary::Table *table= tabs[i_ops];
      Vector<NdbRecAttr *> &recAttr= values[i_ops];

      int overrun= 0;
      unsigned i;
      unsigned n_columns= table->getNoOfColumns();
      while (pOp->next(&overrun) > 0)
      {
	if (overrun)
	{
	  g_err << "buffer overrun\n";
	  DBUG_RETURN(-1);
	}
	r++;
	
	Uint32 gci= pOp->getGCI();

	if (!pOp->isConsistent()) {
	  g_err << "A node failure has occured and events might be missing\n";
	  DBUG_RETURN(-1);
	}
	
	int noRetries= 0;
	do
	{
	  NdbTransaction *trans= ndb->startTransaction();
	  if (trans == 0)
	  {
	    g_err << "startTransaction failed "
		  << ndb->getNdbError().code << " "
		  << ndb->getNdbError().message << endl;
	    DBUG_RETURN(-1);
	  }
	  
	  NdbOperation *op= trans->getNdbOperation(table);
	  if (op == 0)
	  {
	    g_err << "getNdbOperation failed "
		  << trans->getNdbError().code << " "
		  << trans->getNdbError().message << endl;
	    DBUG_RETURN(-1);
	  }
	  
	  switch (pOp->getEventType()) {
	  case NdbDictionary::Event::TE_INSERT:
	    if (op->insertTuple())
	    {
	      g_err << "insertTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(-1);
	    }
	    break;
	  case NdbDictionary::Event::TE_DELETE:
	    if (op->deleteTuple())
	    {
	      g_err << "deleteTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(-1);
	    }
	    break;
	  case NdbDictionary::Event::TE_UPDATE:
	    if (op->updateTuple())
	    {
	      g_err << "updateTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(-1);
	    }
	    break;
	  default:
	    abort();
	  }
	  
	  for (i= 0; i < n_columns; i++)
	  {
	    if (recAttr[i]->isNULL())
	    {
	      if (table->getColumn(i)->getPrimaryKey())
	      {
		g_err << "internal error: primary key isNull()="
		      << recAttr[i]->isNULL() << endl;
		DBUG_RETURN(NDBT_FAILED);
	      }
	      switch (pOp->getEventType()) {
	      case NdbDictionary::Event::TE_INSERT:
		if (recAttr[i]->isNULL() < 0)
		{
		  g_err << "internal error: missing value for insert\n";
		  DBUG_RETURN(NDBT_FAILED);
		}
		break;
	      case NdbDictionary::Event::TE_DELETE:
		break;
	      case NdbDictionary::Event::TE_UPDATE:
		break;
	      default:
		abort();
	      }
	    }
	    if (table->getColumn(i)->getPrimaryKey() &&
		op->equal(i,recAttr[i]->aRef()))
	    {
	      g_err << "equal " << i << " "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	  }
	  
	  switch (pOp->getEventType()) {
	  case NdbDictionary::Event::TE_INSERT:
	    for (i= 0; i < n_columns; i++)
	    {
	      if (!table->getColumn(i)->getPrimaryKey() &&
		  op->setValue(i,recAttr[i]->isNULL() ? 0:recAttr[i]->aRef()))
	      {
		g_err << "setValue(insert) " << i << " "
		      << op->getNdbError().code << " "
		      << op->getNdbError().message << endl;
		DBUG_RETURN(-1);
	      }
	    }
	    break;
	  case NdbDictionary::Event::TE_DELETE:
	    break;
	  case NdbDictionary::Event::TE_UPDATE:
	    for (i= 0; i < n_columns; i++)
	    {
	      if (!table->getColumn(i)->getPrimaryKey() &&
		  recAttr[i]->isNULL() >= 0 &&
		  op->setValue(i,recAttr[i]->isNULL() ? 0:recAttr[i]->aRef()))
	      {
		g_err << "setValue(update) " << i << " "
		      << op->getNdbError().code << " "
		      << op->getNdbError().message << endl;
		DBUG_RETURN(NDBT_FAILED);
	      }
	    }
	    break;
	  case NdbDictionary::Event::TE_ALL:
	    abort();
	  }
	  if (trans->execute(Commit) == 0)
	  {
	    trans->close();
	    // everything ok
	    break;
	  }
	  if (noRetries++ == 10 ||
	      trans->getNdbError().status != NdbError::TemporaryError)
	  {
	    g_err << "execute " << r << " failed "
		  << trans->getNdbError().code << " "
		  << trans->getNdbError().message << endl;
	    trans->close();
	    DBUG_RETURN(-1);
	  }
	  trans->close();
	  NdbSleep_MilliSleep(100); // sleep before retying
	} while(1);
      }
    }
  }
  DBUG_RETURN(r);
}

static int verify_copy(Ndb *ndb,
		       Vector<const NdbDictionary::Table *> &tabs1,
		       Vector<const NdbDictionary::Table *> &tabs2)
{
  for (unsigned i= 0; i < tabs1.size(); i++)
    if (tabs1[i])
    {
      HugoTransactions hugoTrans(*tabs1[i]);
      if (hugoTrans.compare(ndb, tabs2[i]->getName(), 0))
	return -1;
    }
  return 0;
}

NDB_STD_OPTS_VARS;

static const char* _dbname = "TEST_DB";
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS(""),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:F:L";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  DBUG_ENTER("main");
  Ndb_cluster_connection con(opt_connect_str);
  if(con.connect(12, 5, 1))
  {
    DBUG_RETURN(NDBT_ProgramExit(NDBT_FAILED));
  }
  

  Ndb ndb(&con,_dbname);
  ndb.init();
  while (ndb.waitUntilReady() != 0);

  NdbDictionary::Dictionary * dict = ndb.getDictionary();
  int no_error= 1;
  int i;

  // create all tables
  Vector<const NdbDictionary::Table*> pTabs;
  for (i= 0; no_error && argc; argc--, i++)
  {
    dict->dropTable(argv[i]);
    NDBT_Tables::createTable(&ndb, argv[i]);
    const NdbDictionary::Table *pTab= dict->getTable(argv[i]);
    if (pTab == 0)
    {
      ndbout << "Failed to create table" << endl;
      ndbout << dict->getNdbError() << endl;
      no_error= 0;
      break;
    }
    pTabs.push_back(pTab);
  }
  pTabs.push_back(NULL);

  // create an event for each table
  for (i= 0; no_error && pTabs[i]; i++)
  {
    HugoTransactions ht(*pTabs[i]);
    if (ht.createEvent(&ndb)){
      no_error= 0;
      break;
    }
  }

  // create an event operation for each event
  Vector<NdbEventOperation *> pOps;
  for (i= 0; no_error && pTabs[i]; i++)
  {
    char buf[1024];
    sprintf(buf, "%s_EVENT", pTabs[i]->getName());
    NdbEventOperation *pOp= ndb.createEventOperation(buf, 1000);
    if ( pOp == NULL )
    {
      no_error= 0;
      break;
    }
    pOps.push_back(pOp);
  }

  // get storage for each event operation
  Vector<Vector<NdbRecAttr*> > values;
  Vector<Vector<NdbRecAttr*> > pre_values;
  for (i= 0; no_error && pTabs[i]; i++)
  {
    int n_columns= pTabs[i]->getNoOfColumns();
    Vector<NdbRecAttr*> tmp_a;
    Vector<NdbRecAttr*> tmp_b;
    for (int j = 0; j < n_columns; j++) {
      tmp_a.push_back(pOps[i]->getValue(pTabs[i]->getColumn(j)->getName()));
      tmp_b.push_back(pOps[i]->getPreValue(pTabs[i]->getColumn(j)->getName()));
    }
    values.push_back(tmp_a);
    pre_values.push_back(tmp_b);
  }

  // start receiving events
  for (i= 0; no_error && pTabs[i]; i++)
  {
    if ( pOps[i]->execute() )
    {
      no_error= 0;
      break;
    }
  }

  // create a "shadow" table for each table
  Vector<const NdbDictionary::Table*> pShadowTabs;
  for (i= 0; no_error && pTabs[i]; i++)
  {
    char buf[1024];
    sprintf(buf, "%s_SHADOW", pTabs[i]->getName());

    dict->dropTable(buf);
    if (dict->getTable(buf))
    {
      no_error= 0;
      break;
    }

    NdbDictionary::Table table_shadow(*pTabs[i]);
    table_shadow.setName(buf);
    dict->createTable(table_shadow);
    pShadowTabs.push_back(dict->getTable(buf));
    if (!pShadowTabs[i])
    {
      no_error= 0;
      break;
    }
  }

  // create a hugo operation per table
  Vector<HugoOperations *> hugo_ops;
  for (i= 0; no_error && pTabs[i]; i++)
  {
    hugo_ops.push_back(new HugoOperations(*pTabs[i]));
  }

  sleep(5);

  // insert 3 records per table
  do {
    if (start_transaction(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }
    for (i= 0; no_error && pTabs[i]; i++)
    {
      hugo_ops[i]->pkInsertRecord(&ndb, 0, 3);
    }
    if (execute_commit(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }
    if(close_transaction(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }
  } while(0);

  // copy events and verify
  do {
    if (copy_events(&ndb, pOps, pShadowTabs, values) < 0)
    {
      no_error= 0;
      break;
    }
    if (verify_copy(&ndb, pTabs, pShadowTabs))
    {
      no_error= 0;
      break;
    }
  } while (0);

  // update 2 records in first table
  do {
    if (start_transaction(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }

    hugo_ops[0]->pkUpdateRecord(&ndb, 2);

    if (execute_commit(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }
    if(close_transaction(&ndb, hugo_ops))
    {
      no_error= 0;
      break;
    }
  } while(0);

  // copy events and verify
  do {
    if (copy_events(&ndb, pOps, pShadowTabs, values) < 0)
    {
      no_error= 0;
      break;
    }
    if (verify_copy(&ndb, pTabs, pShadowTabs))
    {
      no_error= 0;
      break;
    }
  } while (0);

  if (no_error)
    DBUG_RETURN(NDBT_ProgramExit(NDBT_OK));
  DBUG_RETURN(NDBT_ProgramExit(NDBT_FAILED));
}

template class Vector<HugoOperations *>;
template class Vector<NdbEventOperation *>;
template class Vector<NdbRecAttr*>;
template class Vector<Vector<NdbRecAttr*> >;
