
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbRestarter.hpp>
#include <HugoOperations.hpp>
#include <UtilTransactions.hpp>
#include <signaldata/DumpStateOrd.hpp>

struct CASE 
{
  bool start_row;
  bool end_row;
  bool curr_row;
  const char * op1;
  const char * op2;
  int val;
};

static CASE g_ops[] =
{
  { false, true,  false, "INSERT", 0,        0 },
  { false, true,  false, "INSERT", "UPDATE", 0 },
  { false, false, false, "INSERT", "DELETE", 0 },
  { true,  true,  false, "UPDATE", 0,        0 },
  { true,  true,  false, "UPDATE", "UPDATE", 0 },
  { true,  false, false, "UPDATE", "DELETE", 0 },
  { true,  false, false, "DELETE", 0,        0 },
  { true,  true,  false, "DELETE", "INSERT", 0 }
};
const size_t OP_COUNT = (sizeof(g_ops)/sizeof(g_ops[0]));

static Ndb* g_ndb = 0;
static CASE* g_cases;
static HugoOperations* g_hugo_ops;

static int g_rows = 1000;
static int g_setup_tables = 1;
static const char * g_tablename = "T1";
static const NdbDictionary::Table* g_table = 0;
static NdbRestarter g_restarter;

static int init_ndb(int argc, char** argv);
static int parse_args(int argc, char** argv);
static int connect_ndb();
static int drop_all_tables();
static int load_table();
static int pause_lcp();
static int do_op(int row);
static int continue_lcp(int error);
static int commit();
static int restart();
static int validate();

#define require(x) { bool b = x; if(!b){g_err << __LINE__ << endl; abort();}}

int 
main(int argc, char ** argv){

  require(!init_ndb(argc, argv));
  require(!parse_args(argc, argv));
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
  require(g_hugo_ops = new HugoOperations(* g_table));
  require(!g_hugo_ops->startTransaction(g_ndb));
  
  g_cases= new CASE[g_rows];
  require(!load_table());
  
  g_info << "Performing all ops wo/ inteference of LCP" << endl;
  
  g_info << "Testing pre LCP operations, ZLCP_OP_WRITE_RT_BREAK" << endl;
  g_info << "  where ZLCP_OP_WRITE_RT_BREAK is finished before SAVE_PAGES"
	 << endl;
  require(!pause_lcp());
  for(size_t j = 0; j<g_rows; j++){
    require(!do_op(j));
  }
  require(!continue_lcp(5900));
  require(!commit());
  require(!restart());
  require(!validate());
  
  g_info << "Testing pre LCP operations, ZLCP_OP_WRITE_RT_BREAK" << endl;
  g_info << "  where ZLCP_OP_WRITE_RT_BREAK is finished after SAVE_PAGES"
	 << endl;
  require(!load_table());
  require(!pause_lcp());
  for(size_t j = 0; j<g_rows; j++){
    require(!do_op(j));
  }
  require(!continue_lcp(5901));
  require(!commit());
  require(!restart());
  require(!validate());
  
  g_info << "Testing pre LCP operations, undo-ed at commit" << endl;
  require(!load_table());
  require(!pause_lcp());
  for(size_t j = 0; j<g_rows; j++){
    require(!do_op(j));
  }
  require(!continue_lcp(5902));
  require(!commit());
  require(!continue_lcp(5903));
  require(!restart());
  require(!validate());
}

static int init_ndb(int argc, char** argv)
{
  return 0;
}

static int parse_args(int argc, char** argv)
{
  return 0;
}

static int connect_ndb()
{
  g_ndb = new Ndb("TEST_DB");
  g_ndb->init();
  if(g_ndb->waitUntilReady(30) == 0){
    int args[] = { DumpStateOrd::DihMaxTimeBetweenLCP };
    return g_restarter.dumpStateAllNodes(args, 1);
  }
  return -1;
}

static int disconnect_ndb()
{
  delete g_ndb;
  g_ndb = 0;
  g_table = 0;
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
  for(size_t i = 0; i<g_rows; i++){
    g_cases[i] = g_ops[ i % OP_COUNT];
    if(g_cases[i].start_row){
      g_cases[i].curr_row = true;
      g_cases[i].val = rand();
      require(!ops.pkInsertRecord(g_ndb, i, 1, g_cases[i].val));
    }
    if((i+1) % 100 == 0){
      require(!ops.execute_Commit(g_ndb));
      require(!ops.getTransaction()->restart());
    }
  }
  if((g_rows+1) % 100 != 0)
    require(!ops.execute_Commit(g_ndb));
  return 0;
}

static int pause_lcp()
{
  return 0;
}

static int do_op(int row)
{
  HugoOperations & ops = * g_hugo_ops;
  if(strcmp(g_cases[row].op1, "INSERT") == 0){
    require(!g_cases[row].curr_row);
    g_cases[row].curr_row = true;
    g_cases[row].val = rand();
    require(!ops.pkInsertRecord(g_ndb, row, 1, g_cases[row].val));
  } else if(strcmp(g_cases[row].op1, "UPDATE") == 0){
    require(g_cases[row].curr_row);
    g_cases[row].val = rand();
    require(!ops.pkUpdateRecord(g_ndb, row, 1, g_cases[row].val));
  } else if(strcmp(g_cases[row].op1, "DELETE") == 0){
    require(g_cases[row].curr_row);    
    g_cases[row].curr_row = false;
    require(!ops.pkDeleteRecord(g_ndb, row, 1));
  }

  require(!ops.execute_NoCommit(g_ndb));
  
  if(g_cases[row].op2 == 0){
  } else if(strcmp(g_cases[row].op2, "INSERT") == 0){
    require(!g_cases[row].curr_row);
    g_cases[row].curr_row = true;
    g_cases[row].val = rand();
    require(!ops.pkInsertRecord(g_ndb, row, 1, g_cases[row].val));
  } else if(strcmp(g_cases[row].op2, "UPDATE") == 0){
    require(g_cases[row].curr_row);
    g_cases[row].val = rand();
    require(!ops.pkUpdateRecord(g_ndb, row, 1, g_cases[row].val));
  } else if(strcmp(g_cases[row].op2, "DELETE") == 0){
    require(g_cases[row].curr_row);    
    g_cases[row].curr_row = false;    
    require(!ops.pkDeleteRecord(g_ndb, row, 1));
  }
  
  if(g_cases[row].op2 != 0)
    require(!ops.execute_NoCommit(g_ndb));  
  return 0;
}

static int continue_lcp(int error)
{
  error = 0;
  if(g_restarter.insertErrorInAllNodes(error) == 0){
    int args[] = { DumpStateOrd::DihStartLcpImmediately };
    return g_restarter.dumpStateAllNodes(args, 1);
  }
  return -1;
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
  disconnect_ndb();
  delete g_hugo_ops;
  
  require(!g_restarter.restartAll());
  require(!g_restarter.waitClusterStarted(30));
  require(!connect_ndb());
  
  g_table = g_ndb->getDictionary()->getTable(g_tablename);
  require(g_table);
  require(g_hugo_ops = new HugoOperations(* g_table));
  require(!g_hugo_ops->startTransaction(g_ndb));
  return 0;
}

static int validate()
{
  HugoOperations ops(* g_table);
  for(size_t i = 0; i<g_rows; i++){
    require(g_cases[i].curr_row == g_cases[i].end_row);
    require(!ops.startTransaction(g_ndb));
    ops.pkReadRecord(g_ndb, i, 1);
    int res = ops.execute_Commit(g_ndb);
    if(g_cases[i].curr_row){
      require(res == 0 && ops.verifyUpdatesValue(g_cases[i].val) == 0);
    } else {
      require(res == 626);
    }
    ops.closeTransaction(g_ndb);
  }
  return 0;
}

