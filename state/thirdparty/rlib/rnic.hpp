#pragma once

#include <infiniband/verbs.h>

#include <vector>

#include "logging.hpp"


namespace rdmaio {

// The name of the particular port on the RNIC.
typedef struct {
  uint64_t subnet_prefix;
  uint64_t interface_id;
  uint32_t local_id;
} address_t;

struct RNicInfo {
  typedef struct {
    uint port_id;
    std::string link_layer;
  } PortInfo;

  RNicInfo(const char* name, int id, ibv_context* ctx) : dev_id(id),
                                                         dev_name(name) {
    query_port_infos(ctx);
    query_active_gids(ctx);
  }

  bool query_dev_attribute(ibv_context* ctx, ibv_device_attr& attr) {
    int rc = ibv_query_device(ctx, &attr);
    if (rc != 0) {
      RDMA_LOG(rdma_loglevel::ERROR) << "query device attribute error: " << strerror(errno);
      return false;
    }
    return true;
  }

  // fill in the active_ports
  void query_port_infos(ibv_context* ctx) {
    ibv_device_attr attr;
    if (!query_dev_attribute(ctx, attr))
      return;

    // query port info
    for (uint port_id = 1; port_id <= attr.phys_port_cnt; ++port_id) {
      struct ibv_port_attr port_attr;
      int rc = ibv_query_port(ctx, port_id, &port_attr);
      if (rc != 0) {
        RDMA_LOG(rdma_loglevel::ERROR) << "query port_id " << port_id << " on device " << dev_id << "error.";
        continue;
      }

      // check port status
      if (port_attr.phys_state != IBV_PORT_ACTIVE && port_attr.phys_state != IBV_PORT_ACTIVE_DEFER) {
        RDMA_LOG(rdma_loglevel::WARNING) << "query port_id " << port_id << " on device " << dev_id << " not active.";
        continue;
      }

      std::string link_layer = "";
      switch (port_attr.link_layer) {
        case IBV_LINK_LAYER_ETHERNET:link_layer = "RoCE";
          break;
        case IBV_LINK_LAYER_INFINIBAND:link_layer = "Infiniband";
          break;
        default:RDMA_LOG(rdma_loglevel::WARNING) << "unknown link layer at this port: " << port_attr.link_layer;
          link_layer = "Unknown";
      };
      active_ports.push_back({port_id, link_layer});
    }
  }

  /**
   * I assume that the active gid is the same in the RNIC
   */
  void query_active_gids(ibv_context* ctx) {
    if (active_ports.size() == 0)
      return;

    int port_id = active_ports[0].port_id;
    struct ibv_port_attr port_attr;
    int rc = ibv_query_port(ctx, port_id, &port_attr);

    if (rc != 0) {
      RDMA_LOG(rdma_loglevel::WARNING) << "query port attribute at dev " << dev_name << ",port " << port_id
                        << "; w error: " << strerror(errno);
      return;
    }

    for (int i = 0; i < port_attr.gid_tbl_len; ++i) {
      ibv_gid gid = {};
      ibv_query_gid(ctx, port_id, i, &gid);
      if (gid.global.interface_id) {
        active_gids.push_back(i);
      }
    }
  }

  void print() const {
    RDMA_LOG(3) << to_string();
  }

  std::string to_string() const {
    std::ostringstream oss;

    oss << "device " << dev_name << " has " << active_ports.size() << " active ports.";
    for (auto i : active_ports) {
      oss << "port " << i.port_id << " w link layer " << i.link_layer << ".";
    }
    for (uint i = 0; i < active_gids.size(); ++i) {
      oss << "active gid: " << active_gids[i] << ".";
    }
    return oss.str();
  }

  // members
  int dev_id;
  std::string dev_name;
  std::vector<PortInfo> active_ports;
  std::vector<int> active_gids;
};

class RdmaCtrl;

struct RNicHandler {
  RNicHandler(int dev_id, int port_id, ibv_context* ctx, ibv_pd* pd, int lid, int gid = 0) : dev_id(dev_id),
                                                                                             port_id(port_id),
                                                                                             ctx(ctx),
                                                                                             pd(pd),
                                                                                             lid(lid),
                                                                                             gid(gid) {
  }

  address_t query_addr() {
    return query_addr(gid);
  }

  address_t query_addr(uint8_t gid_index) {
    ibv_gid gid;
    ibv_query_gid(ctx, port_id, gid_index, &gid);

    address_t addr{
      .subnet_prefix = gid.global.subnet_prefix,
      .interface_id = gid.global.interface_id,
      .local_id = gid_index};
    return addr;
  }

  friend class RdmaCtrl;

  ~RNicHandler() {
    // delete ctx & pd
    RDMA_VERIFY(rdma_loglevel::INFO, ibv_close_device(ctx) == 0) << "failed to close device " << dev_id;
    RDMA_VERIFY(rdma_loglevel::INFO, ibv_dealloc_pd(pd) == 0) << "failed to dealloc pd at device " << dev_id
                                               << "; w error " << strerror(errno);
  }

 public:
  uint16_t dev_id;   // which RNIC
  uint16_t port_id;  // which port

  struct ibv_context* ctx;
  struct ibv_pd* pd;
  uint16_t lid;
  uint16_t gid;
};

}  // namespace rdmaio
