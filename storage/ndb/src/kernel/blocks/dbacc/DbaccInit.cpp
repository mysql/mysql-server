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



#define DBACC_C
#include "Dbacc.hpp"

#define DEBUG(x) { ndbout << "ACC::" << x << endl; }

void Dbacc::initData() 
{
  cdirarraysize = ZDIRARRAY;
  coprecsize = ZOPRECSIZE;
  cpagesize = ZPAGESIZE;
  clcpConnectsize = ZLCP_CONNECTSIZE;
  ctablesize = ZTABLESIZE;
  cfragmentsize = ZFRAGMENTSIZE;
  crootfragmentsize = ZROOTFRAGMENTSIZE;
  cdirrangesize = ZDIRRANGESIZE;
  coverflowrecsize = ZOVERFLOWRECSIZE;
  cfsConnectsize = ZFS_CONNECTSIZE;
  cfsOpsize = ZFS_OPSIZE;
  cscanRecSize = ZSCAN_REC_SIZE;
  csrVersionRecSize = ZSR_VERSION_REC_SIZE;

  
  dirRange = 0;
  directoryarray = 0;
  fragmentrec = 0;
  fsConnectrec = 0;
  fsOprec = 0;
  lcpConnectrec = 0;
  operationrec = 0;
  overflowRecord = 0;
  page8 = 0;
  rootfragmentrec = 0;
  scanRec = 0;
  srVersionRec = 0;
  tabrec = 0;
  undopage = 0;

  // Records with constant sizes
}//Dbacc::initData()

void Dbacc::initRecords() 
{
  // Records with dynamic sizes
  page8 = (Page8*)allocRecord("Page8",
			      sizeof(Page8), 
			      cpagesize,
			      false);

  operationrec = (Operationrec*)allocRecord("Operationrec",
					    sizeof(Operationrec),
					    coprecsize);

  dirRange = (DirRange*)allocRecord("DirRange",
				    sizeof(DirRange), 
				    cdirrangesize);

  undopage = (Undopage*)allocRecord("Undopage",
				    sizeof(Undopage), 
				    cundopagesize,
				    false);
  
  directoryarray = (Directoryarray*)allocRecord("Directoryarray",
						sizeof(Directoryarray), 
						cdirarraysize);

  fragmentrec = (Fragmentrec*)allocRecord("Fragmentrec",
					  sizeof(Fragmentrec), 
					  cfragmentsize);

  fsConnectrec = (FsConnectrec*)allocRecord("FsConnectrec",
					    sizeof(FsConnectrec), 
					    cfsConnectsize);

  fsOprec = (FsOprec*)allocRecord("FsOprec",
				  sizeof(FsOprec), 
				  cfsOpsize);

  lcpConnectrec = (LcpConnectrec*)allocRecord("LcpConnectrec",
					      sizeof(LcpConnectrec),
					      clcpConnectsize);

  overflowRecord = (OverflowRecord*)allocRecord("OverflowRecord",
						sizeof(OverflowRecord),
						coverflowrecsize);

  rootfragmentrec = (Rootfragmentrec*)allocRecord("Rootfragmentrec",
						  sizeof(Rootfragmentrec), 
						  crootfragmentsize);

  scanRec = (ScanRec*)allocRecord("ScanRec",
				  sizeof(ScanRec), 
				  cscanRecSize);

  srVersionRec = (SrVersionRec*)allocRecord("SrVersionRec",
					    sizeof(SrVersionRec), 
					    csrVersionRecSize);

  tabrec = (Tabrec*)allocRecord("Tabrec",
				sizeof(Tabrec),
				ctablesize);

  // Initialize BAT for interface to file system

  NewVARIABLE* bat = allocateBat(3);
  bat[1].WA = &page8->word32[0];
  bat[1].nrr = cpagesize;
  bat[1].ClusterSize = sizeof(Page8);
  bat[1].bits.q = 11;
  bat[1].bits.v = 5;
  bat[2].WA = &undopage->undoword[0];
  bat[2].nrr = cundopagesize;
  bat[2].ClusterSize = sizeof(Undopage);
  bat[2].bits.q = 13;
  bat[2].bits.v = 5;
}//Dbacc::initRecords()

Dbacc::Dbacc(const class Configuration & conf):
  SimulatedBlock(DBACC, conf),
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
  addRecSignal(GSN_ACC_OVER_REC, &Dbacc::execACC_OVER_REC);
  addRecSignal(GSN_ACC_SAVE_PAGES, &Dbacc::execACC_SAVE_PAGES);
  addRecSignal(GSN_NEXTOPERATION, &Dbacc::execNEXTOPERATION);
  addRecSignal(GSN_READ_PSEUDO_REQ, &Dbacc::execREAD_PSEUDO_REQ);

  // Received signals
  addRecSignal(GSN_STTOR, &Dbacc::execSTTOR);
  addRecSignal(GSN_SR_FRAGIDREQ, &Dbacc::execSR_FRAGIDREQ);
  addRecSignal(GSN_LCP_FRAGIDREQ, &Dbacc::execLCP_FRAGIDREQ);
  addRecSignal(GSN_LCP_HOLDOPREQ, &Dbacc::execLCP_HOLDOPREQ);
  addRecSignal(GSN_END_LCPREQ, &Dbacc::execEND_LCPREQ);
  addRecSignal(GSN_ACC_LCPREQ, &Dbacc::execACC_LCPREQ);
  addRecSignal(GSN_START_RECREQ, &Dbacc::execSTART_RECREQ);
  addRecSignal(GSN_ACC_CONTOPREQ, &Dbacc::execACC_CONTOPREQ);
  addRecSignal(GSN_ACCKEYREQ, &Dbacc::execACCKEYREQ);
  addRecSignal(GSN_ACCSEIZEREQ, &Dbacc::execACCSEIZEREQ);
  addRecSignal(GSN_ACCFRAGREQ, &Dbacc::execACCFRAGREQ);
  addRecSignal(GSN_ACC_SRREQ, &Dbacc::execACC_SRREQ);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbacc::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_ABORTREQ, &Dbacc::execACC_ABORTREQ);
  addRecSignal(GSN_ACC_SCANREQ, &Dbacc::execACC_SCANREQ);
  addRecSignal(GSN_ACCMINUPDATE, &Dbacc::execACCMINUPDATE);
  addRecSignal(GSN_ACC_COMMITREQ, &Dbacc::execACC_COMMITREQ);
  addRecSignal(GSN_ACC_TO_REQ, &Dbacc::execACC_TO_REQ);
  addRecSignal(GSN_ACC_LOCKREQ, &Dbacc::execACC_LOCKREQ);
  addRecSignal(GSN_FSOPENCONF, &Dbacc::execFSOPENCONF);
  addRecSignal(GSN_FSCLOSECONF, &Dbacc::execFSCLOSECONF);
  addRecSignal(GSN_FSWRITECONF, &Dbacc::execFSWRITECONF);
  addRecSignal(GSN_FSREADCONF, &Dbacc::execFSREADCONF);
  addRecSignal(GSN_NDB_STTOR, &Dbacc::execNDB_STTOR);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbacc::execDROP_TAB_REQ);
  addRecSignal(GSN_FSREMOVECONF, &Dbacc::execFSREMOVECONF);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbacc::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_SET_VAR_REQ,  &Dbacc::execSET_VAR_REQ);

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
                    &fsConnectptr,
                    &fsOpptr,
                    &lcpConnectptr,
                    &operationRecPtr,
                    &idrOperationRecPtr,
                    &copyInOperPtr,
                    &copyOperPtr,
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
                    &priPageptr,
                    &pwiPageptr,
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
                    &rootfragrecptr,
                    &scanPtr,
                    &srVersionPtr,
                    &tabptr,
                    &undopageptr
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
  
  deallocRecord((void **)&fsConnectrec, "FsConnectrec",
		sizeof(FsConnectrec), 
		cfsConnectsize);
  
  deallocRecord((void **)&fsOprec, "FsOprec",
		sizeof(FsOprec), 
		cfsOpsize);
  
  deallocRecord((void **)&lcpConnectrec, "LcpConnectrec",
		sizeof(LcpConnectrec),
		clcpConnectsize);
  
  deallocRecord((void **)&operationrec, "Operationrec",
		sizeof(Operationrec),
		coprecsize);
  
  deallocRecord((void **)&overflowRecord, "OverflowRecord",
		sizeof(OverflowRecord),
		coverflowrecsize);

  deallocRecord((void **)&page8, "Page8",
		sizeof(Page8), 
		cpagesize);
  
  deallocRecord((void **)&rootfragmentrec, "Rootfragmentrec",
		sizeof(Rootfragmentrec), 
		crootfragmentsize);
  
  deallocRecord((void **)&scanRec, "ScanRec",
		sizeof(ScanRec), 
		cscanRecSize);
  
  deallocRecord((void **)&srVersionRec, "SrVersionRec",
		sizeof(SrVersionRec), 
		csrVersionRecSize);
  
  deallocRecord((void **)&tabrec, "Tabrec",
		sizeof(Tabrec),
		ctablesize);
  
  deallocRecord((void **)&undopage, "Undopage",
		sizeof(Undopage), 
		cundopagesize);

}//Dbacc::~Dbacc()

BLOCK_FUNCTIONS(Dbacc)
