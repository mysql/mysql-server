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

#ifndef NDB_DBLQH_PROXY_HPP
#define NDB_DBLQH_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/TabCommit.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/GCP.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/LqhTransReq.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/EmptyLcp.hpp>

class DblqhProxy : public LocalProxy {
public:
  DblqhProxy(Block_context& ctx);
  virtual ~DblqhProxy();
  BLOCK_DEFINES(DblqhProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // system info

  struct LcpRecord {
    bool m_idle;
    Uint32 m_lcpId;
    Uint32 m_frags;
    LcpRecord() {
      m_idle = true;
      m_lcpId = 0;
      m_frags = 0; // completed
    };
  };
  LcpRecord c_lcpRecord;

  // GSN_NDB_STTOR
  virtual void callNDB_STTOR(Signal*);

  // GSN_CREATE_TAB_REQ
  struct Ss_CREATE_TAB_REQ : SsParallel {
    CreateTabReq m_req;
    Uint32 m_lqhConnectPtr[MaxWorkers];
    Ss_CREATE_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendCREATE_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendCREATE_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_CREATE_TAB_REQ;
    }
  };
  SsPool<Ss_CREATE_TAB_REQ> c_ss_CREATE_TAB_REQ;
  void execCREATE_TAB_REQ(Signal*);
  void sendCREATE_TAB_REQ(Signal*, Uint32 ssId);
  void execCREATE_TAB_CONF(Signal*);
  void execCREATE_TAB_REF(Signal*);
  void sendCREATE_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_LQHADDATTREQ [ sub-op ]
  struct Ss_LQHADDATTREQ : SsParallel {
    LqhAddAttrReq m_req;
    Uint32 m_reqlength;
    Ss_LQHADDATTREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLQHADDATTREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLQHADDATTCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQHADDATTREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LQHADDATTREQ;
    }
  };
  SsPool<Ss_LQHADDATTREQ> c_ss_LQHADDATTREQ;
  void execLQHADDATTREQ(Signal*);
  void sendLQHADDATTREQ(Signal*, Uint32 ssId);
  void execLQHADDATTCONF(Signal*);
  void execLQHADDATTREF(Signal*);
  void sendLQHADDATTCONF(Signal*, Uint32 ssId);

  // GSN_LQHFRAGREQ [ pass-through ]
  void execLQHFRAGREQ(Signal*);

  // GSN_TAB_COMMITREQ [ sub-op ]
  struct Ss_TAB_COMMITREQ : SsParallel {
    TabCommitReq m_req;
    Ss_TAB_COMMITREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendTAB_COMMITREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendTAB_COMMITCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TAB_COMMITREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_TAB_COMMITREQ;
    }
  };
  SsPool<Ss_TAB_COMMITREQ> c_ss_TAB_COMMITREQ;
  void execTAB_COMMITREQ(Signal*);
  void sendTAB_COMMITREQ(Signal*, Uint32 ssId);
  void execTAB_COMMITCONF(Signal*);
  void execTAB_COMMITREF(Signal*);
  void sendTAB_COMMITCONF(Signal*, Uint32 ssId);

  // GSN_LCP_FRAG_ORD
  struct Ss_LCP_FRAG_ORD : SsParallel {
    /*
     * Used for entire LCP.  There is no start signal to LQH so we
     * keep state in LcpRecord.  Last signal has only lastFragmentFlag
     * set and is treated as a fictional signal GSN_LCP_COMPLETE_ORD.
     */
    static const char* name() { return "LCP_FRAG_ORD"; }
    Ss_LCP_FRAG_ORD() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLCP_FRAG_ORD;
      m_sendCONF = (SsFUNC)0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LCP_FRAG_ORD>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LCP_FRAG_ORD;
    }
  };
  SsPool<Ss_LCP_FRAG_ORD> c_ss_LCP_FRAG_ORD;
  static Uint32 getSsId(const LcpFragOrd* req) {
    return SsIdBase | (req->lcpId & 0xFFFF);
  }
  static Uint32 getSsId(const LcpFragRep* conf) {
    return SsIdBase | (conf->lcpId & 0xFFFF);
  }
  static Uint32 getSsId(const LcpCompleteRep* conf) {
    return SsIdBase | (conf->lcpId & 0xFFFF);
  }
  void execLCP_FRAG_ORD(Signal*);
  void sendLCP_FRAG_ORD(Signal*, Uint32 ssId);
  void execLCP_FRAG_REP(Signal*);

  // GSN_LCP_COMPLETE_ORD [ sub-op, fictional gsn ]
  struct Ss_LCP_COMPLETE_ORD : SsParallel {
    static const char* name() { return "LCP_COMPLETE_ORD"; }
    LcpFragOrd m_req;
    // pointers to Ss_END_LCP_REQ for PGMAN, TSMAN, LGMAN
    enum { BlockCnt = 3 };
    struct BlockInfo {
      Uint32 m_blockNo;
      Uint32 m_ssId;
      BlockInfo() : m_blockNo(0), m_ssId(0) {}
    } m_endLcp[BlockCnt];
    Ss_LCP_COMPLETE_ORD() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLCP_COMPLETE_ORD;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLCP_COMPLETE_REP;
      m_endLcp[0].m_blockNo = PGMAN;
      m_endLcp[1].m_blockNo = TSMAN;
      m_endLcp[2].m_blockNo = LGMAN;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LCP_COMPLETE_ORD>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LCP_COMPLETE_ORD;
    }
  };
  SsPool<Ss_LCP_COMPLETE_ORD> c_ss_LCP_COMPLETE_ORD;
  void execLCP_COMPLETE_ORD(Signal*);
  void sendLCP_COMPLETE_ORD(Signal*, Uint32 ssId);
  void execLCP_COMPLETE_REP(Signal*);
  void sendLCP_COMPLETE_REP(Signal*, Uint32 ssId);
  
  // GSN_END_LCP_REQ [ sub-op ]
  struct Ss_END_LCP_REQ : SsParallel {
    /*
     * Starts with worker REQs so the roles of sendREQ/sendCONF
     * are reversed.  Workers are forced to send END_LCP_REQ because
     * making LCP_COMPLETE_REP answer here is too complicated.
     * Note TSMAN sends no END_LCP_CONF.
     */
    static const char* name() { return "END_LCP_REQ"; }
    Uint32 m_reqcount;
    Uint32 m_backupId;
    Uint32 m_proxyBlockNo;
    Uint32 m_confcount;
    EndLcpReq m_req[MaxWorkers];
    Ss_END_LCP_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendEND_LCP_CONF;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendEND_LCP_REQ;
      m_reqcount = 0;
      m_backupId = 0;
      m_proxyBlockNo = 0;
      m_confcount = 0;
    };
    enum { poolSize = 3 }; // PGMAN, TSMAN, LGMAN
    static SsPool<Ss_END_LCP_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_END_LCP_REQ;
    }
  };
  SsPool<Ss_END_LCP_REQ> c_ss_END_LCP_REQ;
  static Uint32 getSsId(const EndLcpReq* req) {
    return (req->proxyBlockNo << 16) | (req->backupId & 0xFFFF);
  }
  static Uint32 getSsId(const EndLcpConf* conf) {
    return conf->senderData;
  }
  void execEND_LCP_REQ(Signal*);
  void sendEND_LCP_REQ(Signal*, Uint32 ssId);
  void execEND_LCP_CONF(Signal*);
  void sendEND_LCP_CONF(Signal*, Uint32 ssId);

  // GSN_GCP_SAVEREQ
  struct Ss_GCP_SAVEREQ : SsParallel {
    static const char* name() { return "GCP_SAVEREQ"; }
    GCPSaveReq m_req;
    Ss_GCP_SAVEREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendGCP_SAVEREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendGCP_SAVECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_GCP_SAVEREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_GCP_SAVEREQ;
    }
  };
  SsPool<Ss_GCP_SAVEREQ> c_ss_GCP_SAVEREQ;
  Uint32 getSsId(const GCPSaveReq* req) {
    return SsIdBase | (req->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveConf* conf) {
    return SsIdBase | (conf->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveRef* ref) {
    return SsIdBase | (ref->gci & 0xFFFF);
  }
  void execGCP_SAVEREQ(Signal*);
  void sendGCP_SAVEREQ(Signal*, Uint32 ssId);
  void execGCP_SAVECONF(Signal*);
  void execGCP_SAVEREF(Signal*);
  void sendGCP_SAVECONF(Signal*, Uint32 ssId);

  // GSN_SUB_GCP_COMPLETE_REP
  void execSUB_GCP_COMPLETE_REP(Signal*);

  // GSN_PREP_DROP_TAB_REQ
  struct Ss_PREP_DROP_TAB_REQ : SsParallel {
    PrepDropTabReq m_req;
    Ss_PREP_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendPREP_DROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendPREP_DROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_PREP_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_PREP_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_PREP_DROP_TAB_REQ> c_ss_PREP_DROP_TAB_REQ;
  Uint32 getSsId(const PrepDropTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const PrepDropTabConf* conf) {
    return SsIdBase | conf->tableId;
  }
  Uint32 getSsId(const PrepDropTabRef* ref) {
    return SsIdBase | ref->tableId;
  }
  void execPREP_DROP_TAB_REQ(Signal*);
  void sendPREP_DROP_TAB_REQ(Signal*, Uint32 ssId);
  void execPREP_DROP_TAB_CONF(Signal*);
  void execPREP_DROP_TAB_REF(Signal*);
  void sendPREP_DROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_DROP_TAB_REQ> c_ss_DROP_TAB_REQ;
  Uint32 getSsId(const DropTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const DropTabConf* conf) {
    return SsIdBase | conf->tableId;
  }
  Uint32 getSsId(const DropTabRef* ref) {
    return SsIdBase | ref->tableId;
  }
  void execDROP_TAB_REQ(Signal*);
  void sendDROP_TAB_REQ(Signal*, Uint32 ssId);
  void execDROP_TAB_CONF(Signal*);
  void execDROP_TAB_REF(Signal*);
  void sendDROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_ALTER_TAB_REQ
  struct Ss_ALTER_TAB_REQ : SsParallel {
    AlterTabReq m_req;
    Uint32 m_sections;
    // wl4391_todo check max length in various cases
    enum { MaxSection0 = 2 * MAX_ATTRIBUTES_IN_TABLE };
    Uint32 m_sz0;
    Uint32 m_section0[MaxSection0];
    Ss_ALTER_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendALTER_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendALTER_TAB_CONF;
      m_sections = 0;
      m_sz0 = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_ALTER_TAB_REQ;
    }
  };
  SsPool<Ss_ALTER_TAB_REQ> c_ss_ALTER_TAB_REQ;
  Uint32 getSsId(const AlterTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const AlterTabConf* conf) {
    return conf->senderData;
  }
  Uint32 getSsId(const AlterTabRef* ref) {
    return ref->senderData;
  }
  void execALTER_TAB_REQ(Signal*);
  void sendALTER_TAB_REQ(Signal*, Uint32 ssId);
  void execALTER_TAB_CONF(Signal*);
  void execALTER_TAB_REF(Signal*);
  void sendALTER_TAB_CONF(Signal*, Uint32 ssId);

  /**
   * GSN_START_FRAGREQ needs to be serialized wrt START_RECREQ
   *   so send it via proxy, even if DIH knows where to send it...
   */
  void execSTART_FRAGREQ(Signal*);

  // GSN_START_RECREQ
  struct Ss_START_RECREQ : SsParallel {
    /*
     * The proxy is also proxy for signals from workers to global
     * blocks LGMAN, TSMAN.  These are run (sequentially) using
     * the sub-op START_RECREQ_2.
     */
    static const char* name() { return "START_RECREQ"; }
    StartRecReq m_req;
    // pointers to START_RECREQ_2 for LGMAN, TSMAN
    enum { m_req2cnt = 2 };
    struct {
      Uint32 m_blockNo;
      Uint32 m_ssId;
    } m_req2[m_req2cnt];
    Ss_START_RECREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendSTART_RECREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendSTART_RECCONF;
      m_req2[0].m_blockNo = LGMAN;
      m_req2[1].m_blockNo = TSMAN;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_START_RECREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_START_RECREQ;
    }
  };
  SsPool<Ss_START_RECREQ> c_ss_START_RECREQ;
  void execSTART_RECREQ(Signal*);
  void sendSTART_RECREQ(Signal*, Uint32 ssId);
  void execSTART_RECCONF(Signal*);
  void sendSTART_RECCONF(Signal*, Uint32 ssId);

  // GSN_START_RECREQ_2 [ sub-op, fictional gsn ]
  struct Ss_START_RECREQ_2 : SsParallel {
    static const char* name() { return "START_RECREQ_2"; }
    struct Req {
      enum { SignalLength = 2 };
      Uint32 lcpId;
      Uint32 proxyBlockNo;
    };
    // senderData is unnecessary as signal is unique per proxyBlockNo
    struct Conf {
      enum { SignalLength = 1 };
      Uint32 senderRef;
    };
    Req m_req;
    Conf m_conf;
    Ss_START_RECREQ_2() {
      // reversed sendREQ/sendCONF
      m_sendREQ = (SsFUNC)&DblqhProxy::sendSTART_RECCONF_2;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendSTART_RECREQ_2;
    }
    enum { poolSize = 2 };
    static SsPool<Ss_START_RECREQ_2>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_START_RECREQ_2;
    }
  };
  SsPool<Ss_START_RECREQ_2> c_ss_START_RECREQ_2;
  Uint32 getSsId(const Ss_START_RECREQ_2::Req* req) {
    return SsIdBase | req->proxyBlockNo;
  }
  Uint32 getSsId(const Ss_START_RECREQ_2::Conf* conf) {
    return SsIdBase | refToBlock(conf->senderRef);
  }
  void execSTART_RECREQ_2(Signal*);
  void sendSTART_RECREQ_2(Signal*, Uint32 ssId);
  void execSTART_RECCONF_2(Signal*);
  void sendSTART_RECCONF_2(Signal*, Uint32 ssId);

  // GSN_LQH_TRANSREQ
  struct Ss_LQH_TRANSREQ : SsParallel {
    static const char* name() { return "LQH_TRANSREQ"; }
    LqhTransReq m_req;
    LqhTransConf m_conf; // latest conf
    Ss_LQH_TRANSREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLQH_TRANSREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLQH_TRANSCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQH_TRANSREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LQH_TRANSREQ;
    }
  };
  SsPool<Ss_LQH_TRANSREQ> c_ss_LQH_TRANSREQ;
  void execLQH_TRANSREQ(Signal*);
  void sendLQH_TRANSREQ(Signal*, Uint32 ssId);
  void execLQH_TRANSCONF(Signal*);
  void sendLQH_TRANSCONF(Signal*, Uint32 ssId);

  // GSN_EMPTY_LCP_REQ
  struct Ss_EMPTY_LCP_REQ : SsParallel {
    static const char* name() { return "EMPTY_LCP_REQ"; }
    EmptyLcpReq m_req;
    EmptyLcpConf m_conf; // build final conf here
    Ss_EMPTY_LCP_REQ() {
      m_conf.idle = 1;
      m_sendREQ = (SsFUNC)&DblqhProxy::sendEMPTY_LCP_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendEMPTY_LCP_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_EMPTY_LCP_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_EMPTY_LCP_REQ;
    }
  };
  SsPool<Ss_EMPTY_LCP_REQ> c_ss_EMPTY_LCP_REQ;
  void execEMPTY_LCP_REQ(Signal*);
  void sendEMPTY_LCP_REQ(Signal*, Uint32 ssId);
  void execEMPTY_LCP_CONF(Signal*);
  void execEMPTY_LCP_REF(Signal*);
  void sendEMPTY_LCP_CONF(Signal*, Uint32 ssId);

  // GSN_EXEC_SR_1 [ fictional gsn ]
  struct Ss_EXEC_SR_1 : SsParallel {
    /*
     * Handle EXEC_SRREQ and EXEC_SRCONF.  These are broadcast
     * signals (not REQ/CONF).  EXEC_SR_1 receives one signal and
     * sends it to its workers.  EXEC_SR_2 waits for signal from
     * all workers and broadcasts it to all nodes.  These are
     * required to handle mixed versions (non-mt, mt-lqh-1,2,4).
     */
    static const char* name() { return "EXEC_SR_1"; }
    struct Sig {
      enum { SignalLength = 1 };
      Uint32 nodeId;
    };
    GlobalSignalNumber m_gsn;
    Sig m_sig;
    Ss_EXEC_SR_1() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendEXEC_SR_1;
      m_sendCONF = (SsFUNC)0;
      m_gsn = 0;
    };
    enum { poolSize = 1 };
    static SsPool<Ss_EXEC_SR_1>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_EXEC_SR_1;
    }
  };
  SsPool<Ss_EXEC_SR_1> c_ss_EXEC_SR_1;
  Uint32 getSsId(const Ss_EXEC_SR_1::Sig* sig) {
    return SsIdBase | refToNode(sig->nodeId);
  };
  void execEXEC_SRREQ(Signal*);
  void execEXEC_SRCONF(Signal*);
  void execEXEC_SR_1(Signal*, GlobalSignalNumber gsn);
  void sendEXEC_SR_1(Signal*, Uint32 ssId);

  // GSN_EXEC_SR_2 [ fictional gsn ]
  struct Ss_EXEC_SR_2 : SsParallel {
    static const char* name() { return "EXEC_SR_2"; }
    struct Sig {
      enum { SignalLength = 1 + NdbNodeBitmask::Size };
      Uint32 nodeId;
      Uint32 sr_nodes[NdbNodeBitmask::Size]; // local signal so ok to add
    };
    GlobalSignalNumber m_gsn;
    Uint32 m_sigcount;
    Sig m_sig; // all signals must be identical
    Ss_EXEC_SR_2() {
      // reversed roles
      m_sendREQ = (SsFUNC)0;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendEXEC_SR_2;
      m_gsn = 0;
      m_sigcount = 0;
    };
    enum { poolSize = 1 };
    static SsPool<Ss_EXEC_SR_2>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_EXEC_SR_2;
    }
  };
  SsPool<Ss_EXEC_SR_2> c_ss_EXEC_SR_2;
  Uint32 getSsId(const Ss_EXEC_SR_2::Sig* sig) {
    return SsIdBase | refToNode(sig->nodeId);
  };
  void execEXEC_SR_2(Signal*, GlobalSignalNumber gsn);
  void sendEXEC_SR_2(Signal*, Uint32 ssId);
};

#endif
