/**
 * run 1 node on the current host
 *
 * - 1 PRIMARY
 *
 * via HTTP interface
 *
 * - md_query_count
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

// at start, .connects is undefined
// at first connect, set it to 1
// at each following connect, increment it.
//
// .globals is shared between mock-server threads
if (mysqld.global.connects === undefined) {
  mysqld.global.connects = 1;
} else {
  mysqld.global.connects = mysqld.global.connects + 1;
}

if (mysqld.global.update_last_check_in_count === undefined) {
  mysqld.global.update_last_check_in_count = 0;
}

// how many times the router.last_check_in was updated (this was updated)
// in schema pre-2.4 version
if (mysqld.global.old_update_last_check_in_count === undefined) {
  mysqld.global.old_update_last_check_in_count = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
}


function prepare_globals() {
  if (mysqld.global.gr_node_host === undefined) {
    mysqld.global.gr_node_host = "127.0.0.1";
  }

  if (mysqld.global.routing_guidelines === undefined) {
    mysqld.global.routing_guidelines = "";
  }

  if (mysqld.global.router_rw_classic_port === undefined) {
    mysqld.global.router_rw_classic_port = "";
  }

  if (mysqld.global.router_ro_classic_port === undefined) {
    mysqld.global.router_ro_classic_port = "";
  }

  if (mysqld.global.router_rw_x_port === undefined) {
    mysqld.global.router_rw_x_port = "";
  }

  if (mysqld.global.router_ro_x_port === undefined) {
    mysqld.global.router_ro_x_port = "";
  }

  if (mysqld.global.router_rw_split_classic_port === undefined) {
    mysqld.global.router_rw_split_classic_port = "";
  }

  if (mysqld.global.gr_id === undefined) {
    mysqld.global.gr_id = "uuid";
  }

  if (mysqld.global.gr_nodes === undefined) {
    mysqld.global.gr_nodes = [];
  }

  if (mysqld.global.cluster_nodes === undefined) {
    mysqld.global.cluster_nodes = [];
  }

  if (mysqld.global.notices === undefined) {
    mysqld.global.notices = [];
  }

  if (mysqld.global.mysqlx_wait_timeout_unsupported === undefined) {
    mysqld.global.mysqlx_wait_timeout_unsupported = 0;
  }

  if (mysqld.global.gr_notices_unsupported === undefined) {
    mysqld.global.gr_notices_unsupported = 0;
  }

  if (mysqld.global.cluster_name === undefined) {
    mysqld.global.cluster_name = "test";
  }

  if (mysqld.global.gr_pos === undefined) {
    mysqld.global.gr_pos = 0;
  }

  if (mysqld.global.router_options === undefined) {
    mysqld.global.router_options = "{}";
  }

  if (mysqld.global.metadata_schema_version === undefined) {
    mysqld.global.metadata_schema_version = [2, 3, 0];
  }

  if (mysqld.global.server_version === undefined) {
    // Let's keep the default server version as some known compatible version.
    // If there is a need to some specific compatibility checks, this should be
    // overwritten from the test.
    mysqld.global.server_version = "8.3.0";
  }

  // ensure the cluster-type is set even if set_mock_metadata() did not set
  // it.
  if (mysqld.global.cluster_type === undefined) {
    mysqld.global.cluster_type = "gr";
  }
}

({
  handshake: {
    auth: {
      username: mysqld.global.user,
      password: mysqld.global.password,
    },
    greeting: {
      server_version: mysqld.global.server_version,
      exec_time: mysqld.global.connect_exec_time === undefined ?
          0 :
          mysqld.global.connect_exec_time,
    }
  },
  stmts: function(stmt) {
    prepare_globals();

    var members = gr_memberships.gr_members(
        mysqld.global.gr_node_host, mysqld.global.gr_nodes);

    const online_gr_nodes = members
                                .filter(function(memb, indx) {
                                  return (memb[3] === "ONLINE");
                                })
                                .length;

    const recovering_gr_nodes = members
                                    .filter(function(memb, indx) {
                                      return (memb[3] === "RECOVERING");
                                    })
                                    .length;

    const member_state = members[mysqld.global.gr_pos] ?
        members[mysqld.global.gr_pos][3] :
        undefined;

    var options = {
      group_replication_members: members,
      gr_member_state: member_state,
      gr_members_all: members.length,
      gr_members_online: online_gr_nodes,
      gr_members_recovering: recovering_gr_nodes,
      innodb_cluster_instances: gr_memberships.cluster_nodes(
          mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
      gr_id: mysqld.global.gr_id,
      cluster_type: mysqld.global.cluster_type,
      innodb_cluster_name: mysqld.global.cluster_name,
      router_options: mysqld.global.router_options,
      metadata_schema_version: mysqld.global.metadata_schema_version,
      routing_guidelines: mysqld.global.routing_guidelines,
      router_info: mysqld.global.router_info,
      router_rw_classic_port: mysqld.global.router_rw_classic_port,
      router_ro_classic_port: mysqld.global.router_ro_classic_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      router_rw_x_port: mysqld.global.router_rw_x_port,
      router_ro_x_port: mysqld.global.router_ro_x_port,
    };

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses(
        [
          "router_set_session_options",
          "router_set_gr_consistency_level",
          "router_select_cluster_type_v2",
          "select_port",
          "router_commit",
          "router_rollback",
          "router_select_schema_version",
          "router_check_member_state",
          "router_select_members_count",
          "router_select_group_membership",
          "router_clusterset_present",
          "router_select_router_options_view",
          "get_routing_guidelines_version",
          "get_guidelines_router_info",
          "get_routing_guidelines",
        ],
        options);

    var router_select_metadata =
        common_stmts.get("router_select_metadata_v2_gr", options);

    var router_start_transaction =
        common_stmts.get("router_start_transaction", options);

    var router_update_attributes =
        common_stmts.get("router_update_attributes_v2", options);

    var router_update_last_check_in_v2_4 =
        common_stmts.get("router_update_last_check_in_v2_4", options);

    var router_update_last_check_in_old =
        common_stmts.get("router_update_last_check_in_v2", options);


    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else if (stmt === "set @@mysqlx_wait_timeout = 28800") {
      if (mysqld.global.mysqlx_wait_timeout_unsupported === 0) return {
          ok: {}
        }
      else
        return {
          error: {
            code: 1193,
            sql_state: "HY001",
            message: "Unknown system variable 'mysqlx_wait_timeout'"
          }
        }
    } else if (stmt === "enable_notices") {
      if (mysqld.global.gr_notices_unsupported === 0) return {
          ok: {}
        }
      else
        return {
          error: {
            code: 5163,
            sql_state: "HY001",
            message:
                "Invalid notice name group_replication/membership/quorum_loss"
          }
        }
    } else if (stmt === "SHOW STATUS LIKE 'Ssl_session_cache_hits'") {
      return {
        result: {
          columns: [{name: "Ssl_session_cache_hits", type: "LONG"}],
          rows: [[mysqld.session.ssl_session_cache_hits]]
        }
      }
    } else if (stmt === "SELECT @@GLOBAL.super_read_only") {
      // for splitting.
      return {
        result: {
          columns: [{name: "super_read_only", type: "LONG"}],
          rows:
              [
                [mysqld.global.gr_pos == 0 ? "0" : "1"],
              ]
        }
      }
    } else if (
        stmt ===
        "SELECT 'collation_connection', @@SESSION.`collation_connection` UNION SELECT 'character_set_client', @@SESSION.`character_set_client` UNION SELECT 'sql_mode', @@SESSION.`sql_mode`") {
      // restore session state
      return {
        result: {
          columns: [
            {name: "collation_connection", type: "STRING"},
            {name: "@@SESSION.collation_connection", type: "STRING"},
          ],
          rows: [
            ["collation_connection", "utf8mb4_0900_ai_ci"],
            ["character_set_client", "utf8mb4"],
            ["sql_mode", "bar"],
          ]
        }
      };
    } else if (
        stmt.indexOf('SET @@SESSION.session_track_system_variables = "*"') !==
        -1) {
      // the trackers that are needed for connection-sharing.
      return {
        ok: {
          session_trackers: [
            {
              type: "system_variable",
              name: "session_track_system_variables",
              value: "*"
            },
            {
              type: "system_variable",
              name: "session_track_gtids",
              value: "OWN_GTID"
            },
            {
              type: "system_variable",
              name: "session_track_transaction_info",
              value: "CHARACTERISTICS"
            },
            {
              type: "system_variable",
              name: "session_track_state_change",
              value: "ON"
            },
            {
              type: "system_variable",
              name: "session_track_schema",
              value: "ON"
            },
            {
              type: "trx_characteristics",
              value: "",
            },
          ]
        }
      };
    } else if (stmt === router_update_last_check_in_v2_4.stmt) {
      mysqld.global.update_last_check_in_count++;
      return router_update_last_check_in_v2_4;
    } else if (stmt === router_update_last_check_in_old.stmt) {
      mysqld.global.old_update_last_check_in_count++;
      return router_update_last_check_in_old;
    } else if (res = stmt.match(router_update_attributes.stmt_regex)) {
      mysqld.global.upd_attr_config_json = res[7];
      mysqld.global.update_attributes_count++;
      return router_update_attributes;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
