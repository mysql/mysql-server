/* Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <NDBT_ReturnCodes.h>
#include <ndb_global.h>
#include <ndb_opts.h>
#include <HugoTransactions.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <TestNdbEventOperation.hpp>
#include <UtilTransactions.hpp>

static void usage() { ndb_std_print_version(); }

static int start_transaction(Ndb *ndb, Vector<HugoOperations *> &ops) {
  if (ops[0]->startTransaction(ndb) != NDBT_OK) return -1;
  NdbTransaction *t = ops[0]->getTransaction();
  for (int i = ops.size() - 1; i > 0; i--) {
    ops[i]->setTransaction(t);
  }
  return 0;
}

static int close_transaction(Ndb *ndb, Vector<HugoOperations *> &ops) {
  if (ops[0]->closeTransaction(ndb) != NDBT_OK) return -1;
  for (int i = ops.size() - 1; i > 0; i--) {
    ops[i]->setTransaction(NULL);
  }
  return 0;
}

static int execute_commit(Ndb *ndb, Vector<HugoOperations *> &ops) {
  if (ops[0]->execute_Commit(ndb) != NDBT_OK) return -1;
  return 0;
}

static int copy_events(Ndb *ndb) {
  DBUG_ENTER("copy_events");
  int r = 0;
  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  while (1) {
    int res = ndb->pollEvents(1000);  // wait for event or 1000 ms
    DBUG_PRINT("info", ("pollEvents res=%d", res));
    if (res <= 0) {
      break;
    }
    int error = 0;
    NdbEventOperation *pOp;
    while ((pOp = ndb->nextEvent())) {
      char buf[1024];
      sprintf(buf, "%s_SHADOW", pOp->getTable()->getName());
      const NdbDictionary::Table *table = dict->getTable(buf);

      if (table == 0) {
        g_err << "unable to find table " << buf << endl;
        DBUG_RETURN(-1);
      }

      if (pOp->isOverrun()) {
        g_err << "buffer overrun\n";
        DBUG_RETURN(-1);
      }
      r++;

      Uint32 gci = pOp->getGCI();

      if (!pOp->isConsistent()) {
        g_err << "A node failure has occurred and events might be missing\n";
        DBUG_RETURN(-1);
      }

      int noRetries = 0;
      do {
        NdbTransaction *trans = ndb->startTransaction();
        if (trans == 0) {
          g_err << "startTransaction failed " << ndb->getNdbError().code << " "
                << ndb->getNdbError().message << endl;
          DBUG_RETURN(-1);
        }

        NdbOperation *op = trans->getNdbOperation(table);
        if (op == 0) {
          g_err << "getNdbOperation failed " << trans->getNdbError().code << " "
                << trans->getNdbError().message << endl;
          DBUG_RETURN(-1);
        }

        switch (pOp->getEventType()) {
          case NdbDictionary::Event::TE_INSERT:
            if (op->insertTuple()) {
              g_err << "insertTuple " << op->getNdbError().code << " "
                    << op->getNdbError().message << endl;
              DBUG_RETURN(-1);
            }
            break;
          case NdbDictionary::Event::TE_DELETE:
            if (op->deleteTuple()) {
              g_err << "deleteTuple " << op->getNdbError().code << " "
                    << op->getNdbError().message << endl;
              DBUG_RETURN(-1);
            }
            break;
          case NdbDictionary::Event::TE_UPDATE:
            if (op->updateTuple()) {
              g_err << "updateTuple " << op->getNdbError().code << " "
                    << op->getNdbError().message << endl;
              DBUG_RETURN(-1);
            }
            break;
          default:
            abort();
        }

        {
          for (const NdbRecAttr *pk = pOp->getFirstPkAttr(); pk;
               pk = pk->next()) {
            if (pk->isNULL()) {
              g_err << "internal error: primary key isNull()=" << pk->isNULL()
                    << endl;
              DBUG_RETURN(NDBT_FAILED);
            }
            if (op->equal(pk->getColumn()->getColumnNo(), pk->aRef())) {
              g_err << "equal " << pk->getColumn()->getColumnNo() << " "
                    << op->getNdbError().code << " "
                    << op->getNdbError().message << endl;
              DBUG_RETURN(NDBT_FAILED);
            }
          }
        }

        switch (pOp->getEventType()) {
          case NdbDictionary::Event::TE_INSERT: {
            for (const NdbRecAttr *data = pOp->getFirstDataAttr(); data;
                 data = data->next()) {
              if (data->isNULL() < 0 ||
                  op->setValue(data->getColumn()->getColumnNo(),
                               data->isNULL() ? 0 : data->aRef())) {
                g_err << "setValue(insert) " << data->getColumn()->getColumnNo()
                      << " " << op->getNdbError().code << " "
                      << op->getNdbError().message << endl;
                DBUG_RETURN(-1);
              }
            }
            break;
          }
          case NdbDictionary::Event::TE_DELETE:
            break;
          case NdbDictionary::Event::TE_UPDATE: {
            for (const NdbRecAttr *data = pOp->getFirstDataAttr(); data;
                 data = data->next()) {
              if (data->isNULL() >= 0 &&
                  op->setValue(data->getColumn()->getColumnNo(),
                               data->isNULL() ? 0 : data->aRef())) {
                g_err << "setValue(update) " << data->getColumn()->getColumnNo()
                      << " " << op->getNdbError().code << " "
                      << op->getNdbError().message << endl;
                DBUG_RETURN(NDBT_FAILED);
              }
            }
            break;
          }
          case NdbDictionary::Event::TE_ALL:
            abort();
        }
        if (trans->execute(Commit) == 0) {
          trans->close();
          // everything ok
          break;
        }
        if (noRetries++ == 10 ||
            trans->getNdbError().status != NdbError::TemporaryError) {
          g_err << "execute " << r << " failed " << trans->getNdbError().code
                << " " << trans->getNdbError().message << endl;
          trans->close();
          DBUG_RETURN(-1);
        }
        trans->close();
        NdbSleep_MilliSleep(100);  // sleep before retrying
      } while (1);
    }  // for
    if (error) {
      g_err << "nextEvent()\n";
      DBUG_RETURN(-1);
    }
  }  // while(1)
  DBUG_RETURN(r);
}

static int verify_copy(Ndb *ndb, Vector<const NdbDictionary::Table *> &tabs1,
                       Vector<const NdbDictionary::Table *> &tabs2) {
  for (unsigned i = 0; i < tabs1.size(); i++)
    if (tabs1[i]) {
      HugoTransactions hugoTrans(*tabs1[i]);
      if (hugoTrans.compare(ndb, tabs2[i]->getName(), 0)) return -1;
    }
  return 0;
}

static const char *_dbname = "TEST_DB";
struct my_option my_long_options[] = {
    NDB_STD_OPTS(""),
    {"database", 'd', "Name of database table is in", &_dbname, &_dbname, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);

  int ho_error;
#ifndef NDEBUG
  opt_debug = "d:t:F:L";
#endif
  if ((ho_error = opts.handle_options(())))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  DBUG_ENTER("main");
  Ndb_cluster_connection con(opt_connect_str);
  con.configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con.connect(12, 5, 1)) {
    DBUG_RETURN(NDBT_ProgramExit(NDBT_FAILED));
  }

  Ndb ndb(&con, _dbname);
  ndb.init();
  while (ndb.waitUntilReady() != 0)
    ;

  NdbDictionary::Dictionary *dict = ndb.getDictionary();
  int no_error = 1;
  int i;

  // create all tables
  Vector<const NdbDictionary::Table *> pTabs;
  if (argc == 0) {
    NDBT_Tables::dropAllTables(&ndb);
    NDBT_Tables::createAllTables(&ndb);
    for (i = 0; no_error && i < NDBT_Tables::getNumTables(); i++) {
      const NdbDictionary::Table *pTab =
          dict->getTable(NDBT_Tables::getTable(i)->getName());
      if (pTab == 0) {
        ndbout << "Failed to create table" << endl;
        ndbout << dict->getNdbError() << endl;
        no_error = 0;
        break;
      }
      pTabs.push_back(pTab);
    }
  } else {
    for (i = 0; no_error && argc; argc--, i++) {
      dict->dropTable(argv[i]);
      NDBT_Tables::createTable(&ndb, argv[i]);
      const NdbDictionary::Table *pTab = dict->getTable(argv[i]);
      if (pTab == 0) {
        ndbout << "Failed to create table" << endl;
        ndbout << dict->getNdbError() << endl;
        no_error = 0;
        break;
      }
      pTabs.push_back(pTab);
    }
  }
  pTabs.push_back(NULL);

  // create an event for each table
  for (i = 0; no_error && pTabs[i]; i++) {
    HugoTransactions ht(*pTabs[i]);
    if (ht.createEvent(&ndb)) {
      no_error = 0;
      break;
    }
  }

  // create an event operation for each event
  Vector<NdbEventOperation *> pOps;
  for (i = 0; no_error && pTabs[i]; i++) {
    char buf[1024];
    sprintf(buf, "%s_EVENT", pTabs[i]->getName());
    NdbEventOperation *pOp = ndb.createEventOperation(buf, 1000);
    if (pOp == NULL) {
      no_error = 0;
      break;
    }
    pOps.push_back(pOp);
  }

  // get storage for each event operation
  for (i = 0; no_error && pTabs[i]; i++) {
    int n_columns = pTabs[i]->getNoOfColumns();
    for (int j = 0; j < n_columns; j++) {
      pOps[i]->getValue(pTabs[i]->getColumn(j)->getName());
      pOps[i]->getPreValue(pTabs[i]->getColumn(j)->getName());
    }
  }

  // start receiving events
  for (i = 0; no_error && pTabs[i]; i++) {
    if (pOps[i]->execute()) {
      no_error = 0;
      break;
    }
  }

  // create a "shadow" table for each table
  Vector<const NdbDictionary::Table *> pShadowTabs;
  for (i = 0; no_error && pTabs[i]; i++) {
    char buf[1024];
    sprintf(buf, "%s_SHADOW", pTabs[i]->getName());

    dict->dropTable(buf);
    if (dict->getTable(buf)) {
      no_error = 0;
      break;
    }

    NdbDictionary::Table table_shadow(*pTabs[i]);
    table_shadow.setName(buf);
    dict->createTable(table_shadow);
    pShadowTabs.push_back(dict->getTable(buf));
    if (!pShadowTabs[i]) {
      no_error = 0;
      break;
    }
  }

  // create a hugo operation per table
  Vector<HugoOperations *> hugo_ops;
  for (i = 0; no_error && pTabs[i]; i++) {
    hugo_ops.push_back(new HugoOperations(*pTabs[i]));
  }

  int n_records = 3;
  // insert n_records records per table
  do {
    if (start_transaction(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }
    for (i = 0; no_error && pTabs[i]; i++) {
      hugo_ops[i]->pkInsertRecord(&ndb, 0, n_records);
    }
    if (execute_commit(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }
    if (close_transaction(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }
  } while (0);

  // copy events and verify
  do {
    if (copy_events(&ndb) < 0) {
      no_error = 0;
      break;
    }
    if (verify_copy(&ndb, pTabs, pShadowTabs)) {
      no_error = 0;
      break;
    }
  } while (0);

  // update n_records-1 records in first table
  do {
    if (start_transaction(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }

    hugo_ops[0]->pkUpdateRecord(&ndb, n_records - 1);

    if (execute_commit(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }
    if (close_transaction(&ndb, hugo_ops)) {
      no_error = 0;
      break;
    }
  } while (0);

  // copy events and verify
  do {
    if (copy_events(&ndb) < 0) {
      no_error = 0;
      break;
    }
    if (verify_copy(&ndb, pTabs, pShadowTabs)) {
      no_error = 0;
      break;
    }
  } while (0);

  {
    NdbRestarts restarts;
    for (int j = 0; j < 10; j++) {
      // restart a node
      if (no_error) {
        int timeout = 240;
        if (restarts.executeRestart("RestartRandomNodeAbort", timeout)) {
          no_error = 0;
          break;
        }
      }

      // update all n_records records on all tables
      if (start_transaction(&ndb, hugo_ops)) {
        no_error = 0;
        break;
      }

      for (int r = 0; r < n_records; r++) {
        for (i = 0; pTabs[i]; i++) {
          hugo_ops[i]->pkUpdateRecord(&ndb, r);
        }
      }
      if (execute_commit(&ndb, hugo_ops)) {
        no_error = 0;
        break;
      }
      if (close_transaction(&ndb, hugo_ops)) {
        no_error = 0;
        break;
      }

      // copy events and verify
      if (copy_events(&ndb) < 0) {
        no_error = 0;
        break;
      }
      if (verify_copy(&ndb, pTabs, pShadowTabs)) {
        no_error = 0;
        break;
      }
    }
  }

  // drop the event operations
  for (i = 0; i < (int)pOps.size(); i++) {
    if (ndb.dropEventOperation(pOps[i])) {
      no_error = 0;
    }
  }

  if (no_error) DBUG_RETURN(NDBT_ProgramExit(NDBT_OK));
  DBUG_RETURN(NDBT_ProgramExit(NDBT_FAILED));
}

template class Vector<HugoOperations *>;
template class Vector<NdbEventOperation *>;
template class Vector<NdbRecAttr *>;
template class Vector<Vector<NdbRecAttr *>>;
