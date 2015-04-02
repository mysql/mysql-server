/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_IMPL_HPP
#define NDB_IMPL_HPP

#include <ndb_global.h>
#include "API.hpp"
#include <NdbOut.hpp>
#include <kernel/ndb_limits.h>
#include <NdbTick.h>

#include "NdbQueryOperationImpl.hpp"
#include "ndb_cluster_connection_impl.hpp"
#include "NdbDictionaryImpl.hpp"
#include "ObjectMap.hpp"
#include "trp_client.hpp"
#include "trp_node.hpp"
#include "NdbWaiter.hpp"
#include "WakeupHandler.hpp"

template <class T>
struct Ndb_free_list_t 
{
  Ndb_free_list_t();
  ~Ndb_free_list_t();
  
  int fill(Ndb*, Uint32 cnt);
  T* seize(Ndb*);
  void release(T*);
  void release(Uint32 cnt, T* head, T* tail);
  void clear();
  Uint32 get_sizeof() const { return sizeof(T); }
  T * m_free_list;
  Uint32 m_alloc_cnt, m_free_cnt;
};

/**
 * Private parts of the Ndb object (corresponding to Ndb.hpp in public API)
 */
class NdbImpl : public trp_client
{
public:
  NdbImpl(Ndb_cluster_connection *, Ndb&);
  ~NdbImpl();

  int send_event_report(bool is_poll_owner, Uint32 *data, Uint32 length);
  int send_dump_state_all(Uint32 *dumpStateCodeArray, Uint32 len);
  void set_TC_COMMIT_ACK_immediate(bool flag);
private:
  /**
   * Implementation methods for
   * send_event_report
   * send_dump_state_all
   */
  void init_dump_state_signal(NdbApiSignal *aSignal,
                              Uint32 *dumpStateCodeArray,
                              Uint32 len);
  int send_to_nodes(NdbApiSignal *aSignal,
                    bool is_poll_owner,
                    bool send_to_all);
  int send_to_node(NdbApiSignal *aSignal,
                   Uint32 tNode,
                   bool is_poll_owner);
public:

  Ndb &m_ndb;
  Ndb * m_next_ndb_object, * m_prev_ndb_object;
  
  Ndb_cluster_connection_impl &m_ndb_cluster_connection;
  TransporterFacade * const m_transporter_facade;

  NdbDictionaryImpl m_dictionary;

  // Ensure good distribution of connects
  Uint32 theCurrentConnectIndex;
  Ndb_cluster_connection_node_iter m_node_iter;

  NdbObjectIdMap theNdbObjectIdMap;

  Uint32 theNoOfDBnodes; // The number of DB nodes  
  Uint8 theDBnodes[MAX_NDB_NODES]; // The node number of the DB nodes

 // 1 indicates to release all connections to node 
  Uint32 the_release_ind[MAX_NDB_NODES];

  NdbWaiter             theWaiter;

  WakeupHandler* wakeHandler;

  NdbEventOperationImpl *m_ev_op;

  int m_optimized_node_selection;

  BaseString m_ndbObjectName; // Ndb name
  BaseString m_dbname; // Database name
  BaseString m_schemaname; // Schema name

  BaseString m_prefix; // Buffer for preformatted internal name <db>/<schema>/

  int update_prefix()
  {
    if (!m_prefix.assfmt("%s%c%s%c", m_dbname.c_str(), table_name_separator,
                         m_schemaname.c_str(), table_name_separator))
    {
      return -1;
    }
    return 0;
  }

/*
  We need this friend accessor function to work around a HP compiler problem,
  where template class friends are not working.
*/
  static inline void setNdbError(Ndb &ndb,int code){
    ndb.theError.code = code;
    return;
  }

  bool forceShortRequests;

  static inline void setForceShortRequests(Ndb* ndb, bool val)
  {
    ndb->theImpl->forceShortRequests = val;
  }

  Uint32 get_waitfor_timeout() const {
    return m_ndb_cluster_connection.m_config.m_waitfor_timeout;
  }
  const NdbApiConfig& get_ndbapi_config_parameters() const {
    return m_ndb_cluster_connection.m_config;
  }

  BaseString m_systemPrefix; // Buffer for preformatted for <sys>/<def>/
  
  Uint64 customData;

  Uint64 clientStats[ Ndb::NumClientStatistics ];
  
  inline void incClientStat(const Ndb::ClientStatistics stat, const Uint64 inc) {
    assert(stat < Ndb::NumClientStatistics);
    if (likely(stat < Ndb::NumClientStatistics))
      clientStats[ stat ] += inc;
  };

  inline void decClientStat(const Ndb::ClientStatistics stat, const Uint64 dec) {
    assert(stat < Ndb::NumClientStatistics);
    if (likely(stat < Ndb::NumClientStatistics))
      clientStats[ stat ] -= dec;
  };
  
  inline void setClientStat(const Ndb::ClientStatistics stat, const Uint64 val) {
    assert(stat < Ndb::NumClientStatistics);
    if (likely(stat < Ndb::NumClientStatistics))
      clientStats[ stat ] = val;
  };

  /* We don't record the sent/received bytes of some GSNs as they are 
   * generated constantly and are not targetted to specific
   * Ndb instances.
   * See also TransporterFacade::TRACE_GSN
   */
  static bool recordGSN(Uint32 gsn)
  {
    switch(gsn)
    {
    case GSN_API_REGREQ:
    case GSN_API_REGCONF:
    case GSN_SUB_GCP_COMPLETE_REP:
    case GSN_SUB_GCP_COMPLETE_ACK:
      return false;
    default:
      return true;
    }
  }
  
  /**
   * NOTE free lists must be _after_ theNdbObjectIdMap take
   *   assure that destructors are run in correct order
   * NOTE these has to be in this specific order to make destructor run in
   *      correct order
   */
  Ndb_free_list_t<NdbRecAttr> theRecAttrIdleList;  
  Ndb_free_list_t<NdbApiSignal> theSignalIdleList;
  Ndb_free_list_t<NdbLabel> theLabelList;
  Ndb_free_list_t<NdbBranch> theBranchList;
  Ndb_free_list_t<NdbSubroutine> theSubroutineList;
  Ndb_free_list_t<NdbCall> theCallList;
  Ndb_free_list_t<NdbBlob> theNdbBlobIdleList;
  Ndb_free_list_t<NdbReceiver> theScanList;
  Ndb_free_list_t<NdbLockHandle> theLockHandleList;
  Ndb_free_list_t<NdbIndexScanOperation> theScanOpIdleList;
  Ndb_free_list_t<NdbOperation>  theOpIdleList;  
  Ndb_free_list_t<NdbIndexOperation> theIndexOpIdleList;
  Ndb_free_list_t<NdbTransaction> theConIdleList; 

  /**
   * For some test cases it is necessary to flush out the TC_COMMIT_ACK
   * immediately since we immediately will check that the commit ack
   * marker resource is released.
   */
  bool send_TC_COMMIT_ACK_immediate_flag;

  /**
   * trp_client interface
   */
  virtual void trp_deliver_signal(const NdbApiSignal*,
                                  const LinearSectionPtr p[3]);
  virtual void trp_wakeup();
  virtual void recordWaitTimeNanos(Uint64 nanos);
  // Is node available for running transactions
  bool   get_node_alive(NodeId nodeId) const;
  bool   get_node_stopping(NodeId nodeId) const;
  bool   getIsDbNode(NodeId nodeId) const;
  bool   getIsNodeSendable(NodeId nodeId) const;
  Uint32 getNodeGrp(NodeId nodeId) const;
  Uint32 getNodeSequence(NodeId nodeId) const;
  Uint32 getNodeNdbVersion(NodeId nodeId) const;
  Uint32 getMinDbNodeVersion() const;
  bool check_send_size(Uint32 node_id, Uint32 send_size) const { return true;}

  int sendSignal(NdbApiSignal*, Uint32 nodeId);
  int sendSignal(NdbApiSignal*, Uint32 nodeId,
                 const LinearSectionPtr ptr[3], Uint32 secs);
  int sendSignal(NdbApiSignal*, Uint32 nodeId,
                 const GenericSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(NdbApiSignal*, Uint32 nodeId,
                           const LinearSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(NdbApiSignal*, Uint32 nodeId,
                           const GenericSectionPtr ptr[3], Uint32 secs);
  void* int2void(Uint32 val);
  static NdbReceiver* void2rec(void* val);
  static NdbTransaction* void2con(void* val);
  static NdbOperation* void2rec_op(void* val);
  static NdbIndexOperation* void2rec_iop(void* val);
  NdbTransaction* lookupTransactionFromOperation(const TcKeyConf* conf);
};

#ifdef VM_TRACE
#define TRACE_DEBUG(x) ndbout << x << endl;
#else
#define TRACE_DEBUG(x)
#endif

#define CHECK_STATUS_MACRO \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(-1);}}
#define CHECK_STATUS_MACRO_VOID \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_VOID_RETURN;}}
#define CHECK_STATUS_MACRO_ZERO \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(0);}}
#define CHECK_STATUS_MACRO_NULL \
   {if (checkInitState() == -1) { theError.code = 4100; DBUG_RETURN(NULL);}}

inline
void *
NdbImpl::int2void(Uint32 val)
{
  return theNdbObjectIdMap.getObject(val);
}

inline
NdbReceiver*
NdbImpl::void2rec(void* val)
{
  return (NdbReceiver*)val;
}

inline
NdbTransaction*
NdbImpl::void2con(void* val)
{
  return (NdbTransaction*)val;
}

inline
NdbOperation*
NdbImpl::void2rec_op(void* val)
{
  return (NdbOperation*)(void2rec(val)->getOwner());
}

inline
NdbIndexOperation*
NdbImpl::void2rec_iop(void* val)
{
  return (NdbIndexOperation*)(void2rec(val)->getOwner());
}

inline 
NdbTransaction * 
NdbReceiver::getTransaction(ReceiverType type) const
{
  switch(type)
  {
  case NDB_UNINITIALIZED:
    assert(false);
    return NULL;
  case NDB_QUERY_OPERATION:
    return &((NdbQueryOperationImpl*)m_owner)->getQuery().getNdbTransaction();
  default:
    return ((NdbOperation*)m_owner)->theNdbCon;
  }
}


inline
int
Ndb::checkInitState()
{
  theError.code = 0;

  if (theInitState != Initialised)
    return -1;
  return 0;
}

Uint32 convertEndian(Uint32 Data);

enum LockMode { 
  Read, 
  Update,
  Insert,
  Delete 
};

template<class T>
inline
Ndb_free_list_t<T>::Ndb_free_list_t()
{
  m_free_list= 0; 
  m_alloc_cnt= m_free_cnt= 0; 
}

template<class T>
inline
Ndb_free_list_t<T>::~Ndb_free_list_t()
{
  clear();
}
    
template<class T>
inline
int
Ndb_free_list_t<T>::fill(Ndb* ndb, Uint32 cnt)
{
#ifndef HAVE_VALGRIND
  if (m_free_list == 0)
  {
    m_free_cnt++;
    m_alloc_cnt++;
    m_free_list = new T(ndb);
    if (m_free_list == 0)
    {
      NdbImpl::setNdbError(*ndb, 4000);
      assert(false);
      return -1;
    }
  }
  while(m_alloc_cnt < cnt)
  {
    T* obj= new T(ndb);
    if(obj == 0)
    {
      NdbImpl::setNdbError(*ndb, 4000);
      assert(false);
      return -1;
    }
    obj->next(m_free_list);
    m_free_cnt++;
    m_alloc_cnt++;
    m_free_list = obj;
  }
  return 0;
#else
  return 0;
#endif
}

template<class T>
inline
T*
Ndb_free_list_t<T>::seize(Ndb* ndb)
{
#ifndef HAVE_VALGRIND
  T* tmp = m_free_list;
  if (tmp)
  {
    m_free_list = (T*)tmp->next();
    tmp->next(NULL);
    m_free_cnt--;
    return tmp;
  }
  
  if((tmp = new T(ndb)))
  {
    m_alloc_cnt++;
  }
  else
  {
    NdbImpl::setNdbError(*ndb, 4000);
    assert(false);
  }
  return tmp;
#else
  return new T(ndb);
#endif
}

template<class T>
inline
void
Ndb_free_list_t<T>::release(T* obj)
{
#ifndef HAVE_VALGRIND
  obj->next(m_free_list);
  m_free_list = obj;
  m_free_cnt++;
#else
  delete obj;
#endif
}


template<class T>
inline
void
Ndb_free_list_t<T>::clear()
{
  T* obj = m_free_list;
  while(obj)
  {
    T* curr = obj;
    obj = (T*)obj->next();
    delete curr;
    m_alloc_cnt--;
  }
}

template<class T>
inline
void
Ndb_free_list_t<T>::release(Uint32 cnt, T* head, T* tail)
{
#ifndef HAVE_VALGRIND
  if (cnt)
  {
#ifdef VM_TRACE
    {
      T* tmp = head;
      while (tmp != 0 && tmp != tail) tmp = (T*)tmp->next();
      assert(tmp == tail);
    }
#endif
    tail->next(m_free_list);
    m_free_list = head;
    m_free_cnt += cnt;
  }
#else
  if (cnt)
  {
    T* tmp = head;
    while (tmp != 0 && tmp != tail)
    {
      T * next = (T*)tmp->next();
      delete tmp;
      tmp = next;
    }
    delete tail;
  }
#endif
}

inline
bool
NdbImpl::getIsDbNode(NodeId n) const {
  return
    getNodeInfo(n).defined &&
    getNodeInfo(n).m_info.m_type == NodeInfo::DB;
}

inline
Uint32
NdbImpl::getNodeGrp(NodeId n) const {
  return getNodeInfo(n).m_state.nodeGroup;
}


inline
bool
NdbImpl::get_node_alive(NodeId n) const {
  return getNodeInfo(n).m_alive;
}

inline
bool
NdbImpl::get_node_stopping(NodeId n) const {
  const trp_node & node = getNodeInfo(n);
  assert(node.m_info.getType() == NodeInfo::DB);
  return (!node.m_state.getSingleUserMode() &&
          node.m_state.startLevel >= NodeState::SL_STOPPING_1);
}

inline
bool
NdbImpl::getIsNodeSendable(NodeId n) const {
  const trp_node & node = getNodeInfo(n);
  const Uint32 startLevel = node.m_state.startLevel;
  const NodeInfo::NodeType node_type = node.m_info.getType();
  assert(node_type == NodeInfo::DB ||
         node_type == NodeInfo::MGM);

  return node.compatible && (startLevel == NodeState::SL_STARTED ||
                             startLevel == NodeState::SL_STOPPING_1 ||
                             node.m_state.getSingleUserMode() ||
                             node_type == NodeInfo::MGM);
}

inline
Uint32
NdbImpl::getNodeSequence(NodeId n) const {
  return getNodeInfo(n).m_info.m_connectCount;
}

inline
Uint32
NdbImpl::getNodeNdbVersion(NodeId n) const
{
  return getNodeInfo(n).m_info.m_version;
}

inline
void
NdbImpl::recordWaitTimeNanos(Uint64 nanos)
{
  incClientStat( Ndb::WaitNanosCount, nanos );
}

inline
int
NdbImpl::sendSignal(NdbApiSignal * signal, Uint32 nodeId)
{
  if (getIsNodeSendable(nodeId))
  {
    if (likely(recordGSN(signal->theVerId_signalNumber)))
    {
      incClientStat(Ndb::BytesSentCount, signal->getLength() << 2);
    }
    return raw_sendSignal(signal, nodeId);
  }
  return -1;
}

inline
int
NdbImpl::sendSignal(NdbApiSignal * signal, Uint32 nodeId,
                    const LinearSectionPtr ptr[3], Uint32 secs)
{
  if (getIsNodeSendable(nodeId))
  {
    if (likely(recordGSN(signal->theVerId_signalNumber)))
    {
      incClientStat(Ndb::BytesSentCount,
                    ((signal->getLength() << 2) +
                     ((secs > 2)? ptr[2].sz << 2: 0) + 
                     ((secs > 1)? ptr[1].sz << 2: 0) +
                     ((secs > 0)? ptr[0].sz << 2: 0)));
    }
    return raw_sendSignal(signal, nodeId, ptr, secs);
  }
  return -1;
}

inline
int
NdbImpl::sendSignal(NdbApiSignal * signal, Uint32 nodeId,
                    const GenericSectionPtr ptr[3], Uint32 secs)
{
  if (getIsNodeSendable(nodeId))
  {  
    if (likely(recordGSN(signal->theVerId_signalNumber)))
    { 
      incClientStat(Ndb::BytesSentCount, 
                    ((signal->getLength() << 2) +
                     ((secs > 2)? ptr[2].sz << 2 : 0) + 
                     ((secs > 1)? ptr[1].sz << 2: 0) +
                     ((secs > 0)? ptr[0].sz << 2: 0)));
    }
    return raw_sendSignal(signal, nodeId, ptr, secs);
  }
  return -1;
}

inline
int
NdbImpl::sendFragmentedSignal(NdbApiSignal * signal, Uint32 nodeId,
                              const LinearSectionPtr ptr[3], Uint32 secs)
{
  if (getIsNodeSendable(nodeId))
  {
    if (likely(recordGSN(signal->theVerId_signalNumber)))
    {
      incClientStat(Ndb::BytesSentCount, 
                    ((signal->getLength() << 2) +
                     ((secs > 2)? ptr[2].sz << 2 : 0) + 
                     ((secs > 1)? ptr[1].sz << 2: 0) +
                     ((secs > 0)? ptr[0].sz << 2: 0)));
    }
    return raw_sendFragmentedSignal(signal, nodeId, ptr, secs);
  }
  return -1;
}

inline
int
NdbImpl::sendFragmentedSignal(NdbApiSignal * signal, Uint32 nodeId,
                              const GenericSectionPtr ptr[3], Uint32 secs)
{
  if (getIsNodeSendable(nodeId))
  {
    if (likely(recordGSN(signal->theVerId_signalNumber)))
    {
      incClientStat(Ndb::BytesSentCount,
                    ((signal->getLength() << 2) +
                     ((secs > 2)? ptr[2].sz << 2 : 0) + 
                     ((secs > 1)? ptr[1].sz << 2 : 0) +
                     ((secs > 0)? ptr[0].sz << 2 : 0)));
    }
    return raw_sendFragmentedSignal(signal, nodeId, ptr, secs);
  }
  return -1;
}

inline
void
NdbImpl::trp_wakeup()
{
  wakeHandler->notifyWakeup();
}

#endif
