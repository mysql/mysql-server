/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_DBLQH_PROXY_HPP
#define NDB_DBLQH_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/GCP.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/LqhTransReq.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/TabCommit.hpp>

#define JAM_FILE_ID 445

class DblqhProxy : public LocalProxy {
 public:
  DblqhProxy(Block_context &ctx);
  ~DblqhProxy() override;
  BLOCK_DEFINES(DblqhProxy);

 protected:
  SimulatedBlock *newWorker(Uint32 instanceNo) override;

  // system info
  Uint32 c_tableRecSize;
  Uint8 *c_tableRec;  // bool => table exists

  // GSN_NDB_STTOR
  void callNDB_STTOR(Signal *) override;
  void callREAD_CONFIG_REQ(Signal *) override;

  // GSN_CREATE_TAB_REQ
  struct Ss_CREATE_TAB_REQ : SsParallel {
    CreateTabReq m_req;
    Uint32 m_lqhConnectPtr[MaxWorkers];
    Ss_CREATE_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendCREATE_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendCREATE_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_TAB_REQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_CREATE_TAB_REQ;
    }
  };
  SsPool<Ss_CREATE_TAB_REQ> c_ss_CREATE_TAB_REQ;
  void execCREATE_TAB_REQ(Signal *);
  void sendCREATE_TAB_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execCREATE_TAB_CONF(Signal *);
  void execCREATE_TAB_REF(Signal *);
  void sendCREATE_TAB_CONF(Signal *, Uint32 ssId);

  // GSN_LQHADDATTREQ [ sub-op ]
  struct Ss_LQHADDATTREQ : SsParallel {
    LqhAddAttrReq m_req;
    Uint32 m_reqlength;
    Ss_LQHADDATTREQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendLQHADDATTREQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendLQHADDATTCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQHADDATTREQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_LQHADDATTREQ;
    }
  };
  SsPool<Ss_LQHADDATTREQ> c_ss_LQHADDATTREQ;
  void execLQHADDATTREQ(Signal *);
  void sendLQHADDATTREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execLQHADDATTCONF(Signal *);
  void execLQHADDATTREF(Signal *);
  void sendLQHADDATTCONF(Signal *, Uint32 ssId);

  // GSN_LQHFRAGREQ [ pass-through ]
  void execLQHFRAGREQ(Signal *);

  // GSN_TAB_COMMITREQ [ sub-op ]
  struct Ss_TAB_COMMITREQ : SsParallel {
    TabCommitReq m_req;
    Ss_TAB_COMMITREQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendTAB_COMMITREQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendTAB_COMMITCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TAB_COMMITREQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_TAB_COMMITREQ;
    }
  };
  SsPool<Ss_TAB_COMMITREQ> c_ss_TAB_COMMITREQ;
  void execTAB_COMMITREQ(Signal *);
  void sendTAB_COMMITREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execTAB_COMMITCONF(Signal *);
  void execTAB_COMMITREF(Signal *);
  void sendTAB_COMMITCONF(Signal *, Uint32 ssId);

  // GSN_GCP_SAVEREQ
  struct Ss_GCP_SAVEREQ : SsParallel {
    static const char *name() { return "GCP_SAVEREQ"; }
    GCPSaveReq m_req;
    Ss_GCP_SAVEREQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendGCP_SAVEREQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendGCP_SAVECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_GCP_SAVEREQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_GCP_SAVEREQ;
    }
  };
  SsPool<Ss_GCP_SAVEREQ> c_ss_GCP_SAVEREQ;
  Uint32 getSsId(const GCPSaveReq *req) {
    return SsIdBase | (req->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveConf *conf) {
    return SsIdBase | (conf->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveRef *ref) {
    return SsIdBase | (ref->gci & 0xFFFF);
  }
  void execGCP_SAVEREQ(Signal *);
  void sendGCP_SAVEREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execGCP_SAVECONF(Signal *);
  void execGCP_SAVEREF(Signal *);
  void sendGCP_SAVECONF(Signal *, Uint32 ssId);

  // GSN_SUB_GCP_COMPLETE_REP
  void execSUB_GCP_COMPLETE_REP(Signal *);

  // GSN_START_LCP_ORD
  void execSTART_LCP_ORD(Signal *);

  // GSN_UNDO_LOG_LEVEL_REP
  void execUNDO_LOG_LEVEL_REP(Signal *);

  // GSN_PREP_DROP_TAB_REQ
  struct Ss_PREP_DROP_TAB_REQ : SsParallel {
    PrepDropTabReq m_req;
    Ss_PREP_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendPREP_DROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendPREP_DROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_PREP_DROP_TAB_REQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_PREP_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_PREP_DROP_TAB_REQ> c_ss_PREP_DROP_TAB_REQ;
  Uint32 getSsId(const PrepDropTabReq *req) { return SsIdBase | req->tableId; }
  Uint32 getSsId(const PrepDropTabConf *conf) {
    return SsIdBase | conf->tableId;
  }
  Uint32 getSsId(const PrepDropTabRef *ref) { return SsIdBase | ref->tableId; }
  void execPREP_DROP_TAB_REQ(Signal *);
  void sendPREP_DROP_TAB_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execPREP_DROP_TAB_CONF(Signal *);
  void execPREP_DROP_TAB_REF(Signal *);
  void sendPREP_DROP_TAB_CONF(Signal *, Uint32 ssId);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_DROP_TAB_REQ> c_ss_DROP_TAB_REQ;
  Uint32 getSsId(const DropTabReq *req) { return SsIdBase | req->tableId; }
  Uint32 getSsId(const DropTabConf *conf) { return SsIdBase | conf->tableId; }
  Uint32 getSsId(const DropTabRef *ref) { return SsIdBase | ref->tableId; }
  void execDROP_TAB_REQ(Signal *);
  void sendDROP_TAB_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execDROP_TAB_CONF(Signal *);
  void execDROP_TAB_REF(Signal *);
  void sendDROP_TAB_CONF(Signal *, Uint32 ssId);

  // GSN_ALTER_TAB_REQ
  struct Ss_ALTER_TAB_REQ : SsParallel {
    AlterTabReq m_req;
    Ss_ALTER_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendALTER_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendALTER_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_TAB_REQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_ALTER_TAB_REQ;
    }
  };
  SsPool<Ss_ALTER_TAB_REQ> c_ss_ALTER_TAB_REQ;
  Uint32 getSsId(const AlterTabReq *req) { return SsIdBase | req->tableId; }
  Uint32 getSsId(const AlterTabConf *conf) { return conf->senderData; }
  Uint32 getSsId(const AlterTabRef *ref) { return ref->senderData; }
  void execALTER_TAB_REQ(Signal *);
  void sendALTER_TAB_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execALTER_TAB_CONF(Signal *);
  void execALTER_TAB_REF(Signal *);
  void sendALTER_TAB_CONF(Signal *, Uint32 ssId);

  /**
   * GSN_START_FRAGREQ needs to be serialized wrt START_RECREQ
   *   so send it via proxy, even if DIH knows where to send it...
   */
  void execSTART_FRAGREQ(Signal *);

  // GSN_START_RECREQ
  struct Ss_START_RECREQ : SsParallel {
    /*
     * The proxy is also proxy for signals from workers to global
     * blocks LGMAN, TSMAN.  These are run (sequentially) using
     * the sub-op START_RECREQ_2.
     */
    static const char *name() { return "START_RECREQ"; }
    StartRecReq m_req;
    Uint32 phaseToSend;
    Uint32 restoreFragCompletedCount;
    Uint32 undoDDCompletedCount;
    Uint32 execREDOLogCompletedCount;
    // pointers to START_RECREQ_2 for LGMAN, TSMAN
    enum { m_req2cnt = 2 };
    struct {
      Uint32 m_blockNo;
      Uint32 m_ssId;
    } m_req2[m_req2cnt];
    Ss_START_RECREQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendSTART_RECREQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendSTART_RECCONF;
      m_req2[0].m_blockNo = LGMAN;
      m_req2[1].m_blockNo = TSMAN;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_START_RECREQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_START_RECREQ;
    }
  };
  SsPool<Ss_START_RECREQ> c_ss_START_RECREQ;
  void execSTART_RECREQ(Signal *);
  void sendSTART_RECREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execSTART_RECCONF(Signal *);
  void execLOCAL_RECOVERY_COMP_REP(Signal *);
  void sendSTART_RECCONF(Signal *, Uint32 ssId);

  // GSN_START_RECREQ_2 [ sub-op, fictional gsn ]
  struct Ss_START_RECREQ_2 : SsParallel {
    static const char *name() { return "START_RECREQ_2"; }
    struct Req {
      enum { SignalLength = 3 };
      Uint32 lcpId;
      Uint32 localLcpId;
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
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendSTART_RECCONF_2;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendSTART_RECREQ_2;
    }
    enum { poolSize = 2 };
    static SsPool<Ss_START_RECREQ_2> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_START_RECREQ_2;
    }
  };
  SsPool<Ss_START_RECREQ_2> c_ss_START_RECREQ_2;
  Uint32 getSsId(const Ss_START_RECREQ_2::Req *req) {
    return SsIdBase | req->proxyBlockNo;
  }
  Uint32 getSsId(const Ss_START_RECREQ_2::Conf *conf) {
    return SsIdBase | refToBlock(conf->senderRef);
  }
  void execSTART_RECREQ_2(Signal *);
  void sendSTART_RECREQ_2(Signal *, Uint32 ssId);
  void execSTART_RECCONF_2(Signal *);
  void sendSTART_RECCONF_2(Signal *, Uint32 ssId, SectionHandle *);

  // GSN_LQH_TRANSREQ
  struct Ss_LQH_TRANSREQ : SsParallel {
    static const char *name() { return "LQH_TRANSREQ"; }
    /**
     * Is this entry valid, or has it been made obsolete by
     *   a new LQH_TRANSREQ (i.e a new TC-failure)
     */
    bool m_valid;
    Uint32 m_maxInstanceId;
    LqhTransReq m_req;
    LqhTransConf m_conf;  // latest conf
    Ss_LQH_TRANSREQ() {
      m_valid = true;
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendLQH_TRANSREQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendLQH_TRANSCONF;
    }
    enum { poolSize = MAX_NDB_NODES };
    static SsPool<Ss_LQH_TRANSREQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_LQH_TRANSREQ;
    }
  };
  SsPool<Ss_LQH_TRANSREQ> c_ss_LQH_TRANSREQ;
  void execLQH_TRANSREQ(Signal *);
  void sendLQH_TRANSREQ(Signal *, Uint32 ssId, SectionHandle *);
  void execLQH_TRANSCONF(Signal *);
  void sendLQH_TRANSCONF(Signal *, Uint32 ssId);

  // GSN_EXEC_SR_1 [ fictional gsn ]
  struct Ss_EXEC_SR_1 : SsParallel {
    /*
     * Handle EXEC_SRREQ and EXEC_SRCONF.  These are broadcast
     * signals (not REQ/CONF).  EXEC_SR_1 receives one signal and
     * sends it to its workers.  EXEC_SR_2 waits for signal from
     * all workers and broadcasts it to all nodes.  These are
     * required to handle mixed versions (non-mt, mt-lqh-1,2,4).
     */
    static const char *name() { return "EXEC_SR_1"; }
    struct Sig {
      enum { SignalLength = 1 };
      Uint32 nodeId;
    };
    GlobalSignalNumber m_gsn;
    Sig m_sig;
    Ss_EXEC_SR_1() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendEXEC_SR_1;
      m_sendCONF = (SsFUNCREP)0;
      m_gsn = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_EXEC_SR_1> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_EXEC_SR_1;
    }
  };
  SsPool<Ss_EXEC_SR_1> c_ss_EXEC_SR_1;
  Uint32 getSsId(const Ss_EXEC_SR_1::Sig *sig) {
    return SsIdBase | refToNode(sig->nodeId);
  }
  void execEXEC_SRREQ(Signal *);
  void execEXEC_SRCONF(Signal *);
  void execEXEC_SR_1(Signal *, GlobalSignalNumber gsn);
  void sendEXEC_SR_1(Signal *, Uint32 ssId, SectionHandle *);

  // GSN_EXEC_SR_2 [ fictional gsn ]
  struct Ss_EXEC_SR_2 : SsParallel {
    static const char *name() { return "EXEC_SR_2"; }
    struct Sig {
      enum { SignalLength = 1 + NdbNodeBitmask::Size };
      Uint32 nodeId;
      Uint32 sr_nodes[NdbNodeBitmask::Size];  // local signal so ok to add
    };
    GlobalSignalNumber m_gsn;
    Uint32 m_sigcount;
    Sig m_sig;  // all signals must be identical
    Ss_EXEC_SR_2() {
      // reversed roles
      m_sendREQ = (SsFUNCREQ)0;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendEXEC_SR_2;
      m_gsn = 0;
      m_sigcount = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_EXEC_SR_2> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_EXEC_SR_2;
    }
  };
  SsPool<Ss_EXEC_SR_2> c_ss_EXEC_SR_2;
  Uint32 getSsId(const Ss_EXEC_SR_2::Sig *sig) {
    return SsIdBase | refToNode(sig->nodeId);
  }
  void execEXEC_SR_2(Signal *, GlobalSignalNumber gsn);
  void sendEXEC_SR_2(Signal *, Uint32 ssId);

  /**
   * GSN_EXEC_FRAGREQ & GSN_EXEC_FRAGCONF needs to
   *   be passed via proxy for correct serialization
   *   wrt to GSN_EXEC_SRREQ & GSN_EXEC_SRCONF
   */
  void execEXEC_FRAGREQ(Signal *);
  void execEXEC_FRAGCONF(Signal *);

  // GSN_DROP_FRAG_REQ
  struct Ss_DROP_FRAG_REQ : SsParallel {
    DropFragReq m_req;
    Ss_DROP_FRAG_REQ() {
      m_sendREQ = (SsFUNCREQ)&DblqhProxy::sendDROP_FRAG_REQ;
      m_sendCONF = (SsFUNCREP)&DblqhProxy::sendDROP_FRAG_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_FRAG_REQ> &pool(LocalProxy *proxy) {
      return ((DblqhProxy *)proxy)->c_ss_DROP_FRAG_REQ;
    }
  };
  SsPool<Ss_DROP_FRAG_REQ> c_ss_DROP_FRAG_REQ;
  Uint32 getSsId(const DropFragReq *req) {
    return SsIdBase | (req->tableId ^ req->fragId);
  }
  Uint32 getSsId(const DropFragConf *conf) {
    return SsIdBase | (conf->tableId ^ conf->fragId);
  }
  Uint32 getSsId(const DropFragRef *ref) {
    return SsIdBase | (ref->tableId ^ ref->fragId);
  }
  void execDROP_FRAG_REQ(Signal *);
  void sendDROP_FRAG_REQ(Signal *, Uint32 ssId, SectionHandle *);
  void execDROP_FRAG_CONF(Signal *);
  void execDROP_FRAG_REF(Signal *);
  void sendDROP_FRAG_CONF(Signal *, Uint32 ssId);

  // LCP handling
  void execLCP_FRAG_ORD(Signal *);
  void execLCP_FRAG_REP(Signal *);
  void execEND_LCPCONF(Signal *);
  void execLCP_COMPLETE_REP(Signal *);
  void execWAIT_ALL_COMPLETE_LCP_REQ(Signal *);
  void execWAIT_COMPLETE_LCP_CONF(Signal *);
  void execINFO_GCP_STOP_TIMER(Signal *);
  void execSTART_NODE_LCP_REQ(Signal *);
  void execSTART_NODE_LCP_CONF(Signal *);

  Uint32 m_outstanding_wait_lcp;
  BlockReference m_wait_all_lcp_sender;
  bool m_received_wait_all;
  bool m_lcp_started;
  Uint32 m_outstanding_start_node_lcp_req;

  struct LcpRecord {
    enum {
      L_IDLE = 0,
      L_RUNNING = 1,
      L_COMPLETING_1 = 2,
      L_COMPLETING_2 = 3
    } m_state;
    Uint32 m_lcpId;
    Uint32 m_keepGci;
    Uint32 m_lcp_frag_ord_cnt;       // No of LCP_FRAG_ORD received
    Uint32 m_lcp_frag_rep_cnt;       // No of LCP_FRAG_REP sent
    Uint32 m_complete_outstanding;   // Outstanding signals waiting for
    LcpFragOrd m_last_lcp_frag_ord;  // Last received LCP_FRAG_ORD
    bool m_lastFragmentFlag;

    LcpRecord() {
      m_state = L_IDLE;
      m_lcpId = 0;
      m_lcp_frag_ord_cnt = 0;
      m_lcp_frag_rep_cnt = 0;
      m_lastFragmentFlag = false;
    }
  };
  LcpRecord c_lcpRecord;
  Uint32 getNoOfOutstanding(const LcpRecord &) const;
  void completeLCP(Signal *signal);
  void sendLCP_COMPLETE_REP(Signal *);
};

#undef JAM_FILE_ID

#endif
