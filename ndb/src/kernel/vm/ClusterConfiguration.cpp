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

#include "ClusterConfiguration.hpp"
#include <ErrorHandlingMacros.hpp>

#include <pc.hpp>
#include <BlockNumbers.h>
#include <signaldata/AccSizeAltReq.hpp>
#include <signaldata/DictSizeAltReq.hpp>
#include <signaldata/DihSizeAltReq.hpp>
#include <signaldata/LqhSizeAltReq.hpp>
#include <signaldata/TcSizeAltReq.hpp>
#include <signaldata/TupSizeAltReq.hpp>
#include <signaldata/TuxSizeAltReq.hpp>

ClusterConfiguration::ClusterConfiguration()
{
  for (unsigned i= 0; i< MAX_SIZEALT_BLOCKS; i++)             // initialize
    for (unsigned j= 0; j< MAX_SIZEALT_RECORD; j++) {
      the_clusterData.SizeAltData.varSize[i][j].valid = false;
      the_clusterData.SizeAltData.varSize[i][j].nrr = 0;
    }

  for (unsigned i1 = 0; i1< 5; i1++)                          // initialize
    for (unsigned j1= 0; j1< CmvmiCfgConf::NO_OF_WORDS; j1++)
      the_clusterData.ispValues[i1][j1] = 0;
  
  the_clusterData.SizeAltData.noOfNodes = 0;
  the_clusterData.SizeAltData.noOfNDBNodes = 0;
  the_clusterData.SizeAltData.noOfAPINodes = 0;
  the_clusterData.SizeAltData.noOfMGMNodes = 0;
}

ClusterConfiguration::~ClusterConfiguration(){
}

void
setValue(VarSize* dst, const int index, UintR variableValue){
  assert(dst != NULL);
  assert(index >= 0 && index < MAX_SIZEALT_RECORD);

  dst[index].nrr   = variableValue;
  dst[index].valid = true;
}

void
ClusterConfiguration::calcSizeAlteration()
{
  SizeAlt *size = &the_clusterData.SizeAltData;

  size->noOfTables++;         		 // Remove impact of system table
  size->noOfTables += size->noOfIndexes; // Indexes are tables too
  size->noOfAttributes += 2;  // ---"----

  size->noOfTables *= 2;      // Remove impact of Dict need 2 ids for each table

  Uint32 noOfDBNodes = size->noOfNDBNodes;
  if (noOfDBNodes > 15) {
    noOfDBNodes = 15;
  }//if
  Uint32 noOfLocalScanRecords = (noOfDBNodes * size->noOfScanRecords) + 1;
  Uint32 noOfTCScanRecords = size->noOfScanRecords;
  {
    /**
     * Acc Size Alt values
     */
    size->blockNo[ACC] = DBACC;
    
    VarSize * const acc = &(size->varSize[ACC][0]);
    
    // Can keep 65536 pages (= 0.5 GByte)
    setValue(acc, AccSizeAltReq::IND_DIR_RANGE, 
	     4 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas); 
    
    setValue(acc, AccSizeAltReq::IND_DIR_ARRAY,
	     (size->noOfIndexPages >> 8) + 
	     4 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(acc, AccSizeAltReq::IND_FRAGMENT,
	     2 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    /*-----------------------------------------------------------------------*/
    // The extra operation records added are used by the scan and node 
    // recovery process. 
    // Node recovery process will have its operations dedicated to ensure
    // that they never have a problem with allocation of the operation record.
    // The remainder are allowed for use by the scan processes.
    /*-----------------------------------------------------------------------*/
    setValue(acc, AccSizeAltReq::IND_OP_RECS,
	     size->noOfReplicas*((16 * size->noOfOperations) / 10 + 50) + 
             (noOfLocalScanRecords * MAX_PARALLEL_SCANS_PER_FRAG) +
             NODE_RECOVERY_SCAN_OP_RECORDS);

    setValue(acc, AccSizeAltReq::IND_OVERFLOW_RECS,
	     size->noOfIndexPages + 
	     2 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(acc, AccSizeAltReq::IND_PAGE8, 
	     size->noOfIndexPages + 32);
  
    setValue(acc, AccSizeAltReq::IND_ROOT_FRAG, 
	     NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(acc, AccSizeAltReq::IND_TABLE, 
	     size->noOfTables);
    
    setValue(acc, AccSizeAltReq::IND_SCAN, 
	     noOfLocalScanRecords);
  }
  
  {
    /**
     * Dict Size Alt values
     */
    size->blockNo[DICT] = DBDICT;
    
    VarSize * const dict = &(size->varSize[DICT][0]);
    
    setValue(dict, DictSizeAltReq::IND_ATTRIBUTE, 
             size->noOfAttributes);
    
    setValue(dict, DictSizeAltReq::IND_CONNECT, 
             size->noOfOperations + 32);   
    
    setValue(dict, DictSizeAltReq::IND_FRAG_CONNECT, 
	     NO_OF_FRAG_PER_NODE * size->noOfNDBNodes * size->noOfReplicas);

    setValue(dict, DictSizeAltReq::IND_TABLE, 
             size->noOfTables);
    
    setValue(dict, DictSizeAltReq::IND_TC_CONNECT, 
             2* size->noOfOperations);
  }
  
  {
    /**
     * Dih Size Alt values
     */
    size->blockNo[DIH] = DBDIH;
    
    VarSize * const dih = &(size->varSize[DIH][0]);
    
    setValue(dih, DihSizeAltReq::IND_API_CONNECT, 
             2 * size->noOfTransactions);
    
    setValue(dih, DihSizeAltReq::IND_CONNECT, 
             size->noOfOperations + 46);
    
    setValue(dih, DihSizeAltReq::IND_FRAG_CONNECT, 
	     NO_OF_FRAG_PER_NODE *  size->noOfTables *  size->noOfNDBNodes);
    
    int temp;
    temp = size->noOfReplicas - 2;
    if (temp < 0)
      temp = 1;
    else
      temp++;
    setValue(dih, DihSizeAltReq::IND_MORE_NODES, 
             temp * NO_OF_FRAG_PER_NODE *
             size->noOfTables *  size->noOfNDBNodes);
    
    setValue(dih, DihSizeAltReq::IND_REPLICAS, 
             NO_OF_FRAG_PER_NODE * size->noOfTables *
             size->noOfNDBNodes * size->noOfReplicas);

    setValue(dih, DihSizeAltReq::IND_TABLE, 
             size->noOfTables);
  }
  
  {
    /**
     * Lqh Size Alt values
     */
    size->blockNo[LQH] = DBLQH;
    
    VarSize * const lqh = &(size->varSize[LQH][0]);

    setValue(lqh, LqhSizeAltReq::IND_FRAG, 
             NO_OF_FRAG_PER_NODE * size->noOfTables * size->noOfReplicas);
    
    setValue(lqh, LqhSizeAltReq::IND_CONNECT, 
             size->noOfReplicas*((11 * size->noOfOperations) / 10 + 50));
    
    setValue(lqh, LqhSizeAltReq::IND_TABLE, 
             size->noOfTables);

    setValue(lqh, LqhSizeAltReq::IND_TC_CONNECT, 
             size->noOfReplicas*((16 * size->noOfOperations) / 10 + 50));
    
    setValue(lqh, LqhSizeAltReq::IND_REPLICAS, 
             size->noOfReplicas);

    setValue(lqh, LqhSizeAltReq::IND_LOG_FILES, 
             (4 * the_clusterData.ispValues[1][4]));

    setValue(lqh, LqhSizeAltReq::IND_SCAN, 
             noOfLocalScanRecords);

  }
  
  {
    /**
     * Tc Size Alt values
     */
    size->blockNo[TC] = DBTC;
    
    VarSize * const tc = &(size->varSize[TC][0]);
    
    setValue(tc, TcSizeAltReq::IND_API_CONNECT, 
             3 * size->noOfTransactions);
    
    setValue(tc, TcSizeAltReq::IND_TC_CONNECT, 
             size->noOfOperations + 16 + size->noOfTransactions);
    
    setValue(tc, TcSizeAltReq::IND_TABLE, 
             size->noOfTables);
    
    setValue(tc, TcSizeAltReq::IND_LOCAL_SCAN, 
             noOfLocalScanRecords);
    
    setValue(tc, TcSizeAltReq::IND_TC_SCAN, 
             noOfTCScanRecords);
  }
  
  {
    /**
     * Tup Size Alt values
     */
    size->blockNo[TUP] = DBTUP;
    
    VarSize * const tup = &(size->varSize[TUP][0]);

    setValue(tup, TupSizeAltReq::IND_DISK_PAGE_ARRAY, 
	     2 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(tup, TupSizeAltReq::IND_DISK_PAGE_REPRESENT, 
             size->noOfDiskClusters);
    
    setValue(tup, TupSizeAltReq::IND_FRAG, 
         2 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(tup, TupSizeAltReq::IND_PAGE_CLUSTER, 
             size->noOfFreeClusters);
    
    setValue(tup, TupSizeAltReq::IND_LOGIC_PAGE, 
             size->noOfDiskBufferPages + size->noOfDiskClusters);
    
    setValue(tup, TupSizeAltReq::IND_OP_RECS, 
             size->noOfReplicas*((16 * size->noOfOperations) / 10 + 50));
    
    setValue(tup, TupSizeAltReq::IND_PAGE, 
             size->noOfDataPages);
    
    setValue(tup, TupSizeAltReq::IND_PAGE_RANGE, 
	     4 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas);
    
    setValue(tup, TupSizeAltReq::IND_TABLE, 
             size->noOfTables);
    
    setValue(tup, TupSizeAltReq::IND_TABLE_DESC, 
	     4 * NO_OF_FRAG_PER_NODE * size->noOfAttributes* size->noOfReplicas +
	     12 * NO_OF_FRAG_PER_NODE * size->noOfTables* size->noOfReplicas );
    
    setValue(tup, TupSizeAltReq::IND_DELETED_BLOCKS, 
             size->noOfFreeClusters);

    setValue(tup, TupSizeAltReq::IND_STORED_PROC,
             noOfLocalScanRecords);
  }

  {
    /**
     * Tux Size Alt values
     */
    size->blockNo[TUX] = DBTUX;
    
    VarSize * const tux = &(size->varSize[TUX][0]);
    
    setValue(tux, TuxSizeAltReq::IND_INDEX, 
             size->noOfTables);
    
    setValue(tux, TuxSizeAltReq::IND_FRAGMENT, 
         2 * NO_OF_FRAG_PER_NODE * size->noOfTables * size->noOfReplicas);
    
    setValue(tux, TuxSizeAltReq::IND_ATTRIBUTE, 
             size->noOfIndexes * 4);
    
    setValue(tux, TuxSizeAltReq::IND_SCAN, 
	     noOfLocalScanRecords);
  }
}
  
const ClusterConfiguration::ClusterData&
ClusterConfiguration::clusterData() const
{
  return the_clusterData;
}

void ClusterConfiguration::init(const Properties & p, const Properties & db){
  const char * msg = "Invalid configuration fetched";

  ClusterData & cd = the_clusterData;
  
  struct AttribStorage { const char * attrib; Uint32 * storage; };
  AttribStorage tmp[] = {
    {"MaxNoOfConcurrentScans", &cd.SizeAltData.noOfScanRecords },
    {"MaxNoOfTables", &cd.SizeAltData.noOfTables },
    {"MaxNoOfIndexes", &cd.SizeAltData.noOfIndexes },
    {"NoOfReplicas", &cd.SizeAltData.noOfReplicas },
    {"MaxNoOfAttributes", &cd.SizeAltData.noOfAttributes },
    {"MaxNoOfConcurrentOperations", &cd.SizeAltData.noOfOperations },
    {"MaxNoOfConcurrentTransactions", &cd.SizeAltData.noOfTransactions },
    {"NoOfIndexPages", &cd.SizeAltData.noOfIndexPages },
    {"NoOfDataPages",  &cd.SizeAltData.noOfDataPages },
    {"NoOfDiskBufferPages", &cd.SizeAltData.noOfDiskBufferPages },
    {"NoOfDiskClusters", &cd.SizeAltData.noOfDiskClusters },
    {"NoOfFreeDiskClusters", &cd.SizeAltData.noOfFreeClusters },
    {"TimeToWaitAlive", &cd.ispValues[0][0] },
    {"HeartbeatIntervalDbDb", &cd.ispValues[0][2] },
    {"HeartbeatIntervalDbApi", &cd.ispValues[0][3] },
    {"ArbitrationTimeout", &cd.ispValues[0][5] },
    {"TimeBetweenLocalCheckpoints", &cd.ispValues[1][2] },
    {"NoOfFragmentLogFiles", &cd.ispValues[1][4] },
    {"MaxNoOfConcurrentScans", &cd.SizeAltData.noOfScanRecords },
    {"NoOfConcurrentCheckpointsDuringRestart", &cd.ispValues[1][5] },
    {"TransactionDeadlockDetectionTimeout", &cd.ispValues[1][6] },
    {"NoOfConcurrentProcessesHandleTakeover", &cd.ispValues[1][7] },
    {"TimeBetweenGlobalCheckpoints", &cd.ispValues[2][3] },
    {"NoOfConcurrentCheckpointsAfterRestart", &cd.ispValues[2][4] },
    {"TransactionInactiveTimeout", &cd.ispValues[2][7] },
    {"NoOfDiskPagesToDiskDuringRestartTUP", &cd.ispValues[3][8] },
    {"NoOfDiskPagesToDiskAfterRestartTUP", &cd.ispValues[3][9] },
    {"NoOfDiskPagesToDiskDuringRestartACC", &cd.ispValues[3][10] },
    {"NoOfDiskPagesToDiskAfterRestartACC", &cd.ispValues[3][11] },
    {"NoOfDiskClustersPerDiskFile", &cd.ispValues[4][8] },
    {"NoOfDiskFiles", &cd.ispValues[4][9] },
    {"NoOfReplicas", &cd.ispValues[2][2] }
  };


  const int sz = sizeof(tmp)/sizeof(AttribStorage);
  for(int i = 0; i<sz; i++){
    if(!db.get(tmp[i].attrib, tmp[i].storage)){
      char buf[255];
      BaseString::snprintf(buf, sizeof(buf), "%s not found", tmp[i].attrib);
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
    }
  }

  if(!p.get("NoOfNodes", &cd.SizeAltData.noOfNodes)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "NoOfNodes missing");
  }
  
  Properties::Iterator it(&p);
  const char * name = 0;
  Uint32 nodeNo = 0;
  for(name = it.first(); name != NULL; name = it.next()){
    if(strncmp(name, "Node_", strlen("Node_")) == 0){

      Uint32 nodeId;
      const char * nodeType;
      const Properties * node;
      
      if(!p.get(name, &node)){
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "Node data missing");
      }
      
      if(!node->get("Id", &nodeId)){
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "Node data (Id) missing");
      }
      
      if(!node->get("Type", &nodeType)){
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "Node data (Type) missing");
      }
      
      if(nodeId > MAX_NODES){
	char buf[255];
	snprintf(buf, sizeof(buf),
		 "Maximum DB node id allowed is: %d", MAX_NDB_NODES);
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
      }
      
      if(nodeId == 0){
	char buf[255];
	snprintf(buf, sizeof(buf),
		 "Minimum node id allowed in the cluster is: 1");
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
      }

      for(unsigned j = 0; j<nodeNo; j++){
	if(cd.nodeData[j].nodeId == nodeId){
	  char buf[255];
	  BaseString::snprintf(buf, sizeof(buf), "Two node can not have the same node id");
	  ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
	}
      }
      
      {
	for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++){
	  Uint32 logLevel;
	  if(db.get(LogLevel::LOGLEVEL_CATEGORY_NAME[j].name, &logLevel)){
	    cd.SizeAltData.logLevel.setLogLevel((LogLevel::EventCategory)j, 
						logLevel);
	  }
	}
      }
      
      cd.nodeData[nodeNo].nodeId = nodeId;
      const char* tmpApiMgmProperties = 0;
      if(strcmp("DB", nodeType) == 0){
	cd.nodeData[nodeNo].nodeType = NodeInfo::DB;
	cd.SizeAltData.noOfNDBNodes++; // No of NDB processes
	
	if(nodeId > MAX_NDB_NODES){
	  char buf[255];
	  BaseString::snprintf(buf, sizeof(buf), "Maximum node id for a ndb node is: %d", MAX_NDB_NODES);
	  ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
	}
	if(cd.SizeAltData.noOfNDBNodes > MAX_NDB_NODES){
	  char buf[255];
	  BaseString::snprintf(buf, sizeof(buf),
		   "Maximum %d ndb nodes is allowed in the cluster", 
		  MAX_NDB_NODES);
	  ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
	}
      } else if(strcmp("API", nodeType) == 0){
	cd.nodeData[nodeNo].nodeType = NodeInfo::API;
	cd.SizeAltData.noOfAPINodes++; // No of API processes
	tmpApiMgmProperties = "API";
      } else if(strcmp("REP", nodeType) == 0){
	cd.nodeData[nodeNo].nodeType = NodeInfo::REP;
	//cd.SizeAltData.noOfAPINodes++; // No of API processes
	tmpApiMgmProperties = "REP";
      } else if(strcmp("MGM", nodeType) == 0){
	cd.nodeData[nodeNo].nodeType = NodeInfo::MGM;
	cd.SizeAltData.noOfMGMNodes++; // No of MGM processes
	tmpApiMgmProperties = "MGM";
      } else {
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, 
		  "Invalid configuration: Unknown node type",
		  nodeType);
      }
      
      if (tmpApiMgmProperties) {
	/*
	  const Properties* q = 0;
	  
	  if (!p.get(tmpApiMgmProperties, nodeId, &q)) {
	  ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, tmpApiMgmProperties);
	  } else {
	*/
        Uint32 rank = 0;
        if (node->get("ArbitrationRank", &rank) && rank > 0) {
          cd.nodeData[nodeNo].arbitRank = rank;
	  //        }
	}
      } else {
	cd.nodeData[nodeNo].arbitRank = 0;
      }
      
      nodeNo++;
    }
  } 
  cd.SizeAltData.exist = true;
  calcSizeAlteration();  
}


