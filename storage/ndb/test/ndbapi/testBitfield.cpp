
#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <HugoTransactions.hpp>

static const char* _dbname = "TEST_DB";
static int g_loops = 7;

static void usage()
{
  ndb_std_print_version();
}
#if 0
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       const char *argument)
{
  return ndb_std_get_one_option(optid, opt, argument ? argument :
				"d:t:O,/tmp/testBitfield.trace");
}
#endif

static const NdbDictionary::Table* create_random_table(Ndb*);
static int transactions(Ndb*, const NdbDictionary::Table* tab);
static int unique_indexes(Ndb*, const NdbDictionary::Table* tab);
static int ordered_indexes(Ndb*, const NdbDictionary::Table* tab);
static int node_restart(Ndb*, const NdbDictionary::Table* tab);
static int system_restart(Ndb*, const NdbDictionary::Table* tab);

int 
main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;

  argc--;
  argv++;
  
  Ndb_cluster_connection con(opt_connect_str);
  if(con.connect(12, 5, 1))
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  

  Ndb* pNdb;
  pNdb = new Ndb(&con, _dbname);  
  pNdb->init();
  while (pNdb->waitUntilReady() != 0);
  int res = NDBT_FAILED;

  NdbDictionary::Dictionary * dict = pNdb->getDictionary();

  const NdbDictionary::Table* pTab = 0;
  for (int i = 0; i < (argc ? argc : g_loops) ; i++)
  {
    res = NDBT_FAILED;
    if(argc == 0)
    {
      pTab = create_random_table(pNdb);
    }
    else
    {
      dict->dropTable(argv[i]);
      NDBT_Tables::createTable(pNdb, argv[i]);
      pTab = dict->getTable(argv[i]);
    }
    
    if (pTab == 0)
    {
      ndbout << "Failed to create table" << endl;
      ndbout << dict->getNdbError() << endl;
      break;
    }
    
    if(transactions(pNdb, pTab))
      break;

    if(unique_indexes(pNdb, pTab))
      break;

    if(ordered_indexes(pNdb, pTab))
      break;
    
    if(node_restart(pNdb, pTab))
      break;
    
    if(system_restart(pNdb, pTab))
      break;

    dict->dropTable(pTab->getName());
    res = NDBT_OK;
  }

  if(res != NDBT_OK && pTab)
  {
    dict->dropTable(pTab->getName());
  }
  
  delete pNdb;
  return NDBT_ProgramExit(res);
}

static 
const NdbDictionary::Table* 
create_random_table(Ndb* pNdb)
{
  do {
    NdbDictionary::Table tab;
    Uint32 cols = 1 + (rand() % (NDB_MAX_ATTRIBUTES_IN_TABLE - 1));
    Uint32 keys = NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY;
    Uint32 length = 4090;
    Uint32 key_size = NDB_MAX_KEYSIZE_IN_WORDS;
    
    BaseString name; 
    name.assfmt("TAB_%d", rand() & 65535);
    tab.setName(name.c_str());
    for(int i = 0; i<cols && length > 2; i++)
    {
      NdbDictionary::Column col;
      name.assfmt("COL_%d", i);
      col.setName(name.c_str());
      if(i == 0 || i == 1)
      {
	col.setType(NdbDictionary::Column::Unsigned);
	col.setLength(1); 
	col.setNullable(false);
	col.setPrimaryKey(i == 0);
	tab.addColumn(col);
	continue;
      }
      
      col.setType(NdbDictionary::Column::Bit);
      
      Uint32 len = 1 + (rand() % (length - 1));
      col.setLength(len); length -= len;
      int nullable = (rand() >> 16) & 1;
      col.setNullable(nullable); length -= nullable;
      col.setPrimaryKey(false);
      tab.addColumn(col);
    }
    
    pNdb->getDictionary()->dropTable(tab.getName());
    if(pNdb->getDictionary()->createTable(tab) == 0)
    {
      ndbout << (NDBT_Table&)tab << endl;
      return pNdb->getDictionary()->getTable(tab.getName());
    }
  } while(0);
  return 0;
}

static 
int
transactions(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  int i = 0;
  HugoTransactions trans(* tab);
  i |= trans.loadTable(pNdb, 1000);
  i |= trans.pkReadRecords(pNdb, 1000, 13); 
  i |= trans.scanReadRecords(pNdb, 1000, 25);
  i |= trans.pkUpdateRecords(pNdb, 1000, 37);
  i |= trans.scanUpdateRecords(pNdb, 1000, 25);
  i |= trans.pkDelRecords(pNdb, 500, 23);
  i |= trans.clearTable(pNdb);
  return i;
}

static 
int 
unique_indexes(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
ordered_indexes(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
node_restart(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}

static 
int 
system_restart(Ndb* pNdb, const NdbDictionary::Table* tab)
{
  return 0;
}
