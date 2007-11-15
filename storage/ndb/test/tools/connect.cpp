/* Copyright (C) 2003 MySQL AB

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
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>

NDB_STD_OPTS_VARS;

static int _loop = 25;
static int _sleep = 25;
static int _drop = 1;

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
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage()
{
  char desc[] =  "This program connects to ndbd, and then disconnects\n";
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

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
    Ndb_cluster_connection con(opt_connect_str);
    if(con.connect(12, 5, 1) != 0)
    {
      ndbout << "Unable to connect to management server." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    if (con.wait_until_ready(30,30) != 0)
    {
      ndbout << "Cluster nodes not ready in 30 seconds." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    Ndb MyNdb(&con, "TEST_DB");
    if(MyNdb.init() != 0){
      ERR(MyNdb.getNdbError());
      return NDBT_ProgramExit(NDBT_FAILED);
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
      
      if (pOp->execute())
      { 
        ndbout << "operation execution failed: " << pOp->getNdbError()
               << endl;
        return NDBT_ProgramExit(NDBT_FAILED);
      }
      ops.push_back(pOp);
    }
    
    if (_sleep)
    {
      NdbSleep_MilliSleep(10 + rand() % _sleep);
    }
    
    for (Uint32 i = 0; i<ops.size(); i++)
    {
      switch(_drop){
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
  
  return NDBT_ProgramExit(NDBT_OK);
}

template class Vector<NdbEventOperation*>;
