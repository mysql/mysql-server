/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SUMA_H
#define SUMA_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include <NodeBitmask.hpp>

#include <IntrusiveList.hpp>
#include <KeyTable.hpp>
#include <DataBuffer.hpp>
#include <SignalCounter.hpp>
#include <AttributeHeader.hpp>
#include <AttributeList.hpp>

#include <signaldata/UtilSequence.hpp>
#include <signaldata/SumaImpl.hpp>
#include <ndbapi/NdbDictionary.hpp>
#include <NdbTick.h>

#define JAM_FILE_ID 469


class Suma : public SimulatedBlock {
  BLOCK_DEFINES(Suma);
public:
  Suma(Block_context& ctx);
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

 /**
   * Dict interface
   */
  void execGET_TABINFOREF(Signal* signal);
  void execGET_TABINFO_CONF(Signal* signal);

  void execGET_TABLEID_CONF(Signal* signal);
  void execGET_TABLEID_REF(Signal* signal);

  void execDROP_TAB_CONF(Signal* signal);
  void execALTER_TAB_REQ(Signal* signal);
  void execCREATE_TAB_CONF(Signal* signal);

  void execDICT_LOCK_REF(Signal*);
  void execDICT_LOCK_CONF(Signal*);

  /**
   * Scan interface
   */
  void execSCAN_HBREP(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execKEYINFO20(Signal* signal);
  void execSUB_SYNC_CONTINUE_REF(Signal* signal);
  void execSUB_SYNC_CONTINUE_CONF(Signal* signal);
  
  /**
   * Trigger logging
   */
  void execTRIG_ATTRINFO(Signal* signal);
  void execFIRE_TRIG_ORD(Signal* signal);
  void execFIRE_TRIG_ORD_L(Signal* signal);
  void execSUB_GCP_COMPLETE_REP(Signal* signal);
  void sendSUB_GCP_COMPLETE_REP(Signal* signal);
  Uint32 mark_epoch_inflight(Uint64 gci);
  void unmark_epoch_inflight(Signal* signal, Uint32 inflight_index);
  
  /**
   * DIH signals
   */
  void sendDIH_SCAN_TAB_REQ(Signal *signal,
                            Uint32 synPtrI,
                            Uint32 tableId,
                            Uint32 schemaTransId);
  void sendDIGETNODESREQ(Signal *signal,
                         Uint32 synPtrI,
                         Uint32 tableId,
                         Uint32 fragNo);
  void execDIH_SCAN_TAB_REF(Signal* signal);
  void execDIH_SCAN_TAB_CONF(Signal* signal);
  void execCHECKNODEGROUPSCONF(Signal *signal);
  void execGCP_PREPARE(Signal *signal);

  /**
   * Trigger administration
   */
  void execCREATE_TRIG_IMPL_REF(Signal* signal);
  void execCREATE_TRIG_IMPL_CONF(Signal* signal);
  void execDROP_TRIG_IMPL_REF(Signal* signal);
  void execDROP_TRIG_IMPL_CONF(Signal* signal);
  
  /**
   * continueb
   */
  void execCONTINUEB(Signal* signal);

  void execCREATE_NODEGROUP_IMPL_REQ(Signal*);
  void execDROP_NODEGROUP_IMPL_REQ(Signal*);
public:

  void suma_ndbrequire(bool v);

  // wl4391_todo big enough for now
  // Keep m_fragDesc within 32 bit,
  // m_dummy is used to pass value.
  union FragmentDescriptor { 
    struct  {
      Uint16 m_fragmentNo;
      Uint8 m_lqhInstanceKey;
      Uint8 m_nodeId;
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
    Subscriber() {}
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 nextList;

    union { Uint32 nextPool; Uint32 prevList; };
  };
  typedef Ptr<Subscriber> SubscriberPtr;
  typedef ArrayPool<Subscriber> Subscriber_pool;
  typedef DLList<Subscriber_pool> Subscriber_list;
  typedef LocalDLList<Subscriber_pool> Local_Subscriber_list;

  struct Table;
  friend struct Table;
  typedef Ptr<Table> TablePtr;

  typedef ArrayPool<DataBufferSegment<15> > SyncRecordBuffer_pool;
  typedef DataBuffer<15,SyncRecordBuffer_pool> SyncRecordBuffer;
  typedef LocalDataBuffer<15,SyncRecordBuffer_pool> LocalSyncRecordBuffer;
  struct SyncRecord {
    SyncRecord(Suma& s, SyncRecordBuffer_pool & p)
      : suma(s)
#ifdef ERROR_INSERT
	, cerrorInsert(s.cerrorInsert)
#endif
    {}
    
    void release();

    Uint32 m_senderRef;
    Uint32 m_senderData;

    Uint32 m_subscriptionPtrI;
    Uint32 m_error;
    Uint32 m_requestInfo;

    Uint32 m_frag_cnt; // only scan this many fragments...
    Uint32 m_frag_id;  // only scan this specific fragment...
    Uint32 m_scan_batchsize;
    Uint32 m_tableId;  // redundant...

    /**
     * Fragments
     */
    Uint32 m_scan_cookie;
    SyncRecordBuffer::Head m_fragments;  // Fragment descriptors

    /**
     * Sync data
     */
    Uint32 m_currentFragment;       // Index in tabPtr.p->m_fragments
    Uint32 m_currentNoOfAttributes; // No of attributes for current table
    SyncRecordBuffer::Head m_attributeList; // Attribute if other than default
    SyncRecordBuffer::Head m_boundInfo;  // For range scan
    
    /**
     * Current row 
     * (assumes max 1 concurrent frag scan / syncrecord for LM_Exclusive)
     */
    Uint32 m_sourceInstance;
    Uint32 m_headersSection;
    Uint32 m_dataSection;


    void startScan(Signal*);
    void nextScan(Signal*);
    bool getNextFragment(TablePtr * tab, FragmentDescriptor * fd);
    void completeScan(Signal*, int error= 0);

    Suma & suma;
#ifdef ERROR_INSERT
    UintR &cerrorInsert;
#endif
    BlockNumber number() const { return suma.number(); }
    EmulatedJamBuffer *jamBuffer() const { return suma.jamBuffer(); }
    [[noreturn]] void progError(int line,
                                int cause,
                                const char * extra,
                                const char * check)
    {
      suma.progError(line, cause, extra, check);
    }
    
    Uint32 prevList; Uint32 ptrI;
    union { Uint32 nextPool; Uint32 nextList; };
  };
  typedef ArrayPool<SyncRecord> SyncRecord_pool;
  typedef SLList<SyncRecord_pool> SyncRecord_sllist;
  typedef DLList<SyncRecord_pool> SyncRecord_dllist;
  typedef LocalDLList<SyncRecord_pool> Local_SyncRecord_dllist;
  friend struct SyncRecord;

  struct SubOpRecord
  {
    SubOpRecord() {}

    enum OpType
    {
      R_SUB_START_REQ,
      R_SUB_STOP_REQ,
      R_START_ME_REQ,
      R_API_FAIL_REQ,
      R_SUB_ABORT_START_REQ
    };

    Uint32 m_opType;
    Uint32 m_subPtrI;
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_subscriberRef;
    Uint32 m_subscriberData;

    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
  };
  typedef ArrayPool<SubOpRecord> SubOpRecord_pool;
  typedef DLFifoList<SubOpRecord_pool> SubOpRecord_fifo;
  typedef LocalDLFifoList<SubOpRecord_pool> Local_SubOpRecord_fifo;
  friend struct SubOpRecord;

  struct Subscription
  {
    Uint32 m_seq_no;
    Uint32 m_subscriptionId;
    Uint32 m_subscriptionKey;
    Uint32 m_subscriptionType;
    Uint32 m_schemaTransId;
    Uint16 m_options;

    enum Options {
      REPORT_ALL       = 0x1,
      REPORT_SUBSCRIBE = 0x2,
      MARKED_DROPPED   = 0x4,
      NO_REPORT_DDL    = 0x8
    };

    enum State {
      UNDEFINED,
      DEFINED,
      DEFINING
    };

    enum TriggerState {
      T_UNDEFINED,
      T_CREATING,
      T_DEFINED,
      T_DROPPING,
      T_ERROR
    };

    State m_state;
    TriggerState m_trigger_state;

    Subscriber_list::Head m_subscribers;
    SubOpRecord_fifo::Head m_create_req;
    SubOpRecord_fifo::Head m_start_req;
    SubOpRecord_fifo::Head m_stop_req;
    SyncRecord_dllist::Head m_syncRecords;
    
    Uint32 m_errorCode;
    Uint32 m_outstanding_trigger;
    Uint32 m_triggers[3];

    Uint32 nextList, prevList;
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
  typedef ArrayPool<Subscription> Subscription_pool;
  typedef DLList<Subscription_pool> Subscription_list;
  typedef LocalDLList<Subscription_pool> Local_Subscription_list;
  typedef DLHashTable<Subscription_pool> Subscription_hash;
  typedef KeyTable<Subscription_pool> Subscription_keyhash;

  struct Table {
    Table() { m_tableId = ~0; }
    void release(Suma&);

    Subscription_list::Head m_subscriptions;

    enum State {
      UNDEFINED,
      DEFINING,
      DEFINED,
      DROPPED
    };
    State m_state;

    Uint32 m_ptrI;

    bool parseTable(SegmentedSectionPtr ptr, Suma &suma);
    /**
     * Create triggers
     */
    void createAttributeMask(AttributeMask&, Suma &suma);
    
    union { Uint32 m_tableId; Uint32 key; };
    Uint32 m_schemaVersion;

    Uint32 m_error;
    
    Uint32 m_noOfAttributes;

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

    // copy from Subscription
    Uint32 m_schemaTransId;
  };
  typedef ArrayPool<Table> Table_pool;
  typedef KeyTable<Table_pool> Table_keyhash;
  /**
   * 
   */

  /**
   * Lists
   */
  Table_keyhash c_tables;
  Subscription_hash c_subscriptions;
  
  /**
   * Pools
   */
  Subscriber_pool c_subscriberPool;
  Table_pool c_tablePool;
  Subscription_pool c_subscriptionPool;
  SyncRecord_pool c_syncPool;
  SyncRecordBuffer_pool c_dataBufferPool;
  SubOpRecord_pool c_subOpPool;

  Uint32 c_maxBufferedEpochs;

  NodeBitmask c_failedApiNodes;
  Uint32 c_failedApiNodesState[MAX_NODES];

  /**
   * Functions
   */
  bool removeSubscribersOnNode(Signal *signal, Uint32 nodeId);

  void sendSubIdRef(Signal* signal,Uint32 senderRef,Uint32 senderData,Uint32 errorCode);

  void sendSubCreateRef(Signal* signal, Uint32 ref, Uint32 data, Uint32 error);
  void sendSubStartRef(Signal* signal, Uint32 ref, Uint32 data, Uint32 error);
  void sendSubStopRef(Signal* signal, Uint32 ref, Uint32 data, Uint32 error);
  void report_sub_stop_conf(Signal* signal,
                            Ptr<SubOpRecord> subOpPtr,
                            Ptr<Subscriber> ptr,
                            bool report,
                            Local_Subscriber_list& list);

  void sendSubSyncRef(Signal* signal, Uint32 errorCode);  
  void sendSubRemoveRef(Signal* signal, const SubRemoveReq& ref,
			Uint32 errorCode);
  void sendSubStopReq(Signal* signal, bool unlock= false);

  void completeSubRemove(SubscriptionPtr subPtr);
  
  void send_sub_start_stop_event(Signal *signal,
                                 Ptr<Subscriber> ptr,
                                 NdbDictionary::Event::_TableEvent event,
                                 bool report,
                                 Local_Subscriber_list& list);
  
  Uint32 getFirstGCI(Signal* signal);
  void send_fragmented_SUB_TABLE_DATA_callback(Signal* signal,
                                               Uint32 inflight_index,
                                               Uint32 returnCode);


  void create_triggers(Signal*, Ptr<Subscription>);
  void drop_triggers(Signal*, Ptr<Subscription>);
  void drop_triggers_complete(Signal*, Ptr<Subscription>);

  bool check_sub_start(Uint32 subscriberRef);
  void report_sub_start_conf(Signal* signal, Ptr<Subscription> subPtr);
  void report_sub_start_ref(Signal* signal, Ptr<Subscription> subPtr, Uint32);

  void sub_stop_req(Signal*);
  void check_remove_queue(Signal*, Ptr<Subscription>,
                          Ptr<SubOpRecord>,bool,bool);
  void check_release_subscription(Signal* signal, Ptr<Subscription>);
  void get_tabinfo_ref_release(Signal*, Ptr<Table>);

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
  void execREAD_CONFIG_REQ(Signal* signal);

  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execNDB_STTOR(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execDBINFO_SCANREQ(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execSIGNAL_DROPPED_REP(Signal* signal);
  void execAPI_START_REP(Signal* signal);
  void execAPI_FAILREQ(Signal* signal) ;

  void api_fail_gci_list(Signal*, Uint32 node);
  void api_fail_subscriber_list(Signal*, Uint32 node);
  void api_fail_subscription(Signal*);
  void api_fail_block_cleanup(Signal* signal, Uint32 failedNode);
  void api_fail_block_cleanup_callback(Signal* signal,
                                       Uint32 failedNodeId,
                                       Uint32 elementsCleaned);

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

  void execSTOP_ME_REQ(Signal*);

  void copySubscription(Signal* signal, Subscription_hash::Iterator);
  void sendSubCreateReq(Signal* signal, Ptr<Subscription>);
  void copySubscriber(Signal*, Ptr<Subscription>, Ptr<Subscriber>);
  void abort_start_me(Signal*, Ptr<Subscription>, bool lockowner);

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

  // for LQH transporter overload check
  const NodeBitmask& getSubscriberNodes() const { return c_subscriber_nodes; }

protected:
  virtual bool getParam(const char * param, Uint32 * retVal);

private:
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
    Uint32 m_wait_handover_timeout_ms; // Max time to wait in phase 101 for API nodes to connect
    bool m_wait_handover;
    NDB_TICKS m_wait_handover_expire;
    NDB_TICKS m_wait_handover_message_expire;
    bool m_forced_disconnect_attempted;
    Uint32 m_restart_server_node_id;
    NdbNodeBitmask m_handover_nodes;
  } c_startup;

  /**
   * for graceful shutdown
   */
  struct Shutdown
  {
    bool m_wait_handover;
    Uint32 m_senderRef;
    Uint32 m_senderData;
  } c_shutdown;

  struct Restart
  {
    Uint16 m_abort;
    Uint16 m_waiting_on_self;
    Uint32 m_ref;
    Uint32 m_max_seq;
    Uint32 m_subPtrI;
    Uint32 m_subOpPtrI;
    Uint32 m_bucket; // In c_subscribers hashtable
  } c_restart;

  Uint32 c_current_seq; // Sequence no on subscription(s)
  Uint32 c_outstanding_drop_trig_req;

  NodeBitmask c_connected_nodes;  // (NODE/API) START REP / (API/NODE) FAIL REQ
  NodeBitmask c_subscriber_nodes; // 

  /**
   * for all Suma's to keep track of other Suma's in Node group
   */
  Uint32 c_nodeGroup;
  Uint32 c_noNodesInGroup;
  Uint32 c_nodesInGroup[MAX_REPLICAS];
  NdbNodeBitmask c_nodes_in_nodegroup_mask;  // NodeId's of nodes in nodegroup

  void send_dict_lock_req(Signal* signal, Uint32 state);
  void send_dict_unlock_ord(Signal* signal, Uint32 state);
  void send_start_me_req(Signal* signal);
  void check_start_handover(Signal* signal);
  void check_wait_handover_timeout(Signal* signal);
  void check_wait_handover_message(NDB_TICKS now);
  void send_handover_req(Signal* signal, Uint32 type);

  void calculate_sub_data_stream(Uint16 bucket, Uint16 buckets, Uint16 replicas);
  Uint16 get_sub_data_stream(Uint16 bucket) const;
  Uint32 get_responsible_node(Uint32 B) const;
  Uint32 get_responsible_node(Uint32 B, const NdbNodeBitmask& mask) const;
  bool check_switchover(Uint32 bucket, Uint64 gci);

  void fix_nodegroup();

public:  
  struct Page_pos
  {
    Uint32 m_page_id;
    Uint32 m_page_pos;  
    Uint64 m_max_gci;   // max gci on page
    Uint64 m_last_gci;  // last gci on page
  };
private:
  
  struct Bucket 
  {
    enum {
      BUCKET_STARTING = 0x1  // On starting node
      ,BUCKET_HANDOVER = 0x2 // On running node
      ,BUCKET_TAKEOVER = 0x4 // On takeing over node
      ,BUCKET_RESEND   = 0x8 // On takeing over node
      ,BUCKET_CREATED_SELF  = 0x10 // New nodegroup (me)
      ,BUCKET_CREATED_OTHER = 0x20 // New nodegroup (not me)
      ,BUCKET_CREATED_MASK  = (BUCKET_CREATED_SELF | BUCKET_CREATED_OTHER)
      ,BUCKET_DROPPED_SELF  = 0x40 // New nodegroup (me) uses hi 8 bit for cnt
      ,BUCKET_DROPPED_OTHER = 0x80 // New nodegroup (not me)
      ,BUCKET_DROPPED_MASK  = (BUCKET_DROPPED_SELF | BUCKET_DROPPED_OTHER)
      ,BUCKET_SHUTDOWN = 0x100 // Graceful shutdown
      ,BUCKET_SHUTDOWN_TO = 0x200 // Graceful shutdown
    };
    Uint16 m_state;
    Uint16 m_switchover_node;
    Uint16 m_nodes[MAX_REPLICAS]; 
    Uint16 m_sub_data_stream;
    Uint32 m_buffer_tail;   // Page
    Uint64 m_switchover_gci;
    Uint64 m_max_acked_gci;
    Page_pos m_buffer_head;
  };
  
  struct Buffer_page 
  {
    STATIC_CONST( DATA_WORDS = 8192 - 10);
    STATIC_CONST( GCI_SZ32 = 2 );

    Uint32 _tupdata1;
    Uint32 _tupdata2;
    Uint32 _tupdata3;
    Uint32 _tupdata4;
    Uint32 m_page_state;     // Used by TUP buddy algorithm
    Uint32 m_page_chunk_ptr_i;
    Uint32 m_next_page;      
    Uint32 m_words_used;     // 
    Uint32 m_max_gci_hi;     //
    Uint32 m_max_gci_lo;     //
    Uint32 m_data[DATA_WORDS];
  };
  typedef ArrayPool<Buffer_page> Buffer_page_pool;
  
  STATIC_CONST( NO_OF_BUCKETS = 24 ); // 24 = 4*3*2*1! 
  Uint32 c_no_of_buckets;
  struct Bucket c_buckets[NO_OF_BUCKETS];
  Uint32 c_subscriber_per_node[MAX_NODES];

  STATIC_CONST( BUCKET_MASK_SIZE = (((NO_OF_BUCKETS+31)>> 5)) );
  typedef Bitmask<BUCKET_MASK_SIZE> Bucket_mask;
  Bucket_mask m_active_buckets;
  Bucket_mask m_switchover_buckets;  
  
  void init_buffers();
  Uint32* get_buffer_ptr(Signal*, Uint32 buck, Uint64 gci, Uint32 sz);
  Uint32 seize_page();
  void free_page(Uint32 page_id, Buffer_page* page);
  void out_of_buffer(Signal*);
  void out_of_buffer_release(Signal* signal, Uint32 buck);

  void start_resend(Signal*, Uint32 bucket);
  void resend_bucket(Signal*, Uint32 bucket, Uint64 gci,
		     Uint32 page_pos, Uint64 last_gci);
  void release_gci(Signal*, Uint32 bucket, Uint64 gci);

  Uint64 get_current_gci(Signal*);

  void checkMaxBufferedEpochs(Signal *signal);

  Uint64 m_max_seen_gci;      // FIRE_TRIG_ORD
  Uint64 m_max_sent_gci;      // FIRE_TRIG_ORD -> send
  Uint64 m_last_complete_gci; // SUB_GCP_COMPLETE_REP
  Uint64 m_out_of_buffer_gci;
  Uint32 m_gcp_complete_rep_count;
  bool m_missing_data;

  struct Gcp_record 
  {
    Uint64 m_gci;
    NodeBitmask m_subscribers;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef ArrayPool<Gcp_record> Gcp_record_pool;
  typedef DLCFifoList<Gcp_record_pool> Gcp_record_fifo;
  Gcp_record_pool c_gcp_pool;
  Gcp_record_fifo c_gcp_list;

  struct Page_chunk
  {
    STATIC_CONST( CHUNK_PAGE_SIZE = 32768 );
    STATIC_CONST( PAGES_PER_CHUNK = 16 );

    Uint32 m_page_id;
    Uint32 m_size;
    Uint32 m_free;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef ArrayPool<Page_chunk> Page_chunk_pool;

  Uint32 m_first_free_page;
  Page_chunk_pool c_page_chunk_pool;
  Buffer_page_pool c_page_pool;

#ifdef VM_TRACE
  Uint64 m_gcp_monitor;
#endif

  struct SubGcpCompleteCounter
  {
    Uint64 m_gci;
    Uint32 m_cnt;
    Uint32 m_flags;
  };

  Uint32 m_gcp_rep_cnt;
  /**
    Next complete epoch to send report for.
  */
  Uint32 m_snd_gcp_rep_counter_index;
  /**
    Oldest incomplete epoch.
  */
  Uint32 m_min_gcp_rep_counter_index;
  /**
    Index of next epoch to store.
  */
  Uint32 m_max_gcp_rep_counter_index;

  STATIC_CONST(MAX_LDM_EPOCH_LAG = 50);
  SubGcpCompleteCounter m_gcp_rep_counter[MAX_LDM_EPOCH_LAG];

  Uint32 m_oldest_gcp_inflight_index;
  Uint32 m_newest_gcp_inflight_index;
  SubGcpCompleteCounter m_gcp_inflight[2];

  /* Buffer used in Suma::execALTER_TAB_REQ(). */
  Uint32 b_dti_buf[MAX_WORDS_META_FILE];
  Uint32 b_dti_buf_ref_count;
  Uint64 m_current_gci;

  Uint32 m_startphase;
  Uint32 m_typeOfStart;

  void sendScanSubTableData(Signal* signal, Ptr<SyncRecord>, Uint32);
};

inline
Uint16
Suma::get_sub_data_stream(Uint16 bucket) const
{
  ndbassert(bucket < NO_OF_BUCKETS);
  const Bucket* ptr= c_buckets + bucket;
  return ptr->m_sub_data_stream;
}

#undef JAM_FILE_ID

#endif
