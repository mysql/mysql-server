/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbSleep.h>
#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include "my_alloc.h"

static int opt_loop = 25;
static int opt_sleep = 25;
static int opt_drop = 1;
static int opt_subloop = 5;
static int opt_wait_all = 0;

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::connectstring,
    NdbStdOpt::ndb_nodeid,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    NDB_STD_OPT_DEBUG{"loop", 'l', "loops", &opt_loop, &opt_loop, 0, GET_INT,
                      REQUIRED_ARG, opt_loop, 0, 0, 0, 0, 0},
    {"sleep", 's', "Sleep (ms) between connection attempt", &opt_sleep,
     &opt_sleep, 0, GET_INT, REQUIRED_ARG, opt_sleep, 0, 0, 0, 0, 0},
    {"drop", 'd',
     "Drop event operations before disconnect (0 = no, 1 = yes, else rand",
     &opt_drop, &opt_drop, 0, GET_INT, REQUIRED_ARG, opt_drop, 0, 0, 0, 0, 0},
    {"subscribe-loop", NDB_OPT_NOSHORT, "Loop in subscribe/unsubscribe",
     &opt_subloop, &opt_subloop, 0, GET_INT, REQUIRED_ARG, opt_subloop, 0, 0, 0,
     0, 0},
    {"wait-all", NDB_OPT_NOSHORT, "Wait for all ndb-nodes (i.e not only some)",
     &opt_wait_all, &opt_wait_all, 0, GET_INT, REQUIRED_ARG, opt_wait_all, 0, 0,
     0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
#ifndef NDEBUG
  opt_debug = "d:t:O,/tmp/ndb_connect.trace";
#endif
  if (opts.handle_options()) return NDBT_ProgramExit(NDBT_WRONGARGS);

  for (int i = 0; i < opt_loop; i++) {
    Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
    con.configure_tls(opt_tls_search_path, opt_mgm_tls);
    if (con.connect(12, 5, 1) != 0) {
      ndbout << "Unable to connect to management server."
             << "loop: " << i << "(of " << opt_loop << ")" << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    int res = con.wait_until_ready(30, 30);
    if (res < 0 || (opt_wait_all && res != 0)) {
      ndbout << "nodeid: " << con.node_id() << "loop: " << i << "(of "
             << opt_loop << ")"
             << " - Cluster nodes not ready in 30 seconds." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    Ndb MyNdb(&con, "TEST_DB");
    if (MyNdb.init() != 0) {
      NDB_ERR(MyNdb.getNdbError());
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    for (int k = opt_subloop; k >= 1; k--) {
      if (k > 1 && ((k % 25) == 0)) {
        ndbout_c("subscribe/unsubscribe: %u", opt_subloop - k);
      }
      Vector<NdbEventOperation *> ops;
      const NdbDictionary::Dictionary *dict = MyNdb.getDictionary();
      for (int j = 0; j < argc; j++) {
        const NdbDictionary::Table *pTab = dict->getTable(argv[j]);
        if (pTab == 0) {
          ndbout_c("Failed to retreive table: \"%s\"", argv[j]);
        }

        BaseString tmp;
        tmp.appfmt("EV-%s", argv[j]);
        NdbEventOperation *pOp = MyNdb.createEventOperation(tmp.c_str());
        if (pOp == NULL) {
          ndbout << "Event operation creation failed: " << MyNdb.getNdbError()
                 << endl;
          return NDBT_ProgramExit(NDBT_FAILED);
        }

        for (int a = 0; a < pTab->getNoOfColumns(); a++) {
          pOp->getValue(pTab->getColumn(a)->getName());
          pOp->getPreValue(pTab->getColumn(a)->getName());
        }

        ops.push_back(pOp);
        if (pOp->execute()) {
          ndbout << "operation execution failed: " << pOp->getNdbError()
                 << endl;
          k = 1;
        }
      }

      if (opt_sleep) {
        NdbSleep_MilliSleep(10 + rand() % opt_sleep);
      } else {
        ndbout_c("NDBT_ProgramExit: SLEEPING OK");
        while (true) NdbSleep_SecSleep(5);
      }

      for (Uint32 i = 0; i < ops.size(); i++) {
        switch (k == 1 ? opt_drop : 1) {
          case 0:
            break;
          do_drop:
          case 1:
            if (MyNdb.dropEventOperation(ops[i])) {
              ndbout << "drop event operation failed " << MyNdb.getNdbError()
                     << endl;
              return NDBT_ProgramExit(NDBT_FAILED);
            }
            break;
          default:
            if ((rand() % 100) > 50) goto do_drop;
        }
      }
    }
  }

  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<NdbEventOperation *>;
