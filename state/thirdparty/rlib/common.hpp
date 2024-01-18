#pragma once

#include <cstdint>

#include "logging.hpp"
#include "mr.hpp"
#include "rnic.hpp"


namespace rdmaio {

// connection status
enum ConnStatus {
  SUCC = 0,
  TIMEOUT = 1,
  WRONG_ARG = 2,
  ERR = 3,
  NOT_READY = 4,
  UNKNOWN_RDMA = 5
};

/**
 * The connection information exchanged between different QPs.
 * RC/UC QPs uses lid & addr to conncet to remote QPs, while qpn is used upon send requests.
 * local_node_id & port_id is used for UD QP to create addresses.
 */
struct QPAttr {
  address_t addr;
  uint16_t lid;
  uint32_t qpn;
  uint32_t psn;
  uint16_t node_id;
  uint16_t port_id;
};

/**
 * The QP connection requests sent to remote.
 * from_node & from_worker identifies which QP it shall connect to
 */
struct QPConnArg {
  uint16_t from_node;
  uint32_t from_worker;
  uint8_t qp_type;  // RC QP or UD QP
  QPAttr qp_attr;
};

/**
 * The MR connection requests sent to remote.
 */
struct MRConnArg {
  uint64_t mr_id;
};

struct ConnArg {
  enum {
    MR,
    QP
  } type;
  union {
    QPConnArg qp;
    MRConnArg mr;
  } payload;
};

struct ConnReply {
  ConnStatus ack;
  union {
    QPAttr qp;
    MemoryAttr mr;
  } payload;
};

inline int convert_mtu(ibv_mtu type) {
  int mtu = 0;
  switch (type) {
    case IBV_MTU_256:mtu = 256;
      break;
    case IBV_MTU_512:mtu = 512;
      break;
    case IBV_MTU_1024:mtu = 1024;
      break;
    case IBV_MTU_2048:mtu = 2048;
      break;
    case IBV_MTU_4096:mtu = 4096;
      break;
  }
  return mtu;
}

// The structure used to configure UDQP
typedef struct {
  int max_send_size;
  int max_recv_size;
  int qkey;
  int psn;
} UDConfig;

typedef struct {
  int access_flags;
  int max_rd_atomic;
  int max_dest_rd_atomic;
  int rq_psn;
  int sq_psn;
  int timeout;
} RCConfig;

}  // namespace rdmaio
