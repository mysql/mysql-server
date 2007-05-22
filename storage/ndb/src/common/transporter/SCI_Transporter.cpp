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

#include <ndb_global.h> 

#include "SCI_Transporter.hpp" 
#include <NdbOut.hpp> 
#include <NdbSleep.h> 
#include <NdbTick.h> 
#include <NdbTick.h> 

#include "TransporterInternalDefinitions.hpp" 
#include <TransporterCallback.hpp> 

#include <InputStream.hpp>
#include <OutputStream.hpp> 

#define FLAGS 0  
#define DEBUG_TRANSPORTER 
SCI_Transporter::SCI_Transporter(TransporterRegistry &t_reg,
                                 const char *lHostName,
                                 const char *rHostName,
                                 int r_port,
				 bool isMgmConnection,
                                 Uint32 packetSize,         
				 Uint32 bufferSize,       
				 Uint32 nAdapters, 
				 Uint16 remoteSciNodeId0,        
				 Uint16 remoteSciNodeId1, 
				 NodeId _localNodeId,      
				 NodeId _remoteNodeId,
				 NodeId serverNodeId,
				 bool chksm,  
				 bool signalId, 
				 Uint32 reportFreq) :  
  Transporter(t_reg, tt_SCI_TRANSPORTER,
	      lHostName, rHostName, r_port, isMgmConnection, _localNodeId,
              _remoteNodeId, serverNodeId, 0, false, chksm, signalId) 
{
  DBUG_ENTER("SCI_Transporter::SCI_Transporter");
  m_PacketSize = (packetSize + 3)/4 ; 
  m_BufferSize = bufferSize; 
  m_sendBuffer.m_buffer = NULL;
  
  m_RemoteSciNodeId = remoteSciNodeId0; 
   
  if(remoteSciNodeId0 == 0 || remoteSciNodeId1 == 0) 
    m_numberOfRemoteNodes=1; 
  else 
    m_numberOfRemoteNodes=2; 
 
  m_RemoteSciNodeId1 = remoteSciNodeId1; 
 
   
  m_initLocal=false; 
  m_failCounter=0; 
  m_remoteNodes[0]=remoteSciNodeId0; 
  m_remoteNodes[1]=remoteSciNodeId1; 
  m_adapters = nAdapters;   
  m_ActiveAdapterId=0; 
  m_StandbyAdapterId=1; 
  
  m_mapped = false; 
  m_sciinit=false; 
  
  sciAdapters= new SciAdapter[nAdapters* (sizeof (SciAdapter))]; 
  if(sciAdapters==NULL) { 
  } 
  m_SourceSegm= new sourceSegm[nAdapters* (sizeof (sourceSegm))]; 
  if(m_SourceSegm==NULL) { 
  } 
  m_TargetSegm= new targetSegm[nAdapters* (sizeof (targetSegm))]; 
  if(m_TargetSegm==NULL) { 
  } 
  m_reportFreq= reportFreq; 
  
  //reset all statistic counters. 
#ifdef DEBUG_TRANSPORTER 
 i1024=0; 
 i2048=0; 
 i2049=0; 
 i10242048=0; 
 i20484096=0; 
 i4096=0; 
 i4097=0; 
#endif
  DBUG_VOID_RETURN;
} 
 
void SCI_Transporter::disconnectImpl() 
{ 
  DBUG_ENTER("SCI_Transporter::disconnectImpl");
  sci_error_t err; 
  if(m_mapped){ 
    setDisconnect(); 
    DBUG_PRINT("info", ("connect status = %d, remote node = %d",
    (int)getConnectionStatus(), remoteNodeId)); 
    disconnectRemote(); 
    disconnectLocal(); 
  } 
  
  // Empty send buffer 

  m_sendBuffer.m_dataSize = 0;

  m_initLocal=false; 
  m_mapped = false; 
  
  if(m_sciinit) { 
    for(Uint32 i=0; i<m_adapters ; i++) {       
      SCIClose(sciAdapters[i].scidesc, FLAGS, &err);  
      
      if(err != SCI_ERR_OK)  { 
	report_error(TE_SCI_UNABLE_TO_CLOSE_CHANNEL); 
        DBUG_PRINT("error",
        ("Cannot close channel to the driver. Error code 0x%x",  
		    err)); 
      } 
    } 
  } 
  m_sciinit=false; 
   
#ifdef DEBUG_TRANSPORTER 
      ndbout << "total: " <<  i1024+ i10242048 + i2048+i2049 << endl; 
      ndbout << "<1024: " << i1024 << endl; 
      ndbout << "1024-2047: " << i10242048 << endl; 
      ndbout << "==2048: " << i2048 << endl; 
      ndbout << "2049-4096: " << i20484096 << endl; 
      ndbout << "==4096: " << i4096 << endl; 
      ndbout << ">4096: " << i4097 << endl; 
#endif 
  DBUG_VOID_RETURN;  
}  
 
 
bool SCI_Transporter::initTransporter() { 
  DBUG_ENTER("SCI_Transporter::initTransporter");
  if(m_BufferSize < (2*MAX_MESSAGE_SIZE + 4096)){ 
    m_BufferSize = 2 * MAX_MESSAGE_SIZE + 4096; 
  } 

  // Allocate buffers for sending, send buffer size plus 2048 bytes for avoiding
  // the need to send twice when a large message comes around. Send buffer size is
  // measured in words. 
  Uint32 sz = 4 * m_PacketSize + MAX_MESSAGE_SIZE;;
  
  m_sendBuffer.m_sendBufferSize = 4 * ((sz + 3) / 4); 
  m_sendBuffer.m_buffer = new Uint32[m_sendBuffer.m_sendBufferSize / 4];
  m_sendBuffer.m_dataSize = 0;
 
  DBUG_PRINT("info",
  ("Created SCI Send Buffer with buffer size %d and packet size %d",
              m_sendBuffer.m_sendBufferSize, m_PacketSize * 4));
  if(!getLinkStatus(m_ActiveAdapterId) ||  
     (m_adapters > 1 &&
     !getLinkStatus(m_StandbyAdapterId))) { 
    DBUG_PRINT("error",
    ("The link is not fully operational. Check the cables and the switches")); 
    //NDB should terminate 
    report_error(TE_SCI_LINK_ERROR); 
    DBUG_RETURN(false); 
  } 
  DBUG_RETURN(true); 
} // initTransporter()  

 
 
Uint32 SCI_Transporter::getLocalNodeId(Uint32 adapterNo) 
{ 
  sci_query_adapter_t queryAdapter; 
  sci_error_t  error; 
  Uint32 _localNodeId; 
   
  queryAdapter.subcommand = SCI_Q_ADAPTER_NODEID; 
  queryAdapter.localAdapterNo = adapterNo; 
  queryAdapter.data = &_localNodeId; 
   
  SCIQuery(SCI_Q_ADAPTER,(void*)(&queryAdapter),(Uint32)NULL,&error); 
   
  if(error != SCI_ERR_OK) 
    return 0; 
  return _localNodeId;  
} 
 
 
bool SCI_Transporter::getLinkStatus(Uint32 adapterNo) 
{ 
  sci_query_adapter_t queryAdapter; 
  sci_error_t  error; 
  int linkstatus; 
  queryAdapter.subcommand = SCI_Q_ADAPTER_LINK_OPERATIONAL; 
   
  queryAdapter.localAdapterNo = adapterNo; 
  queryAdapter.data = &linkstatus; 
   
  SCIQuery(SCI_Q_ADAPTER,(void*)(&queryAdapter),(Uint32)NULL,&error); 
   
  if(error != SCI_ERR_OK) { 
    DBUG_PRINT("error", ("error %d querying adapter", error)); 
    return false; 
  } 
  if(linkstatus<=0) 
    return false; 
  return true; 
} 
 
 
 
sci_error_t SCI_Transporter::initLocalSegment() { 
  DBUG_ENTER("SCI_Transporter::initLocalSegment");
  Uint32 segmentSize = m_BufferSize; 
  Uint32 offset  = 0; 
  sci_error_t err; 
  if(!m_sciinit) { 
    for(Uint32 i=0; i<m_adapters ; i++) { 
      SCIOpen(&(sciAdapters[i].scidesc), FLAGS, &err); 
      sciAdapters[i].localSciNodeId=getLocalNodeId(i); 
      DBUG_PRINT("info", ("SCInode iD %d  adapter %d\n",  
	         sciAdapters[i].localSciNodeId, i)); 
      if(err != SCI_ERR_OK) { 
        DBUG_PRINT("error",
        ("Cannot open an SCI virtual device. Error code 0x%x", 
		   err)); 
	DBUG_RETURN(err); 
      } 
    } 
  } 
   
  m_sciinit=true; 
 
  SCICreateSegment(sciAdapters[0].scidesc,            
		   &(m_SourceSegm[0].localHandle),  
		   hostSegmentId(localNodeId, remoteNodeId),    
		   segmentSize,                
		   0, 
		   0, 
		   0,         
		   &err);             
   
  if(err != SCI_ERR_OK) { 
    DBUG_PRINT("error", ("Error creating segment, err = 0x%x", err));
    DBUG_RETURN(err); 
  } else { 
    DBUG_PRINT("info", ("created segment id : %d",
	       hostSegmentId(localNodeId, remoteNodeId))); 
  } 
   
  /** Prepare the segment*/ 
  for(Uint32 i=0; i < m_adapters; i++) { 
    SCIPrepareSegment((m_SourceSegm[0].localHandle),  
		      i, 
		      FLAGS, 
		      &err); 
     
    if(err != SCI_ERR_OK) { 
      DBUG_PRINT("error",
    ("Local Segment is not accessible by an SCI adapter. Error code 0x%x\n",
                  err)); 
      DBUG_RETURN(err); 
    } 
  } 
 
  
  m_SourceSegm[0].mappedMemory =  
    SCIMapLocalSegment((m_SourceSegm[0].localHandle), 
		       &(m_SourceSegm[0].lhm[0].map), 
		       offset, 
		       segmentSize, 
		       NULL, 
		       FLAGS, 
		       &err); 
 
 
 
  if(err != SCI_ERR_OK) { 
    DBUG_PRINT("error", ("Cannot map area of size %d. Error code 0x%x", 
	        segmentSize,err)); 
    doDisconnect(); 
    DBUG_RETURN(err); 
  } 
  
  
  /** Make the local segment available*/ 
  for(Uint32 i=0; i < m_adapters; i++) { 
    SCISetSegmentAvailable((m_SourceSegm[0].localHandle),  
			     i, 
			   FLAGS, 
			   &err); 
     
    if(err != SCI_ERR_OK) { 
      DBUG_PRINT("error",
   ("Local Segment is not available for remote connections. Error code 0x%x\n",
                 err)); 
      DBUG_RETURN(err); 
    } 
  } 
  setupLocalSegment(); 
  DBUG_RETURN(err); 
   
} // initLocalSegment() 
 
 
bool SCI_Transporter::doSend() { 
#ifdef DEBUG_TRANSPORTER  
  NDB_TICKS startSec=0, stopSec=0; 
  Uint32 startMicro=0, stopMicro=0, totalMicro=0; 
#endif
  sci_error_t             err; 
  Uint32 retry=0; 
 
  const char * const sendPtr = (char*)m_sendBuffer.m_buffer;
  const Uint32 sizeToSend    = 4 * m_sendBuffer.m_dataSize; //Convert to number of bytes
  
  if (sizeToSend > 0){
#ifdef DEBUG_TRANSPORTER 
    if(sizeToSend < 1024 ) 
      i1024++; 
    if(sizeToSend > 1024 && sizeToSend < 2048 ) 
      i10242048++; 
    if(sizeToSend==2048) 
      i2048++; 
    if(sizeToSend>2048 && sizeToSend < 4096) 
      i20484096++; 
    if(sizeToSend==4096) 
      i4096++; 
    if(sizeToSend==4097) 
      i4097++; 
#endif
      
  tryagain:
    retry++;
    if (retry > 3) { 
      DBUG_PRINT("error", ("SCI Transfer failed"));
      report_error(TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
      return false; 
    } 
    Uint32 * insertPtr = (Uint32 *) 
      (m_TargetSegm[m_ActiveAdapterId].writer)->getWritePtr(sizeToSend); 
    
    if(insertPtr != 0) {	   
      
      const Uint32 remoteOffset=(Uint32) 
	((char*)insertPtr -  
	 (char*)(m_TargetSegm[m_ActiveAdapterId].mappedMemory)); 
      
      SCIMemCpy(m_TargetSegm[m_ActiveAdapterId].sequence, 
		(void*)sendPtr, 
		m_TargetSegm[m_ActiveAdapterId].rhm[m_ActiveAdapterId].map, 
		remoteOffset, 
		sizeToSend, 
		SCI_FLAG_ERROR_CHECK, 
		&err);   
      
      if (err != SCI_ERR_OK) { 
        if (err == SCI_ERR_OUT_OF_RANGE ||
            err == SCI_ERR_SIZE_ALIGNMENT ||
            err == SCI_ERR_OFFSET_ALIGNMENT) { 
          DBUG_PRINT("error", ("Data transfer error = %d", err));
          report_error(TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
	  return false; 
        } 
        if(err == SCI_ERR_TRANSFER_FAILED) { 
	  if(getLinkStatus(m_ActiveAdapterId))
	    goto tryagain; 
          if (m_adapters == 1) {
            DBUG_PRINT("error", ("SCI Transfer failed"));
            report_error(TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
	    return false; 
          }
	  m_failCounter++; 
	  Uint32 temp=m_ActiveAdapterId;	    	     
	  if (getLinkStatus(m_StandbyAdapterId)) { 
	    failoverShmWriter();		 
	    SCIStoreBarrier(m_TargetSegm[m_StandbyAdapterId].sequence,0); 
	    m_ActiveAdapterId=m_StandbyAdapterId; 
	    m_StandbyAdapterId=temp; 
            DBUG_PRINT("error", ("Swapping from adapter %u to %u",
                       m_StandbyAdapterId, m_ActiveAdapterId));
	  } else {
	    report_error(TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
            DBUG_PRINT("error", ("SCI Transfer failed")); 
	  }
        }
      } else { 
	SHM_Writer * writer = (m_TargetSegm[m_ActiveAdapterId].writer);
	writer->updateWritePtr(sizeToSend); 
	
	Uint32 sendLimit = writer->getBufferSize();
	sendLimit -= writer->getWriteIndex();
	
	m_sendBuffer.m_dataSize = 0;
	m_sendBuffer.m_forceSendLimit = sendLimit;
      } 
    } else { 
      /** 
       * If we end up here, the SCI segment is full.  
       */ 
      DBUG_PRINT("error", ("the segment is full for some reason")); 
      return false; 
    } //if  
  } 
  return true; 
} // doSend() 

 
 
void SCI_Transporter::failoverShmWriter() { 
#if 0
  (m_TargetSegm[m_StandbyAdapterId].writer)
    ->copyIndexes((m_TargetSegm[m_StandbyAdapterId].writer));
#endif
} //failoverShm 
 
 
void SCI_Transporter::setupLocalSegment()   
{ 
   DBUG_ENTER("SCI_Transporter::setupLocalSegment"); 
   Uint32 sharedSize = 0; 
   sharedSize =4096;   //start of the buffer is page aligend 
    
   Uint32 sizeOfBuffer = m_BufferSize; 
 
   sizeOfBuffer -= sharedSize; 
 
   Uint32 * localReadIndex =  
     (Uint32*)m_SourceSegm[m_ActiveAdapterId].mappedMemory;  
   Uint32 * localWriteIndex =  (Uint32*)(localReadIndex+ 1); 
   m_localStatusFlag = (Uint32*)(localReadIndex + 3); 
 
   char * localStartOfBuf = (char*)  
     ((char*)m_SourceSegm[m_ActiveAdapterId].mappedMemory+sharedSize); 
 
   * localReadIndex = 0; 
   * localWriteIndex = 0; 

   const Uint32 slack = MAX_MESSAGE_SIZE;

   reader = new SHM_Reader(localStartOfBuf,  
			   sizeOfBuffer, 
			   slack,
			   localReadIndex, 
			   localWriteIndex);
    
   reader->clear(); 
   DBUG_VOID_RETURN;
} //setupLocalSegment 
 
void SCI_Transporter::setupRemoteSegment()   
{ 
   DBUG_ENTER("SCI_Transporter::setupRemoteSegment");
   Uint32 sharedSize = 0; 
   sharedSize =4096;   //start of the buffer is page aligned 
 
   Uint32 sizeOfBuffer = m_BufferSize; 
   const Uint32 slack = MAX_MESSAGE_SIZE;
   sizeOfBuffer -= sharedSize; 

   Uint32 *segPtr = (Uint32*) m_TargetSegm[m_ActiveAdapterId].mappedMemory ;   
    
   Uint32 * remoteReadIndex = (Uint32*)segPtr;  
   Uint32 * remoteWriteIndex = (Uint32*)(segPtr + 1); 
   m_remoteStatusFlag = (Uint32*)(segPtr + 3);
    
   char * remoteStartOfBuf = ( char*)((char*)segPtr+(sharedSize)); 
    
   writer = new SHM_Writer(remoteStartOfBuf,  
			   sizeOfBuffer, 
			   slack,
			   remoteReadIndex, 
			   remoteWriteIndex);
   
   writer->clear(); 
    
   m_TargetSegm[0].writer=writer; 
 
   m_sendBuffer.m_forceSendLimit = writer->getBufferSize();
    
   if(createSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
     report_error(TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
     DBUG_PRINT("error", ("Unable to create sequence on active"));
     doDisconnect(); 
   } 
   if (m_adapters > 1) {
     segPtr = (Uint32*) m_TargetSegm[m_StandbyAdapterId].mappedMemory ; 
    
     Uint32 * remoteReadIndex2 = (Uint32*)segPtr;  
     Uint32 * remoteWriteIndex2 = (Uint32*) (segPtr + 1); 
     m_remoteStatusFlag2 = (Uint32*)(segPtr + 3);
    
     char * remoteStartOfBuf2 = ( char*)((char *)segPtr+sharedSize); 
    
     /** 
      * setup a writer. writer2 is used to mirror the changes of 
      * writer on the standby 
      * segment, so that in the case of a failover, we can switch 
      * to the stdby seg. quickly.* 
      */ 
     writer2 = new SHM_Writer(remoteStartOfBuf2,  
                              sizeOfBuffer, 
                              slack,
                              remoteReadIndex2, 
                              remoteWriteIndex2);

     * remoteReadIndex = 0; 
     * remoteWriteIndex = 0; 
     writer2->clear(); 
     m_TargetSegm[1].writer=writer2; 
     if(createSequence(m_StandbyAdapterId)!=SCI_ERR_OK) { 
       report_error(TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
       DBUG_PRINT("error", ("Unable to create sequence on standby"));
       doDisconnect(); 
     } 
   }
   DBUG_VOID_RETURN; 
} //setupRemoteSegment 

bool
SCI_Transporter::init_local()
{
  DBUG_ENTER("SCI_Transporter::init_local");
  if(!m_initLocal) { 
    if(initLocalSegment()!=SCI_ERR_OK){ 
      NdbSleep_MilliSleep(10);
      //NDB SHOULD TERMINATE AND COMPUTER REBOOTED! 
      report_error(TE_SCI_CANNOT_INIT_LOCALSEGMENT);
      DBUG_RETURN(false);
    } 
    m_initLocal=true;
  } 
  DBUG_RETURN(true);
}

bool
SCI_Transporter::init_remote()
{
  DBUG_ENTER("SCI_Transporter::init_remote");
  sci_error_t err; 
  Uint32 offset = 0;
  if(!m_mapped ) {
    DBUG_PRINT("info", ("Map remote segments"));
    for(Uint32 i=0; i < m_adapters ; i++) {
      m_TargetSegm[i].rhm[i].remoteHandle=0;
      SCIConnectSegment(sciAdapters[i].scidesc,
                        &(m_TargetSegm[i].rhm[i].remoteHandle),
                        m_remoteNodes[i],
                        remoteSegmentId(localNodeId, remoteNodeId),
                        i,
                        0,
                        0,
                        0,
                        0,
                        &err);

      if(err != SCI_ERR_OK) {
        NdbSleep_MilliSleep(10);
        DBUG_PRINT("error", ("Error connecting segment, err 0x%x", err));
        DBUG_RETURN(false);
      }
    }
    // Map the remote memory segment into program space  
    for(Uint32 i=0; i < m_adapters ; i++) {
      m_TargetSegm[i].mappedMemory =
        SCIMapRemoteSegment((m_TargetSegm[i].rhm[i].remoteHandle),
                            &(m_TargetSegm[i].rhm[i].map),
                            offset,
                            m_BufferSize,
                            NULL,
                            FLAGS,
                            &err);

      if(err!= SCI_ERR_OK) {
        DBUG_PRINT("error",
          ("Cannot map a segment to the remote node %d. Error code 0x%x",
          m_RemoteSciNodeId, err));
        //NDB SHOULD TERMINATE AND COMPUTER REBOOTED! 
        report_error(TE_SCI_CANNOT_MAP_REMOTESEGMENT);
        DBUG_RETURN(false);
      }
    }
    m_mapped=true;
    setupRemoteSegment();
    setConnected();
    DBUG_PRINT("info", ("connected and mapped to segment, remoteNode: %d",
               remoteNodeId));
    DBUG_PRINT("info", ("remoteSegId: %d",
               remoteSegmentId(localNodeId, remoteNodeId)));
    DBUG_RETURN(true);
  } else {
    DBUG_RETURN(getConnectionStatus());
  }
}

bool
SCI_Transporter::connect_client_impl(NDB_SOCKET_TYPE sockfd)
{
  SocketInputStream s_input(sockfd);
  SocketOutputStream s_output(sockfd);
  char buf[256];
  DBUG_ENTER("SCI_Transporter::connect_client_impl");
  // Wait for server to create and attach
  if (s_input.gets(buf, 256) == 0) {
    DBUG_PRINT("error", ("No initial response from server in SCI"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  if (!init_local()) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  // Send ok to server
  s_output.println("sci client 1 ok");

  if (!init_remote()) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  // Wait for ok from server
  if (s_input.gets(buf, 256) == 0) {
    DBUG_PRINT("error", ("No second response from server in SCI"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  // Send ok to server
  s_output.println("sci client 2 ok");

  NDB_CLOSE_SOCKET(sockfd);
  DBUG_PRINT("info", ("Successfully connected client to node %d",
              remoteNodeId));
  DBUG_RETURN(true);
}

bool
SCI_Transporter::connect_server_impl(NDB_SOCKET_TYPE sockfd)
{
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);
  char buf[256];
  DBUG_ENTER("SCI_Transporter::connect_server_impl");

  if (!init_local()) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  // Send ok to client
  s_output.println("sci server 1 ok");

  // Wait for ok from client
  if (s_input.gets(buf, 256) == 0) {
    DBUG_PRINT("error", ("No response from client in SCI"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  if (!init_remote()) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  // Send ok to client
  s_output.println("sci server 2 ok");
  // Wait for ok from client
  if (s_input.gets(buf, 256) == 0) {
    DBUG_PRINT("error", ("No second response from client in SCI"));
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  NDB_CLOSE_SOCKET(sockfd);
  DBUG_PRINT("info", ("Successfully connected server to node %d",
              remoteNodeId));
  DBUG_RETURN(true);
}
 
sci_error_t SCI_Transporter::createSequence(Uint32 adapterid) { 
  sci_error_t err; 
  SCICreateMapSequence((m_TargetSegm[adapterid].rhm[adapterid].map),  
		       &(m_TargetSegm[adapterid].sequence),  
		       SCI_FLAG_FAST_BARRIER,  
		       &err);  
  return err; 
} // createSequence()  
 
bool SCI_Transporter::disconnectLocal()  
{
  DBUG_ENTER("SCI_Transporter::disconnectLocal"); 
  sci_error_t err; 
  m_ActiveAdapterId=0; 
 
  /** Free resources used by a local segment 
   */ 
 
  SCIUnmapSegment(m_SourceSegm[0].lhm[0].map,0,&err); 
  if(err!=SCI_ERR_OK) { 
    report_error(TE_SCI_UNABLE_TO_UNMAP_SEGMENT); 
    DBUG_PRINT("error", ("Unable to unmap segment"));
    DBUG_RETURN(false); 
  } 
 
  SCIRemoveSegment((m_SourceSegm[m_ActiveAdapterId].localHandle), 
		   FLAGS, 
		   &err); 
  
  if(err!=SCI_ERR_OK) { 
    report_error(TE_SCI_UNABLE_TO_REMOVE_SEGMENT); 
    DBUG_PRINT("error", ("Unable to remove segment"));
    DBUG_RETURN(false); 
  } 
  DBUG_PRINT("info", ("Local memory segment is unmapped and removed")); 
  DBUG_RETURN(true); 
} // disconnectLocal() 
 
 
bool SCI_Transporter::disconnectRemote()  { 
  DBUG_ENTER("SCI_Transporter::disconnectRemote");
  sci_error_t err; 
  for(Uint32 i=0; i<m_adapters; i++) { 
    /** 
     * Segment unmapped, disconnect from the remotely connected segment 
     */   
    SCIUnmapSegment(m_TargetSegm[i].rhm[i].map,0,&err); 
    if(err!=SCI_ERR_OK) { 
      report_error(TE_SCI_UNABLE_TO_UNMAP_SEGMENT); 
      DBUG_PRINT("error", ("Unable to unmap segment"));
      DBUG_RETURN(false); 
    } 
	 
    SCIDisconnectSegment(m_TargetSegm[i].rhm[i].remoteHandle, 
			 FLAGS, 
			 &err); 
    if(err!=SCI_ERR_OK) { 
      report_error(TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT); 
      DBUG_PRINT("error", ("Unable to disconnect segment"));
      DBUG_RETURN(false); 
    } 
    DBUG_PRINT("info", ("Remote memory segment is unmapped and disconnected")); 
  } 
  DBUG_RETURN(true); 
} // disconnectRemote() 


SCI_Transporter::~SCI_Transporter() { 
  DBUG_ENTER("SCI_Transporter::~SCI_Transporter");
  // Close channel to the driver 
  doDisconnect(); 
  if(m_sendBuffer.m_buffer != NULL)
    delete[] m_sendBuffer.m_buffer;
  DBUG_VOID_RETURN;
} // ~SCI_Transporter() 
 
void SCI_Transporter::closeSCI() { 
  // Termination of SCI 
  sci_error_t err; 
  DBUG_ENTER("SCI_Transporter::closeSCI");
   
  // Disconnect and remove remote segment 
  disconnectRemote(); 
 
  // Unmap and remove local segment 
   
  disconnectLocal(); 
   
  // Closes an SCI virtual device 
  SCIClose(activeSCIDescriptor, FLAGS, &err);  
   
  if(err != SCI_ERR_OK) {
    DBUG_PRINT("error",
      ("Cannot close SCI channel to the driver. Error code 0x%x",  
      err)); 
  }
  SCITerminate(); 
  DBUG_VOID_RETURN;
} // closeSCI() 
 
Uint32 *
SCI_Transporter::getWritePtr(Uint32 lenBytes, Uint32 prio)
{

  Uint32 sci_buffer_remaining = m_sendBuffer.m_forceSendLimit;
  Uint32 send_buf_size = m_sendBuffer.m_sendBufferSize;
  Uint32 curr_data_size = m_sendBuffer.m_dataSize << 2;
  Uint32 new_curr_data_size = curr_data_size + lenBytes;
  if ((curr_data_size >= send_buf_size) ||
      (curr_data_size >= sci_buffer_remaining)) {
    /**
     * The new message will not fit in the send buffer. We need to
     * send the send buffer before filling it up with the new
     * signal data. If current data size will spill over buffer edge
     * we will also send to ensure correct operation.
     */  
    if (!doSend()) { 
      /**
       * We were not successfull sending, report 0 as meaning buffer full and
       * upper levels handle retries and other recovery matters.
       */
      return 0;
    }
  }
  /**
   * New signal fits, simply fill it up with more data.
   */
  Uint32 sz = m_sendBuffer.m_dataSize;
  return &m_sendBuffer.m_buffer[sz];
}

void
SCI_Transporter::updateWritePtr(Uint32 lenBytes, Uint32 prio){
  
  Uint32 sz = m_sendBuffer.m_dataSize;
  Uint32 packet_size = m_PacketSize;
  sz += ((lenBytes + 3) >> 2);
  m_sendBuffer.m_dataSize = sz;
  
  if(sz > packet_size) { 
    /**------------------------------------------------- 
     * Buffer is full and we are ready to send. We will 
     * not wait since the signal is already in the buffer. 
     * Force flag set has the same indication that we 
     * should always send. If it is not possible to send 
     * we will not worry since we will soon be back for 
     * a renewed trial. 
     *------------------------------------------------- 
     */ 
    doSend();
  }
}

enum SciStatus {
  SCIDISCONNECT = 1,
  SCICONNECTED  = 2
};
 
bool 
SCI_Transporter::getConnectionStatus() { 
  if(*m_localStatusFlag == SCICONNECTED &&  
     (*m_remoteStatusFlag == SCICONNECTED || 
     ((m_adapters > 1) &&
      *m_remoteStatusFlag2 == SCICONNECTED))) 
    return true; 
  else 
    return false; 
} 
 
void  
SCI_Transporter::setConnected() { 
  *m_remoteStatusFlag = SCICONNECTED; 
  if (m_adapters > 1) {
    *m_remoteStatusFlag2 = SCICONNECTED; 
  }
  *m_localStatusFlag = SCICONNECTED; 
} 
 
void  
SCI_Transporter::setDisconnect() { 
  if(getLinkStatus(m_ActiveAdapterId)) 
    *m_remoteStatusFlag = SCIDISCONNECT; 
  if (m_adapters > 1) {
    if(getLinkStatus(m_StandbyAdapterId)) 
      *m_remoteStatusFlag2 = SCIDISCONNECT; 
  }
} 
 
bool 
SCI_Transporter::checkConnected() { 
  if (*m_localStatusFlag == SCIDISCONNECT) { 
    return false; 
  } 
  else 
    return true; 
} 
 
static bool init = false; 
 
bool  
SCI_Transporter::initSCI() { 
  DBUG_ENTER("SCI_Transporter::initSCI");
  if(!init){ 
    sci_error_t error; 
    // Initialize SISCI library 
    SCIInitialize(0, &error); 
    if(error != SCI_ERR_OK)  { 
      DBUG_PRINT("error", ("Cannot initialize SISCI library."));
      DBUG_PRINT("error",
      ("Inconsistency between SISCI library and SISCI driver. Error code 0x%x",
      error)); 
      DBUG_RETURN(false);
    } 
    init = true; 
  } 
  DBUG_RETURN(true);
} 
 
Uint32
SCI_Transporter::get_free_buffer() const
{
  return (m_TargetSegm[m_ActiveAdapterId].writer)->get_free_buffer();
}

