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

#ifndef SCI_Transporter_H 
#define SCI_Transporter_H 
#include "Transporter.hpp" 
#include "SHM_Buffer.hpp" 
 
 
#include <sisci_api.h> 
#include <sisci_error.h> 
#include <sisci_types.h> 
 
#include <ndb_types.h> 
 
/** 
 *  The SCI Transporter 
 * 
 *  The design goal of the SCI transporter is to deliver high performance  
 *  data transfers (low latency, high bandwidth) combined with very high  
 *  availability (failover support).  
 *  High performance is an inherit feature of SCI and the, whereas failover  
 *  support is implemented at the application level.  
 *  In SCI the programming model is similar to the shared memory paradigm.  
 *  A process on one node (A) allocates a memory segment and import the 
 *  segment to  its virtual address space. Another node (B) can connect to  
 *  the segment and map this segment into its virtual address space.  
 *  If A writes data to the segment, then B can read it and vice versa, through 
 *  ordinary loads and stores. This is also called PIO (programmable IO), and  
 *  is one thing that distinguish SCI from other interconnects such as, 
 *  ethernet, Gig-e, Myrinet, and Infiniband. By using PIO, lower network  
 *  latency is achieved, compared to the interconnects mentioned above. 
 *  In order for NDB to utilize SCI,  the SCI transporter relies on the  
 *  SISCI api. The SISCI api provides a high level abstraction to the low 
 *  level SCI driver called PCISCI driver. 
 *  The SISCI api provides functions to setup, export, and import 
 *  memory segments in a process virtual address space, and also functions to 
 *  guarantee the correctness of data transfers between nodes. Basically, the  
 *  
 *  In NDB Cluster, each SCI transporter creates a local segment  
 *  that is mapped into the virtual address space. After the creation of the  
 *  local segment, the SCI transporter connects to a segment created by another 
 *  transporter at a remote node, and the maps the remote segment into its  
 *  virtual address space. However, since NDB Cluster relies on redundancy 
 *  at the network level, by using dual SCI adapters communica 
 * 
 * 
 */ 


/**  
 * class SCITransporter 
 * @brief - main class for the SCI transporter. 
 */ 
class SCI_Transporter : public Transporter { 
  friend class TransporterRegistry; 
public:   
 
  /** 
   * Init the transporter. Allocate sendbuffers and open a SCI virtual device 
   * for each adapter. 
   * @return true if successful, otherwize false 
   */ 
  bool initTransporter();                 
   
   
  /** 
   * Creates a sequence for error checking. 
   * @param adapterid the adapter on which to create a new sequence. 
   * @return SCI_ERR_OK if ok, otherwize something else. 
   */ 
  sci_error_t createSequence(Uint32 adapterid);      
   
   
  /** 
   * starts a sequence for error checking. 
   * The actual checking that a sequence is correct is done implicitly 
   * in SCIMemCpy (in doSend).  
   * @param adapterid the adapter on which to start the sequence. 
   * @return SCI_ERR_OK if ok, otherwize something else. 
   */ 
  sci_error_t startSequence(Uint32 adapterid);          
 
 
  /** Initiate Local Segment: create a memory segment, 
   * prepare a memory segment, map the local segment  
   * into  memory space and make segment available. 
   * @return SCI_ERR_OK if ok, otherwize something else. 
   */ 
  sci_error_t initLocalSegment();        
 
  /** 
   * Calculate the segment id for the remote segment 
   * @param localNodeId - local id (e.g. 1 = mgm , 2 = ndb.2 etc.) 
   * @param remoteNodeId - remote id (e.g. 1 = mgm , 2 = ndb.2 etc.) 
   * @return a segment id 
   */ 
  Uint32  remoteSegmentId(Uint16 localNodeId, Uint16 remoteNodeId);     
 
  // Get local segment id (inline) 
  Uint32  hostSegmentId(Uint16 localNodeId, Uint16 remoteNodeId); 
   
  /** 
   * closeSCI closes the SCI virtual device 
   */ 
  void closeSCI();                       
 
 
  /** 
   * Check the status of the remote node, 
   * if it is connected or has disconnected 
   * @return true if connected, otherwize false. 
   */ 
  bool checkConnected(); 
 
  /** 
   * Check if the segment are properly connected to each other (remotely 
   * and locally).  
   * @return True if the both the local segment is mapped and the  
   * remote segment is mapped. Otherwize false. 
   */ 
  bool getConnectionStatus(); 
   
private: 
  SCI_Transporter(TransporterRegistry &t_reg,
                  const char *local_host,
                  const char *remote_host,
                  int port,
                  Uint32 packetSize,  
		  Uint32 bufferSize, 
		  Uint32 nAdapters, 
		  Uint16 remoteSciNodeId0,  
		  Uint16 remoteSciNodeId1,  
		  NodeId localNodeID,  
		  NodeId remoteNodeID,  
		  bool checksum,  
		  bool signalId, 
		  Uint32 reportFreq = 4096); 
 
   /** 
   * Destructor. Disconnects the transporter. 
   */ 
	~SCI_Transporter();    
  bool m_mapped; 
  bool m_initLocal; 
  bool m_sciinit; 
  Uint32 m_swapCounter; 
  Uint32 m_failCounter; 
  /** 
   * For statistics on transfered packets  
   */   
//#ifdef DEBUG_TRANSPORTER 
#if 1
  Uint32 i1024; 
  Uint32 i2048; 
  Uint32 i2049; 
  Uint32 i10242048; 
  Uint32 i20484096; 
  Uint32 i4096; 
  Uint32 i4097; 
#endif
 
  volatile Uint32 * m_localStatusFlag; 
  volatile Uint32 * m_remoteStatusFlag; 
  volatile Uint32 * m_remoteStatusFlag2; 

  struct {
    Uint32 * m_buffer;       // The buffer
    Uint32 m_dataSize;       // No of words in buffer
    Uint32 m_sendBufferSize; // Buffer size
    Uint32 m_forceSendLimit; // Send when buffer is this full
  } m_sendBuffer;

  SHM_Reader * reader; 
  SHM_Writer * writer; 
  SHM_Writer * writer2; 
 
  /** 
   * Statistics 
   */ 
  Uint32 m_reportFreq; 
 
 
  Uint32 m_adapters;   
  Uint32 m_numberOfRemoteNodes; 
 
  Uint16 m_remoteNodes[2]; 
 
  typedef struct SciAdapter { 
    sci_desc_t scidesc; 
    Uint32 localSciNodeId; 
    bool linkStatus; 
  } SciAdapter; 
 
  SciAdapter* sciAdapters; 
  Uint32 m_ActiveAdapterId; 
  Uint32 m_StandbyAdapterId; 
 
  typedef struct sourceSegm { 
    sci_local_segment_t localHandle; // Handle to local segment to be mapped
    struct localHandleMap { 
      sci_map_t map;                   // Handle to the new mapped segment.  
                                       // 2 = max adapters in one node 
    } lhm[2];  
     
    volatile void *mappedMemory; // Used when reading 
  } sourceSegm; 
 
  typedef struct targetSegm { 
    struct remoteHandleMap { 
      sci_remote_segment_t remoteHandle; //Handle to local segment to be mapped
      sci_map_t          map;            //Handle to the new mapped segment 
    } rhm[2]; 
 
    sci_sequence_status_t m_SequenceStatus;    // Used for error checking 
    sci_sequence_t sequence;  
    volatile void * mappedMemory;              // Used when writing 
    SHM_Writer * writer; 
  } targetSegm; 
   
  sci_sequence_status_t m_SequenceStatus;    // Used for error checking 
 
 
  // Shared between all SCI users  active=(either prim or second) 
  sci_desc_t     activeSCIDescriptor;    
  
  sourceSegm*     m_SourceSegm;               // Local segment reference 
  targetSegm*     m_TargetSegm;               // Remote segment reference 
  
  Uint32 m_LocalAdapterId;    // Adapter Id  
  Uint16 m_LocalSciNodeId;    // The SCI-node Id of this machine (adapter 0) 
  Uint16 m_LocalSciNodeId1;   // The SCI-node Id of this machine (adapter 1) 
  Uint16 m_RemoteSciNodeId;   // The SCI-node Id of remote machine (adapter 0) 
  Uint16 m_RemoteSciNodeId1;  // The SCI-node Id of remote machine (adapter 1) 
 
  Uint32 m_PacketSize;        // The size of each data packet 
  Uint32 m_BufferSize;        // Mapped SCI buffer size  
 
  Uint32 * getWritePtr(Uint32 lenBytes, Uint32 prio);
  void updateWritePtr(Uint32 lenBytes, Uint32 prio);

  /** 
   * doSend. Copies the data from the source (the send buffer) to the  
   * shared mem. segment. 
   * Sequences are used for error checking. 
   * If an error occurs, the transfer is retried. 
   * If the link that we need to swap to is broken, we will disconnect.
   * @return Returns true if datatransfer ok. If not retriable 
   * then false is returned. 
   */ 
  bool doSend();   
 
  /** 
   * @param adapterNo  the adapter for which to retrieve the node id. 
   * @return Returns the node id for an adapter. 
   */ 
  Uint32 getLocalNodeId(Uint32 adapterNo); 
             
  bool hasDataToRead() const { 
    return reader->empty() == false;
  } 
 
  bool hasDataToSend() const {
    return m_sendBuffer.m_dataSize > 0;
  }

  /**  
   * Make the local segment unavailable, no new connections will be accepted. 
   * @return Returns true if the segment was successfully disconnected. 
   */ 
  bool disconnectLocal();                   
 
  /**  
   * Make the local segment unavailable, no new connections will be accepted. 
   * @return Returns true if the segment was successfully disconnected. 
   */ 
  bool disconnectRemote();       
   
  void resetToInitialState(); 
             
  /** 
   *  It is always possible to send data with SCI! 
   *  @return True (always) 
   */ 
  bool sendIsPossible(struct timeval * timeout); 
   

  void getReceivePtr(Uint32 ** ptr, Uint32 &size){
    size = reader->getReadPtr(* ptr);
  }

  void updateReceivePtr(Uint32 size){
    reader->updateReadPtr(size);
  }
 
  /** 
   *   Corresponds to SHM_Transporter::setupBuffers() 
   *   Initiates the start pointer of the buffer and read pointers. 
   *   Initiate the localSegment for the SHM reader. 
   */ 
  void setupLocalSegment();   
 
  /** 
   *  Initiate the remoteSegment for the SHM writer 
   */ 
  void setupRemoteSegment();   
 
  /** 
   * Set the connect flag in the remote memory segment (write through) 
   */ 
  void setConnected();   
   
  /** 
   * Set the disconnect flag in the remote memory segment (write through) 
   */ 
  void setDisconnect();   
   
  /** 
   * Check if there is a link between the adapter and the switch 
   * @param adapterNo  the adapter for which to retrieve the link status. 
   * @return Returns true if there is a link between adapter and switch. 
   * Otherwize false is returned and the cables must be checked. 
   */ 
  bool getLinkStatus(Uint32 adapterNo); 
 
  /** 
   * failoverShmWriter takes the state of the active writer and inserts into 
   * the standby writer. 
   */ 
  void failoverShmWriter(); 
 
  bool init_local();
  bool init_remote();

protected: 
   
  /** Perform a connection between segment 
   * This is a client node, trying to connect to a remote segment. 
   * @param timeout, the time the connect thread sleeps before  
   * retrying. 
   * @return Returns true on success, otherwize falser 
   */ 
  bool connect_server_impl(NDB_SOCKET_TYPE sockfd);
  bool connect_client_impl(NDB_SOCKET_TYPE sockfd);
 
  /** 
   *  We will disconnect if: 
   *  -# the other node has disconnected from us 
   *  -# unrecoverable error in transmission, on both adapters 
   *  -# if we are shutdown properly 
   */ 
  void disconnectImpl(); 
 
  static bool initSCI(); 
}; 
 
 
/** The theLocalAdapterId combined with the theRemoteNodeId constructs 
 *  (SCI ids)* a unique identifier for the local segment 
 */ 
inline  
Uint32 
SCI_Transporter::hostSegmentId(Uint16 SciLocalNodeId,  
			       Uint16 SciRemoteNodeId) { 
 
  return (SciLocalNodeId << 16) | SciRemoteNodeId;  
} 
 
/** The theLocalAdapterId combined with the theRemoteNodeId constructs 
 *  (SCI ids)* a unique identifier for the remote segment 
 */ 
inline  
Uint32 
SCI_Transporter::remoteSegmentId(Uint16 SciLocalNodeId, 
				 Uint16 SciRemoteNodeId) { 
   
  return (SciRemoteNodeId << 16) | SciLocalNodeId; 
} 
 
 
#endif 
