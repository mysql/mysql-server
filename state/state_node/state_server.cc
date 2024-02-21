#include "state_server.h"
#include <assert.h>
#include "util/common.h"
#include "util/json_util.h"

/**
 * allocate memory for state node
 */
void StateServer::AllocMem() {
  txn_list = (char *)malloc(txn_list_size);
  assert(txn_list);
  log_buffer = (char *)malloc(log_buf_size);
  assert(log_buffer);
  lock_buffer = (char *)malloc(lock_buf_size);
  assert(lock_buffer);
}

/**
 * init memory region
 */
void StateServer::InitMem() {
  memset(txn_list, 0, txn_list_size);
  memset(log_buffer, 0, log_buf_size);
  memset(lock_buffer, 0, lock_buf_size);
}

/**
 * Init RDMA controller and regist memory for each state
 */
void StateServer::InitRDMA() {
  rdma_ctrl = std::make_shared<RdmaCtrl>(server_node_id, local_port);
  // using the first RNIC's first port
  RdmaCtrl::DevIdx idx{.dev_id = 0, .port_id = 1};
  rdma_ctrl->open_thread_local_device(idx);
  RDMA_ASSERT(rdma_ctrl->register_memory(STATE_TXN_LIST_ID, txn_list,
                                         txn_list_size,
                                         rdma_ctrl->get_device()) == true);
  RDMA_ASSERT(rdma_ctrl->register_memory(STATE_LOG_BUF_ID, log_buffer,
                                         log_buf_size,
                                         rdma_ctrl->get_device()) == true);
  RDMA_ASSERT(rdma_ctrl->register_memory(STATE_LOCK_BUF_ID, lock_buffer,
                                         lock_buf_size,
                                         rdma_ctrl->get_device()) == true);
}

// /**
//  * send memory store meta to master node via TCP/IP
// */
// void StateServer::SendMeta() {

// }

bool StateServer::Run() {
  while (true) {
  }
}

void StateServer::CleanQP() {}

int main(int argc, char *argv[]) {
  // std::string config_path = "../config/state_server_config.json";
  std::string config_path = "/mysql8/config/state_server_config.json";
  // auto json_config = JsonConfig::load_file(config_path);

  // auto state_node = json_config.get("state_node");
  // int node_id = (int)state_node.get("node_id").get_int64();
  // int local_port = (int)state_node.get("local_port").get_int64();
  // size_t txn_list_size = state_node.get("txn_list_size_GB").get_uint64();
  // size_t log_buf_size = state_node.get("log_buf_size_GB").get_uint64() * 1024
  // * 1024 * 1024; size_t lock_buf_size =
  // state_node.get("lock_buf_size_GB").get_uint64() * 1024 * 1024 * 1024;

  // auto master_node = json_config.get("master_node");
  // auto master_node_ip = master_node.get("master_node_ip");

  cJSON *cjson = parse_json_file(config_path);
  cJSON *state_node = cJSON_GetObjectItem(cjson, "state_node");
  int node_id = cJSON_GetObjectItem(state_node, "node_id")->valueint;
  int local_port = cJSON_GetObjectItem(state_node, "local_port")->valueint;
  size_t txn_list_size =
      (size_t)cJSON_GetObjectItem(state_node, "txn_list_size_GB")->valuedouble;
  size_t log_buf_size =
      ((size_t)cJSON_GetObjectItem(state_node, "log_buf_size_GB")
           ->valuedouble) *
      1024 * 1024 * 1024;
  size_t lock_buf_size =
      ((size_t)cJSON_GetObjectItem(state_node, "lock_buf_size_GB")
           ->valuedouble) *
      1024 * 1024 * 1024;

  cJSON *master_node = cJSON_GetObjectItem(cjson, "master_node");
  std::string master_node_ip =
      cJSON_GetObjectItem(master_node, "master_node_ip")->valuestring;
  cJSON_Delete(cjson);

  std::cout << "node_id: " << node_id << "\nlocal_port: " << local_port
            << "\ntxn_list_size: " << txn_list_size
            << "\nlog_buf_size: " << log_buf_size
            << "\nlock_buf_size: " << lock_buf_size
            << "\nmaster_node_ip: " << master_node_ip << "\n";

  auto server = std::make_shared<StateServer>(
      node_id, local_port, txn_list_size, log_buf_size, lock_buf_size);
  server->AllocMem();
  server->InitMem();
  server->InitRDMA();
  bool run_next_round = server->Run();

  while (run_next_round) {
    server->InitMem();
    server->CleanQP();
    run_next_round = server->Run();
  }

  return 0;
}