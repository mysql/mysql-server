/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ClusterConfiguration_H
#define ClusterConfiguration_H

#include <kernel_types.h>
#include <ndb_limits.h>
#include <Properties.hpp>
#include <ErrorReporter.hpp>
#include <signaldata/CmvmiCfgConf.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <NodeInfo.hpp>

#define JAM_FILE_ID 284


// MaxNumber of sizealteration records in each block
// MaxNumber of blocks with sizealteration, (size of array)
#define MAX_SIZEALT_RECORD 16    
#define MAX_SIZEALT_BLOCKS 8     

enum NdbBlockName { ACC = 0, DICT, DIH, LQH, TC, TUP, TUX, NDB_SIZEALT_OFF };
//  NDB_SIZEALT_OFF is used for block without sizealteration
//  IMPORTANT to assign NDB_SIZEALT_OFF as largest value

struct VarSize {
  int   nrr;
  bool   valid;
};

struct SizeAlt { 
  unsigned int   noOfTables;
  unsigned int   noOfIndexes;
  unsigned int   noOfReplicas;
  unsigned int   noOfNDBNodes;
  unsigned int   noOfAPINodes;
  unsigned int   noOfMGMNodes;
  unsigned int   noOfNodes;
  unsigned int   noOfDiskLessNodes;
  unsigned int   noOfAttributes;
  unsigned int   noOfOperations;
  unsigned int   noOfTransactions;
  unsigned int   noOfIndexPages;
  unsigned int   noOfDataPages;
  unsigned int   noOfDiskBufferPages;
  unsigned int   noOfFreeClusters;
  unsigned int   noOfDiskClusters;
  unsigned int   noOfScanRecords;
  bool       exist;
  VarSize    varSize[MAX_SIZEALT_BLOCKS][MAX_SIZEALT_RECORD];
  unsigned short blockNo[MAX_SIZEALT_BLOCKS];
  LogLevel logLevel;
};


class ClusterConfiguration
{
public:
  
  struct NodeData {
    NodeData() { 
      nodeId = MAX_NODES+1;
      nodeType = NodeInfo::INVALID;
      arbitRank = ~0;
    }
    NodeId nodeId;
    NodeInfo::NodeType nodeType;
    unsigned arbitRank;
  };
  
  struct ClusterData
  {    
    SizeAlt  SizeAltData;
    NodeData nodeData[MAX_NODES];
    Uint32   ispValues[5][CmvmiCfgConf::NO_OF_WORDS];
  };
  
  ClusterConfiguration();
  ~ClusterConfiguration();
  const ClusterData& clusterData() const;
  
  void init(const Properties & p, const Properties & db);
protected:

private:

  ClusterData the_clusterData;

  void calcSizeAlteration();

};


#undef JAM_FILE_ID

#endif // ClusterConfiguration_H

