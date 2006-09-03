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

#include "restore.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/RestoreImpl.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/LqhKey.hpp>
#include <AttributeHeader.hpp>
#include <md5_hash.hpp>
#include <Dblqh.hpp>
#include <dbtup/Dbtup.hpp>
#include <KeyDescriptor.hpp>

#define PAGES LCP_RESTORE_BUFFER

Restore::Restore(Block_context& ctx) :
  SimulatedBlock(RESTORE, ctx),
  m_file_list(m_file_pool),
  m_file_hash(m_file_pool)
{
  BLOCK_CONSTRUCTOR(Restore);
  
  // Add received signals
  addRecSignal(GSN_STTOR, &Restore::execSTTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Restore::execDUMP_STATE_ORD);
  addRecSignal(GSN_CONTINUEB, &Restore::execCONTINUEB);
  addRecSignal(GSN_READ_CONFIG_REQ, &Restore::execREAD_CONFIG_REQ, true);  
  
  addRecSignal(GSN_RESTORE_LCP_REQ, &Restore::execRESTORE_LCP_REQ);

  addRecSignal(GSN_FSOPENREF, &Restore::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Restore::execFSOPENCONF);
  addRecSignal(GSN_FSREADREF, &Restore::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Restore::execFSREADCONF);
  addRecSignal(GSN_FSCLOSEREF, &Restore::execFSCLOSEREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Restore::execFSCLOSECONF);

  addRecSignal(GSN_LQHKEYREF, &Restore::execLQHKEYREF);
  addRecSignal(GSN_LQHKEYCONF, &Restore::execLQHKEYCONF);

  ndbrequire(sizeof(Column) == 8);
}
  
Restore::~Restore()
{
}

BLOCK_FUNCTIONS(Restore)

void
Restore::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];
  c_lqh = (Dblqh*)globalData.getBlock(DBLQH);
  c_tup = (Dbtup*)globalData.getBlock(DBTUP);
  sendSTTORRY(signal);
  
  return;
}//Restore::execNDB_STTOR()

void
Restore::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

#if 0
  Uint32 noBackups = 0, noTables = 0, noAttribs = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &m_diskless));
  ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_BACKUPS, &noBackups);
  //  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_ATTRIBUTES, &noAttribs));

  noAttribs++; //RT 527 bug fix

  c_backupPool.setSize(noBackups);
  c_backupFilePool.setSize(3 * noBackups);
  c_tablePool.setSize(noBackups * noTables);
  c_attributePool.setSize(noBackups * noAttribs);
  c_triggerPool.setSize(noBackups * 3 * noTables);

  // 2 = no of replicas
  c_fragmentPool.setSize(noBackups * NO_OF_FRAG_PER_NODE * noTables);
  
  Uint32 szMem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MEM, &szMem);
  Uint32 noPages = (szMem + sizeof(Page32) - 1) / sizeof(Page32);
  // We need to allocate an additional of 2 pages. 1 page because of a bug in
  // ArrayPool and another one for DICTTAINFO.
  c_pagePool.setSize(noPages + NO_OF_PAGES_META_FILE + 2); 

  Uint32 szDataBuf = (2 * 1024 * 1024);
  Uint32 szLogBuf = (2 * 1024 * 1024);
  Uint32 szWrite = 32768;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_DATA_BUFFER_MEM, &szDataBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_LOG_BUFFER_MEM, &szLogBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_WRITE_SIZE, &szWrite);
  
  c_defaults.m_logBufferSize = szLogBuf;
  c_defaults.m_dataBufferSize = szDataBuf;
  c_defaults.m_minWriteSize = szWrite;
  c_defaults.m_maxWriteSize = szWrite;
  
  { // Init all tables
    ArrayList<Table> tables(c_tablePool);
    TablePtr ptr;
    while(tables.seize(ptr)){
      new (ptr.p) Table(c_attributePool, c_fragmentPool);
    }
    tables.release();
  }

  {
    ArrayList<BackupFile> ops(c_backupFilePool);
    BackupFilePtr ptr;
    while(ops.seize(ptr)){
      new (ptr.p) BackupFile(* this, c_pagePool);
    }
    ops.release();
  }
  
  {
    ArrayList<BackupRecord> recs(c_backupPool);
    BackupRecordPtr ptr;
    while(recs.seize(ptr)){
      new (ptr.p) BackupRecord(* this, c_pagePool, c_tablePool, 
			       c_backupFilePool, c_triggerPool);
    }
    recs.release();
  }

  // Initialize BAT for interface to file system
  {
    Page32Ptr p;
    ndbrequire(c_pagePool.seizeId(p, 0));
    c_startOfPages = (Uint32 *)p.p;
    c_pagePool.release(p);
    
    NewVARIABLE* bat = allocateBat(1);
    bat[0].WA = c_startOfPages;
    bat[0].nrr = c_pagePool.getSize()*sizeof(Page32)/sizeof(Uint32);
  }
#endif
  m_file_pool.setSize(1);
  m_databuffer_pool.setSize((1*128+PAGES+List::getSegmentSize()-1)/List::getSegmentSize());
  
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Restore::sendSTTORRY(Signal* signal){
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
}

void
Restore::execCONTINUEB(Signal* signal){
  jamEntry();

  switch(signal->theData[0]){
  case RestoreContinueB::RESTORE_NEXT:
  {
    FilePtr file_ptr;
    m_file_pool.getPtr(file_ptr, signal->theData[1]);
    restore_next(signal, file_ptr);
    return;
  }
  case RestoreContinueB::READ_FILE:
  {
    FilePtr file_ptr;
    m_file_pool.getPtr(file_ptr, signal->theData[1]);
    read_file(signal, file_ptr);
    return;
  }
  default:
    ndbrequire(false);
  }
}

void
Restore::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();
}

void
Restore::execRESTORE_LCP_REQ(Signal* signal){
  jamEntry();

  Uint32 err= 0;
  RestoreLcpReq* req= (RestoreLcpReq*)signal->getDataPtr();
  Uint32 senderRef= req->senderRef;
  Uint32 senderData= req->senderData;
  do
  {
    FilePtr file_ptr;
    if(!m_file_list.seize(file_ptr))
    {
      err= RestoreLcpRef::NoFileRecord;
      break;
    }

    if((err= init_file(req, file_ptr)))
    {
      break;
    }

    open_file(signal, file_ptr, req->lcpNo);
    return;
  } while(0);

  RestoreLcpRef* ref= (RestoreLcpRef*)signal->getDataPtrSend();
  ref->senderData= senderData;
  ref->senderRef= reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_RESTORE_LCP_REF, signal, 
	     RestoreLcpRef::SignalLength, JBB);
}

Uint32
Restore::init_file(const RestoreLcpReq* req, FilePtr file_ptr)
{
  new (file_ptr.p) File();
  file_ptr.p->m_sender_ref = req->senderRef;
  file_ptr.p->m_sender_data = req->senderData;

  file_ptr.p->m_fd = RNIL;
  file_ptr.p->m_file_type = BackupFormat::LCP_FILE;
  file_ptr.p->m_status = File::FIRST_READ;
  
  file_ptr.p->m_table_id = req->tableId;
  file_ptr.p->m_fragment_id = req->fragmentId;
  file_ptr.p->m_table_version = RNIL;

  file_ptr.p->m_bytes_left = 0; // Bytes read from FS
  file_ptr.p->m_current_page_ptr_i = RNIL;
  file_ptr.p->m_current_page_pos = 0; 
  file_ptr.p->m_current_page_index = 0;
  file_ptr.p->m_current_file_page = 0;
  file_ptr.p->m_outstanding_reads = 0;
  file_ptr.p->m_outstanding_operations = 0;
  file_ptr.p->m_rows_restored = 0;
  LocalDataBuffer<15> pages(m_databuffer_pool, file_ptr.p->m_pages);
  LocalDataBuffer<15> columns(m_databuffer_pool, file_ptr.p->m_columns);

  ndbassert(columns.isEmpty());
  columns.release();

  ndbassert(pages.isEmpty());
  pages.release();
  
  Uint32 buf_size= PAGES*GLOBAL_PAGE_SIZE;
  Uint32 page_count= (buf_size+GLOBAL_PAGE_SIZE-1)/GLOBAL_PAGE_SIZE;
  if(!pages.seize(page_count))
  {
    return RestoreLcpRef::OutOfDataBuffer;
  }

  List::Iterator it;
  for(pages.first(it); !it.isNull(); pages.next(it))
  {
    * it.data = RNIL;
  }

  Uint32 err= 0;
  for(pages.first(it); !it.isNull(); pages.next(it))
  {
    Ptr<GlobalPage> page_ptr;
    if(!m_global_page_pool.seize(page_ptr))
    {
      err= RestoreLcpRef::OutOfReadBufferPages;
      break;
    }
    * it.data = page_ptr.i;
  }
  
  if(err)
  {
    for(pages.first(it); !it.isNull(); pages.next(it))
    {
      if(* it.data == RNIL)
	break;
      m_global_page_pool.release(* it.data);
    }
  }
  else
  {
    pages.first(it);
    file_ptr.p->m_current_page_ptr_i = *it.data;
  }
  return err;
}

void
Restore::release_file(FilePtr file_ptr)
{
  LocalDataBuffer<15> pages(m_databuffer_pool, file_ptr.p->m_pages);
  LocalDataBuffer<15> columns(m_databuffer_pool, file_ptr.p->m_columns);

  List::Iterator it;
  for(pages.first(it); !it.isNull(); pages.next(it))
  {
    if(* it.data == RNIL)
      continue;
    m_global_page_pool.release(* it.data);
  }

  ndbout_c("RESTORE table: %d %lld rows applied", 
	   file_ptr.p->m_table_id,
	   file_ptr.p->m_rows_restored);
  
  columns.release();
  pages.release();
  m_file_list.release(file_ptr);
}

void
Restore::open_file(Signal* signal, FilePtr file_ptr, Uint32 lcpNo)
{
  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = FsOpenReq::OM_READONLY ;
  req->userPointer = file_ptr.i;
  
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, lcpNo);
  FsOpenReq::v5_setTableId(req->fileNumber, file_ptr.p->m_table_id);
  FsOpenReq::v5_setFragmentId(req->fileNumber, file_ptr.p->m_fragment_id);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Restore::execFSOPENREF(Signal* signal)
{
  FsRef* ref= (FsRef*)signal->getDataPtr();
  FilePtr file_ptr;
  jamEntry();
  m_file_pool.getPtr(file_ptr, ref->userPointer);

  Uint32 errCode= ref->errorCode;
  Uint32 osError= ref->osErrorCode;

  RestoreLcpRef* rep= (RestoreLcpRef*)signal->getDataPtrSend();
  rep->senderData= file_ptr.p->m_sender_data;
  rep->errorCode = errCode;
  rep->extra[0] = osError;
  sendSignal(file_ptr.p->m_sender_ref, 
	     GSN_RESTORE_LCP_REF, signal, RestoreLcpRef::SignalLength+1, JBB);

  release_file(file_ptr);
}

void
Restore::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  FilePtr file_ptr;
  FsConf* conf= (FsConf*)signal->getDataPtr();
  m_file_pool.getPtr(file_ptr, conf->userPointer);
  
  file_ptr.p->m_fd = conf->filePointer;

  /**
   * Start thread's
   */

  file_ptr.p->m_status |= File::FILE_THREAD_RUNNING;
  signal->theData[0] = RestoreContinueB::READ_FILE;
  signal->theData[1] = file_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);  
  
  file_ptr.p->m_status |= File::RESTORE_THREAD_RUNNING;
  signal->theData[0] = RestoreContinueB::RESTORE_NEXT;
  signal->theData[1] = file_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);  
}

void
Restore::restore_next(Signal* signal, FilePtr file_ptr)
{
  Uint32 *data, len= 0;
  Uint32 status = file_ptr.p->m_status;
  Uint32 page_count = file_ptr.p->m_pages.getSize();
  do 
  {
    Uint32 left= file_ptr.p->m_bytes_left;
    if(left < 8)
    {
      /**
       * Not enought bytes to read header
       */
      break;
    }
    Ptr<GlobalPage> page_ptr, next_page_ptr = { 0, 0 };
    m_global_page_pool.getPtr(page_ptr, file_ptr.p->m_current_page_ptr_i);
    List::Iterator it;
    
    Uint32 pos= file_ptr.p->m_current_page_pos;
    if(status & File::READING_RECORDS)
    {
      /**
       * We are reading records
       */
      len= ntohl(* (page_ptr.p->data + pos)) + 1;
    }
    else
    {
      /**
       * Section length is in 2 word
       */
      if(pos + 1 == GLOBAL_PAGE_SIZE_WORDS)
      {
	/**
	 * But that's stored on next page...
	 *   and since we have atleast 8 bytes left in buffer
	 *   we can be sure that that's in buffer
	 */
	LocalDataBuffer<15> pages(m_databuffer_pool, file_ptr.p->m_pages);
	Uint32 next_page = file_ptr.p->m_current_page_index + 1;
	pages.position(it, next_page % page_count);
	m_global_page_pool.getPtr(next_page_ptr, * it.data);
	len= ntohl(* next_page_ptr.p->data);
      }
      else
      {
	len= ntohl(* (page_ptr.p->data + pos + 1));
      }
    }

    if(file_ptr.p->m_status & File::FIRST_READ)
    {
      len= 3;
      file_ptr.p->m_status &= ~(Uint32)File::FIRST_READ;
    }
    
    if(4 * len > left)
    {
      /**
       * Not enought bytes to read "record"
       */
      ndbout_c("records: %d len: %x left: %d", 
	       status & File::READING_RECORDS, 4*len, left);
      
      if (unlikely((status & File:: FILE_THREAD_RUNNING) == 0))
      {
	ndbrequire(false);
      }
      len= 0;
      break;
    }
    
    /**
     * Entire record is in buffer
     */

    if(pos + len >= GLOBAL_PAGE_SIZE_WORDS)
    {
      /**
       * But it's split over pages
       */
      if(next_page_ptr.p == 0)
      {
	LocalDataBuffer<15> pages(m_databuffer_pool, file_ptr.p->m_pages);
	Uint32 next_page = file_ptr.p->m_current_page_index + 1;
	pages.position(it, next_page % page_count);
	m_global_page_pool.getPtr(next_page_ptr, * it.data);
      }
      file_ptr.p->m_current_page_ptr_i = next_page_ptr.i;
      file_ptr.p->m_current_page_pos = (pos + len) - GLOBAL_PAGE_SIZE_WORDS;
      file_ptr.p->m_current_page_index = 
	(file_ptr.p->m_current_page_index + 1) % page_count;
      
      Uint32 first = (GLOBAL_PAGE_SIZE_WORDS - pos);
      memcpy(page_ptr.p, page_ptr.p->data+pos, 4 * first);
      memcpy(page_ptr.p->data+first, next_page_ptr.p, 4 * (len - first));
      data= page_ptr.p->data;
    } 
    else
    {
      file_ptr.p->m_current_page_pos = pos + len;
      data= page_ptr.p->data+pos;
    }
    
    file_ptr.p->m_bytes_left -= 4*len;
    
    if(status & File::READING_RECORDS)
    {
      if(len == 1)
      {
	file_ptr.p->m_status = status & ~(Uint32)File::READING_RECORDS;
      }
      else
      {
	parse_record(signal, file_ptr, data, len);
      }
    }
    else
    {
      switch(ntohl(* data)){
      case BackupFormat::FILE_HEADER:
	parse_file_header(signal, file_ptr, data-3, len+3);
	break;
      case BackupFormat::FRAGMENT_HEADER:
	file_ptr.p->m_status = status | File::READING_RECORDS;
	parse_fragment_header(signal, file_ptr, data, len);
	break;
      case BackupFormat::FRAGMENT_FOOTER:
	parse_fragment_footer(signal, file_ptr, data, len);
	break;
      case BackupFormat::TABLE_LIST:
	parse_table_list(signal, file_ptr, data, len);
	break;
      case BackupFormat::TABLE_DESCRIPTION:
	parse_table_description(signal, file_ptr, data, len);
	break;
      case BackupFormat::GCP_ENTRY:
	parse_gcp_entry(signal, file_ptr, data, len);
	break;
      case 0x4e444242: // 'NDBB'
	if (check_file_version(signal, ntohl(* (data+2))) == 0)
	{
	  break;
	}
      default:
	parse_error(signal, file_ptr, __LINE__, ntohl(* data));
      }
    }
  } while(0);
  
  if(file_ptr.p->m_bytes_left == 0 && status & File::FILE_EOF)
  {
    file_ptr.p->m_status &= ~(Uint32)File::RESTORE_THREAD_RUNNING;
    /**
     * File is finished...
     */
    close_file(signal, file_ptr);
    return;
  }
  
  signal->theData[0] = RestoreContinueB::RESTORE_NEXT;
  signal->theData[1] = file_ptr.i;

  if(len)
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  else
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
}

void
Restore::read_file(Signal* signal, FilePtr file_ptr)
{
  Uint32 left= file_ptr.p->m_bytes_left;
  Uint32 page_count = file_ptr.p->m_pages.getSize();
  Uint32 free= GLOBAL_PAGE_SIZE * page_count - left;
  Uint32 read_count= free/GLOBAL_PAGE_SIZE;

  if(read_count <= file_ptr.p->m_outstanding_reads)
  {
    signal->theData[0] = RestoreContinueB::READ_FILE;
    signal->theData[1] = file_ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);  
    return;
  }

  read_count -= file_ptr.p->m_outstanding_reads;
  Uint32 curr_page= file_ptr.p->m_current_page_index;
  LocalDataBuffer<15> pages(m_databuffer_pool, file_ptr.p->m_pages);
  
  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = file_ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = file_ptr.i;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag, 
				FsReadWriteReq::fsFormatGlobalPage);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);

  Uint32 start= (curr_page + page_count - read_count) % page_count;
  
  List::Iterator it;
  pages.position(it, start);
  do
  {
    file_ptr.p->m_outstanding_reads++;
    req->varIndex = file_ptr.p->m_current_file_page++;
    req->data.pageData[0] = *it.data;
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBB);
    
    start++;
    if(start == page_count)
    {
      start= 0;
      pages.position(it, start);
    }
    else
    {
      pages.next(it);
    }
  } while(start != curr_page);
}

void
Restore::execFSREADREF(Signal * signal)
{
  jamEntry();
  SimulatedBlock::execFSREADREF(signal);
  ndbrequire(false);
}

void
Restore::execFSREADCONF(Signal * signal)
{
  jamEntry();
  FilePtr file_ptr;
  FsConf* conf= (FsConf*)signal->getDataPtr();
  m_file_pool.getPtr(file_ptr, conf->userPointer);
  
  file_ptr.p->m_bytes_left += conf->bytes_read;
  
  ndbassert(file_ptr.p->m_outstanding_reads);
  file_ptr.p->m_outstanding_reads--;

  if(file_ptr.p->m_outstanding_reads == 0)
  {
    ndbassert(conf->bytes_read <= GLOBAL_PAGE_SIZE);
    if(conf->bytes_read == GLOBAL_PAGE_SIZE)
    {
      read_file(signal, file_ptr);
    }
    else 
    {
      file_ptr.p->m_status |= File::FILE_EOF;
      file_ptr.p->m_status &= ~(Uint32)File::FILE_THREAD_RUNNING;
    }
  }
}

void
Restore::close_file(Signal* signal, FilePtr file_ptr)
{
  FsCloseReq * req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = file_ptr.p->m_fd;
  req->userPointer = file_ptr.i;
  req->userReference = reference();
  req->fileFlag = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
}

void
Restore::execFSCLOSEREF(Signal * signal)
{
  jamEntry();
  SimulatedBlock::execFSCLOSEREF(signal);
  ndbrequire(false);
}

void
Restore::execFSCLOSECONF(Signal * signal)
{
  jamEntry();
  FilePtr file_ptr;
  FsConf* conf= (FsConf*)signal->getDataPtr();
  m_file_pool.getPtr(file_ptr, conf->userPointer);

  file_ptr.p->m_fd = RNIL;

  if(file_ptr.p->m_outstanding_operations == 0)
  {
    RestoreLcpConf* rep= (RestoreLcpConf*)signal->getDataPtrSend();
    rep->senderData= file_ptr.p->m_sender_data;
    sendSignal(file_ptr.p->m_sender_ref, 
	       GSN_RESTORE_LCP_CONF, signal, 
	       RestoreLcpConf::SignalLength, JBB);
    release_file(file_ptr);
  }
}

void
Restore::parse_file_header(Signal* signal, 
			   FilePtr file_ptr, 
			   const Uint32* data, Uint32 len)
{
  const BackupFormat::FileHeader* fh= (BackupFormat::FileHeader*)data;

  if(memcmp(fh->Magic, "NDBBCKUP", 8) != 0)
  {
    parse_error(signal, file_ptr, __LINE__, *data);
    return;
  }
  
  if (check_file_version(signal, ntohl(fh->NdbVersion)))
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->NdbVersion));
    return;
  }
  ndbassert(ntohl(fh->SectionType) == BackupFormat::FILE_HEADER);
  
  if(ntohl(fh->SectionLength) != len-3)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }  
  
  if(ntohl(fh->FileType) != BackupFormat::LCP_FILE)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->FileType));
    return;
  }  
  
  if(fh->ByteOrder != 0x12345678)
  {
    parse_error(signal, file_ptr, __LINE__, fh->ByteOrder);
    return;
  }
}

void
Restore::parse_table_list(Signal* signal, FilePtr file_ptr, 
			  const Uint32 *data, Uint32 len)
{
  const BackupFormat::CtlFile::TableList* fh= 
    (BackupFormat::CtlFile::TableList*)data;
  
  if(ntohl(fh->TableIds[0]) != file_ptr.p->m_table_id)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableIds[0]));
    return;
  } 
}

void
Restore::parse_table_description(Signal* signal, FilePtr file_ptr, 
				 const Uint32 *data, Uint32 len)
{
  bool lcp = file_ptr.p->is_lcp();
  Uint32 disk= 0;
  const BackupFormat::CtlFile::TableDescription* fh= 
    (BackupFormat::CtlFile::TableDescription*)data;
  
  LocalDataBuffer<15> columns(m_databuffer_pool, file_ptr.p->m_columns);

  SimplePropertiesLinearReader it(fh->DictTabInfo, len);
  it.first();
  
  DictTabInfo::Table tmpTab; tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, 
				  DictTabInfo::TableMapping, 
				  DictTabInfo::TableMappingSize, 
				  true, true);
  ndbrequire(stat == SimpleProperties::Break);
  
  if(tmpTab.TableId != file_ptr.p->m_table_id)
  {
    parse_error(signal, file_ptr, __LINE__, tmpTab.TableId);
    return;
  }
  
  Uint32 null_offset = 0;
  Column c; 
  Uint32 colstore[sizeof(Column)/sizeof(Uint32)];

  for(Uint32 i = 0; i<tmpTab.NoOfAttributes; i++) {
    jam();
    DictTabInfo::Attribute tmp; tmp.init();
    stat = SimpleProperties::unpack(it, &tmp, 
				    DictTabInfo::AttributeMapping, 
				    DictTabInfo::AttributeMappingSize,
				    true, true);
    
    ndbrequire(stat == SimpleProperties::Break);
    it.next(); // Move Past EndOfAttribute
    
    const Uint32 arr = tmp.AttributeArraySize;
    const Uint32 sz = 1 << tmp.AttributeSize;
    const Uint32 sz32 = (sz * arr + 31) >> 5;
    const bool varsize = tmp.AttributeArrayType != NDB_ARRAYTYPE_FIXED;
    
    c.m_id = tmp.AttributeId;
    c.m_size = sz32;
    c.m_flags = (tmp.AttributeKeyFlag ? Column::COL_KEY : 0);
    c.m_flags |= (tmp.AttributeStorageType == NDB_STORAGETYPE_DISK ?
		  Column::COL_DISK : 0);

    if(lcp && (c.m_flags & Column::COL_DISK))
    {
      /**
       * Restore does not currently handle disk attributes
       *   which is fine as restore LCP shouldn't
       */
      disk++;
      continue;
    }

    if(!tmp.AttributeNullableFlag && !varsize)
    {
    }
    else if (true) // null mask dropped in 5.1
    {
      c.m_flags |= (varsize ? Column::COL_VAR : 0);
      c.m_flags |= (tmp.AttributeNullableFlag ? Column::COL_NULL : 0);
    } 

    memcpy(colstore, &c, sizeof(Column));
    if(!columns.append(colstore, sizeof(Column)/sizeof(Uint32)))
    {
      parse_error(signal, file_ptr, __LINE__, i);
      return;
    }
  }

  if(lcp)
  {
    if (disk)
    {
      c.m_id = AttributeHeader::DISK_REF;
      c.m_size = 2;
      c.m_flags = 0;
      memcpy(colstore, &c, sizeof(Column));
      if(!columns.append(colstore, sizeof(Column)/sizeof(Uint32)))
      {
	parse_error(signal, file_ptr, __LINE__, 0);
	return;
      }
    }

    {
      c.m_id = AttributeHeader::ROWID;
      c.m_size = 2;
      c.m_flags = 0;
      memcpy(colstore, &c, sizeof(Column));
      if(!columns.append(colstore, sizeof(Column)/sizeof(Uint32)))
      {
	parse_error(signal, file_ptr, __LINE__, 0);
	return;
      }
    }

    if (tmpTab.RowGCIFlag)
    {
      c.m_id = AttributeHeader::ROW_GCI;
      c.m_size = 2;
      c.m_flags = 0;
      memcpy(colstore, &c, sizeof(Column));
      if(!columns.append(colstore, sizeof(Column)/sizeof(Uint32)))
      {
	parse_error(signal, file_ptr, __LINE__, 0);
	return;
      }
    }
  }
  
  file_ptr.p->m_table_version = tmpTab.TableVersion;
}

void
Restore::parse_fragment_header(Signal* signal, FilePtr file_ptr, 
			       const Uint32 *data, Uint32 len)
{
  const BackupFormat::DataFile::FragmentHeader* fh= 
    (BackupFormat::DataFile::FragmentHeader*)data;
  if(ntohl(fh->TableId) != file_ptr.p->m_table_id)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableId));
    return;
  } 
  
  if(ntohl(fh->ChecksumType) != 0)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }
  
  file_ptr.p->m_fragment_id = ntohl(fh->FragmentNo);

  if(file_ptr.p->is_lcp())
  {
    /**
     * Temporary reset DBTUP's #disk attributes on table
     */
    c_tup->start_restore_lcp(file_ptr.p->m_table_id,
			     file_ptr.p->m_fragment_id);
  }
}

void
Restore::parse_record(Signal* signal, FilePtr file_ptr, 
		      const Uint32 *data, Uint32 len)
{
  List::Iterator it;
  LocalDataBuffer<15> columns(m_databuffer_pool, file_ptr.p->m_columns);  

  Uint32 * const key_start = signal->getDataPtrSend()+24;
  Uint32 * const attr_start = key_start + MAX_KEY_SIZE_IN_WORDS;

  data += 1;
  const Uint32* const dataStart = data;

  Uint32 *keyData = key_start;
  Uint32 *attrData = attr_start;
  union {
    Column c;
    Uint32 _align[sizeof(Column)/sizeof(Uint32)];
  };
  bool disk = false;
  bool rowid = false;
  bool gci = false;
  Uint32 tableId = file_ptr.p->m_table_id;

  Uint64 gci_val;
  Local_key rowid_val;
  columns.first(it);
  while(!it.isNull())
  {
    _align[0] = *it.data; ndbrequire(columns.next(it));
    _align[1] = *it.data; columns.next(it);
    
    if (c.m_id == AttributeHeader::ROWID)
    {
      rowid_val.m_page_no = data[0];
      rowid_val.m_page_idx = data[1];
      data += 2;
      rowid = true;
      continue;
    }

    if (c.m_id == AttributeHeader::ROW_GCI)
    {
      memcpy(&gci_val, data, 8);
      data += 2;
      gci = true;
      continue;
    }
    
    if (! (c.m_flags & (Column::COL_VAR | Column::COL_NULL)))
    {
      ndbrequire(data < dataStart + len);

      if(c.m_flags & Column::COL_KEY)
      {
        memcpy(keyData, data, 4*c.m_size);
        keyData += c.m_size;
      } 

      AttributeHeader::init(attrData++, c.m_id, c.m_size << 2);
      memcpy(attrData, data, 4*c.m_size);
      attrData += c.m_size;
      data += c.m_size;
    }

    if(c.m_flags & Column::COL_DISK)
      disk= true;
  }

  // second part is data driven
  while (data + 2 < dataStart + len) {
    Uint32 sz= ntohl(*data); data++;
    Uint32 id= ntohl(*data); data++; // column_no

    ndbrequire(columns.position(it, 2 * id));

    _align[0] = *it.data; ndbrequire(columns.next(it));
    _align[1] = *it.data;

    Uint32 sz32 = (sz + 3) >> 2;
    ndbassert(c.m_flags & (Column::COL_VAR | Column::COL_NULL));
    if (c.m_flags & Column::COL_KEY)
    {
      memcpy(keyData, data, 4 * sz32);
      keyData += sz32;
    }

    AttributeHeader::init(attrData++, c.m_id, sz);
    memcpy(attrData, data, sz);

    attrData += sz32;
    data += sz32;
  }

  ndbrequire(data == dataStart + len - 1);

  ndbrequire(disk == false); // Not supported...
  ndbrequire(rowid == true);
  Uint32 keyLen = keyData - key_start;
  Uint32 attrLen = attrData - attr_start;
  LqhKeyReq * req = (LqhKeyReq *)signal->getDataPtrSend();
  
  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tableId);
  if (desc->noOfKeyAttr != desc->noOfVarKeys)
  {
    reorder_key(desc, key_start, keyLen);
  }
  
  Uint32 hashValue;
  if (g_key_descriptor_pool.getPtr(tableId)->hasCharAttr)
    hashValue = calulate_hash(tableId, key_start);
  else
    hashValue = md5_hash((Uint64*)key_start, keyLen);
  
  Uint32 tmp= 0;
  LqhKeyReq::setAttrLen(tmp, attrLen);
  req->attrLen = tmp;

  tmp= 0;
  LqhKeyReq::setKeyLen(tmp, keyLen);
  LqhKeyReq::setLastReplicaNo(tmp, 0);
  /* ---------------------------------------------------------------------- */
  // Indicate Application Reference is present in bit 15
  /* ---------------------------------------------------------------------- */
  LqhKeyReq::setApplicationAddressFlag(tmp, 0);
  LqhKeyReq::setDirtyFlag(tmp, 1);
  LqhKeyReq::setSimpleFlag(tmp, 1);
  LqhKeyReq::setOperation(tmp, ZINSERT);
  LqhKeyReq::setSameClientAndTcFlag(tmp, 0);
  LqhKeyReq::setAIInLqhKeyReq(tmp, 0);
  LqhKeyReq::setNoDiskFlag(tmp, disk ? 0 : 1);
  LqhKeyReq::setRowidFlag(tmp, 1);
  LqhKeyReq::setGCIFlag(tmp, gci);
  req->clientConnectPtr = file_ptr.i;
  req->hashValue = hashValue;
  req->requestInfo = tmp;
  req->tcBlockref = reference();
  req->savePointId = 0;
  req->tableSchemaVersion = file_ptr.p->m_table_id + 
    (file_ptr.p->m_table_version << 16);
  req->fragmentData = file_ptr.p->m_fragment_id;
  req->transId1 = 0;
  req->transId2 = 0;
  req->scanInfo = 0;
  memcpy(req->variableData, key_start, 16);
  Uint32 pos = keyLen > 4 ? 4 : keyLen;
  req->variableData[pos++] = rowid_val.m_page_no;
  req->variableData[pos++] = rowid_val.m_page_idx;
  if (gci)
    req->variableData[pos++] = (Uint32)gci_val;
  file_ptr.p->m_outstanding_operations++;
  EXECUTE_DIRECT(DBLQH, GSN_LQHKEYREQ, signal, 
		 LqhKeyReq::FixedSignalLength+pos);
  
  if(keyLen > 4)
  {
    c_lqh->receive_keyinfo(signal,
			   key_start + 4,
			   keyLen - 4);
  }
  
  c_lqh->receive_attrinfo(signal, attr_start, attrLen);
}

void
Restore::reorder_key(const KeyDescriptor* desc,
		     Uint32 *data, Uint32 len)
{
  Uint32 i;
  Uint32 *var= data;
  Uint32 Tmp[MAX_KEY_SIZE_IN_WORDS];
  for(i = 0; i<desc->noOfKeyAttr; i++)
  {
    Uint32 attr = desc->keyAttr[i].attributeDescriptor;
    switch(AttributeDescriptor::getArrayType(attr)){
    case NDB_ARRAYTYPE_FIXED:
      var += AttributeDescriptor::getSizeInWords(attr);
    }
  }

  Uint32 *dst = Tmp;
  Uint32 *src = data;
  for(i = 0; i<desc->noOfKeyAttr; i++)
  {
    Uint32 sz;
    Uint32 attr = desc->keyAttr[i].attributeDescriptor;
    switch(AttributeDescriptor::getArrayType(attr)){
    case NDB_ARRAYTYPE_FIXED:
      sz = AttributeDescriptor::getSizeInWords(attr);
      memcpy(dst, src, 4 * sz);
      src += sz;
      break;
    case NDB_ARRAYTYPE_SHORT_VAR:
      sz = (1 + ((char*)var)[0] + 3) >> 2;
      memcpy(dst, var, 4 * sz);
      var += sz;
      break;
    case NDB_ARRAYTYPE_MEDIUM_VAR:
      sz = (2 + ((char*)var)[0] +  256*((char*)var)[1] + 3) >> 2;
      memcpy(dst, var, 4 * sz);
      var += sz;
      break;
    default:
      ndbrequire(false);
      sz = 0;
    }
    dst += sz;
  }
  ndbassert((dst - Tmp) == len);
  memcpy(data, Tmp, 4*len);
}

Uint32
Restore::calulate_hash(Uint32 tableId, const Uint32 *src)
{
  jam();
  Uint64 Tmp[(MAX_KEY_SIZE_IN_WORDS*MAX_XFRM_MULTIPLY) >> 1];
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 keyLen = xfrm_key(tableId, src, (Uint32*)Tmp, sizeof(Tmp) >> 2, 
			   keyPartLen);
  ndbrequire(keyLen);
  
  return md5_hash(Tmp, keyLen);
}

void
Restore::execLQHKEYREF(Signal*)
{
  ndbrequire(false);
}

void
Restore::execLQHKEYCONF(Signal* signal)
{
  FilePtr file_ptr;
  LqhKeyConf * conf = (LqhKeyConf *)signal->getDataPtr();
  m_file_pool.getPtr(file_ptr, conf->connectPtr);
  
  ndbassert(file_ptr.p->m_outstanding_operations);
  file_ptr.p->m_outstanding_operations--;
  file_ptr.p->m_rows_restored++;
  if(file_ptr.p->m_outstanding_operations == 0 && file_ptr.p->m_fd == RNIL)
  {
    RestoreLcpConf* rep= (RestoreLcpConf*)signal->getDataPtrSend();
    rep->senderData= file_ptr.p->m_sender_data;
    sendSignal(file_ptr.p->m_sender_ref, 
	       GSN_RESTORE_LCP_CONF, signal, 
	       RestoreLcpConf::SignalLength, JBB);
    release_file(file_ptr);
  }
}

void
Restore::parse_fragment_footer(Signal* signal, FilePtr file_ptr, 
			       const Uint32 *data, Uint32 len)
{
  const BackupFormat::DataFile::FragmentFooter* fh= 
    (BackupFormat::DataFile::FragmentFooter*)data;
  if(ntohl(fh->TableId) != file_ptr.p->m_table_id)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableId));
    return;
  } 
  
  if(ntohl(fh->Checksum) != 0)
  {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }

  if(file_ptr.p->is_lcp())
  {
    /**
     * Temporary reset DBTUP's #disk attributes on table
     */
    c_tup->complete_restore_lcp(file_ptr.p->m_table_id,
				file_ptr.p->m_fragment_id);
  }

  file_ptr.p->m_fragment_id = RNIL;
}

void
Restore::parse_gcp_entry(Signal* signal, FilePtr file_ptr, 
			 const Uint32 *data, Uint32 len)
{
  
}

void
Restore::parse_error(Signal* signal,
		     FilePtr file_ptr, Uint32 line, Uint32 extra)
{
  ndbrequire(false);
}

NdbOut& 
operator << (NdbOut& ndbout, const Restore::Column& col)
{
  ndbout << "[ Col: id: " << col.m_id 
	 << " size: " << col.m_size 
	 << " key: " << (Uint32)(col.m_flags & Restore::Column::COL_KEY)
	 << " variable: " << (Uint32)(col.m_flags & Restore::Column::COL_VAR)
	 << " null: " << (Uint32)(col.m_flags & Restore::Column::COL_NULL)
	 << " disk: " << (Uint32)(col.m_flags & Restore::Column::COL_DISK) 
	 << "]";

  return ndbout;
}

int
Restore::check_file_version(Signal* signal, Uint32 file_version)
{
  if (file_version < MAKE_VERSION(5,1,6))
  {
    char buf[255];
    char verbuf[255];
    getVersionString(file_version, 0, verbuf, sizeof(verbuf));
    BaseString::snprintf(buf, sizeof(buf),
			 "Unsupported version of LCP files found on disk, "
			 " found: %s", verbuf);
    
    progError(__LINE__, 
	      NDBD_EXIT_SR_RESTARTCONFLICT,
	      buf);
    return -1;
  }
  return 0;
}
