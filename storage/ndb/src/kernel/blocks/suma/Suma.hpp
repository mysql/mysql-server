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

#ifndef SUMA_H
#define SUMA_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include <NodeBitmask.hpp>

#include <SLList.hpp>
#include <DLList.hpp>
#include <DLFifoList.hpp>
#include <KeyTable.hpp>
#include <DataBuffer.hpp>
#include <SignalCounter.hpp>
#include <AttributeHeader.hpp>
#include <AttributeList.hpp>

#include <signaldata/UtilSequence.hpp>
#include <signaldata/SumaImpl.hpp>

class Suma : public SimulatedBlock {
  BLOCK_DEFINES(Suma);
public:
  Suma(const Configuration & conf);
  virtual ~Suma();

  /**
   * Private interface
   */
  void execSUB_CREATE_REQ(Signal* signal);
  void execSUB_REMOVE_REQ(Signal* signal);
  
  void execSUB_START_REQ(Signal* signal);
  void execSUB_STOP_REQ(Signal* signal);
  
  void execSUB_SYNC_REQ(Signal* signal);
  void execSUB_ABORT_SYNC_REQ(Signal* signal);

  void execSUB_STOP_CONF(Signal* signal);
  void execSUB_STOP_REF(Signal* signal);

 /**
   * Dict interface
   */
#if 0
  void execLIST_TABLES_REF(Signal* signal);
  void execLIST_TABLES_CONF(Signal* signal);
#endif
  void execGET_TABINFOREF(Signal* signal);
  void execGET_TABINFO_CONF(Signal* signal);

  void execGET_TABLEID_CONF(Signal* signal);
  void execGET_TABLEID_REF(Signal* signal);

  void execDROP_TAB_CONF(Signal* signal);
  void execALTER_TAB_CONF(Signal* signal);
  void execCREATE_TAB_CONF(Signal* signal);
  /**
   * Scan interface
   */
  void execSCAN_HBREP(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execSUB_SYNC_CONTINUE_REF(Signal* signal);
  void execSUB_SYNC_CONTINUE_CONF(Signal* signal);
  
  /**
   * Trigger logging
   */
  void execTRIG_ATTRINFO(Signal* signal);
  void execFIRE_TRIG_ORD(Signal* signal);
  void execSUB_GCP_COMPLETE_REP(Signal* signal);
  
  /**
   * DIH signals
   */
  void execDI_FCOUNTREF(Signal* signal);
  void execDI_FCOUNTCONF(Signal* signal);
  void execDIGETPRIMREF(Signal* signal);
  void execDIGETPRIMCONF(Signal* signal);

  /**
   * Trigger administration
   */
  void execCREATE_TRIG_REF(Signal* signal);
  void execCREATE_TRIG_CONF(Signal* signal);
  void execDROP_TRIG_REF(Signal* signal);
  void execDROP_TRIG_CONF(Signal* signal);
  
  /**
   * continueb
   */
  void execCONTINUEB(Signal* signal);

public:

  void suma_ndbrequire(bool v);

  typedef DataBuffer<15> TableList;
  
  union FragmentDescriptor { 
    struct  {
      Uint16 m_fragmentNo;
      Uint16 m_nodeId;
    } m_fragDesc;
    Uint32 m_dummy;
  };
  
  /**
   * Used when sending SCAN_FRAG
   */
  union AttributeDescriptor {
    struct {
      Uint16 attrId;
      Uint16 unused;
    } m_attrDesc;
    Uint32 m_dummy;
  };

  struct Subscriber {
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_subPtrI; //reference to subscription
    Uint32 nextList;

    union { Uint32 nextPool; Uint32 prevList; };
  };
  typedef Ptr<Subscriber> SubscriberPtr;

  /**
   * Subscriptions
   */
  class Table;
  typedef Ptr<Table> TablePtr;

  struct SyncRecord {
    SyncRecord(Suma& s, DataBuffer<15>::DataBufferPool & p)
      : m_tableList(p), suma(s)
#ifdef ERROR_INSERT
	, cerrorInsert(s.cerrorInsert)
#endif
    {}
    
    void release();

    Uint32 m_senderRef;
    Uint32 m_senderData;

    Uint32 m_subscriptionPtrI;
    Uint32 m_error;
    Uint32 m_currentTable;
    TableList m_tableList;    // Tables to sync
    TableList::DataBufferIterator m_tableList_it;

    /**
     * Sync data
     */
    Uint32 m_currentFragment;       // Index in tabPtr.p->m_fragments
    DataBuffer<15>::Head m_attributeList; // Attribute if other than default
    DataBuffer<15>::Head m_tabList; // tables if other than default
    
    Uint32 m_currentTableId;        // Current table
    Uint32 m_currentNoOfAttributes; // No of attributes for current table

    void startScan(Signal*);
    void nextScan(Signal*);
    bool getNextFragment(TablePtr * tab, FragmentDescriptor * fd);
    void completeScan(Signal*, int error= 0);

    Suma & suma;
#ifdef ERROR_INSERT
    UintR &cerrorInsert;
#endif
    BlockNumber number() const { return suma.number(); }
    void progError(int line, int cause, const char * extra) { 
      suma.progError(line, cause, extra); 
    }
    
    union { Uint32 nextPool; Uint32 nextList; Uint32 prevList; Uint32 ptrI; };
  };
  friend struct SyncRecord;

  int initTable(Signal *signal,Uint32 tableId, TablePtr &tabPtr,
		Ptr<SyncRecord> syncPtr);
  int initTable(Signal *signal,Uint32 tableId, TablePtr &tabPtr,
		SubscriberPtr subbPtr);
  int initTable(Signal *signal,Uint32 tableId, TablePtr &tabPtr);
  
  void completeOneSubscriber(Signal* signal, TablePtr tabPtr, SubscriberPtr subbPtr);
  void completeAllSubscribers(Signal* signal, TablePtr tabPtr);
  void completeInitTable(Signal* signal, TablePtr tabPtr);

  struct Table {
    Table() { m_tableId = ~0; n_subscribers = 0; }
    void release(Suma&);
    void checkRelease(Suma &suma);

    DLList<Subscriber>::Head c_subscribers;
    DLList<SyncRecord>::Head c_syncRecords;

    enum State {
      UNDEFINED,
      DEFINING,
      DEFINED,
      DROPPED,
      ALTERED
    };
    State m_state;

    Uint32 m_ptrI;
    SubscriberPtr m_drop_subbPtr;

    Uint32 n_subscribers;

    bool parseTable(SegmentedSectionPtr ptr, Suma &suma);
    /**
     * Create triggers
     */
    int setupTrigger(Signal* signal, Suma &suma);
    void completeTrigger(Signal* signal);
    void createAttributeMask(AttributeMask&, Suma &suma);
    
    /**
     * Drop triggers
     */
    void dropTrigger(Signal* signal,Suma&);
    void runDropTrigger(Signal* signal, Uint32 triggerId,Suma&);

    /**
     * Sync meta
     */    
#if 0
    void runLIST_TABLES_CONF(Signal* signal);
#endif
    
    union { Uint32 m_tableId; Uint32 key; };
    Uint32 m_schemaVersion;
    Uint32 m_hasTriggerDefined[3]; // Insert/Update/Delete
    Uint32 m_triggerIds[3]; // Insert/Update/Delete

    Uint32 m_error;
    /**
     * Default order in which to ask for attributes during scan
     *   1) Fixed, not nullable
     *   2) Rest
     */
    DataBuffer<15>::Head m_attributes; // Attribute id's
    
    /**
     * Fragments
     */
    Uint32 m_fragCount;
    DataBuffer<15>::Head m_fragments;  // Fragment descriptors
    
    /**
     * Hash table stuff
     */
    Uint32 nextHash;
    union { Uint32 prevHash; Uint32 nextPool; };
    Uint32 hashValue() const {
      return m_tableId;
    }
    bool equal(const Table& rec) const {
      return m_tableId == rec.m_tableId;
    }
  };

  struct Subscription {
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_subscriptionId;
    Uint32 m_subscriptionKey;
    Uint32 m_subscriptionType;

    enum State {
      UNDEFINED,
      LOCKED,
      DEFINED,
      DROPPED
    };
    State m_state;
    Uint32 n_subscribers;

    Uint32 nextHash;
    union { Uint32 prevHash; Uint32 nextPool; };

    Uint32 hashValue() const {
      return m_subscriptionId + m_subscriptionKey;
    }

    bool equal(const Subscription & s) const {
      return 
	m_subscriptionId == s.m_subscriptionId && 
	m_subscriptionKey == s.m_subscriptionKey;
    }
    /**
     * The following holds the tables included 
     * in the subscription.
     */
    Uint32 m_tableId;
    Uint32 m_table_ptrI;
  };
  typedef Ptr<Subscription> SubscriptionPtr;
    
  /**
   * 
   */
  DLList<Subscriber> c_metaSubscribers;
  DLList<Subscriber> c_removeDataSubscribers;

  /**
   * Lists
   */
  KeyTable<Table> c_tables;
  DLHashTable<Subscription> c_subscriptions;
  
  /**
   * Pools
   */
  ArrayPool<Subscriber> c_subscriberPool;
  ArrayPool<Table> c_tablePool;
  ArrayPool<Subscription> c_subscriptionPool;
  ArrayPool<SyncRecord> c_syncPool;
  DataBuffer<15>::DataBufferPool c_dataBufferPool;

  NodeBitmask c_failedApiNodes;
  
  /**
   * Functions
   */
  bool removeSubscribersOnNode(Signal *signal, Uint32 nodeId);

  bool checkTableTriggers(SegmentedSectionPtr ptr);

  void addTableId(Uint32 TableId,
		  SubscriptionPtr subPtr, SyncRecord *psyncRec);

  void sendSubIdRef(Signal* signal,Uint32 senderRef,Uint32 senderData,Uint32 errorCode);
  void sendSubCreateRef(Signal* signal, Uint32 errorCode);
  void sendSubStartRef(Signal*, SubscriberPtr, Uint32 errorCode, SubscriptionData::Part);
  void sendSubStartRef(Signal* signal, Uint32 errorCode);
  void sendSubStopRef(Signal* signal, Uint32 errorCode);
  void sendSubSyncRef(Signal* signal, Uint32 errorCode);  
  void sendSubRemoveRef(Signal* signal, const SubRemoveReq& ref,
			Uint32 errorCode);
  void sendSubStartComplete(Signal*, SubscriberPtr, Uint32, 
			    SubscriptionData::Part);
  void sendSubStopComplete(Signal*, SubscriberPtr);
  void sendSubStopReq(Signal* signal, bool unlock= false);

  void completeSubRemove(SubscriptionPtr subPtr);

  Uint32 getFirstGCI(Signal* signal);

  /**
   * Table admin
   */
  void convertNameToId( SubscriptionPtr subPtr, Signal * signal);

  /**
   * Public interface
   */
  void execCREATE_SUBSCRIPTION_REQ(Signal* signal);
  void execDROP_SUBSCRIPTION_REQ(Signal* signal);
  
  void execSTART_SUBSCRIPTION_REQ(Signal* signal);
  void execSTOP_SUBSCRIPTION_REQ(Signal* signal);
  
  void execSYNC_SUBSCRIPTION_REQ(Signal* signal);
  void execABORT_SYNC_REQ(Signal* signal);

  /**
   * Framework signals
   */

  void getNodeGroupMembers(Signal* signal);

  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execNDB_STTOR(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execSIGNAL_DROPPED_REP(Signal* signal);
  void execAPI_START_REP(Signal* signal);
  void execAPI_FAILREQ(Signal* signal) ;

  void execSUB_GCP_COMPLETE_ACK(Signal* signal);

  /**
   * Controller interface
   */
  void execSUB_CREATE_REF(Signal* signal);
  void execSUB_CREATE_CONF(Signal* signal);

  void execSUB_DROP_REF(Signal* signal);
  void execSUB_DROP_CONF(Signal* signal);

  void execSUB_START_REF(Signal* signal);
  void execSUB_START_CONF(Signal* signal);

  void execSUB_ABORT_SYNC_REF(Signal* signal);
  void execSUB_ABORT_SYNC_CONF(Signal* signal);

  void execSUMA_START_ME_REQ(Signal* signal);
  void execSUMA_START_ME_REF(Signal* signal);
  void execSUMA_START_ME_CONF(Signal* signal);
  void execSUMA_HANDOVER_REQ(Signal* signal);
  void execSUMA_HANDOVER_REF(Signal* signal);
  void execSUMA_HANDOVER_CONF(Signal* signal);

  /**
   * Subscription generation interface
   */
  void createSequence(Signal* signal);
  void createSequenceReply(Signal* signal,
			   UtilSequenceConf* conf,
			   UtilSequenceRef* ref);
  void execUTIL_SEQUENCE_CONF(Signal* signal);  
  void execUTIL_SEQUENCE_REF(Signal* signal);
  void execCREATE_SUBID_REQ(Signal* signal);
  
  /**
   * for Suma that is restarting another
   */

  struct Restart {
    Restart(Suma& s);

    Suma & suma;
    Uint32 nodeId;

    DLHashTable<Subscription>::Iterator c_subIt;
    KeyTable<Table>::Iterator c_tabIt;

    void progError(int line, int cause, const char * extra) { 
      suma.progError(line, cause, extra); 
    }

    void resetNode(Uint32 sumaRef);
    void runSUMA_START_ME_REQ(Signal*, Uint32 sumaRef);
    void startNode(Signal*, Uint32 sumaRef);

    void createSubscription(Signal* signal, Uint32 sumaRef);
    void nextSubscription(Signal* signal, Uint32 sumaRef);
    void runSUB_CREATE_CONF(Signal* signal);
    void completeSubscription(Signal* signal, Uint32 sumaRef);

    void startSubscriber(Signal* signal, Uint32 sumaRef);
    void nextSubscriber(Signal* signal, Uint32 sumaRef, SubscriberPtr subbPtr);
    void sendSubStartReq(SubscriptionPtr subPtr, SubscriberPtr subbPtr,
			 Signal* signal, Uint32 sumaRef);
    void runSUB_START_CONF(Signal* signal);
    void completeSubscriber(Signal* signal, Uint32 sumaRef);

    void completeRestartingNode(Signal* signal, Uint32 sumaRef);
  } Restart;

private:
  friend class Restart;
  /**
   * Variables
   */
  NodeId c_masterNodeId;
  NdbNodeBitmask c_alive_nodes;
  
  /**
   * for restarting Suma not to start sending data too early
   */
  struct Startup
  {
    bool m_wait_handover;
    Uint32 m_restart_server_node_id;
    NdbNodeBitmask m_handover_nodes;
  } c_startup;
  
  NodeBitmask c_connected_nodes;  // (NODE/API) START REP / (API/NODE) FAIL REQ
  NodeBitmask c_subscriber_nodes; // 

  /**
   * for all Suma's to keep track of other Suma's in Node group
   */
  Uint32 c_nodeGroup;
  Uint32 c_noNodesInGroup;
  Uint32 c_nodesInGroup[MAX_REPLICAS];
  NdbNodeBitmask c_nodes_in_nodegroup_mask;  // NodeId's of nodes in nodegroup

  void send_start_me_req(Signal* signal);
  void check_start_handover(Signal* signal);
  void send_handover_req(Signal* signal);

  Uint32 get_responsible_node(Uint32 B) const;
  Uint32 get_responsible_node(Uint32 B, const NdbNodeBitmask& mask) const;
  bool check_switchover(Uint32 bucket, Uint32 gci);

public:  
  struct Page_pos
  {
    Uint32 m_page_id;
    Uint32 m_page_pos;  
    Uint32 m_max_gci;   // max gci on page
    Uint32 m_last_gci;  // last gci on page
  };
private:
  
  struct Bucket 
  {
    enum {
      BUCKET_STARTING = 0x1  // On starting node
      ,BUCKET_HANDOVER = 0x2 // On running node
      ,BUCKET_TAKEOVER = 0x4 // On takeing over node
      ,BUCKET_RESEND   = 0x8 // On takeing over node
    };
    Uint16 m_state;
    Uint16 m_switchover_node;
    Uint16 m_nodes[MAX_REPLICAS]; 
    Uint32 m_switchover_gci;
    Uint32 m_max_acked_gci;
    Uint32 m_buffer_tail;   // Page
    Page_pos m_buffer_head;
  };
  
  struct Buffer_page 
  {
    STATIC_CONST( DATA_WORDS = 8192 - 5);
    Uint32 m_page_state;     // Used by TUP buddy algorithm
    Uint32 m_page_chunk_ptr_i;
    Uint32 m_next_page;      
    Uint32 m_words_used;     // 
    Uint32 m_max_gci;        //
    Uint32 m_data[DATA_WORDS];
  };
  
  STATIC_CONST( NO_OF_BUCKETS = 24 ); // 24 = 4*3*2*1! 
  Uint32 c_no_of_buckets;
  struct Bucket c_buckets[NO_OF_BUCKETS];
  
  STATIC_CONST( BUCKET_MASK_SIZE = (((NO_OF_BUCKETS+31)>> 5)) );
  typedef Bitmask<BUCKET_MASK_SIZE> Bucket_mask;
  Bucket_mask m_active_buckets;
  Bucket_mask m_switchover_buckets;  
  
  class Dbtup* m_tup;
  void init_buffers();
  Uint32* get_buffer_ptr(Signal*, Uint32 buck, Uint32 gci, Uint32 sz);
  Uint32 seize_page();
  void free_page(Uint32 page_id, Buffer_page* page);
  void out_of_buffer(Signal*);
  void out_of_buffer_release(Signal* signal, Uint32 buck);

  void start_resend(Signal*, Uint32 bucket);
  void resend_bucket(Signal*, Uint32 bucket, Uint32 gci, 
		     Uint32 page_pos, Uint32 last_gci);
  void release_gci(Signal*, Uint32 bucket, Uint32 gci);

  Uint32 m_max_seen_gci;      // FIRE_TRIG_ORD
  Uint32 m_max_sent_gci;      // FIRE_TRIG_ORD -> send
  Uint32 m_last_complete_gci; // SUB_GCP_COMPLETE_REP
  Uint32 m_out_of_buffer_gci;
  Uint32 m_gcp_complete_rep_count;

  struct Gcp_record 
  {
    Uint32 m_gci;
    NodeBitmask m_subscribers;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  ArrayPool<Gcp_record> c_gcp_pool;
  DLFifoList<Gcp_record> c_gcp_list;

  struct Page_chunk
  {
    Uint32 m_page_id;
    Uint32 m_size;
    Uint32 m_free;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };

  Uint32 m_first_free_page;
  ArrayPool<Page_chunk> c_page_chunk_pool;
};

#endif
