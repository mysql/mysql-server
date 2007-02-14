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



#define DBACC_C
#include "Dbacc.hpp"

#define DEBUG(x) { ndbout << "ACC::" << x << endl; }

void Dbacc::initData() 
{
  cdirarraysize = ZDIRARRAY;
  coprecsize = ZOPRECSIZE;
  cpagesize = ZPAGESIZE;
  ctablesize = ZTABLESIZE;
  cfragmentsize = ZFRAGMENTSIZE;
  cdirrangesize = ZDIRRANGESIZE;
  coverflowrecsize = ZOVERFLOWRECSIZE;
  cscanRecSize = ZSCAN_REC_SIZE;

  
  dirRange = 0;
  directoryarray = 0;
  fragmentrec = 0;
  operationrec = 0;
  overflowRecord = 0;
  page8 = 0;
  scanRec = 0;
  tabrec = 0;

  cnoOfAllocatedPages = cpagesize = 0;
  // Records with constant sizes
}//Dbacc::initData()

void Dbacc::initRecords() 
{
  // Records with dynamic sizes
  page8 = (Page8*)allocRecord("Page8",
			      sizeof(Page8), 
			      cpagesize,
			      false,
            CFG_DB_INDEX_MEM);

  operationrec = (Operationrec*)allocRecord("Operationrec",
					    sizeof(Operationrec),
					    coprecsize);

  dirRange = (DirRange*)allocRecord("DirRange",
				    sizeof(DirRange), 
				    cdirrangesize);

  directoryarray = (Directoryarray*)allocRecord("Directoryarray",
						sizeof(Directoryarray), 
						cdirarraysize);

  fragmentrec = (Fragmentrec*)allocRecord("Fragmentrec",
					  sizeof(Fragmentrec), 
					  cfragmentsize);

  overflowRecord = (OverflowRecord*)allocRecord("OverflowRecord",
						sizeof(OverflowRecord),
						coverflowrecsize);

  scanRec = (ScanRec*)allocRecord("ScanRec",
				  sizeof(ScanRec), 
				  cscanRecSize);

  tabrec = (Tabrec*)allocRecord("Tabrec",
				sizeof(Tabrec),
				ctablesize);
}//Dbacc::initRecords()

Dbacc::Dbacc(Block_context& ctx):
  SimulatedBlock(DBACC, ctx),
  c_tup(0)
{
  BLOCK_CONSTRUCTOR(Dbacc);

  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbacc::execDUMP_STATE_ORD);
  addRecSignal(GSN_DEBUG_SIG, &Dbacc::execDEBUG_SIG);
  addRecSignal(GSN_CONTINUEB, &Dbacc::execCONTINUEB);
  addRecSignal(GSN_ACC_CHECK_SCAN, &Dbacc::execACC_CHECK_SCAN);
  addRecSignal(GSN_EXPANDCHECK2, &Dbacc::execEXPANDCHECK2);
  addRecSignal(GSN_SHRINKCHECK2, &Dbacc::execSHRINKCHECK2);
  addRecSignal(GSN_READ_PSEUDO_REQ, &Dbacc::execREAD_PSEUDO_REQ);

  // Received signals
  addRecSignal(GSN_STTOR, &Dbacc::execSTTOR);
  addRecSignal(GSN_ACCKEYREQ, &Dbacc::execACCKEYREQ);
  addRecSignal(GSN_ACCSEIZEREQ, &Dbacc::execACCSEIZEREQ);
  addRecSignal(GSN_ACCFRAGREQ, &Dbacc::execACCFRAGREQ);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbacc::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_ABORTREQ, &Dbacc::execACC_ABORTREQ);
  addRecSignal(GSN_ACC_SCANREQ, &Dbacc::execACC_SCANREQ);
  addRecSignal(GSN_ACCMINUPDATE, &Dbacc::execACCMINUPDATE);
  addRecSignal(GSN_ACC_COMMITREQ, &Dbacc::execACC_COMMITREQ);
  addRecSignal(GSN_ACC_TO_REQ, &Dbacc::execACC_TO_REQ);
  addRecSignal(GSN_ACC_LOCKREQ, &Dbacc::execACC_LOCKREQ);
  addRecSignal(GSN_NDB_STTOR, &Dbacc::execNDB_STTOR);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbacc::execDROP_TAB_REQ);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbacc::execREAD_CONFIG_REQ, true);

  initData();

#ifdef VM_TRACE
  {
    void* tmp[] = { &expDirRangePtr,
		    &gnsDirRangePtr,
		    &newDirRangePtr,
		    &rdDirRangePtr,
		    &nciOverflowrangeptr,
                    &expDirptr,
                    &rdDirptr,
                    &sdDirptr,
                    &nciOverflowDirptr,
                    &fragrecptr,
                    &operationRecPtr,
                    &idrOperationRecPtr,
                    &mlpqOperPtr,
                    &queOperPtr,
                    &readWriteOpPtr,
                    &iopOverflowRecPtr,
                    &tfoOverflowRecPtr,
                    &porOverflowRecPtr,
                    &priOverflowRecPtr,
                    &rorOverflowRecPtr,
                    &sorOverflowRecPtr,
                    &troOverflowRecPtr,
                    &ancPageptr,
                    &colPageptr,
                    &ccoPageptr,
                    &datapageptr,
                    &delPageptr,
                    &excPageptr,
                    &expPageptr,
                    &gdiPageptr,
                    &gePageptr,
                    &gflPageptr,
                    &idrPageptr,
                    &ilcPageptr,
                    &inpPageptr,
                    &iopPageptr,
                    &lastPageptr,
                    &lastPrevpageptr,
                    &lcnPageptr,
                    &lcnCopyPageptr,
                    &lupPageptr,
                    &ciPageidptr,
                    &gsePageidptr,
                    &isoPageptr,
                    &nciPageidptr,
                    &rsbPageidptr,
                    &rscPageidptr,
                    &slPageidptr,
                    &sscPageidptr,
                    &rlPageptr,
                    &rlpPageptr,
                    &ropPageptr,
                    &rpPageptr,
                    &slPageptr,
                    &spPageptr,
                    &scanPtr,
                    &tabptr
    };
    init_globals_list(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
}//Dbacc::Dbacc()

Dbacc::~Dbacc() 
{
  deallocRecord((void **)&dirRange, "DirRange",
		sizeof(DirRange), 
		cdirrangesize);
  
  deallocRecord((void **)&directoryarray, "Directoryarray",
		sizeof(Directoryarray), 
		cdirarraysize);
  
  deallocRecord((void **)&fragmentrec, "Fragmentrec",
		sizeof(Fragmentrec), 
		cfragmentsize);
  
  deallocRecord((void **)&operationrec, "Operationrec",
		sizeof(Operationrec),
		coprecsize);
  
  deallocRecord((void **)&overflowRecord, "OverflowRecord",
		sizeof(OverflowRecord),
		coverflowrecsize);

  deallocRecord((void **)&page8, "Page8",
		sizeof(Page8), 
		cpagesize);
  
  deallocRecord((void **)&scanRec, "ScanRec",
		sizeof(ScanRec), 
		cscanRecSize);
  
  deallocRecord((void **)&tabrec, "Tabrec",
		sizeof(Tabrec),
		ctablesize);
  }//Dbacc::~Dbacc()

BLOCK_FUNCTIONS(Dbacc)
