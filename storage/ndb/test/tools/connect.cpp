/*
   Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>

static int _loop = 25;
static int _sleep = 25;
static int _drop = 1;
static int _subloop = 5;
static int _wait_all = 0;

typedef uchar* gptr;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "loop", 'l', "loops",
    (gptr*) &_loop, (gptr*) &_loop, 0,
    GET_INT, REQUIRED_ARG, _loop, 0, 0, 0, 0, 0 }, 
  { "sleep", 's', "Sleep (ms) between connection attempt",
    (gptr*) &_sleep, (gptr*) &_sleep, 0,
    GET_INT, REQUIRED_ARG, _sleep, 0, 0, 0, 0, 0 }, 
  { "drop", 'd', 
    "Drop event operations before disconnect (0 = no, 1 = yes, else rand",
    (gptr*) &_drop, (gptr*) &_drop, 0,
    GET_INT, REQUIRED_ARG, _drop, 0, 0, 0, 0, 0 }, 
  { "subscribe-loop", NDB_OPT_NOSHORT,
    "Loop in subscribe/unsubscribe",
    (uchar**) &_subloop, (uchar**) &_subloop, 0,
    GET_INT, REQUIRED_ARG, _subloop, 0, 0, 0, 0, 0 }, 
  { "wait-all", NDB_OPT_NOSHORT,
    "Wait for all ndb-nodes (i.e not only some)",
    (uchar**) &_wait_all, (uchar**) &_wait_all, 0,
    GET_INT, REQUIRED_ARG, _wait_all, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int main(int argc, char** argv){
  NDB_INIT(argv[0]);

  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_desc.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  for (int i = 0; i<_loop; i++)
  {
    Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
    if(con.connect(12, 5, 1) != 0)
    {
      ndbout << "Unable to connect to management server." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    int res = con.wait_until_ready(30,30);
    if (res < 0 || (_wait_all && res != 0))
    {
      ndbout << "Cluster nodes not ready in 30 seconds." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    Ndb MyNdb(&con, "TEST_DB");
    if(MyNdb.init() != 0){
      ERR(MyNdb.getNdbError());
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    for (int k = _subloop; k >= 1; k--)
    {
      if (k > 1 && ((k % 25) == 0))
      {
        ndbout_c("subscribe/unsubscribe: %u", _subloop - k);
      }
      Vector<NdbEventOperation*> ops;
      const NdbDictionary::Dictionary * dict= MyNdb.getDictionary();
      for (int j = 0; j < argc; j++) 
      {
        const NdbDictionary::Table * pTab = dict->getTable(argv[j]);
        if (pTab == 0)
        {
          ndbout_c("Failed to retreive table: \"%s\"", argv[j]);
        }
        
        BaseString tmp;
        tmp.appfmt("EV-%s", argv[j]);
        NdbEventOperation* pOp = MyNdb.createEventOperation(tmp.c_str());
        if ( pOp == NULL ) 
        {
          ndbout << "Event operation creation failed: " << 
            MyNdb.getNdbError() << endl;
          return NDBT_ProgramExit(NDBT_FAILED);
        }
        
        for (int a = 0; a < pTab->getNoOfColumns(); a++) 
        {
          pOp->getValue(pTab->getColumn(a)->getName());
          pOp->getPreValue(pTab->getColumn(a)->getName());
        }
        
        ops.push_back(pOp);
        if (pOp->execute())
        { 
          ndbout << "operation execution failed: " << pOp->getNdbError()
                 << endl;
          k = 1;
        }
      }
      
      if (_sleep)
      {
        NdbSleep_MilliSleep(10 + rand() % _sleep);
      }
      else
      {
        ndbout_c("NDBT_ProgramExit: SLEEPING OK");
        while(true) NdbSleep_SecSleep(5);
      }
      
      for (Uint32 i = 0; i<ops.size(); i++)
      {
        switch(k == 1 ? _drop : 1){
        case 0:
          break;
        do_drop:
        case 1:
          if (MyNdb.dropEventOperation(ops[i]))
          {
            ndbout << "drop event operation failed " 
                   << MyNdb.getNdbError() << endl;
            return NDBT_ProgramExit(NDBT_FAILED);
          }
          break;
        default:
          if ((rand() % 100) > 50)
            goto do_drop;
        }
      }
    }
  }
  
  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<NdbEventOperation*>;
