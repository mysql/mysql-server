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


#include <pc.hpp>
#define DBLQH_C
#include "Dblqh.hpp"
#include <ndb_limits.h>

#define DEBUG(x) { ndbout << "LQH::" << x << endl; }

void Dblqh::initData() 
{
  caddfragrecFileSize = ZADDFRAGREC_FILE_SIZE;
  cattrinbufFileSize = ZATTRINBUF_FILE_SIZE;
  c_no_attrinbuf_recs= ZATTRINBUF_FILE_SIZE;
  cdatabufFileSize = ZDATABUF_FILE_SIZE;
  cgcprecFileSize = ZGCPREC_FILE_SIZE;
  chostFileSize = MAX_NDB_NODES;
  clcpFileSize = ZNO_CONCURRENT_LCP;
  clfoFileSize = 0;
  clogFileFileSize = 0;
  clogPartFileSize = ZLOG_PART_FILE_SIZE;
  cpageRefFileSize = ZPAGE_REF_FILE_SIZE;
  cscanrecFileSize = 0;
  ctabrecFileSize = 0;
  ctcConnectrecFileSize = 0;
  ctcNodeFailrecFileSize = MAX_NDB_NODES;

  addFragRecord = 0;
  attrbuf = 0;
  databuf = 0;
  gcpRecord = 0;
  hostRecord = 0;
  lcpRecord = 0;
  logPartRecord = 0;
  logFileRecord = 0;
  logFileOperationRecord = 0;
  logPageRecord = 0;
  logPageRecordUnaligned= 0;
  pageRefRecord = 0;
  tablerec = 0;
  tcConnectionrec = 0;
  tcNodeFailRecord = 0;
  
  // Records with constant sizes

  cLqhTimeOutCount = 0;
  cLqhTimeOutCheckCount = 0;
  cbookedAccOps = 0;
  m_backup_ptr = RNIL;
  clogFileSize = 16;
  cmaxLogFilesInPageZero = 40;
}//Dblqh::initData()

void Dblqh::initRecords() 
{
  // Records with dynamic sizes
  addFragRecord = (AddFragRecord*)allocRecord("AddFragRecord",
					      sizeof(AddFragRecord), 
					      caddfragrecFileSize);
  attrbuf = (Attrbuf*)allocRecord("Attrbuf",
				  sizeof(Attrbuf), 
				  cattrinbufFileSize);

  databuf = (Databuf*)allocRecord("Databuf",
				  sizeof(Databuf), 
				  cdatabufFileSize);

  gcpRecord = (GcpRecord*)allocRecord("GcpRecord",
				      sizeof(GcpRecord), 
				      cgcprecFileSize);

  hostRecord = (HostRecord*)allocRecord("HostRecord",
					sizeof(HostRecord), 
					chostFileSize);

  lcpRecord = (LcpRecord*)allocRecord("LcpRecord",
				      sizeof(LcpRecord), 
				      clcpFileSize);

  for(Uint32 i = 0; i<clcpFileSize; i++){
    new (&lcpRecord[i])LcpRecord();
  }

  logPartRecord = (LogPartRecord*)allocRecord("LogPartRecord",
					      sizeof(LogPartRecord), 
					      clogPartFileSize);

  logFileRecord = (LogFileRecord*)allocRecord("LogFileRecord",
					      sizeof(LogFileRecord),
					      clogFileFileSize);

  logFileOperationRecord = (LogFileOperationRecord*)
    allocRecord("LogFileOperationRecord", 
		sizeof(LogFileOperationRecord), 
		clfoFileSize);

  logPageRecord =
    (LogPageRecord*)allocRecordAligned("LogPageRecord",
                                       sizeof(LogPageRecord),
                                       clogPageFileSize,
                                       &logPageRecordUnaligned,
                                       NDB_O_DIRECT_WRITE_ALIGNMENT,
                                       false);

  pageRefRecord = (PageRefRecord*)allocRecord("PageRefRecord",
					      sizeof(PageRefRecord),
					      cpageRefFileSize);

  cscanNoFreeRec = cscanrecFileSize;
  c_scanRecordPool.setSize(cscanrecFileSize);
  c_scanTakeOverHash.setSize(64);

  tablerec = (Tablerec*)allocRecord("Tablerec",
				    sizeof(Tablerec), 
				    ctabrecFileSize);

  tcConnectionrec = (TcConnectionrec*)allocRecord("TcConnectionrec",
						  sizeof(TcConnectionrec),
						  ctcConnectrecFileSize);
  
  m_commitAckMarkerPool.setSize(ctcConnectrecFileSize);
  m_commitAckMarkerHash.setSize(1024);
  
  tcNodeFailRecord = (TcNodeFailRecord*)allocRecord("TcNodeFailRecord",
						    sizeof(TcNodeFailRecord),
						    ctcNodeFailrecFileSize);
  
  /*
  ndbout << "FRAGREC SIZE = " << sizeof(Fragrecord) << endl;
  ndbout << "TAB SIZE = " << sizeof(Tablerec) << endl;
  ndbout << "GCP SIZE = " << sizeof(GcpRecord) << endl;
  ndbout << "LCP SIZE = " << sizeof(LcpRecord) << endl;
  ndbout << "LCPLOC SIZE = " << sizeof(LcpLocRecord) << endl;
  ndbout << "LOGPART SIZE = " << sizeof(LogPartRecord) << endl;
  ndbout << "LOGFILE SIZE = " << sizeof(LogFileRecord) << endl;
  ndbout << "TC SIZE = " << sizeof(TcConnectionrec) << endl;
  ndbout << "HOST SIZE = " << sizeof(HostRecord) << endl;
  ndbout << "LFO SIZE = " << sizeof(LogFileOperationRecord) << endl;
  ndbout << "PR SIZE = " << sizeof(PageRefRecord) << endl;
  ndbout << "SCAN SIZE = " << sizeof(ScanRecord) << endl;
*/

  // Initialize BAT for interface to file system
  NewVARIABLE* bat = allocateBat(2);
  bat[1].WA = &logPageRecord->logPageWord[0];
  bat[1].nrr = clogPageFileSize;
  bat[1].ClusterSize = sizeof(LogPageRecord);
  bat[1].bits.q = ZTWOLOG_PAGE_SIZE;
  bat[1].bits.v = 5;
}//Dblqh::initRecords()

Dblqh::Dblqh(Block_context& ctx):
  SimulatedBlock(DBLQH, ctx),
  c_lcp_waiting_fragments(c_fragment_pool),
  c_lcp_restoring_fragments(c_fragment_pool),
  c_lcp_complete_fragments(c_fragment_pool),
  m_commitAckMarkerHash(m_commitAckMarkerPool),
  c_scanTakeOverHash(c_scanRecordPool)
{
  BLOCK_CONSTRUCTOR(Dblqh);

  addRecSignal(GSN_PACKED_SIGNAL, &Dblqh::execPACKED_SIGNAL);
  addRecSignal(GSN_DEBUG_SIG, &Dblqh::execDEBUG_SIG);
  addRecSignal(GSN_ATTRINFO, &Dblqh::execATTRINFO);
  addRecSignal(GSN_KEYINFO, &Dblqh::execKEYINFO);
  addRecSignal(GSN_LQHKEYREQ, &Dblqh::execLQHKEYREQ);
  addRecSignal(GSN_LQHKEYREF, &Dblqh::execLQHKEYREF);
  addRecSignal(GSN_COMMIT, &Dblqh::execCOMMIT);
  addRecSignal(GSN_COMPLETE, &Dblqh::execCOMPLETE);
  addRecSignal(GSN_LQHKEYCONF, &Dblqh::execLQHKEYCONF);
#ifdef VM_TRACE
  addRecSignal(GSN_TESTSIG, &Dblqh::execTESTSIG);
#endif
  addRecSignal(GSN_CONTINUEB, &Dblqh::execCONTINUEB);
  addRecSignal(GSN_START_RECREQ, &Dblqh::execSTART_RECREQ);
  addRecSignal(GSN_START_RECCONF, &Dblqh::execSTART_RECCONF);
  addRecSignal(GSN_EXEC_FRAGREQ, &Dblqh::execEXEC_FRAGREQ);
  addRecSignal(GSN_EXEC_FRAGCONF, &Dblqh::execEXEC_FRAGCONF);
  addRecSignal(GSN_EXEC_FRAGREF, &Dblqh::execEXEC_FRAGREF);
  addRecSignal(GSN_START_EXEC_SR, &Dblqh::execSTART_EXEC_SR);
  addRecSignal(GSN_EXEC_SRREQ, &Dblqh::execEXEC_SRREQ);
  addRecSignal(GSN_EXEC_SRCONF, &Dblqh::execEXEC_SRCONF);
  addRecSignal(GSN_SCAN_HBREP, &Dblqh::execSCAN_HBREP);

  addRecSignal(GSN_ALTER_TAB_REQ, &Dblqh::execALTER_TAB_REQ);

  // Trigger signals, transit to from TUP
  addRecSignal(GSN_CREATE_TRIG_REQ, &Dblqh::execCREATE_TRIG_REQ);
  addRecSignal(GSN_CREATE_TRIG_CONF, &Dblqh::execCREATE_TRIG_CONF);
  addRecSignal(GSN_CREATE_TRIG_REF, &Dblqh::execCREATE_TRIG_REF);

  addRecSignal(GSN_DROP_TRIG_REQ, &Dblqh::execDROP_TRIG_REQ);
  addRecSignal(GSN_DROP_TRIG_CONF, &Dblqh::execDROP_TRIG_CONF);
  addRecSignal(GSN_DROP_TRIG_REF, &Dblqh::execDROP_TRIG_REF);

  addRecSignal(GSN_DUMP_STATE_ORD, &Dblqh::execDUMP_STATE_ORD);
  addRecSignal(GSN_NODE_FAILREP, &Dblqh::execNODE_FAILREP);
  addRecSignal(GSN_CHECK_LCP_STOP, &Dblqh::execCHECK_LCP_STOP);
  addRecSignal(GSN_SEND_PACKED, &Dblqh::execSEND_PACKED);
  addRecSignal(GSN_TUP_ATTRINFO, &Dblqh::execTUP_ATTRINFO);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dblqh::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_LQHFRAGREQ, &Dblqh::execLQHFRAGREQ);
  addRecSignal(GSN_LQHADDATTREQ, &Dblqh::execLQHADDATTREQ);
  addRecSignal(GSN_TUP_ADD_ATTCONF, &Dblqh::execTUP_ADD_ATTCONF);
  addRecSignal(GSN_TUP_ADD_ATTRREF, &Dblqh::execTUP_ADD_ATTRREF);
  addRecSignal(GSN_ACCFRAGCONF, &Dblqh::execACCFRAGCONF);
  addRecSignal(GSN_ACCFRAGREF, &Dblqh::execACCFRAGREF);
  addRecSignal(GSN_TUPFRAGCONF, &Dblqh::execTUPFRAGCONF);
  addRecSignal(GSN_TUPFRAGREF, &Dblqh::execTUPFRAGREF);
  addRecSignal(GSN_TAB_COMMITREQ, &Dblqh::execTAB_COMMITREQ);
  addRecSignal(GSN_ACCSEIZECONF, &Dblqh::execACCSEIZECONF);
  addRecSignal(GSN_ACCSEIZEREF, &Dblqh::execACCSEIZEREF);
  addRecSignal(GSN_READ_NODESCONF, &Dblqh::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &Dblqh::execREAD_NODESREF);
  addRecSignal(GSN_STTOR, &Dblqh::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &Dblqh::execNDB_STTOR);
  addRecSignal(GSN_TUPSEIZECONF, &Dblqh::execTUPSEIZECONF);
  addRecSignal(GSN_TUPSEIZEREF, &Dblqh::execTUPSEIZEREF);
  addRecSignal(GSN_ACCKEYCONF, &Dblqh::execACCKEYCONF);
  addRecSignal(GSN_ACCKEYREF, &Dblqh::execACCKEYREF);
  addRecSignal(GSN_TUPKEYCONF, &Dblqh::execTUPKEYCONF);
  addRecSignal(GSN_TUPKEYREF, &Dblqh::execTUPKEYREF);
  addRecSignal(GSN_ABORT, &Dblqh::execABORT);
  addRecSignal(GSN_ABORTREQ, &Dblqh::execABORTREQ);
  addRecSignal(GSN_COMMITREQ, &Dblqh::execCOMMITREQ);
  addRecSignal(GSN_COMPLETEREQ, &Dblqh::execCOMPLETEREQ);
#ifdef VM_TRACE
  addRecSignal(GSN_MEMCHECKREQ, &Dblqh::execMEMCHECKREQ);
#endif
  addRecSignal(GSN_SCAN_FRAGREQ, &Dblqh::execSCAN_FRAGREQ);
  addRecSignal(GSN_SCAN_NEXTREQ, &Dblqh::execSCAN_NEXTREQ);
  addRecSignal(GSN_ACC_SCANCONF, &Dblqh::execACC_SCANCONF);
  addRecSignal(GSN_ACC_SCANREF, &Dblqh::execACC_SCANREF);
  addRecSignal(GSN_NEXT_SCANCONF, &Dblqh::execNEXT_SCANCONF);
  addRecSignal(GSN_NEXT_SCANREF, &Dblqh::execNEXT_SCANREF);
  addRecSignal(GSN_STORED_PROCCONF, &Dblqh::execSTORED_PROCCONF);
  addRecSignal(GSN_STORED_PROCREF, &Dblqh::execSTORED_PROCREF);
  addRecSignal(GSN_COPY_FRAGREQ, &Dblqh::execCOPY_FRAGREQ);
  addRecSignal(GSN_COPY_ACTIVEREQ, &Dblqh::execCOPY_ACTIVEREQ);
  addRecSignal(GSN_COPY_STATEREQ, &Dblqh::execCOPY_STATEREQ);
  addRecSignal(GSN_LQH_TRANSREQ, &Dblqh::execLQH_TRANSREQ);
  addRecSignal(GSN_TRANSID_AI, &Dblqh::execTRANSID_AI);
  addRecSignal(GSN_INCL_NODEREQ, &Dblqh::execINCL_NODEREQ);
  addRecSignal(GSN_LCP_PREPARE_REF, &Dblqh::execLCP_PREPARE_REF);
  addRecSignal(GSN_LCP_PREPARE_CONF, &Dblqh::execLCP_PREPARE_CONF);
  addRecSignal(GSN_END_LCPCONF, &Dblqh::execEND_LCPCONF);

  addRecSignal(GSN_EMPTY_LCP_REQ, &Dblqh::execEMPTY_LCP_REQ);
  addRecSignal(GSN_LCP_FRAG_ORD, &Dblqh::execLCP_FRAG_ORD);
  
  addRecSignal(GSN_START_FRAGREQ, &Dblqh::execSTART_FRAGREQ);
  addRecSignal(GSN_START_RECREF, &Dblqh::execSTART_RECREF);
  addRecSignal(GSN_GCP_SAVEREQ, &Dblqh::execGCP_SAVEREQ);
  addRecSignal(GSN_FSOPENREF, &Dblqh::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Dblqh::execFSOPENCONF);
  addRecSignal(GSN_FSCLOSECONF, &Dblqh::execFSCLOSECONF);
  addRecSignal(GSN_FSWRITECONF, &Dblqh::execFSWRITECONF);
  addRecSignal(GSN_FSWRITEREF, &Dblqh::execFSWRITEREF, true);
  addRecSignal(GSN_FSREADCONF, &Dblqh::execFSREADCONF);
  addRecSignal(GSN_FSREADREF, &Dblqh::execFSREADREF, true);
  addRecSignal(GSN_ACC_ABORTCONF, &Dblqh::execACC_ABORTCONF);
  addRecSignal(GSN_TIME_SIGNAL,  &Dblqh::execTIME_SIGNAL);
  addRecSignal(GSN_FSSYNCCONF,  &Dblqh::execFSSYNCCONF);
  addRecSignal(GSN_REMOVE_MARKER_ORD, &Dblqh::execREMOVE_MARKER_ORD);

  //addRecSignal(GSN_DROP_TAB_REQ, &Dblqh::execDROP_TAB_REQ);
  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dblqh::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_WAIT_DROP_TAB_REQ, &Dblqh::execWAIT_DROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &Dblqh::execDROP_TAB_REQ);

  addRecSignal(GSN_LQH_ALLOCREQ, &Dblqh::execLQH_ALLOCREQ);
  addRecSignal(GSN_LQH_WRITELOG_REQ, &Dblqh::execLQH_WRITELOG_REQ);
  addRecSignal(GSN_TUP_DEALLOCREQ, &Dblqh::execTUP_DEALLOCREQ);

  // TUX
  addRecSignal(GSN_TUXFRAGCONF, &Dblqh::execTUXFRAGCONF);
  addRecSignal(GSN_TUXFRAGREF, &Dblqh::execTUXFRAGREF);
  addRecSignal(GSN_TUX_ADD_ATTRCONF, &Dblqh::execTUX_ADD_ATTRCONF);
  addRecSignal(GSN_TUX_ADD_ATTRREF, &Dblqh::execTUX_ADD_ATTRREF);

  addRecSignal(GSN_READ_PSEUDO_REQ, &Dblqh::execREAD_PSEUDO_REQ);

  addRecSignal(GSN_DEFINE_BACKUP_REF, &Dblqh::execDEFINE_BACKUP_REF);
  addRecSignal(GSN_DEFINE_BACKUP_CONF, &Dblqh::execDEFINE_BACKUP_CONF);

  addRecSignal(GSN_BACKUP_FRAGMENT_REF, &Dblqh::execBACKUP_FRAGMENT_REF);
  addRecSignal(GSN_BACKUP_FRAGMENT_CONF, &Dblqh::execBACKUP_FRAGMENT_CONF);

  addRecSignal(GSN_RESTORE_LCP_REF, &Dblqh::execRESTORE_LCP_REF);
  addRecSignal(GSN_RESTORE_LCP_CONF, &Dblqh::execRESTORE_LCP_CONF);

  addRecSignal(GSN_UPDATE_FRAG_DIST_KEY_ORD, 
	       &Dblqh::execUPDATE_FRAG_DIST_KEY_ORD);
  
  addRecSignal(GSN_PREPARE_COPY_FRAG_REQ,
	       &Dblqh::execPREPARE_COPY_FRAG_REQ);
  
  initData();

#ifdef VM_TRACE
  {
    void* tmp[] = { 
      &addfragptr,
      &attrinbufptr,
      &databufptr,
      &fragptr,
      &gcpPtr,
      &lcpPtr,
      &logPartPtr,
      &logFilePtr,
      &lfoPtr,
      &logPagePtr,
      &pageRefPtr,
      &scanptr,
      &tabptr,
      &tcConnectptr,
      &tcNodeFailptr,
    }; 
    init_globals_list(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
  
}//Dblqh::Dblqh()

Dblqh::~Dblqh() 
{
  // Records with dynamic sizes
  deallocRecord((void **)&addFragRecord, "AddFragRecord",
		sizeof(AddFragRecord), 
		caddfragrecFileSize);

  deallocRecord((void**)&attrbuf,
		"Attrbuf",
		sizeof(Attrbuf), 
		cattrinbufFileSize);

  deallocRecord((void**)&databuf,
		"Databuf",
		sizeof(Databuf), 
		cdatabufFileSize);

  deallocRecord((void**)&gcpRecord,
		"GcpRecord",
		sizeof(GcpRecord), 
		cgcprecFileSize);
  
  deallocRecord((void**)&hostRecord,
		"HostRecord",
		sizeof(HostRecord), 
		chostFileSize);
  
  deallocRecord((void**)&lcpRecord,
		"LcpRecord",
		sizeof(LcpRecord), 
		clcpFileSize);

  deallocRecord((void**)&logPartRecord,
		"LogPartRecord",
		sizeof(LogPartRecord), 
		clogPartFileSize);
  
  deallocRecord((void**)&logFileRecord,
		"LogFileRecord",
		sizeof(LogFileRecord),
		clogFileFileSize);

  deallocRecord((void**)&logFileOperationRecord,
		"LogFileOperationRecord", 
		sizeof(LogFileOperationRecord), 
		clfoFileSize);
  
  deallocRecord((void**)&logPageRecordUnaligned,
		"LogPageRecord",
		sizeof(LogPageRecord),
		clogPageFileSize);

  deallocRecord((void**)&pageRefRecord,
		"PageRefRecord",
		sizeof(PageRefRecord),
		cpageRefFileSize);
  

  deallocRecord((void**)&tablerec,
		"Tablerec",
		sizeof(Tablerec), 
		ctabrecFileSize);
  
  deallocRecord((void**)&tcConnectionrec,
		"TcConnectionrec",
		sizeof(TcConnectionrec),
		ctcConnectrecFileSize);
  
  deallocRecord((void**)&tcNodeFailRecord,
		"TcNodeFailRecord",
		sizeof(TcNodeFailRecord),
		ctcNodeFailrecFileSize);
}//Dblqh::~Dblqh()

BLOCK_FUNCTIONS(Dblqh)

