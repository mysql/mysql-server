/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ndb_opts.h>
#include <time.h>
#include "util/require.h"

#include <NdbSleep.h>
#include <NdbTick.h>
#include <mgmapi.h>
#include <portlib/ndb_localtime.h>
#include <NdbOut.hpp>
#include "portlib/ssl_applink.h"

#include "util/TlsKeyManager.hpp"

#include <NdbToolsProgramExitCodes.hpp>

#include <kernel/NodeBitmask.hpp>

#include "my_alloc.h"

static int waitClusterStatus(const char *_addr,
                             const ndb_mgm_node_status _status);
static int printNodesStatus(const ndb_mgm_node_status _status);

static int _no_contact = 0;
static int _not_started = 0;
static int _single_user = 0;
static int _timeout = 120;  // Seconds
static const char *_wait_nodes = 0;
static const char *_nowait_nodes = 0;
static NdbNodeBitmask nowait_nodes_bitmask;
static int _verbose = 1;

static TlsKeyManager tlsKeyManager;

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::mgmd_host,
    NdbStdOpt::connectstring,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::connect_retries,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    NDB_STD_OPT_DEBUG{"no-contact", 'n', "Wait for cluster no contact",
                      &_no_contact, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0,
                      nullptr, 0, nullptr},
    {"not-started", NDB_OPT_NOSHORT, "Wait for cluster not started",
     &_not_started, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"single-user", NDB_OPT_NOSHORT,
     "Wait for cluster to enter single user mode", &_single_user, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"timeout", 't', "Timeout to wait in seconds", &_timeout, nullptr, nullptr,
     GET_INT, REQUIRED_ARG, 120, 0, 0, nullptr, 0, nullptr},
    {"wait-nodes", 'w', "Node ids to wait on, e.g. '1,2-4'", &_wait_nodes,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"nowait-nodes", NDB_OPT_NOSHORT,
     "Nodes that will not be waited for, e.g. '2,3,4-7'", &_nowait_nodes,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"verbose", 'v', "Control the amount of printout", &_verbose, nullptr,
     nullptr, GET_INT, REQUIRED_ARG, 1, 0, 2, nullptr, 0, nullptr},
    NdbStdOpt::end_of_options};

extern "C" void catch_signal(int /*signum*/) {}

#include "../src/common/util/parse_mask.hpp"

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  const char *groups[] = {"mysql_cluster", "ndb_waiter", nullptr};
  Ndb_opts opts(argc, argv, my_long_options, groups);

#ifndef NDEBUG
  opt_debug = "d:t:O,/tmp/ndb_waiter.trace";
#endif

#ifndef _WIN32
  // Catching signal to allow testing of EINTR safeness
  // with "while killall -USR1 ndbwaiter; do true; done"
  signal(SIGUSR1, catch_signal);
#endif

  if (opts.handle_options()) return NdbToolsProgramExitCode::WRONG_ARGS;

  const char *connect_string = argv[0];
  if (connect_string == 0) connect_string = opt_ndb_connectstring;

  enum ndb_mgm_node_status wait_status;
  if (_no_contact) {
    wait_status = NDB_MGM_NODE_STATUS_NO_CONTACT;
  } else if (_not_started) {
    wait_status = NDB_MGM_NODE_STATUS_NOT_STARTED;
  } else if (_single_user) {
    wait_status = NDB_MGM_NODE_STATUS_SINGLEUSER;
  } else {
    wait_status = NDB_MGM_NODE_STATUS_STARTED;
  }

  if (_nowait_nodes) {
    int res = parse_mask(_nowait_nodes, nowait_nodes_bitmask);
    if (res == -2 || (res > 0 && nowait_nodes_bitmask.get(0))) {
      ndbout_c("Invalid nodeid specified in nowait-nodes: %s", _nowait_nodes);
      return NdbToolsProgramExitCode::WRONG_ARGS;
    } else if (res < 0) {
      ndbout_c("Unable to parse nowait-nodes argument: %s", _nowait_nodes);
      return NdbToolsProgramExitCode::WRONG_ARGS;
    }
  }

  if (_wait_nodes) {
    if (_nowait_nodes) {
      ndbout_c("Can not set both wait-nodes and nowait-nodes.");
      return NdbToolsProgramExitCode::WRONG_ARGS;
    }

    int res = parse_mask(_wait_nodes, nowait_nodes_bitmask);
    if (res == -2 || (res > 0 && nowait_nodes_bitmask.get(0))) {
      ndbout_c("Invalid nodeid specified in wait-nodes: %s", _wait_nodes);
      return NdbToolsProgramExitCode::WRONG_ARGS;
    } else if (res < 0) {
      ndbout_c("Unable to parse wait-nodes argument: %s", _wait_nodes);
      return NdbToolsProgramExitCode::WRONG_ARGS;
    }

    // Don't wait for any other nodes than the ones we have set explicitly
    nowait_nodes_bitmask.bitNOT();
  }

  tlsKeyManager.init_mgm_client(opt_tls_search_path);

  int retval = waitClusterStatus(connect_string, wait_status);
  if (retval == -3) {
    return 3;  // Connect to mgmd failed
  }
  if (_verbose == 1) {
    /*
     * Only print and check final node status for verbose=1.
     * If verbose=0 nothing should be printed.
     * If verbose>1 node status will be printed each time status is checked,
     * no need to print it once again at end.
     */
    retval = printNodesStatus(wait_status);
  }
  if (retval != 0) {
    return NdbToolsProgramExitCode::FAILED;
  }

  return NdbToolsProgramExitCode::OK;
}

#define MGMERR(h)                                            \
  if (_verbose > 1)                                          \
    ndbout << "latest_error=" << ndb_mgm_get_latest_error(h) \
           << ", line=" << ndb_mgm_get_latest_error_line(h) << endl;

NdbMgmHandle handle = nullptr;

Vector<ndb_mgm_node_state> ndbNodes;

int getStatus() {
  struct ndb_mgm_cluster_state *status;

  ndbNodes.clear();

  status = ndb_mgm_get_status(handle);
  if (status == nullptr) {
    MGMERR(handle);
    ndb_mgm_disconnect(handle);
    if (ndb_mgm_connect_tls(handle, opt_connect_retries - 1,
                            opt_connect_retry_delay, _verbose > 1,
                            opt_mgm_tls)) {
      MGMERR(handle);
      if (_verbose > 1) ndberr << "Reconnect failed" << endl;
      return -3;
    }
    if (_verbose > 1) ndbout << "Connect succeeded" << endl;
    status = ndb_mgm_get_status(handle);
    if (status == nullptr) {
      MGMERR(handle);
      return -1;
    }
  }

  int count = status->no_of_nodes;
  for (int i = 0; i < count; i++) {
    struct ndb_mgm_node_state *node;
    node = &status->node_states[i];
    switch (node->node_type) {
      case NDB_MGM_NODE_TYPE_NDB:
        if (!nowait_nodes_bitmask.get(node->node_id)) ndbNodes.push_back(*node);
        break;
      case NDB_MGM_NODE_TYPE_MGM:
        /* Don't care about MGM nodes */
        break;
      case NDB_MGM_NODE_TYPE_API:
        /* Don't care about API nodes */
        break;
      default:
        if (node->node_status == NDB_MGM_NODE_STATUS_UNKNOWN ||
            node->node_status == NDB_MGM_NODE_STATUS_NO_CONTACT) {
          ndbNodes.clear();
          free(status);
          return -3;
        }
        abort();
        break;
    }
  }
  free(status);
  return 0;
}

static char *getTimeAsString(char *pStr, size_t len) {
  // Get current time
  time_t now;
  time(&now);

  // Convert to local timezone
  tm tm_buf;
  ndb_localtime_r(&now, &tm_buf);

  // Print to string buffer
  BaseString::snprintf(pStr, len, "%02d:%02d:%02d", tm_buf.tm_hour,
                       tm_buf.tm_min, tm_buf.tm_sec);
  return pStr;
}

int printNodesStatus(const ndb_mgm_node_status _status) {
  int mismatches = 0;
  for (unsigned n = 0; n < ndbNodes.size(); n++) {
    ndb_mgm_node_state *ndbNode = &ndbNodes[n];

    require(ndbNode != nullptr);

    ndbout << "Node " << ndbNode->node_id << ": "
           << ndb_mgm_get_node_status_string(ndbNode->node_status);
    if (ndbNode->node_status != _status) {
      mismatches++;
      ndbout << " (waited for " << ndb_mgm_get_node_status_string(_status)
             << ")";
    }
    ndbout << endl;
  }
  return (mismatches == 0) ? 0 : -1;
}

int waitClusterStatus(const char *_addr, const ndb_mgm_node_status _status) {
  int _startphase = -1;

#ifndef _WIN32
  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
#endif

  handle = ndb_mgm_create_handle();
  if (handle == nullptr) {
    ndberr << "Could not create ndb_mgm handle" << endl;
    return -3;
  }

  if (ndb_mgm_set_connectstring(handle, _addr)) {
    MGMERR(handle);
    if (_addr != nullptr) {
      ndberr << "Connectstring " << _addr << " is invalid" << endl;
    } else {
      ndberr << "Connectstring is invalid" << endl;
    }
    return -3;
  }
  ndb_mgm_set_ssl_ctx(handle, tlsKeyManager.ctx());

  const char *tls_mode;
  if (opt_mgm_tls == 1)
    tls_mode = " (using TLS)";
  else if (tlsKeyManager.ctx())
    tls_mode = " (trying TLS)";
  else
    tls_mode = " (using cleartext)";

  char buf[1024];
  if (_verbose > 1)
    ndbout << "Connecting to management server at "
           << ndb_mgm_get_connectstring(handle, buf, sizeof(buf)) << tls_mode
           << endl;
  if (ndb_mgm_connect_tls(handle, opt_connect_retries - 1,
                          opt_connect_retry_delay, _verbose > 1, opt_mgm_tls)) {
    MGMERR(handle);
    ndberr << "Connection to "
           << ndb_mgm_get_connectstring(handle, buf, sizeof(buf)) << " failed"
           << endl;
    return -3;
  }

  int attempts = 0;
  int resetAttempts = 0;
  const int MAX_RESET_ATTEMPTS = 10;
  bool allInState = false;

  NDB_TICKS start = NdbTick_getCurrentTicks();
  NDB_TICKS now = start;

  while (allInState == false) {
    if (_timeout > 0 &&
        NdbTick_Elapsed(start, now).seconds() > (Uint64)_timeout) {
      /**
       * Timeout has expired waiting for the nodes to enter
       * the state we want
       */
      bool waitMore = false;
      /**
       * Make special check if we are waiting for
       * cluster to become started
       */
      if (_status == NDB_MGM_NODE_STATUS_STARTED) {
        waitMore = true;
        /**
         * First check if any node is not starting
         * then it's no idea to wait anymore
         */
        for (unsigned n = 0; n < ndbNodes.size(); n++) {
          if (ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTED &&
              ndbNodes[n].node_status != NDB_MGM_NODE_STATUS_STARTING) {
            waitMore = false;
            break;
          }
        }
      }

      if (!waitMore || resetAttempts > MAX_RESET_ATTEMPTS) {
        if (_verbose > 1)
          ndberr << "waitNodeState(" << ndb_mgm_get_node_status_string(_status)
                 << ", " << _startphase << ")"
                 << " timeout after " << attempts << " attempts" << endl;
        return -1;
      }

      if (_verbose > 1)
        ndberr << "waitNodeState(" << ndb_mgm_get_node_status_string(_status)
               << ", " << _startphase << ")"
               << " resetting timeout " << resetAttempts << endl;

      start = now;

      resetAttempts++;
    }

    if (attempts > 0) NdbSleep_MilliSleep(100);
    int retval = getStatus();
    if (retval == -3) {
      ndberr << "Connection to "
             << ndb_mgm_get_connectstring(handle, buf, sizeof(buf)) << " failed"
             << endl;
      return -3;
    }
    if (retval != 0) {
      return retval;
    }

    /* Assume all nodes are in state(if there is any) */
    allInState = (ndbNodes.size() > 0);

    /* Loop through all nodes and check their state */
    for (unsigned n = 0; n < ndbNodes.size(); n++) {
      ndb_mgm_node_state *ndbNode = &ndbNodes[n];

      require(ndbNode != nullptr);

      if (_verbose > 1)
        ndbout << "Node " << ndbNode->node_id << ": "
               << ndb_mgm_get_node_status_string(ndbNode->node_status) << endl;

      if (ndbNode->node_status != _status) allInState = false;
    }

    if (_verbose > 1 && !allInState) {
      char timestamp[9];
      ndbout << "[" << getTimeAsString(timestamp, sizeof(timestamp)) << "] "
             << "Waiting for cluster enter state "
             << ndb_mgm_get_node_status_string(_status) << endl;
    }

    attempts++;

    now = NdbTick_getCurrentTicks();
  }
  if (!allInState) return -1;
  return 0;
}

template class Vector<ndb_mgm_node_state>;
