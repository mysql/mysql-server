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

#include "SCI_Transporter.hpp" 
#include <NdbOut.hpp> 
#include <NdbSleep.h> 
#include <NdbTick.h> 
#include <NdbTick.h> 

#include "TransporterInternalDefinitions.hpp" 
#include <TransporterCallback.hpp> 
 
#define FLAGS 0  
 
SCI_Transporter::SCI_Transporter(Uint32 packetSize,         
				 Uint32 bufferSize,       
				 Uint32 nAdapters, 
				 Uint16 remoteSciNodeId0,        
				 Uint16 remoteSciNodeId1, 
				 NodeId _localNodeId,      
				 NodeId _remoteNodeId,     
				 int byte_order, 
				 bool compr,  
				 bool chksm,  
				 bool signalId, 
				 Uint32 reportFreq) :  
  Transporter(_localNodeId, _remoteNodeId, byte_order, compr, chksm, signalId) 
{ 
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
  m_remoteNodes= new Uint16[m_numberOfRemoteNodes]; 
  if(m_remoteNodes == NULL) { 
    //DO WHAT?? 
  } 
  m_swapCounter=0; 
  m_failCounter=0; 
  m_remoteNodes[0]=remoteSciNodeId0; 
  m_remoteNodes[1]=remoteSciNodeId1; 
  m_adapters = nAdapters;   
  // The maximum number of times to try and create,  
  // start and destroy a sequence 
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
   
} 
 
 
 
void SCI_Transporter::disconnectImpl() 
{ 
  sci_error_t err; 
  if(m_mapped){ 
    setDisconnect(); 
#ifdef DEBUG_TRANSPORTER 
    ndbout << "DisconnectImpl " << getConnectionStatus() << endl; 
    ndbout << "remote node " << remoteNodeId << endl; 
#endif 
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
	reportError(callbackObj, localNodeId, TE_SCI_UNABLE_TO_CLOSE_CHANNEL); 
#ifdef DEBUG_TRANSPORTER 
	fprintf(stderr,  
		"\nCannot close channel to the driver. Error code 0x%x",  
		err); 
#endif
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
  
}  
 
 
bool SCI_Transporter::initTransporter() { 
  if(m_BufferSize < (2*MAX_MESSAGE_SIZE)){ 
    m_BufferSize = 2 * MAX_MESSAGE_SIZE; 
  } 

  // Allocate buffers for sending 
  Uint32 sz = 0;
  if(m_BufferSize < (m_PacketSize * 4)){
    sz = m_BufferSize + MAX_MESSAGE_SIZE;
  } else {
    /**
     * 3 packages
     */
    sz = (m_PacketSize * 4) * 3 + MAX_MESSAGE_SIZE;
  }
  
  m_sendBuffer.m_bufferSize = 4 * ((sz + 3) / 4); 
  m_sendBuffer.m_buffer = new Uint32[m_sendBuffer.m_bufferSize / 4];
  m_sendBuffer.m_dataSize = 0;
  
  if(!getLinkStatus(m_ActiveAdapterId) ||  
     !getLinkStatus(m_StandbyAdapterId)) { 
#ifdef DEBUG_TRANSPORTER 
    ndbout << "The link is not fully operational. " << endl; 
    ndbout << "Check the cables and the switches" << endl; 
#endif 
    //reportDisconnect(remoteNodeId, 0); 
    //doDisconnect(); 
    //NDB should terminate 
    reportError(callbackObj, localNodeId, TE_SCI_LINK_ERROR); 
    return false; 
  } 
  
  return true; 
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
#ifdef DEBUG_TRANSPORTER 
    ndbout << "error querying adapter " << endl; 
#endif 
	return false; 
  } 
  if(linkstatus<=0) 
    return false; 
  return true; 
} 
 
 
 
sci_error_t SCI_Transporter::initLocalSegment() { 
  Uint32 segmentSize = m_BufferSize; 
  Uint32 offset  = 0; 
  sci_error_t err; 
  if(!m_sciinit) { 
    for(Uint32 i=0; i<m_adapters ; i++) { 
      SCIOpen(&(sciAdapters[i].scidesc), FLAGS, &err); 
      sciAdapters[i].localSciNodeId=getLocalNodeId(i); 
#ifdef DEBUG_TRANSPORTER 
      ndbout_c("SCInode iD %d  adapter %d\n",  
	       sciAdapters[i].localSciNodeId, i); 
#endif 
      if(err != SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
	ndbout_c("\nCannot open an SCI virtual device. Error code 0x%x", 
		 err); 
#endif 
	return err; 
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
    return err; 
  } else { 
#ifdef DEBUG_TRANSPORTER 
    ndbout << "created segment id : "  
	   <<  hostSegmentId(localNodeId, remoteNodeId) << endl; 
#endif 
  } 
   
  /** Prepare the segment*/ 
  for(Uint32 i=0; i < m_adapters; i++) { 
    SCIPrepareSegment((m_SourceSegm[0].localHandle),  
		      i, 
		      FLAGS, 
		      &err); 
     
    if(err != SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
      ndbout_c("Local Segment is not accessible by an SCI adapter."); 
      ndbout_c("Error code 0x%x\n", err); 
#endif 
      return err; 
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
	   
#ifdef DEBUG_TRANSPORTER 
    fprintf(stderr, "\nCannot map area of size %d. Error code 0x%x", 
	    segmentSize,err); 
    ndbout << "initLocalSegment does a disConnect" << endl; 
#endif 
    doDisconnect(); 
    return err; 
  } 
  
  
  /** Make the local segment available*/ 
  for(Uint32 i=0; i < m_adapters; i++) { 
    SCISetSegmentAvailable((m_SourceSegm[0].localHandle),  
			     i, 
			   FLAGS, 
			   &err); 
     
    if(err != SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
      ndbout_c("\nLocal Segment is not available for remote connections."); 
      ndbout_c("Error code 0x%x\n", err); 
#endif 
      return err; 
    } 
  } 
  
  
  setupLocalSegment(); 
  
  return err; 
   
} // initLocalSegment() 
 
 
bool SCI_Transporter::doSend() { 
#ifdef DEBUG_TRANSPORTER  
  NDB_TICKS startSec=0, stopSec=0; 
  Uint32 startMicro=0, stopMicro=0, totalMicro=0; 
#endif
  sci_error_t             err; 
  Uint32 retry=0; 
 
  const char * const sendPtr = (char*)m_sendBuffer.m_buffer;
  const Uint32 sizeToSend    = m_sendBuffer.m_dataSize;
  
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
    if(startSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
      ndbout << "Start sequence failed" << endl; 
#endif 
      reportError(callbackObj, remoteNodeId, TE_SCI_UNABLE_TO_START_SEQUENCE); 
      return false; 
    } 
    
      
  tryagain:		 
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
      
      
      if(err == SCI_ERR_OUT_OF_RANGE) { 
#ifdef DEBUG_TRANSPORTER 
	ndbout << "Data transfer : out of range error \n" << endl; 
#endif 
	goto tryagain; 
      } 
      if(err == SCI_ERR_SIZE_ALIGNMENT) { 
#ifdef DEBUG_TRANSPORTER 
	ndbout << "Data transfer : aligne\n" << endl; 
#endif 
	goto tryagain; 
      } 
      if(err == SCI_ERR_OFFSET_ALIGNMENT) { 
#ifdef DEBUG_TRANSPORTER           
	ndbout << "Data transfer : offset alignment\n" << endl; 
#endif 
	goto tryagain; 
      }    
      if(err == SCI_ERR_TRANSFER_FAILED) { 
	//(m_TargetSegm[m_StandbyAdapterId].writer)->heavyLock(); 
	if(getLinkStatus(m_ActiveAdapterId)) { 
	  retry++; 
	  if(retry>3) { 
	    reportError(callbackObj, 
			remoteNodeId, TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
	    return false; 
	  } 
	  goto tryagain; 
	}
	m_failCounter++; 
	Uint32 temp=m_ActiveAdapterId;	    	     
	switch(m_swapCounter) { 
	case 0:  
	  /**swap from active (0) to standby (1)*/ 
	  if(getLinkStatus(m_StandbyAdapterId)) { 
#ifdef DEBUG_TRANSPORTER	 
	    ndbout << "Swapping from 0 to 1 " << endl; 
#endif 
	    failoverShmWriter();		 
	    SCIStoreBarrier(m_TargetSegm[m_StandbyAdapterId].sequence,0); 
	    m_ActiveAdapterId=m_StandbyAdapterId; 
	    m_StandbyAdapterId=temp; 
	    SCIRemoveSequence((m_TargetSegm[m_StandbyAdapterId].sequence),
			      FLAGS,  
			      &err); 
	    if(err!=SCI_ERR_OK) { 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_REMOVE_SEQUENCE); 
	      return false; 
	    } 
	    if(startSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
	      ndbout << "Start sequence failed" << endl; 
#endif 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_START_SEQUENCE); 
	      return false; 
	    } 
	    m_swapCounter++; 
#ifdef DEBUG_TRANSPORTER 
	    ndbout << "failover complete.." << endl; 
#endif 
	    goto tryagain; 
	  }  else {
	    reportError(callbackObj, 
			remoteNodeId, TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
	    return false;
	  }
	  return false; 
	  break; 
	case 1: 
	  /** swap back from 1 to 0 
	      must check that the link is up */ 
	  
	  if(getLinkStatus(m_StandbyAdapterId)) { 
	    failoverShmWriter(); 
	    m_ActiveAdapterId=m_StandbyAdapterId; 
	    m_StandbyAdapterId=temp; 
#ifdef DEBUG_TRANSPORTER 
	    ndbout << "Swapping from 1 to 0 " << endl;	 
#endif 
	    if(createSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
	      return false; 
	    } 
	    if(startSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
	      ndbout << "startSequence failed... disconnecting" << endl; 
#endif 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_START_SEQUENCE); 
	      return false; 
	    } 
	    
	    SCIRemoveSequence((m_TargetSegm[m_StandbyAdapterId].sequence) 
			      , FLAGS,  
			      &err); 
	    if(err!=SCI_ERR_OK) { 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_REMOVE_SEQUENCE); 
	      return false;
	    } 
	    
	    if(createSequence(m_StandbyAdapterId)!=SCI_ERR_OK) { 
	      reportError(callbackObj, 
			  remoteNodeId, TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
	      return false; 
	    } 
	    
	    m_swapCounter=0; 
	    
#ifdef DEBUG_TRANSPORTER 
	    ndbout << "failover complete.." << endl; 
#endif 
	    goto tryagain; 
	    
	  } else {
	    reportError(callbackObj, 
			remoteNodeId, TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR);
	    return false;
	  }
	  
	  break; 
	default: 
	  reportError(callbackObj, 
		      remoteNodeId, TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR); 
	  return false; 
	  break; 
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
#ifdef DEBUG_TRANSPORTER 
      ndbout << "the segment is full for some reason" << endl; 
#endif 
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
 
   Uint32 sharedSize = 0; 
   sharedSize += 16; //SHM_Reader::getSharedSize(); 
   sharedSize += 16; //SHM_Writer::getSharedSize(); 
   sharedSize += 32; //SHM_Writer::getSharedSize(); 
   sharedSize =4096;   //start of the buffer is page aligend 
    
   Uint32 sizeOfBuffer = m_BufferSize; 
 
   sizeOfBuffer -= sharedSize; 
 
   Uint32 * localReadIndex =  
     (Uint32*)m_SourceSegm[m_ActiveAdapterId].mappedMemory;  
   Uint32 * localWriteIndex =  
     (Uint32*)(localReadIndex+ 1); 
    
   Uint32 * localEndOfDataIndex = (Uint32*) 
     (localReadIndex + 2); 
 
   m_localStatusFlag = (Uint32*)(localReadIndex + 3); 
 
   Uint32 * sharedLockIndex = (Uint32*) 
     (localReadIndex + 4); 
 
   Uint32 * sharedHeavyLock = (Uint32*) 
     (localReadIndex + 5); 
 
   char * localStartOfBuf = (char*)  
     ((char*)m_SourceSegm[m_ActiveAdapterId].mappedMemory+sharedSize); 
 
     
   * localReadIndex = * localWriteIndex = 0; 
   * localEndOfDataIndex = sizeOfBuffer -1; 
 
   const Uint32 slack = MAX_MESSAGE_SIZE;

   reader = new SHM_Reader(localStartOfBuf,  
			   sizeOfBuffer, 
			   slack,
			   localReadIndex, 
			   localWriteIndex);
    
   * localReadIndex = 0; 
   * localWriteIndex = 0; 
    
   reader->clear(); 
} //setupLocalSegment 
 
 
 
void SCI_Transporter::setupRemoteSegment()   
{ 
   Uint32 sharedSize = 0; 
   sharedSize += 16; //SHM_Reader::getSharedSize(); 
   sharedSize += 16; //SHM_Writer::getSharedSize(); 
   sharedSize += 32;    
   sharedSize =4096;   //start of the buffer is page aligend 
 
 
   Uint32 sizeOfBuffer = m_BufferSize; 
   sizeOfBuffer -= sharedSize; 
   Uint32 * segPtr = (Uint32*) m_TargetSegm[m_StandbyAdapterId].mappedMemory ; 
    
   Uint32 * remoteReadIndex2 = (Uint32*)segPtr;  
   Uint32 * remoteWriteIndex2 = (Uint32*) (segPtr + 1); 
   Uint32 * remoteEndOfDataIndex2 = (Uint32*) (segPtr + 2); 
   Uint32 * sharedLockIndex2 = (Uint32*) (segPtr + 3); 
   m_remoteStatusFlag2 = (Uint32*)(segPtr + 4); 
   Uint32 * sharedHeavyLock2 = (Uint32*) (segPtr + 5); 
    
    
   char * remoteStartOfBuf2 = ( char*)((char *)segPtr+sharedSize); 
    
   segPtr = (Uint32*) m_TargetSegm[m_ActiveAdapterId].mappedMemory ;   
    
   Uint32 * remoteReadIndex = (Uint32*)segPtr;  
   Uint32 * remoteWriteIndex = (Uint32*) (segPtr + 1); 
   Uint32 * remoteEndOfDataIndex = (Uint32*) (segPtr + 2); 
   Uint32 * sharedLockIndex = (Uint32*) (segPtr + 3); 
   m_remoteStatusFlag = (Uint32*)(segPtr + 4); 
   Uint32 * sharedHeavyLock = (Uint32*) (segPtr + 5); 
    
   char * remoteStartOfBuf = ( char*)((char*)segPtr+(sharedSize)); 
    
   * remoteReadIndex = * remoteWriteIndex = 0; 
   * remoteReadIndex2 = * remoteWriteIndex2 = 0; 
   * remoteEndOfDataIndex = sizeOfBuffer - 1; 
   * remoteEndOfDataIndex2 = sizeOfBuffer - 1; 
 
   /** 
    * setup two writers. writer2 is used to mirror the changes of 
    * writer on the standby 
    * segment, so that in the case of a failover, we can switch 
    * to the stdby seg. quickly.* 
    */ 
   const Uint32 slack = MAX_MESSAGE_SIZE;
 
   writer = new SHM_Writer(remoteStartOfBuf,  
			   sizeOfBuffer, 
			   slack,
			   remoteReadIndex, 
			   remoteWriteIndex);
   
   writer2 = new SHM_Writer(remoteStartOfBuf2,  
			    sizeOfBuffer, 
			    slack,
			    remoteReadIndex2, 
			    remoteWriteIndex2);
 
   * remoteReadIndex = 0; 
   * remoteWriteIndex = 0; 
    
   writer->clear(); 
   writer2->clear(); 
    
   m_TargetSegm[0].writer=writer; 
   m_TargetSegm[1].writer=writer2; 
 
   m_sendBuffer.m_forceSendLimit = writer->getBufferSize();
    
   if(createSequence(m_ActiveAdapterId)!=SCI_ERR_OK) { 
     reportThreadError(remoteNodeId, TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
     doDisconnect(); 
   } 
   if(createSequence(m_StandbyAdapterId)!=SCI_ERR_OK) { 
     reportThreadError(remoteNodeId, TE_SCI_UNABLE_TO_CREATE_SEQUENCE); 
     doDisconnect(); 
   } 
   
 
} //setupRemoteSegment 
 
 
bool SCI_Transporter::connectImpl(Uint32 timeout) { 
 
  sci_error_t err; 
  Uint32 offset      = 0; 
 
  if(!m_initLocal) { 
    if(initLocalSegment()!=SCI_ERR_OK){ 
      NdbSleep_MilliSleep(timeout); 
      //NDB SHOULD TERMINATE AND COMPUTER REBOOTED! 
      reportThreadError(localNodeId, TE_SCI_CANNOT_INIT_LOCALSEGMENT); 
      return false; 
    } 
    m_initLocal=true; 
  } 
 
  if(!m_mapped ) { 
 
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
	NdbSleep_MilliSleep(timeout); 
	return false; 
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
#ifdef DEBUG_TRANSPORTER 
	  ndbout_c("\nCannot map a segment to the remote node %d.");  
	  ndbout_c("Error code 0x%x",m_RemoteSciNodeId, err); 
#endif 
	  //NDB SHOULD TERMINATE AND COMPUTER REBOOTED! 
	  reportThreadError(remoteNodeId, TE_SCI_CANNOT_MAP_REMOTESEGMENT); 
	  return false; 
	} 
	
 
    } 
    m_mapped=true; 
    setupRemoteSegment(); 
    setConnected(); 
#ifdef DEBUG_TRANSPORTER 
    ndbout << "connected and mapped to segment : " << endl; 
    ndbout << "remoteNode: " << m_remoteNodes[0] << endl; 
    ndbout << "remoteNode: " << m_remotenodes[1] << endl; 
    ndbout << "remoteSegId: "  
	   << remoteSegmentId(localNodeId, remoteNodeId)  
	   << endl; 
#endif 
    return true; 
  } 
  else { 
    return getConnectionStatus(); 
  } 
} // connectImpl() 
 

sci_error_t SCI_Transporter::createSequence(Uint32 adapterid) { 
  sci_error_t err; 
  SCICreateMapSequence((m_TargetSegm[adapterid].rhm[adapterid].map),  
		       &(m_TargetSegm[adapterid].sequence),  
		       SCI_FLAG_FAST_BARRIER,  
		       &err);  
  
  
  return err; 
} // createSequence()  
 
 
sci_error_t SCI_Transporter::startSequence(Uint32 adapterid) { 
  
  sci_error_t err; 
  /** Perform preliminary error check on an SCI adapter before starting a 
   * sequence of read and write operations on the mapped segment. 
   */ 
  m_SequenceStatus = SCIStartSequence( 
				       (m_TargetSegm[adapterid].sequence),  
				       FLAGS, &err); 
   
   
  // If there still is an error then data cannot be safely send 
	return err; 
} // startSequence() 
 
   
 
bool SCI_Transporter::disconnectLocal()  
{ 
  sci_error_t err; 
  m_ActiveAdapterId=0; 
 
  /** Free resources used by a local segment 
   */ 
 
  SCIUnmapSegment(m_SourceSegm[0].lhm[0].map,0,&err); 
  	if(err!=SCI_ERR_OK) { 
		reportError(callbackObj, 
			    remoteNodeId, TE_SCI_UNABLE_TO_UNMAP_SEGMENT); 
		return false; 
	} 
 
  SCIRemoveSegment((m_SourceSegm[m_ActiveAdapterId].localHandle), 
		   FLAGS, 
		   &err); 
  
  if(err!=SCI_ERR_OK) { 
    reportError(callbackObj, remoteNodeId, TE_SCI_UNABLE_TO_REMOVE_SEGMENT); 
    return false; 
  } 
  
  if(err == SCI_ERR_OK) { 
#ifdef DEBUG_TRANSPORTER 
    printf("Local memory segment is unmapped and removed\n" ); 
#endif
  } 
  return true; 
} // disconnectLocal() 
 
 
bool SCI_Transporter::disconnectRemote()  { 
  sci_error_t err; 
  for(Uint32 i=0; i<m_adapters; i++) { 
    /** 
     * Segment unmapped, disconnect from the remotely connected segment 
     */   
    SCIUnmapSegment(m_TargetSegm[i].rhm[i].map,0,&err); 
    if(err!=SCI_ERR_OK) { 
		reportError(callbackObj, 
			    remoteNodeId, TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT); 
		return false; 
	} 
	 
    SCIDisconnectSegment(m_TargetSegm[i].rhm[i].remoteHandle, 
			 FLAGS, 
			 &err); 
    if(err!=SCI_ERR_OK) { 
      reportError(callbackObj, 
		  remoteNodeId, TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT); 
      return false; 
    } 
#ifdef DEBUG_TRANSPORTER 
    ndbout_c("Remote memory segment is unmapped and disconnected\n" ); 
#endif 
  } 
  return true; 
} // disconnectRemote() 


SCI_Transporter::~SCI_Transporter() { 
  // Close channel to the driver 
#ifdef DEBUG_TRANSPORTER 
  ndbout << "~SCITransporter does a disConnect" << endl; 
#endif 
  doDisconnect(); 
  if(m_sendBuffer.m_buffer != NULL)
    delete[] m_sendBuffer.m_buffer;
} // ~SCI_Transporter() 
 
 
 
 
void SCI_Transporter::closeSCI() { 
  // Termination of SCI 
  sci_error_t err; 
  printf("\nClosing SCI Transporter...\n"); 
   
  // Disconnect and remove remote segment 
  disconnectRemote(); 
 
  // Unmap and remove local segment 
   
  disconnectLocal(); 
   
  // Closes an SCI virtual device 
  SCIClose(activeSCIDescriptor, FLAGS, &err);  
   
  if(err != SCI_ERR_OK)  
    fprintf(stderr,  
	    "\nCannot close SCI channel to the driver. Error code 0x%x",  
	   err); 
  SCITerminate(); 
} // closeSCI() 
 
Uint32 *
SCI_Transporter::getWritePtr(Uint32 lenBytes, Uint32 prio){

  if(m_sendBuffer.full()){
    /**------------------------------------------------- 
     * Buffer was completely full. We have severe problems. 
     * ------------------------------------------------- 
     */ 
    if(!doSend()){ 
      return 0;
    }
  }

  Uint32 sz = m_sendBuffer.m_dataSize;
  return &m_sendBuffer.m_buffer[sz];
}

void
SCI_Transporter::updateWritePtr(Uint32 lenBytes, Uint32 prio){
  
  Uint32 sz = m_sendBuffer.m_dataSize;
  sz += (lenBytes / 4);
  m_sendBuffer.m_dataSize = sz;
  
  if(sz > m_PacketSize) { 
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
      *m_remoteStatusFlag2 == SCICONNECTED)) 
    return true; 
  else 
    return false; 
} 
 
 
void  
SCI_Transporter::setConnected() { 
  *m_remoteStatusFlag = SCICONNECTED; 
  *m_remoteStatusFlag2 = SCICONNECTED; 
  *m_localStatusFlag = SCICONNECTED; 
} 
 
 
void  
SCI_Transporter::setDisconnect() { 
  if(getLinkStatus(m_ActiveAdapterId)) 
    *m_remoteStatusFlag = SCIDISCONNECT; 
  if(getLinkStatus(m_StandbyAdapterId)) 
    *m_remoteStatusFlag2 = SCIDISCONNECT; 
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
  if(!init){ 
    sci_error_t error; 
    // Initialize SISCI library 
    SCIInitialize(0, &error); 
    if(error != SCI_ERR_OK)  { 
#ifdef DEBUG_TRANSPORTER 
      ndbout_c("\nCannot initialize SISCI library."); 
      ndbout_c("\nInconsistency between SISCI library and SISCI driver.Error code 0x%x", error); 
#endif 
      return false; 
    } 
    init = true; 
  } 
  return true; 
} 
 
 
 
 
 
