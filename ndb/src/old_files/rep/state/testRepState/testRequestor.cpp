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

#include "testRequestor.hpp"

#define TEST_REQUIRE(X);  if (!(X)) { \
  ndbout_c("Test failed in line %d", __LINE__); testPassed = false; }


struct Result {
  Uint32 nodeGrp;
  Uint32 first;
  Uint32 last;
  Uint32 force;
};
Result result;

/** Callbacks ****************************************************************/

void 
f_transfer(void *, Signal* signal, Uint32 nodeGrp, Uint32 first, Uint32 last) 
{
  result.nodeGrp = nodeGrp;
  result.first = first;
  result.last = last;
  result.force = 0;
  ndbout_c("Transfer: %d:[%d-%d] ", nodeGrp, first, last);
}

void 
f_apply(void *, Signal* signal, Uint32 nodeGrp, 
	Uint32 first, Uint32 last, Uint32 force)
{
  result.nodeGrp = nodeGrp;
  result.first = first;
  result.last = last;
  result.force = force;
  ndbout_c("Apply: %d:[%d-%d] (Force:%d)", nodeGrp, first, last, force);
}

void 
f_deletePS(void *, Signal* signal, Uint32 nodeGrp, Uint32 first, Uint32 last) 
{
  result.nodeGrp = nodeGrp;
  result.first = first;
  result.last = last;
  result.force = 0;
  ndbout_c("DeletePS: %d:[%d-%d] ", nodeGrp, first, last);
}

void 
f_deleteSS(void *, Signal* signal, Uint32 nodeGrp, Uint32 first, Uint32 last) 
{
  result.nodeGrp = nodeGrp;
  result.first = first;
  result.last = last;
  result.force = 0;
  ndbout_c("DeleteSS: %d:[%d-%d] ", nodeGrp, first, last);
}

void
requestStartMetaLog(void * cbObj, Signal * signal) 
{ 
  ndbout_c("StartMetaLog:");
}

void
requestStartDataLog(void * cbObj, Signal * signal) 
{ 
  ndbout_c("StartDataLog:");
}

void 
requestStartMetaScan(void * cbObj, Signal* signal) 
{
  ndbout_c("StartMetaScan:");
}

void 
requestStartDataScan(void * cbObj, Signal* signal) 
{
  ndbout_c("StartDataScan:");
}


/** Compare ****************************************************************/

bool compare(Uint32 nodeGrp, Uint32 first, Uint32 last, Uint32 force) 
{
  return (result.nodeGrp == nodeGrp && result.first == first &&
	  result.last == last && result.force == force);
}


/** Main *******************************************************************/

void
testRequestor() 
{
  Signal * signal;
  bool testPassed = true;

  Requestor requestor;
  requestor.setObject(0);
  requestor.setIntervalRequests(&f_transfer, 
				&f_apply, 
				&f_deletePS, 
				&f_deleteSS);
  requestor.setStartRequests(&requestStartMetaLog,
			     &requestStartDataLog,
			     &requestStartMetaScan,
			     &requestStartDataScan);
  requestor.setNoOfNodeGroups(1);
  requestor.enable();
  requestor.enableTransfer();
  requestor.enableDelete();
  requestor.enableApply();
  requestor.m_state = Requestor::LOG;

  requestor.printStatus();

  /**
   *  First transfer
   */
  Interval i(12,13);
  requestor.add(RepState::PS, 0, i);
  requestor.execute(signal);
  TEST_REQUIRE(compare(0, 12, 13, 0));

  requestor.printStatus();

  /**
   *  State transtion test
   */

  /**
   *  First apply
   */

  /**
   *  Test end
   */
  if (testPassed) {
    ndbout << "Test passed!" << endl;
  } else {
    ndbout << "Test FAILED!" << endl;
  }
}

int
main () {
  testRequestor();
}
