/*
   Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */


#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <mysqld_error.h>

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <HugoOperations.hpp>
#include <../../src/ndbapi/NdbApiSignal.hpp>
#include <kernel/signaldata/ScanTab.hpp>
#include <kernel/signaldata/QueryTree.hpp>
#include <kernel/AttributeHeader.hpp>


typedef uchar* gptr;

static int _scan = 0;
static const char* _dbname = "TEST_DB";

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("spj_test"),
  { "database", 'd', "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "scan", 's', "Table scan followed by key lookup",
    (uchar**) &_scan, (uchar**) &_scan, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

class NdbScanFilterImpl
{
public:
  

  static void set(NdbScanOperation* op, const Uint32 * src, Uint32 len) {
    op->theTotalCurrAI_Len = 0;
    op->attrInfoRemain = 0;
    op->theFirstATTRINFO = 0;
    op->insertATTRINFOData_NdbRecord((const char*)src, 4*len);
  }
  static void setIsLinkedFlag(NdbScanOperation* op){
    ScanTabReq * req = (ScanTabReq*)(op->theSCAN_TABREQ->getDataPtrSend());
    ScanTabReq::setViaSPJFlag(req->requestInfo, 1);
  }
};


/**
 * SQL:
 drop table if exists t1;
 create table t1 (a int primary key, b int not null) engine = ndb;
 insert into t1 values (1,2), (2,3), (3,1);
*/


int main(int argc, char** argv)
{
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

  Ndb_cluster_connection con(opt_ndb_connectstring);
  if(con.connect(12, 5, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  int res = con.wait_until_ready(30,30);
  if (res != 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  const NdbDictionary::Dictionary * dict= MyNdb.getDictionary();
  const NdbDictionary::Table * pTab = dict->getTable(argv[0]);
  if (pTab == 0)
  {
    ndbout_c("Failed to retreive table: \"%s\"", argv[0]);
    exit(0);
  }
  else
  {
    ndbout_c("Retreived %s", argv[0]);
  }

  const NdbDictionary::Index * pIdx = dict->getIndex("PRIMARY", argv[0]);
  if (pIdx == 0)
  {
    ndbout_c("Failed to retreive index PRIMARY for table: \"%s\"", argv[0]);
    exit(0);
  }
  else
  {
    ndbout_c("Retreived index PRIMARY for table %s", argv[0]);
  }

  NdbTransaction * pTrans = MyNdb.startTransaction();
  NdbScanOperation * pOp = pTrans->scanTable(pTab->getDefaultRecord(), 
                                             NdbOperation::LM_CommittedRead);

  bool scanindexchild = false;
#if 0
  /**
     select STRAIGHT_JOIN *
     from t1 join t1 as t2 
     where t2.a = t1.b and t1.b <= 100 and t2.b <= 3;
   *
   * - ScanFrag
   * PI_ATTR_INTERPRET w/ values inlined
   * - Lookup
   * PI_ATTR_INTERPRET w/ values in subroutine section
   */
  Uint32 request[] = {
    // pos: 0
    0x000d0002,

    // ScanFragNode
    0x00050002, // type/len
    0x00000010, // bits
    0x00000007, // table id
    0x00000001, // table version
    0x00010001, // #cnt linked / [ attr-list ]

    // LookupNode
    0x00070001, // type/len
    0x00000003, // bits
    0x00000007, // table id
    0x00000001, // table version
    0x00000001, // parent list
    0x00000001, // key pattern: #parameters/#len
    QueryPattern::col(0), // P_COL col = 0

    // ScanFragParameters
    0x000c0002, // type/len
    0x00000009, // bits
    0x10000018, // result data
    0x00000005, // #len subroutine / #len interpreted program
    0x00043017, // p0: BRANCH_ATTR_OP_COL | LE | OFFSET-JUMP
    0x00010004, // p0: ATTRID / LEN of VALUE
    0x00000064, // p1: VALUE (100)
    0x00000012, // p2: EXIT_OK
    0x03830013, // p3: EXIT_NOK
    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000, // read any value
    
    // LookupParameters
    0x000d0001, // type/len
    0x00000009, // bits
    0x1000001c, // result data

    0x00020004, // #len subroutine / #len interpreted program
    0x0003301a, // p0: BRANCH_ATTR_OP_COL2 | LE | OFFSET-JUMP
    0x00010000, // p0: attrid: 1, param ref 0
    0x00000012, // p1: EXIT_OK
    0x03830013, // p2: EXIT_NOK
    0x00000004, // param 0 header
    0x00000003, // param 0 value (3)

    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000  // read any value
  };
#elif 0
  /**
   * EXECUTE ?1 = 3
   *   select STRAIGHT_JOIN *
   *   from t1 join t1 as t2 
   *   where t2.a = t1.b and t1.b <= 100 and t2.b <= ?1;
   *
   * - ScanFrag
   * PI_ATTR_INTERPRET w/ values inlined
   * - Lookup
   * NI_ATTR_INTERPRET
   * NI_ATTR_PARAMS & PI_ATTR_PARAMS
   */
  Uint32 request[] = {
    // pos: 0
    0x000d0002,

    // ScanFragNode
    0x00050002, // type/len
    0x00000010, // bits
    0x00000007, // table id
    0x00000001, // table version
    0x00010001, // #cnt linked / [ attr-list ]

    // LookupNode
    0x000e0001, // type/len
    DABits::NI_HAS_PARENT | DABits::NI_KEY_LINKED |
    DABits::NI_ATTR_INTERPRET | DABits::NI_ATTR_PARAMS,
    0x00000007, // table id
    0x00000001, // table version
    0x00000001, // parent list
    0x00000001, // key pattern: #parameters/#len
    QueryPattern::col(0), // P_COL col = 0
    0x00010004, // attrinfo pattern: #len-pattern / #len interpreted program
    0x0003301a, // p0: BRANCH_ATTR_OP_COL_2 | LE | OFFSET-JUMP
    0x00010000, // p0: attrid: 1 / program param 0
    0x00000012, // p1: EXIT_OK
    0x03830013, // p2: EXIT_NOK
    0x00000001, // attr-param pattern: #parameters
    QueryPattern::paramHeader(0), // P_PARAM_WITH_HEADER col=0

    // ScanFragParameters
    0x000c0002, // type/len
    0x00000009, // bits
    0x10000018, // result data
    0x00000005, // #len subroutine / #len interpreted program
    0x00043017, // p0: BRANCH_ATTR_OP_COL | LE | OFFSET-JUMP
    0x00010004, // p1: ATTRID / LEN of VALUE
    0x00000064, // p2: VALUE (100)
    0x00000012, // p3: EXIT_OK
    0x03830013, // p4: EXIT_NOK
    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000, // read any value
        
    // LookupParameters
    0x00080001, // type/len
    DABits::PI_ATTR_LIST | DABits::PI_ATTR_PARAMS, // bits
    0x1000001c, // result data
    0x00000004, // Param 0 header
    0x00000003, // Param 0 value
    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000  // read any value
  };
#elif 0
  /**
   *
   * select STRAIGHT_JOIN *
   * from t1 join t1 as t2 
   * where t2.a = t1.b and t1.b <= 100 and t2.b <= t1.b;
   *
   * - ScanFrag
   * PI_ATTR_INTERPRET w/ values inlined
   * - Lookup
   * NI_ATTR_INTERPRET
   * NI_ATTR_LINKED
   */
  Uint32 request[] = {
    // pos: 0
    0x000d0002,

    // ScanFragNode
    0x00050002, // type/len
    0x00000010, // bits
    0x00000007, // table id
    0x00000001, // table version
    0x00010001, // #cnt linked / [ attr-list ]

    // LookupNode
    0x000e0001, // type/len
    DABits::NI_HAS_PARENT | DABits::NI_KEY_LINKED |
    DABits::NI_ATTR_INTERPRET | DABits::NI_ATTR_LINKED,
    0x00000007, // table id
    0x00000001, // table version
    0x00000001, // parent list
    0x00000001, // key pattern: #parameters/#len
    QueryPattern::col(0), // P_COL col = 0
    0x00010004, // attrinfo pattern: #len-pattern / #len interpreted program
    0x0003301a, // p0: BRANCH_ATTR_OP_COL_2 | LE | OFFSET-JUMP
    0x00010000, // p0: attrid: 1 / program param 0
    0x00000012, // p1: EXIT_OK
    0x03830013, // p2: EXIT_NOK
    0x00000000, // attr-param pattern: #parameters
    QueryPattern::attrInfo(0), // attr-param pattern: P_ATTRINFO col=0

    // ScanFragParameters
    0x000c0002, // type/len
    0x00000009, // bits
    0x10000018, // result data
    0x00000005, // #len subroutine / #len interpreted program
    0x00043017, // p0: BRANCH_ATTR_OP_COL | LE | OFFSET-JUMP
    0x00010004, // p1: ATTRID / LEN of VALUE
    0x00000064, // p2: VALUE (100)
    0x00000012, // p3: EXIT_OK
    0x03830013, // p4: EXIT_NOK
    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000, // read any value
        
    // LookupParameters
    0x00060001, // type/len
    DABits::PI_ATTR_LIST, // bits
    0x1000001c, // result data
    0x00000002, // len user projection
    0xfff00002, // read all
    0xffe90000  // read any value
  };
#else
  /**
     select STRAIGHT_JOIN *
     from t1 join t1 as t2 on t2.a >= t1.b;
  */

  scanindexchild = true;
  Uint32 request[] = {
    // pos: 0
    0x000d0002, 

    // pos: 1 ScanFragNode
    0x00050002, // len-type
    DABits::NI_LINKED_ATTR, // bits
    0x0000000c, // table id
    0x00000001, // table version
    0x00010001, // #cnt, linked attr

    // pos: 6 ScanIndexNode
    0x00090003, // type len
    DABits::NI_HAS_PARENT | DABits::NI_KEY_LINKED, // bits
    0x0000000b, // table id
    0x00000001, // table version
    0x00000001, // parent list
    0x00000003, // key pattern (cnt/len)
    QueryPattern::data(1), // P_DATA len = 1
    0x00000002, // BoundLE
    QueryPattern::attrInfo(0), // P_ATTRINFO col = 0

    // pos: 15 ScanFragParameters
    0x00080002, // type len
    0x00000009, // bits
    0x10000020, // result data
    0x00000001, // param/len interpret program
    0x00000012, // p1 = exit ok
    0x00000002, // len user projection
    0xfff00002, // up 1 - read all
    0xffe90000, // up 2 - read any value

    // pos: 23 ScanIndexParameters
    0x000a0003, // type/len
    0x00020009, // bits
    0xffff0100, // batch size
    0x10000024, // result data
    0x00000001, // param/len interpret program
    0x00000012, // p1 = exit ok
    0x00000003, // len user projection
    0xfff00002, // up 1 - read all
    0xffe90000, // up 2 - read any value
    0xfffb0000
  };
#endif

  Uint32 n0 = (request[1] >> 16);
  Uint32 n1 = (request[1 + n0] >> 16);
  request[0] = ((1 + n0 + n1) << 16) | 2;

  request[1+2] = pTab->getObjectId();
  request[1+3] = pTab->getObjectVersion();

  if (scanindexchild == false)
  {
    request[1 + n0 + 2] = pTab->getObjectId();
    request[1 + n0 + 3] = pTab->getObjectVersion();
  }
  else
  {
    request[1 + n0 + 2] = pIdx->getObjectId();
    request[1 + n0 + 3] = pIdx->getObjectVersion();
  }

  NdbScanFilterImpl::setIsLinkedFlag(pOp);
  NdbScanFilterImpl::set(pOp, request, NDB_ARRAY_SIZE(request));
                         
  pTrans->execute(NoCommit);
  while (true) NdbSleep_SecSleep(1);

  return NDBT_ProgramExit(NDBT_OK);
}

