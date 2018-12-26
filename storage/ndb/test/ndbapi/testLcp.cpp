/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbRestarter.hpp>
#include <HugoOperations.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <getarg.h>
#include <InputStream.hpp>

struct CASE 
{
  bool start_row;
  bool end_row;
  bool curr_row;
  const char * op1;
  const char * op2;
  const char * op3;
  int val;
};

static CASE g_op_types[] =
{
  { false, true,  false, "INS", 0,     0,     0 }, // 0x001 a
  { true,  true,  false, "UPD", 0,     0,     0 }, // 0x002 d
  { true,  false, false, "DEL", 0,     0,     0 }, // 0x004 g

  { false, true,  false, "INS", "UPD", 0,     0 }, // 0x008 b
  { false, false, false, "INS", "DEL", 0,     0 }, // 0x010 c
  { true,  true,  false, "UPD", "UPD", 0,     0 }, // 0x020 e
  { true,  false, false, "UPD", "DEL", 0,     0 }, // 0x040 f
  { true,  true,  false, "DEL", "INS", 0,     0 }, // 0x080 h

  { false, true,  false, "INS", "DEL", "INS", 0 }, // 0x100 i
  { true,  false, false, "DEL", "INS", "DEL", 0 }  // 0x200 j
};
const size_t OP_COUNT = (sizeof(g_op_types)/sizeof(g_op_types[0]));

static Ndb* g_ndb = 0;
static CASE* g_ops;
static Ndb_cluster_connection *g_cluster_connection= 0;
static HugoOperations* g_hugo_ops;
static int g_use_ops = 1 | 2 | 4;
static int g_cases = 0x1;
static int g_case_loop = 2;
static int g_rows = 10;
static int g_setup_tables = 1;
static int g_one_op_at_a_time = 0;
static const char * g_tablename = "T1";
static const NdbDictionary::Table* g_table = 0;
static NdbRestarter g_restarter;

static int init_ndb(int argc, char** argv);
static int parse_args(int argc, char** argv);
static int connect_ndb();
static int drop_all_tables();
static int load_table();
static int pause_lcp(int error);
static int do_op(int row);
static int continue_lcp(int error = 0);
static int commit();
static int restart();
static int validate();

int 
main(int argc, char ** argv){
  ndb_init();
  require(!init_ndb(argc, argv));
  if(parse_args(argc, argv))
    return -1;
  require(!connect_ndb());
  
  if(g_setup_tables){
    require(!drop_all_tables());
    
    if(NDBT_Tables::createTable(g_ndb, g_tablename) != 0){
      exit(-1);
    }
  }
  
  g_table = g_ndb->getDictionary()->getTable(g_tablename);
  if(g_table == 0){
    g_err << "Failed to retreive table: " << g_tablename << endl;
    exit(-1);
  }
  require((g_hugo_ops = new HugoOperations(* g_table)) != 0);
  require(!g_hugo_ops->startTransaction(g_ndb));
  
  g_ops= new CASE[g_rows];
  
  const int use_ops = g_use_ops;
  for(size_t i = 0; i<OP_COUNT; i++)
  {
    if(g_one_op_at_a_time){
      while(i < OP_COUNT && (use_ops & (1 << i)) == 0) i++;
      if(i == OP_COUNT)
	break;
      ndbout_c("-- loop\noperation: %c use_ops: %x", int('a'+i), use_ops);
      g_use_ops = (1 << i);
    } else {
      i = OP_COUNT - 1;
    }
    
    size_t test_case = 0;
    if((1 << test_case++) & g_cases)
    {
      for(size_t tl = 0; tl<(size_t)g_case_loop; tl++){
	g_info << "Performing all ops wo/ inteference of LCP" << endl;
	
	g_info << "Testing pre LCP operations, ZLCP_OP_WRITE_RT_BREAK" << endl;
	g_info << "  where ZLCP_OP_WRITE_RT_BREAK is "
	  " finished before SAVE_PAGES" << endl;
	require(!load_table());
	require(!pause_lcp(5900));
        for(int j = 0; j < g_rows; j++){
	  require(!do_op(j));
	}
	require(!continue_lcp(5900));
	require(!commit());
	require(!pause_lcp(5900));
	require(!restart());
	require(!validate());
      }  
    }
    
    if((1 << test_case++) & g_cases)
    {
      for(int tl = 0; tl<g_case_loop; tl++){
	g_info << "Testing pre LCP operations, ZLCP_OP_WRITE_RT_BREAK" << endl;
	g_info << "  where ZLCP_OP_WRITE_RT_BREAK is finished after SAVE_PAGES"
	       << endl;
	require(!load_table());
	require(!pause_lcp(5901));
        for(int j = 0; j < g_rows; j++){
	  require(!do_op(j));
	}
	require(!continue_lcp(5901));
	require(!commit());
	require(!pause_lcp(5900));
	require(!restart());
	require(!validate());
      }    
    }

    if((1 << test_case++) & g_cases)
    {
      for(int tl = 0; tl<g_case_loop; tl++){
	g_info << "Testing pre LCP operations, undo-ed at commit" << endl;
	require(!load_table());
	require(!pause_lcp(5902));
        for(int j = 0; j < g_rows; j++){
	  require(!do_op(j));
	}
	require(!continue_lcp(5902));
	require(!commit());
	require(!continue_lcp(5903));
	require(!pause_lcp(5900));
	require(!restart());
	require(!validate());
      }
    }
    
    if((1 << test_case++) & g_cases)
    {
      for(int tl = 0; tl < g_case_loop; tl++){
	g_info << "Testing prepared during LCP and committed after" << endl;
	require(!load_table());
	require(!pause_lcp(5904));    // Start LCP, but don't save pages
        for(int j = 0; j < g_rows; j++){
	  require(!do_op(j));
	}
	require(!continue_lcp(5904)); // Start ACC save pages
	require(!pause_lcp(5900));    // Next LCP
	require(!commit());
	require(!restart());
	require(!validate());
      }
    }
  }
}

static int init_ndb(int argc, char** argv)
{
  ndb_init();
  return 0;
}

static int parse_args(int argc, char** argv)
{
  size_t i;
  char * ops= 0, *cases=0;
  struct getargs args[] = {
    { "records", 0, arg_integer, &g_rows, "Number of records", "records" },
    { "operations", 'o', arg_string, &ops, "Operations [a-h]", 0 },
    { "1", '1', arg_flag, &g_one_op_at_a_time, "One op at a time", 0 },
    { "0", '0', arg_negative_flag, &g_one_op_at_a_time, "All ops at once", 0 },
    { "cases", 'c', arg_string, &cases, "Cases [a-c]", 0 },
    { 0, 't', arg_flag, &g_setup_tables, "Create table", 0 },
    { 0, 'u', arg_negative_flag, &g_setup_tables, "Dont create table", 0 }
  };
  
  int optind= 0;
  const int num_args = sizeof(args)/sizeof(args[0]);
  if(getarg(args, num_args, argc, (const char**)argv, &optind)) {
    arg_printusage(args, num_args, argv[0], " tabname1\n");
    ndbout_c("\n -- Operations [a-%c] = ", int('a'+OP_COUNT-1));
    for(i = 0; i<OP_COUNT; i++){
      ndbout_c("\t%c = %s %s", 
	       int('a'+i), g_op_types[i].op1,
	       g_op_types[i].op2 ? g_op_types[i].op2 : "");
    }
    return -1;
  }
  
  if(ops != 0){
    g_use_ops = 0;
    char * s = ops;
    while(* s)
      g_use_ops |= (1 << ((* s++) - 'a'));
  }

  if(cases != 0){
    g_cases = 0;
    char * s = cases;
    while(* s)
      g_cases |= (1 << ((* s++) - 'a'));
  }
  
  ndbout_c("table: %s", g_tablename);
  printf("operations: ");
  for(i = 0; i<OP_COUNT; i++)
    if(g_use_ops & (1 << i))
      printf("%c", int('a'+i));
  printf("\n");
  
  printf("test cases: ");
  for(i = 0; i<3; i++)
    if(g_cases & (1 << i))
      printf("%c", int('1'+i));
  printf("\n");
  printf("-------------\n");  
  return 0;
}

static int connect_ndb()
{
  g_cluster_connection = new Ndb_cluster_connection();
  if(g_cluster_connection->connect(12, 5, 1) != 0)
  {
    return 1;
  }

  g_ndb = new Ndb(g_cluster_connection, "TEST_DB");
  g_ndb->init(256);
  if(g_ndb->waitUntilReady(30) == 0){
    return 0;
//    int args[] = { DumpStateOrd::DihMaxTimeBetweenLCP };
//    return g_restarter.dumpStateAllNodes(args, 1);
  }
  return -1;
}

static int disconnect_ndb()
{
  delete g_ndb;
  delete g_cluster_connection;
  g_ndb = 0;
  g_table = 0;
  g_cluster_connection= 0;
  return 0;
}

static int drop_all_tables()
{
  NdbDictionary::Dictionary * dict = g_ndb->getDictionary();
  require(dict);

  BaseString db = g_ndb->getDatabaseName();
  BaseString schema = g_ndb->getSchemaName();

  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::TypeUndefined) == -1){
      g_err << "Failed to list tables: " << endl
	    << dict->getNdbError() << endl;
      return -1;
  }
  for (unsigned i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element& elt = list.elements[i];
    switch (elt.type) {
    case NdbDictionary::Object::SystemTable:
    case NdbDictionary::Object::UserTable:
      g_ndb->setDatabaseName(elt.database);
      g_ndb->setSchemaName(elt.schema);
      if(dict->dropTable(elt.name) != 0){
	g_err << "Failed to drop table: " 
	      << elt.database << "/" << elt.schema << "/" << elt.name <<endl;
	g_err << dict->getNdbError() << endl;
	return -1;
      }
      break;
    case NdbDictionary::Object::UniqueHashIndex:
    case NdbDictionary::Object::OrderedIndex:
    case NdbDictionary::Object::HashIndexTrigger:
    case NdbDictionary::Object::IndexTrigger:
    case NdbDictionary::Object::SubscriptionTrigger:
    case NdbDictionary::Object::ReadOnlyConstraint:
    default:
      break;
    }
  }
  
  g_ndb->setDatabaseName(db.c_str());
  g_ndb->setSchemaName(schema.c_str());
  
  return 0;
}

static int load_table()
{
  UtilTransactions clear(* g_table);
  require(!clear.clearTable(g_ndb));
  
  HugoOperations ops(* g_table);
  require(!ops.startTransaction(g_ndb));
  size_t op = 0;
  size_t rows = 0;
  size_t uncommitted = 0;
  //bool prepared = false;
  for(int i = 0; i < g_rows; i++){
    for(op %= OP_COUNT; !((1 << op) & g_use_ops); op = (op + 1) % OP_COUNT);
    g_ops[i] = g_op_types[op++];
    if(g_ops[i].start_row){
      g_ops[i].curr_row = true;
      g_ops[i].val = rand();
      require(!ops.pkInsertRecord(g_ndb, i, 1, g_ops[i].val));
      uncommitted++;
    } else {
      g_ops[i].curr_row = false;
    }
    if(uncommitted >= 100){
      require(!ops.execute_Commit(g_ndb));
      require(!ops.getTransaction()->restart());
      rows += uncommitted;
      uncommitted = 0;
    }
  }
  if(uncommitted)
    require(!ops.execute_Commit(g_ndb));

  require(!ops.closeTransaction(g_ndb));
  rows += uncommitted;
  g_info << "Inserted " << rows << " rows" << endl;
  return 0;
}

static int pause_lcp(int error)
{
  int nodes = g_restarter.getNumDbNodes();

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_INFO, 0 };

  ndb_native_socket_t fd= ndb_mgm_listen_event(g_restarter.handle, filter);
  ndb_socket_t my_fd = ndb_socket_create_from_native(fd);

  require(ndb_socket_valid(my_fd));
  require(!g_restarter.insertErrorInAllNodes(error));
  int dump[] = { DumpStateOrd::DihStartLcpImmediately };
  require(!g_restarter.dumpStateAllNodes(dump, 1));
  
  char *tmp;
  char buf[1024];
  SocketInputStream in(my_fd, 1000);
  int count = 0;
  do {
    tmp = in.gets(buf, 1024);
    if(tmp)
    {
      int id;
      if(sscanf(tmp, "%*[^:]: LCP: %d ", &id) == 1 && id == error &&
	 --nodes == 0){
	ndb_socket_close(my_fd);
	return 0;
      }
    }
  } while(count++ < 30);
  
  ndb_socket_close(my_fd);
  return -1;
}

static int do_op(int row)
{
  HugoOperations & ops = * g_hugo_ops;
  if(strcmp(g_ops[row].op1, "INS") == 0){
    require(!g_ops[row].curr_row);
    g_ops[row].curr_row = true;
    g_ops[row].val = rand();
    require(!ops.pkInsertRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op1, "UPD") == 0){
    require(g_ops[row].curr_row);
    g_ops[row].val = rand();
    require(!ops.pkUpdateRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op1, "DEL") == 0){
    require(g_ops[row].curr_row);    
    g_ops[row].curr_row = false;
    require(!ops.pkDeleteRecord(g_ndb, row, 1));
  }

  require(!ops.execute_NoCommit(g_ndb));
  
  if(g_ops[row].op2 == 0){
  } else if(strcmp(g_ops[row].op2, "INS") == 0){
    require(!g_ops[row].curr_row);
    g_ops[row].curr_row = true;
    g_ops[row].val = rand();
    require(!ops.pkInsertRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op2, "UPD") == 0){
    require(g_ops[row].curr_row);
    g_ops[row].val = rand();
    require(!ops.pkUpdateRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op2, "DEL") == 0){
    require(g_ops[row].curr_row);    
    g_ops[row].curr_row = false;    
    require(!ops.pkDeleteRecord(g_ndb, row, 1));
  }
  
  if(g_ops[row].op2 != 0)
    require(!ops.execute_NoCommit(g_ndb));  

  if(g_ops[row].op3 == 0){
  } else if(strcmp(g_ops[row].op3, "INS") == 0){
    require(!g_ops[row].curr_row);
    g_ops[row].curr_row = true;
    g_ops[row].val = rand();
    require(!ops.pkInsertRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op3, "UPD") == 0){
    require(g_ops[row].curr_row);
    g_ops[row].val = rand();
    require(!ops.pkUpdateRecord(g_ndb, row, 1, g_ops[row].val));
  } else if(strcmp(g_ops[row].op3, "DEL") == 0){
    require(g_ops[row].curr_row);    
    g_ops[row].curr_row = false;    
    require(!ops.pkDeleteRecord(g_ndb, row, 1));
  }
  
  if(g_ops[row].op3 != 0)
    require(!ops.execute_NoCommit(g_ndb));  

  return 0;
}

static int continue_lcp(int error)
{
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_INFO, 0 };
  ndb_socket_t my_fd;
  ndb_socket_invalidate(&my_fd);

  if(error){
    ndb_native_socket_t fd = ndb_mgm_listen_event(g_restarter.handle, filter);
    my_fd = ndb_socket_create_from_native(fd);
    require(ndb_socket_valid(my_fd));
  }

  int args[] = { DumpStateOrd::LCPContinue };
  if(g_restarter.dumpStateAllNodes(args, 1) != 0)
    return -1;
  
  if(error){
    char *tmp;
    char buf[1024];
    SocketInputStream in(my_fd, 1000);
    int count = 0;
    int nodes = g_restarter.getNumDbNodes();
    do {
      tmp = in.gets(buf, 1024);
      if(tmp)
      {
	int id;
	if(sscanf(tmp, "%*[^:]: LCP: %d ", &id) == 1 && id == error &&
	   --nodes == 0){
	  ndb_socket_close(my_fd);
	  return 0;
	}
      }
    } while(count++ < 30);
    
    ndb_socket_close(my_fd);
  }
  return 0;
}

static int commit()
{
  HugoOperations & ops = * g_hugo_ops;  
  int res = ops.execute_Commit(g_ndb);
  if(res == 0){
    return ops.getTransaction()->restart();
  }
  return res;
}

static int restart()
{
  g_info << "Restarting cluster" << endl;
  g_hugo_ops->closeTransaction(g_ndb);
  disconnect_ndb();
  delete g_hugo_ops;
  
  require(!g_restarter.restartAll());
  require(!g_restarter.waitClusterStarted(30));
  require(!connect_ndb());
  
  g_table = g_ndb->getDictionary()->getTable(g_tablename);
  require(g_table);
  require((g_hugo_ops = new HugoOperations(* g_table)) != 0);
  require(!g_hugo_ops->startTransaction(g_ndb));
  return 0;
}

static int validate()
{
  HugoOperations ops(* g_table);
  for(int i = 0; i < g_rows; i++){
    require(g_ops[i].curr_row == g_ops[i].end_row);
    require(!ops.startTransaction(g_ndb));
    ops.pkReadRecord(g_ndb, i, 1);
    int res = ops.execute_Commit(g_ndb);
    if(g_ops[i].curr_row){
      require(res == 0 && ops.verifyUpdatesValue(g_ops[i].val) == 0);
    } else {
      require(res == 626);
    }
    ops.closeTransaction(g_ndb);
  }

  for(int j = 0; j<10; j++){
    UtilTransactions clear(* g_table);
    require(!clear.clearTable(g_ndb));
    
    HugoTransactions trans(* g_table);
    require(trans.loadTable(g_ndb, 1024) == 0);
  }
  return 0;
}

