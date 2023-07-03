/*
<<<<<<< HEAD
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.
=======
   Copyright (c) 2003, 2021, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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
#include <ndb_opts.h>

#include <NdbApi.hpp>
#include <NdbOut.hpp>
#include "../src/ndbapi/NdbInfo.hpp"
#include <NdbSleep.h>

#include "my_alloc.h"

static int loops = 1;
static int delay = 5;
const char *load_default_groups[]= { "mysql_cluster",0 };

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "loops", 'l', "Run same select several times",
    &loops, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    loops, 0, 0, nullptr, 0, nullptr },
  { "delay", 256, "Delay between loops (in seconds)",
    &delay, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    delay, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::end_of_options,
};

int
main(int argc, char** argv)
{
<<<<<<< HEAD
  NDB_INIT(argv[0]);
=======
<<<<<<< HEAD
>>>>>>> pr/231
  Ndb_opts opts(argc, argv, my_long_options);
#ifndef NDEBUG
  opt_debug= "d:t:O,/tmp/ndbinfo_select_all.trace";
#endif
  if (opts.handle_options())
=======
  NDB_INIT(argv[0]);
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ndb_load_defaults(NULL,load_default_groups,&argc,&argv);
  int ho_error;
#ifndef NDEBUG
  opt_debug= "d:t:O,/tmp/ndbinfo_select_all.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
  {
    ndb_free_defaults(argv);
>>>>>>> upstream/cluster-7.6
    return 1;
  }

  if (argv[0] == 0)
  {
    ndb_free_defaults(argv);
    return 0;
  }

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndbinfo_select_all");
  if(con.connect(opt_connect_retries - 1, opt_connect_retry_delay, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    ndb_free_defaults(argv);
    return 1;
  }

  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    ndb_free_defaults(argv);
    return 1;
  }

  NdbInfo info(&con, "");
  if (!info.init())
  {
    ndbout << "Failed to init ndbinfo!" << endl;
    ndb_free_defaults(argv);
    return 1;
  }

  const Uint32 batchsizerows = 32;

  for (int ll = 0; loops == 0 || ll < loops; ll++)
  {
    for (int ii = 0; argv[ii] != 0; ii++)
    {
      ndbout << "== " << argv[ii] << " ==" << endl;

      const NdbInfo::Table * pTab = 0;
      int res = info.openTable(argv[ii], &pTab);
      if (res != 0)
      {
        ndbout << "Failed to open: " << argv[ii] << ", res: " << res << endl;
        continue;
      }

      unsigned cols = pTab->columns();
      for (unsigned i = 0; i<cols; i++)
      {
        const NdbInfo::Column * pCol = pTab->getColumn(i);
        ndbout << pCol->m_name.c_str() << "\t";
      }
      ndbout << endl;

      NdbInfoScanOperation * pScan = 0;
      res= info.createScanOperation(pTab, &pScan, batchsizerows);
      if (res != 0)
      {
        ndbout << "Failed to createScan: " << argv[ii] << ", res: " << res<< endl;
        info.closeTable(pTab);
        continue;
      }

      if (pScan->readTuples() != 0)
      {
        ndbout << "scanOp->readTuples failed" << endl;
        ndb_free_defaults(argv);
        return 1;
      }

      Vector<const NdbInfoRecAttr*> recAttrs;
      for (unsigned i = 0; i<cols; i++)
      {
        const NdbInfoRecAttr* pRec = pScan->getValue(i);
        if (pRec == 0)
        {
          ndbout << "Failed to getValue(" << i << ")" << endl;
          ndb_free_defaults(argv);
          return 1;
        }
        recAttrs.push_back(pRec);
      }

      if(pScan->execute() != 0)
      {
        ndbout << "scanOp->execute failed" << endl;
        ndb_free_defaults(argv);
        return 1;
      }

      while(pScan->nextResult() == 1)
      {
        for (unsigned i = 0; i<cols; i++)
        {
          if (recAttrs[i]->isNULL())
          {
            ndbout << "NULL";
          }
          else
          {
            switch(pTab->getColumn(i)->m_type){
            case NdbInfo::Column::String:
              ndbout << recAttrs[i]->c_str();
              break;
            case NdbInfo::Column::Number:
              ndbout << recAttrs[i]->u_32_value();
              break;
            case NdbInfo::Column::Number64:
              ndbout << recAttrs[i]->u_64_value();
              break;
            }
          }
          ndbout << "\t";
        }
        ndbout << endl;
      }

      info.releaseScanOperation(pScan);
      info.closeTable(pTab);
    }

    if ((loops == 0 || ll + 1 != loops) && delay > 0)
    {
      NdbSleep_SecSleep(delay);
    }
  }
  ndb_free_defaults(argv);
  return 0;
}

template class Vector<const NdbInfoRecAttr*>;
