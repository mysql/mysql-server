/*
   Copyright (c) 2012 Oracle and/or its affiliates. All rights reserved.

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

#include "../include/ndbapi/NdbApi.hpp"
#include "ndbapi/NdbInfo.hpp"
#include "../include/util/ndb_opts.h"
#include "../include/util/random.h"
#include "../include/util/ndb_rand.h"
#include "../include/portlib/NdbDir.hpp"
#include "../include/util/Bitmask.hpp"

#include <stdio.h>

int main(int argc, const char**)
{
  if (argc < 0)
  {
    /*
      NOTE! This code should not run, it's sole purpose is
      to check that the public functions of the NdbApi etc. can be
      found in the ndbclient libraries
    */
    (void)ndb_init();
    Ndb_cluster_connection cluster_con;
    Ndb ndb(&cluster_con);
    NdbDictionary::Table tab("");
    NdbDictionary::Index idx("");
    NdbTransaction* trans = ndb.startTransaction();
    NdbOperation* op = trans->getNdbOperation(&tab);
    NdbRecAttr* rec_attr = op->getValue("");
    rec_attr->isNULL();
    NdbScanOperation* sop = trans->getNdbScanOperation(&tab);
    sop->readTuples();
    NdbIndexScanOperation* isop = trans->getNdbIndexScanOperation(&idx);
    isop->get_range_no();
    NdbIndexOperation* iop = trans->getNdbIndexOperation(&idx);
    iop->insertTuple();
    NdbScanFilter scan_filter(op);
    NdbIndexStat index_stat;
    NdbInterpretedCode interpreted_code;
    NdbEventOperation* eop = ndb.createEventOperation("");
    eop->isConsistent();
    NdbBlob* blob = op->getBlobHandle("");
    blob->truncate();
    NdbInfo info(&cluster_con, "");
    ndb_std_print_version();
    (void)myRandom48(0);
    (void)ndb_rand_r(0);
    (void)NdbDir::u_rwx();
    (void)BitmaskImpl::getField(0, 0, 0, 64, 0);
    ndb_end(0);
  }
}
