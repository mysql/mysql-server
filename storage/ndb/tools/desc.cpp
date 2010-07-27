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

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>

void desc_AutoGrowSpecification(struct NdbDictionary::AutoGrowSpecification ags);
int desc_logfilegroup(Ndb *myndb, char* name);
int desc_undofile(Ndb_cluster_connection &con, Ndb *myndb, char* name);
int desc_datafile(Ndb_cluster_connection &con, Ndb *myndb, char* name);
int desc_tablespace(Ndb *myndb,char* name);
int desc_table(Ndb *myndb,char* name);

NDB_STD_OPTS_VARS;

static const char* _dbname = "TEST_DB";
static int _unqualified = 0;
static int _partinfo = 0;

const char *load_default_groups[]= { "mysql_cluster",0 };

static int _retries = 0;
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    &_dbname, &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "unqualified", 'u', "Use unqualified table names",
    &_unqualified, &_unqualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "extra-partition-info", 'p', "Print more info per partition",
    &_partinfo, &_partinfo, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "retries", 'r', "Retry every second for # retries",
    &_retries, &_retries, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void usage()
{
#ifdef NOT_USED
  char desc[] = 
    "tabname\n"\
    "This program list all properties of table(s) in NDB Cluster.\n"\
    "  ex: desc T1 T2 T4\n";
#endif
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static void print_part_info(Ndb* pNdb, NDBT_Table* pTab);

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_desc.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  Ndb_cluster_connection con(opt_connect_str);
  con.set_name("ndb_desc");
  if(con.connect(12, 5, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  for(int i= 0; i<argc;i++)
  {
    if(desc_table(&MyNdb,argv[i]))
      ;
    else if(desc_tablespace(&MyNdb,argv[i]))
      ;
    else if(desc_logfilegroup(&MyNdb,argv[i]))
      ;
    else if(desc_datafile(con, &MyNdb, argv[i]))
      ;
    else if(desc_undofile(con, &MyNdb, argv[i]))
      ;
    else
      ndbout << "No such object: " << argv[i] << endl << endl;
  }

  return NDBT_ProgramExit(NDBT_OK);
}

void desc_AutoGrowSpecification(struct NdbDictionary::AutoGrowSpecification ags)
{
  ndbout << "AutoGrow.min_free: " << ags.min_free << endl;
  ndbout << "AutoGrow.max_size: " << ags.max_size << endl;
  ndbout << "AutoGrow.file_size: " << ags.file_size << endl;
  ndbout << "AutoGrow.filename_pattern: " << ags.filename_pattern << endl;
}

int desc_logfilegroup(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::LogfileGroup lfg= dict->getLogfileGroup(name);
  NdbError err= dict->getNdbError();
  if( (int) err.classification != (int) ndberror_cl_none)
    return 0;

  ndbout << "Type: LogfileGroup" << endl;
  ndbout << "Name: " << lfg.getName() << endl;
  ndbout << "UndoBuffer size: " << lfg.getUndoBufferSize() << endl;
  ndbout << "Version: " << lfg.getObjectVersion() << endl;
  ndbout << "Free Words: " << lfg.getUndoFreeWords() << endl;

  desc_AutoGrowSpecification(lfg.getAutoGrowSpecification());

  ndbout << endl;

  return 1;
}

int desc_tablespace(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::Tablespace ts= dict->getTablespace(name);
  NdbError err= dict->getNdbError();
  if ((int) err.classification != (int) ndberror_cl_none)
    return 0;

  ndbout << "Type: Tablespace" << endl;
  ndbout << "Name: " << ts.getName() << endl;
  ndbout << "Object Version: " << ts.getObjectVersion() << endl;
  ndbout << "Extent Size: " << ts.getExtentSize() << endl;
  ndbout << "Default Logfile Group: " << ts.getDefaultLogfileGroup() << endl;
  ndbout << endl;
  return 1;
}

int desc_undofile(Ndb_cluster_connection &con, Ndb *myndb, char* name)
{
  unsigned id;
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  Ndb_cluster_connection_node_iter iter;

  assert(dict);

  con.init_get_next_node(iter);

  while ((id= con.get_next_node(iter)))
  {
    NdbDictionary::Undofile uf= dict->getUndofile(0, name);
    NdbError err= dict->getNdbError();
    if ((int) err.classification != (int) ndberror_cl_none)
      return 0;

    ndbout << "Type: Undofile" << endl;
    ndbout << "Name: " << name << endl;
    ndbout << "Node: " << id << endl;
    ndbout << "Path: " << uf.getPath() << endl;
    ndbout << "Size: " << uf.getSize() << endl;

    ndbout << "Logfile Group: " << uf.getLogfileGroup() << endl;

    /** FIXME: are these needed, the functions aren't there
	but the prototypes are...

	ndbout << "Number: " << uf.getFileNo() << endl;
    */

    ndbout << endl;
  }

  return 1;
}

int desc_datafile(Ndb_cluster_connection &con, Ndb *myndb, char* name)
{
  unsigned id;
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  Ndb_cluster_connection_node_iter iter;

  con.init_get_next_node(iter);

  while ((id= con.get_next_node(iter)))
  {
    NdbDictionary::Datafile df= dict->getDatafile(id, name);
    NdbError err= dict->getNdbError();
    if ((int) err.classification != (int) ndberror_cl_none)
      return 0;

    ndbout << "Type: Datafile" << endl;
    ndbout << "Name: " << name << endl;
    ndbout << "Node: " << id << endl;
    ndbout << "Path: " << df.getPath() << endl;
    ndbout << "Size: " << df.getSize() << endl;
    ndbout << "Free: " << df.getFree() << endl;

    ndbout << "Tablespace: " << df.getTablespace() << endl;

    /** We probably don't need to display this ever...
	ndbout << "Number: " << uf.getFileNo() << endl;
    */

    ndbout << endl;
  }

  return 1;
}

int desc_table(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary * dict= myndb->getDictionary();
  NDBT_Table* pTab;
  while ((pTab = (NDBT_Table*)dict->getTable(name)) == NULL && --_retries >= 0) NdbSleep_SecSleep(1);
  if (!pTab)
    return 0;

  ndbout << (* pTab) << endl;

  NdbDictionary::Dictionary::List list;
  if (dict->listIndexes(list, name) != 0){
    ndbout << name << ": " << dict->getNdbError() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  ndbout << "-- Indexes -- " << endl;
  ndbout << "PRIMARY KEY(";
  unsigned j;
  for (j= 0; (int)j < pTab->getNoOfPrimaryKeys(); j++)
  {
    const NdbDictionary::Column * col= pTab->getColumn(pTab->getPrimaryKey(j));
    ndbout << col->getName();
    if ((int)j < pTab->getNoOfPrimaryKeys()-1)
      ndbout << ", ";
  }
  ndbout << ") - UniqueHashIndex" << endl;
  for (j= 0; j < list.count; j++) {
    NdbDictionary::Dictionary::List::Element& elt = list.elements[j];
    const NdbDictionary::Index *pIdx = dict->getIndex(elt.name, name);
    if (!pIdx){
      ndbout << name << ": " << dict->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    ndbout << (*pIdx) << endl;
  }
  ndbout << endl;

  if (_partinfo)
    print_part_info(myndb, pTab);
	
  return 1;
}

struct InfoInfo
{
  const char * m_title;
  NdbRecAttr* m_rec_attr;
  const NdbDictionary::Column* m_column;
};


static 
void print_part_info(Ndb* pNdb, NDBT_Table* pTab)
{
  InfoInfo g_part_info[] = {
    { "Partition", 0, NdbDictionary::Column::FRAGMENT },
    { "Row count", 0, NdbDictionary::Column::ROW_COUNT },
    { "Commit count", 0, NdbDictionary::Column::COMMIT_COUNT },
    { "Frag fixed memory", 0, NdbDictionary::Column::FRAGMENT_FIXED_MEMORY },
    { "Frag varsized memory", 0, NdbDictionary::Column::FRAGMENT_VARSIZED_MEMORY },
    { 0, 0, 0 }
  };

  ndbout << "-- Per partition info -- " << endl;
  
  NdbConnection* pTrans = pNdb->startTransaction();
  if (pTrans == 0)
    return;
  
  do
  {
    NdbScanOperation* pOp= pTrans->getNdbScanOperation(pTab->getName());
    if (pOp == NULL)
      break;
    
    int rs = pOp->readTuples(NdbOperation::LM_CommittedRead); 
    if (rs != 0)
      break;
    
    if (pOp->interpret_exit_last_row() != 0)
      break;
    
    Uint32 i = 0;
    for(i = 0; g_part_info[i].m_title != 0; i++)
    {
      if ((g_part_info[i].m_rec_attr = pOp->getValue(g_part_info[i].m_column)) == 0)
	break;
    }

    if (g_part_info[i].m_title != 0)
      break;

    if (pTrans->execute(NoCommit) != 0)
      break;
	
    for (i = 0; g_part_info[i].m_title != 0; i++)
      ndbout << g_part_info[i].m_title << "\t";
    ndbout << endl;
    
    while(pOp->nextResult() == 0)
    {
      for(i = 0; g_part_info[i].m_title != 0; i++)
      {
	ndbout << *g_part_info[i].m_rec_attr << "\t";
      }
      ndbout << endl;
    }
  } while(0);
  
  pTrans->close();
}
