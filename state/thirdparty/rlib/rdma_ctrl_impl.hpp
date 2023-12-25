#include <pthread.h>

#include <map>
#include <mutex>


namespace rdmaio {

/**
 * Simple critical section
 * It uses a single global block to guard RdmaCtrl.
 * This is acceptable, since RdmaCtrl is only the control plane.
 */
class SCS {
 public:
  SCS() {
    get_lock().lock();
  }

  ~SCS() {
    get_lock().unlock();
  }

 private:
  static std::mutex& get_lock() {
    static std::mutex lock;
    return lock;
  }
};

/**
 * convert qp idx(node,worker,idx) -> key
 */
// MZ: It's too narrow for 16-bit node id and 16-bit worker id in original rlib
// inline uint32_t get_rc_key(const QPIdx idx) {
//   return ::rdmaio::encode_qp_id(idx.node_id, RC_ID_BASE + idx.worker_id * 64 + idx.index);
// }

// inline uint32_t get_ud_key(const QPIdx idx) {
//   return ::rdmaio::encode_qp_id(idx.worker_id, UD_ID_BASE + idx.index);
// }

// MZ: We use 32-bit node id and 32-bit worker id
inline uint64_t get_rc_key(const QPIdx idx) {
  return ::rdmaio::encode_qp_64b_id(idx.node_id, idx.worker_id);
}

inline uint64_t get_ud_key(const QPIdx idx) {
  return ::rdmaio::encode_qp_64b_id(idx.worker_id, UD_ID_BASE + idx.index);
}

/**
 * Control plane of RLib
 */
class RdmaCtrl::RdmaCtrlImpl {
 private:
  RNicHandler* opened_rnic = nullptr;

 public:
  RdmaCtrlImpl(int node_id, int tcp_base_port, connection_callback_t callback, std::string local_ip) : node_id_(node_id),
                                                                                                       tcp_base_port_(tcp_base_port),
                                                                                                       local_ip_(local_ip),
                                                                                                       qp_callback_(callback) {
    // start the background thread to handle QP connection request
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&handler_tid_, &attr, &RdmaCtrlImpl::connection_handler_wrapper, this);
  }

  ~RdmaCtrlImpl() {
    running_ = false;  // wait for the handler to join
    pthread_join(handler_tid_, NULL);
    // RDMA_LOG(DBG) << "rdma controler close: does not handle any future connections.";
  }

  RNicHandler* open_thread_local_device(DevIdx idx) {
    // already openend device
    if (rnic_instance() != nullptr)
      return rnic_instance();

    auto handler = open_device(idx);
    rnic_instance() = handler;
    return rnic_instance();
  }

  RNicHandler* open_device(DevIdx idx) {
    RNicHandler* rnic = nullptr;

    struct ibv_device** dev_list = nullptr;
    struct ibv_context* ib_ctx = nullptr;
    struct ibv_pd* pd = nullptr;
    int num_devices;
    int rc;  // return code

    dev_list = ibv_get_device_list(&num_devices);

    if (idx.dev_id >= num_devices || idx.dev_id < 0) {
      RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "wrong dev_id: " << idx.dev_id << "; total " << num_devices << " found";
      goto OPEN_END;
    }

    // alloc ctx
    ib_ctx = ibv_open_device(dev_list[idx.dev_id]);
    if (ib_ctx == nullptr) {
      RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "failed to open ib ctx w error: " << strerror(errno);
      goto OPEN_END;
    }

    // alloc pd
    pd = ibv_alloc_pd(ib_ctx);
    if (pd == nullptr) {
      RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "failed to alloc pd w error: " << strerror(errno);
      RDMA_VERIFY(rdma_loglevel::RDMA_LOG_INFO, ibv_close_device(ib_ctx) == 0) << "failed to close device " << idx.dev_id;
      goto OPEN_END;
    }

    // fill the lid
    ibv_port_attr port_attr;
    rc = ibv_query_port(ib_ctx, idx.port_id, &port_attr);
    if (rc < 0) {
      RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "failed to query port status w error: " << strerror(errno);
      RDMA_VERIFY(rdma_loglevel::RDMA_LOG_INFO, ibv_close_device(ib_ctx) == 0) << "failed to close device " << idx.dev_id;
      RDMA_VERIFY(rdma_loglevel::RDMA_LOG_INFO, ibv_dealloc_pd(pd) == 0) << "failed to dealloc pd";
      goto OPEN_END;
    }

    // success open
    {
      rnic = new RNicHandler(idx.dev_id, idx.port_id, ib_ctx, pd, port_attr.lid);
    }

    OPEN_END:
    if (dev_list != nullptr)
      ibv_free_device_list(dev_list);

    opened_rnic = rnic;

    return rnic;
  }

  RCQP* get_rc_qp(QPIdx idx) {
    RCQP* res = nullptr;
    {
      SCS s;
      res = get_qp<RCQP, get_rc_key>(idx);
    };
    return res;
  }

  UDQP* get_ud_qp(QPIdx idx) {
    UDQP* res = nullptr;
    {
      SCS s;
      res = get_qp<UDQP, get_ud_key>(idx);
    };
    return res;
  }

  /**
     * Note! this is not a thread-safe function
     */
  template <class T, uint64_t (* F)(QPIdx)>
  T* get_qp(QPIdx idx) {
    uint64_t key = F(idx);
    if (qps_.find(key) == qps_.end())
      return nullptr;
    else
      return dynamic_cast<T*>(qps_[key]);
  }

  RCQP* create_rc_qp(QPIdx idx, RNicHandler* dev, MemoryAttr* attr) {
    RCQP* res = nullptr;
    {
      SCS s;
      uint64_t qid = get_rc_key(idx);
      if (qps_.find(qid) != qps_.end()) {
        res = dynamic_cast<RCQP*>(qps_[qid]);
      } else {
        if (attr == NULL)
          res = new RCQP(dev, idx);
        else
          res = new RCQP(dev, idx, *attr);
        qps_.insert(std::make_pair(qid, res));
      }
    };
    return res;
  }

  void destroy_rc_qp() {
    qps_.clear();
  }

  UDQP* create_ud_qp(QPIdx idx, RNicHandler* dev, MemoryAttr* attr) {
    UDQP* res = nullptr;
    uint64_t qid = get_ud_key(idx);

    {
      SCS s;
      if (qps_.find(qid) != qps_.end()) {
        res = dynamic_cast<UDQP*>(qps_[qid]);
      } else {
        if (attr == NULL)
          res = new UDQP(dev, idx);
        else
          res = new UDQP(dev, idx, *attr);
        qps_.insert(std::make_pair(qid, res));
      }
    };
    return res;
  }

  bool register_memory(int mr_id, const char* buf, uint64_t size, RNicHandler* rnic, int flag) {
    Memory* m = new Memory(buf, size, rnic->pd, flag);
    if (!m->valid()) {
      RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "register local_mr to rnic error: " << strerror(errno);
      delete m;
      return false;
    }
    {
      SCS s;
      if (mrs_.find(mr_id) != mrs_.end()) {
        RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "local_mr " << mr_id << " has already been registered!";
        delete m;
      } else {
        mrs_.insert(std::make_pair(mr_id, m));
      }
    }
    return true;
  }

  int get_default_mr(MemoryAttr& attr) {
    SCS s;
    for (auto it = mrs_.begin(); it != mrs_.end(); ++it) {
      int idx = it->first;
      attr = it->second->rattr;
      return idx;
    }
    return -1;
  }

  MemoryAttr get_local_mr(int mr_id) {
    MemoryAttr attr = {};
    {
      SCS s;
      if (mrs_.find(mr_id) != mrs_.end())
        attr = mrs_[mr_id]->rattr;
    }
    return attr;
  }

  void clear_dev_info() {
    cached_infos_.clear();
  }

  static std::vector<RNicInfo> query_devs_helper() {
    int num_devices = 0;
    struct ibv_device** dev_list = nullptr;
    std::vector<RNicInfo> res;

    {  // query the device and its active ports using the underlying APIs
      dev_list = ibv_get_device_list(&num_devices);
      int temp_devices = num_devices;

      if (dev_list == nullptr) {
        RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "cannot get ib devices.";
        num_devices = 0;
        goto QUERY_END;
      }

      for (int dev_id = 0; dev_id < temp_devices; ++dev_id) {
        struct ibv_context* ib_ctx = ibv_open_device(dev_list[dev_id]);
        if (ib_ctx == nullptr) {
          RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "open dev " << dev_id << " error: " << strerror(errno) << " ignored";
          num_devices -= 1;
          continue;
        }
        res.emplace_back(ibv_get_device_name(ib_ctx->device), dev_id, ib_ctx);
        // QUERY_DEV_END:
        // close ib_ctx
        RDMA_VERIFY(rdma_loglevel::RDMA_LOG_INFO, ibv_close_device(ib_ctx) == 0) << "failed to close device " << dev_id;
      }
    }

    QUERY_END:
    if (dev_list != nullptr)
      ibv_free_device_list(dev_list);
    return res;
  }

  std::vector<RNicInfo> query_devs() {
    if (cached_infos_.size() != 0) {
      return cached_infos_;
    }
    cached_infos_ = query_devs_helper();
    return std::vector<RNicInfo>(cached_infos_.begin(), cached_infos_.end());
  }

  RdmaCtrl::DevIdx convert_port_idx(int idx) {
    if (cached_infos_.size() == 0)
      query_devs();

    for (int dev_id = 0; dev_id < (int)cached_infos_.size(); ++dev_id) {
      int port_num = cached_infos_[dev_id].active_ports.size();

      for (int port_id = 1; port_id <= port_num; port_id++) {
        if (idx == 0) {
          // find one
          return DevIdx{.dev_id = dev_id, .port_id = port_id};
        }
        idx -= 1;
      }
    }
    // failed to find the dev according to the idx
    return DevIdx{.dev_id = -1, .port_id = -1};
  }

  RNicHandler* get_device() {
    return rnic_instance();
  }

  void close_device() {
    if (rnic_instance() != nullptr) delete rnic_instance();
    rnic_instance() = nullptr;
  }

  void close_device(RNicHandler* rnic) {
    if (rnic != nullptr)
      delete rnic;
  }

  static void* connection_handler_wrapper(void* context) {
    return ((RdmaCtrlImpl*) context)->connection_handler();
  }

  /**
     * Using TCP to connect in-coming QP & MR requests
     */
  void* connection_handler(void) {
    pthread_detach(pthread_self());

    auto listenfd = PreConnector::get_listen_socket(local_ip_, tcp_base_port_);

    int opt = 1;
    RDMA_VERIFY(rdma_loglevel::RDMA_LOG_ERROR, setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(int)) == 0)
      << "unable to configure socket status.";
    RDMA_VERIFY(rdma_loglevel::RDMA_LOG_ERROR, listen(listenfd, 24) == 0) << "TCP listen error: " << strerror(errno);
    while (running_) {
      asm volatile(""::
      : "memory");

      struct sockaddr_in cli_addr = {0};
      socklen_t clilen = sizeof(cli_addr);
      auto csfd = accept(listenfd, (struct sockaddr*) &cli_addr, &clilen);

      if (csfd < 0) {
        RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "accept a wrong connection error: " << strerror(errno);
        continue;
      }

      if (!PreConnector::wait_recv(csfd, 6000)) {
        close(csfd);
        continue;
      }

      ConnArg arg;
      auto n = recv(csfd, (char*) (&arg), sizeof(ConnArg), MSG_WAITALL);

      if (n != sizeof(ConnArg)) {
        // an invalid message
        close(csfd);
        continue;
      }

      ConnReply reply;
      reply.ack = ERR;

      {  // in a global critical section
        //          SCS s;
        switch (arg.type) {
          case ConnArg::MR:
            if (mrs_.find(arg.payload.mr.mr_id) != mrs_.end()) {
              memcpy((char*) (&(reply.payload.mr)),
                     (char*) (&(mrs_[arg.payload.mr.mr_id]->rattr)), sizeof(MemoryAttr));
              reply.ack = SUCC;
            };
            break;
          case ConnArg::QP: {
            qp_callback_(arg.payload.qp);  // call the user callback
            QP* qp = NULL;
            switch (arg.payload.qp.qp_type) {
              case IBV_QPT_UD: {
                UDQP* ud_qp = get_qp<UDQP, get_ud_key>(
                  create_ud_idx(arg.payload.qp.from_node, arg.payload.qp.from_worker));
                if (ud_qp != nullptr && ud_qp->ready()) {
                  qp = ud_qp;
                }
              }
                break;
              case IBV_QPT_RC: {
                // MZ: Server passively accepts and connects QPs. It is more efficient than the original rlib
                QPIdx idx = create_rc_idx(arg.payload.qp.from_node, arg.payload.qp.from_worker);
                RDMA_LOG(rdma_loglevel::RDMA_LOG_INFO) << "Receive QP from client, my node id: " << arg.payload.qp.from_node << ", client worker id: " << arg.payload.qp.from_worker;
                qp = get_qp<RCQP, get_rc_key>(idx); // For multi round tests
                if (qp == nullptr) {
                  qp = create_rc_qp(idx, opened_rnic, NULL);
                  RDMA_LOG(rdma_loglevel::RDMA_LOG_INFO) << "Create new RCQP for connection";
                  if (!RCQPImpl::readytorcv(qp->qp_, arg.payload.qp.qp_attr, opened_rnic)) {
                    RDMA_LOG(RDMA_LOG_FATAL) << "change qp_attr status to ready to receive error: " << strerror(errno);
                  }
                  if (!RCQPImpl::readytosend(qp->qp_)) {
                    RDMA_LOG(RDMA_LOG_FATAL) << "change qp_attr status to ready to send error: " << strerror(errno);
                  }
                }
              }
                break;
              default:RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "unknown QP connection type: " << arg.payload.qp.qp_type;
            }
            if (qp != nullptr) {
              reply.payload.qp = qp->get_attr();
              reply.ack = SUCC;
            }
            reply.payload.qp.node_id = node_id_;
            break;
          }
          default:RDMA_LOG(rdma_loglevel::RDMA_LOG_WARNING) << "received unknown connect type " << arg.type;
        }
      }  // end simple critical section protection

      PreConnector::send_to(csfd, (char*) (&reply), sizeof(ConnReply));
      PreConnector::wait_close(csfd);  // wait for the client to close the connection
    }
    // end of the server
    close(listenfd);
  }

 private:
  friend class RdmaCtrl;

  static RNicHandler*& rnic_instance() {
    static thread_local RNicHandler* handler = NULL;
    return handler;
  }

  std::vector<RNicInfo> cached_infos_;

  // registered MRs at this control manager
  std::map<int, Memory*> mrs_;

  // created QPs on this control manager
  std::map<uint64_t, QP*> qps_;

  // local node information
  const int node_id_;
  const int tcp_base_port_;
  const std::string local_ip_;

  pthread_t handler_tid_;
  bool running_ = true;

  // connection callback function
  connection_callback_t qp_callback_;

  bool link_symmetric_rcqps(const std::vector<std::string>& cluster, int l_mrid, int mr_id, int wid, int idx) {
    std::vector<bool> ready_list(cluster.size(), false);
    std::vector<MemoryAttr> mrs;

    MemoryAttr local_mr = get_local_mr(l_mrid);

    for (auto s : cluster) {
      // get the target local_mr
      retry:
      MemoryAttr mr = {};
      auto rc = QP::get_remote_mr(s, tcp_base_port_, mr_id, &mr);
      if (rc != SUCC) {
        usleep(2000);
        goto retry;
      }
      mrs.push_back(mr);
    }

    RDMA_ASSERT(mrs.size() == cluster.size());

    while (true) {
      int connected = 0, i = 0;
      for (auto s : cluster) {
        if (ready_list[i]) {
          i++;
          connected++;
          continue;
        }
        RCQP* qp = create_rc_qp(QPIdx{.node_id = i, .worker_id = wid, .index = idx},
                                get_device(), &local_mr);
        RDMA_ASSERT(qp != nullptr);

        if (qp->connect(s, tcp_base_port_,
                        QPIdx{.node_id = node_id_, .worker_id = wid, .index = idx}) == SUCC) {
          ready_list[i] = true;
          connected++;
          qp->bind_remote_mr(mrs[i]);
        }
        i++;
      }
      if ((size_t)connected == cluster.size())
        break;
      else
        usleep(1000);
    }
    return true;  // This example does not use error handling
  }

  void register_qp_callback(connection_callback_t callback) {
    qp_callback_ = callback;
  }
};  //

// link to the main class
inline __attribute__((always_inline))
RdmaCtrl::RdmaCtrl(int node_id, int tcp_base_port, connection_callback_t callback, std::string ip)
  : impl_(new RdmaCtrlImpl(node_id, tcp_base_port, callback, ip)) {
}

inline __attribute__((always_inline))
RdmaCtrl::~RdmaCtrl() {
  impl_.reset();
}

inline __attribute__((always_inline))
std::vector<RNicInfo>
RdmaCtrl::query_devs() {
  return impl_->query_devs();
}

inline __attribute__((always_inline)) void RdmaCtrl::clear_dev_info() {
  return impl_->clear_dev_info();
}

inline __attribute__((always_inline))
RNicHandler*
RdmaCtrl::get_device() {
  return impl_->get_device();
}

inline __attribute__((always_inline))
RNicHandler*
RdmaCtrl::open_thread_local_device(DevIdx idx) {
  return impl_->open_thread_local_device(idx);
}

inline __attribute__((always_inline))
RNicHandler*
RdmaCtrl::open_device(DevIdx idx) {
  return impl_->open_device(idx);
}

inline __attribute__((always_inline)) void RdmaCtrl::close_device() {
  return impl_->close_device();
}

inline __attribute__((always_inline)) void RdmaCtrl::close_device(RNicHandler* rnic) {
  return impl_->close_device(rnic);
}

inline __attribute__((always_inline))
RdmaCtrl::DevIdx
RdmaCtrl::convert_port_idx(int idx) {
  return impl_->convert_port_idx(idx);
}

inline __attribute__((always_inline)) bool RdmaCtrl::register_memory(int id, const char* buf, uint64_t size, RNicHandler* rnic, int flag) {
  return impl_->register_memory(id, buf, size, rnic, flag);
}

inline __attribute__((always_inline))
MemoryAttr
RdmaCtrl::get_local_mr(int mr_id) {
  return impl_->get_local_mr(mr_id);
}

inline __attribute__((always_inline)) int RdmaCtrl::get_default_mr(MemoryAttr& attr) {
  return impl_->get_default_mr(attr);
}

inline __attribute__((always_inline))
RCQP*
RdmaCtrl::create_rc_qp(QPIdx idx, RNicHandler* dev, MemoryAttr* attr) {
  return impl_->create_rc_qp(idx, dev, attr);
}

inline __attribute__((always_inline))
void
RdmaCtrl::destroy_rc_qp() {
  return impl_->destroy_rc_qp();
}

inline __attribute__((always_inline))
UDQP*
RdmaCtrl::create_ud_qp(QPIdx idx, RNicHandler* dev, MemoryAttr* attr) {
  return impl_->create_ud_qp(idx, dev, attr);
}

inline __attribute__((always_inline))
RCQP*
RdmaCtrl::get_rc_qp(QPIdx idx) {
  return impl_->get_rc_qp(idx);
}

inline __attribute__((always_inline))
UDQP*
RdmaCtrl::get_ud_qp(QPIdx idx) {
  return impl_->get_ud_qp(idx);
}

inline __attribute__((always_inline)) int RdmaCtrl::current_node_id() {
  return impl_->node_id_;
}

inline __attribute__((always_inline)) int RdmaCtrl::listening_port() {
  return impl_->tcp_base_port_;
}

inline __attribute__((always_inline)) bool RdmaCtrl::link_symmetric_rcqps(const std::vector<std::string>& cluster,
                                                                          int l_mrid, int mr_id, int wid, int idx) {
  return impl_->link_symmetric_rcqps(cluster, l_mrid, mr_id, wid, idx);
}

inline __attribute__((always_inline))
std::vector<RNicInfo>
RdmaCtrl::query_devs_helper() {
  return RdmaCtrlImpl::query_devs_helper();
}

inline __attribute__((always_inline)) void RdmaCtrl::register_qp_callback(connection_callback_t callback) {
  impl_->register_qp_callback(callback);
}

}  // namespace rdmaio
