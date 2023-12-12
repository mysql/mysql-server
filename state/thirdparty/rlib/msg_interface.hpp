#pragma once

#include <functional>
#include <set>
#include <string>

#include "common.hpp"


namespace rdmaio {

typedef std::function<void(const char*, int, int)> msg_callback_t_;

/**
 * An abstract message interface
 * Assumption: one per thread
 */
class MsgAdapter {
 public:
  MsgAdapter(msg_callback_t_ callback)
    : callback_(callback) {
  }

  MsgAdapter() {
  }

  void set_callback(msg_callback_t_ callback) {
    callback_ = callback;
  }

  virtual ConnStatus connect(std::string ip, int port) = 0;

  /**
   * Basic send interfaces
   */
  virtual ConnStatus send_to(int node_id, const char* msg, int len) = 0;

  virtual ConnStatus send_to(int node_id, int tid, const char* msg, int len) {
    return send_to(node_id, msg, len);
  }

  /**
   * Interfaces which allow batching at the sender's side
   */
  virtual void prepare_pending() {
  }

  virtual ConnStatus send_pending(int node_id, const char* msg, int len) {
    RDMA_ASSERT(false);  // not implemented
  }

  virtual ConnStatus send_pending(int node_id, int tid, const char* msg, int len) {
    return send_pending(node_id, msg, len);
  }

  /**
   * Flush all the currently pended message
   */
  virtual ConnStatus flush_pending() {
    return SUCC;
  }

  /**
   * Examples to use batching at the sender side
   * Broadcast the message to a set of servers
   */
  virtual ConnStatus broadcast_to(const std::set<int>& nodes, const char* msg, int len) {
    prepare_pending();
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      send_pending(*it, msg, len);
    }
    flush_pending();
    return SUCC;  // TODO
  }

  virtual ConnStatus broadcast_to(int* nodes, int num, const char* msg, int len) {
    prepare_pending();
    for (int i = 0; i < num; ++i) {
      send_pending(nodes[i], msg, len);
    }
    flush_pending();
    return SUCC;  // TODO
  }

  /**
   * The receive function
   */
  virtual void poll_comps() = 0;

  /**
   * The size of meta value used by the MsgAdapter for each message
   */
  virtual int msg_meta_len() {
    return 0;
  }

 protected:
  msg_callback_t_ callback_;
};

};  // namespace rdmaio
