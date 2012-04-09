/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

void desc_AutoGrowSpecification(struct NdbDictionary::AutoGrowSpecification ags);
int desc_logfilegroup(Ndb *myndb, char* name);
int desc_undofile(Ndb_cluster_connection &con, Ndb *myndb, char* name);
int desc_datafile(Ndb_cluster_connection &con, Ndb *myndb, char* name);
int desc_tablespace(Ndb *myndb,char* name);
int desc_table(Ndb *myndb,char* name);
int desc_hashmap(Ndb_cluster_connection &con, Ndb *myndb, char* name);

static const char* _dbname = "TEST_DB";
static int _unqualified = 0;
static int _partinfo = 0;
static int _blobinfo = 0;
static int _nodeinfo = 0;

const char *load_default_groups[]= { "mysql_cluster",0 };

static int _retries = 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "unqualified", 'u', "Use unqualified table names",
    (uchar**) &_unqualified, (uchar**) &_unqualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "extra-partition-info", 'p', "Print more info per partition",
    (uchar**) &_partinfo, (uchar**) &_partinfo, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "retries", 'r', "Retry every second for # retries",
    (uchar**) &_retries, (uchar**) &_retries, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "blob-info", 'b', "Show information for hidden blob tables (requires -p)",
    (uchar**) &_blobinfo, (uchar**) &_blobinfo, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "extra-node-info", 'n', "Print node info for partitions (requires -p)",
    (uchar**) &_nodeinfo, (uchar**) &_nodeinfo, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
}

static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static void print_part_info(Ndb* pNdb, NDBT_Table* pTab);

int main(int argc, char** argv){
  NDB_INIT(argv[0]);

  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_desc.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
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
    else if (desc_hashmap(con, &MyNdb, argv[i]))
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
  {
    print_part_info(myndb, pTab);
    ndbout << endl;
    if (_blobinfo)
    {
      int noOfAttributes = pTab->getNoOfColumns();
      for (int i = 0; i < noOfAttributes; i++)
      {
        const NdbDictionary::Column* column = pTab->getColumn(i);
        if ((column->getType() == NdbDictionary::Column::Blob) || 
          (column->getType() == NdbDictionary::Column::Text))
        {
          print_part_info(myndb, (NDBT_Table*) column->getBlobTable());
          ndbout << endl;
        }
      }
    }
  }
	
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
    { "Extent_space", 0, NdbDictionary::Column::FRAGMENT_EXTENT_SPACE },
    { "Free extent_space", 0, NdbDictionary::Column::FRAGMENT_FREE_EXTENT_SPACE },

    { 0, 0, 0 }
  };
  const Uint32 FragmentIdOffset = 0;

  ndbout << "-- Per partition info";

  if (_blobinfo && _partinfo)
    ndbout << " for " << pTab->getName();

  ndbout << " -- " << endl;
  
  const Uint32 codeWords= 1;
  Uint32 codeSpace[ codeWords ];
  NdbInterpretedCode code(NULL, // Table is irrelevant
                          &codeSpace[0],
                          codeWords);
  if ((code.interpret_exit_last_row() != 0) ||
      (code.finalise() != 0))
  {
    return;
  }

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
    
    if (pOp->setInterpretedCode(&code) != 0)
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

    if (_nodeinfo)
    {
      ndbout << "Nodes\t";
    }
    
    ndbout << endl;
    
    while(pOp->nextResult() == 0)
    {
      for(i = 0; g_part_info[i].m_title != 0; i++)
      {
        NdbRecAttr &r= *g_part_info[i].m_rec_attr;
        unsigned long long val;
        switch (r.getType()) {
        case NdbDictionary::Column::Bigunsigned:
          val= r.u_64_value();
          break;
        case NdbDictionary::Column::Unsigned:
          val= r.u_32_value();
          break;
        default:
          abort();
        }
        if (val != 0)
          printf("%-*.llu\t", (int)strlen(g_part_info[i].m_title), val);
        else
          printf("0%*.s\t", (int)strlen(g_part_info[i].m_title), "");
      }

      if (_nodeinfo)
      {
        Uint32 partId = g_part_info[ FragmentIdOffset ].m_rec_attr -> u_32_value();
        
        const Uint32 MaxReplicas = 4;
        Uint32 nodeIds[ MaxReplicas ];
        Uint32 nodeCnt = pTab->getFragmentNodes(partId, &nodeIds[0], MaxReplicas);
        
        if (nodeCnt)
        {
          for (Uint32 n = 0; n < nodeCnt; n++)
          {
            if (n > 0)
              printf(",");
            printf("%u", nodeIds[n]);
          }
          printf("\t");
        }
        else
        {
          printf("-\t");
        }
      }
        
      printf("\n");
    }
  } while(0);
  pTrans->close();
}

int desc_hashmap(Ndb_cluster_connection &con, Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);

  NdbDictionary::HashMap hm;
  if (dict->getHashMap(hm, name) == 0)
  {
    Uint32 len = hm.getMapLen();
    Uint32 * tmp = new Uint32[len];
    hm.getMapValues(tmp, len);
    for (Uint32 i = 0; i<len; i++)
    {
      printf("%.2u ", tmp[i]);
      if (((i+1) % 25) == 0)
        printf("\n");
    }
    if (((len + 1) % 25) != 0)
      printf("\n");
    delete [] tmp;
    return 1;
  }
  return 0;
}
