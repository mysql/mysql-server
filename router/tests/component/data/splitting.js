/**
 * based on metadata_dynamic_nodes_v2_gr.js
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
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

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.mysqlx_wait_timeout_unsupported === undefined) {
  mysqld.global.mysqlx_wait_timeout_unsupported = 0;
}

if (mysqld.global.gr_notices_unsupported === undefined) {
  mysqld.global.gr_notices_unsupported = 0;
}

if (mysqld.global.cluster_type === undefined) {
  mysqld.global.cluster_type = "gr";
}

if (mysqld.global.cluster_name === undefined) {
  mysqld.global.cluster_name = "test";
}

if (mysqld.global.gr_pos === undefined) {
  mysqld.global.gr_pos = 0;
}

if (mysqld.global.router_options === undefined) {
  mysqld.global.router_options = "";
}

if (mysqld.global.update_last_check_in_count === undefined) {
  mysqld.global.update_last_check_in_count = 0;
}

if (mysqld.global.update_attributes_count === undefined) {
  mysqld.global.update_attributes_count = 0;
}

if (mysqld.global.metadata_schema_version === undefined) {
  mysqld.global.metadata_schema_version = [2, 2, 0];
}

if (mysqld.global.fail_connect_once === undefined) {
  mysqld.global.fail_connect_once = false;
}

if (mysqld.global.fail_connect_transient_once === undefined) {
  mysqld.global.fail_connect_transient_once = false;
}


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
};

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_cluster_type_v2",
      "select_port",
      "router_select_schema_version",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_group_membership",
      "router_clusterset_present",
      "router_select_router_options_view",
    ],
    options);

var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options);

var router_update_attributes =
    common_stmts.get("router_update_attributes_v2", options);

var router_update_last_check_in_v2 =
    common_stmts.get("router_update_last_check_in_v2", options);

var next_trx_is_read_only = false;
var in_transaction = false;
var auto_commit = true;
var gtid_source_id = "3E11FA47-71CA-11E1-9E33-C80AA9429562";
var gtid_trx_id = 1;

if (mysqld.global.status_vars === undefined) {
  mysqld.global.status_vars = {
    Connections: 0,
  };
}

// can't reference object-vars directly ...
var global_status_vars = mysqld.global.status_vars;
global_status_vars["Connections"] = global_status_vars["Connections"] + 1;
mysqld.global.status_vars = global_status_vars;

var status_vars = {
  Wait_for_executed_gtid_set: 0,
  Wait_for_executed_gtid_set_no_timeout: 0,
};

({
  handshake: function(is_greeting) {
    if (is_greeting) {
      if (mysqld.global.fail_connect_transient_once) {
        // use MOCK fail_connect_transient_once()
        //
        // used for WL#12794 FR10.6
        mysqld.global.fail_connect_transient_once = false;

        return {
          error: {
            code: 1040,
            message: "(mock) max-connections reached.",
          }
        };
      }
      if (mysqld.global.fail_connect_once) {
        // use MOCK fail_connect_once()
        mysqld.global.fail_connect_once = false;

        return {
          error: {
            code: 1129,
            message: "(mock) host blocked.",
          }
        };
      }
    }
    return {};
  },
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
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
    } else if (
        stmt.indexOf('SET @@SESSION.character_set_client = "utf8mb4"') !== -1) {
      // the trackers that are needed for connection-sharing.
      return {
        ok: {
          session_trackers: [
            {
              type: "system_variable",
              name: "character_set_client",
              value: "utf8mb4",
            },
            {
              type: "system_variable",
              name: "collation_connection",
              value: "utf8mb4_0900_ai_ci",
            },
            {
              type: "system_variable",
              name: "sql_mode",
              value: "bar",
            },
          ]
        }
      };
    } else if (stmt === "DO 1") {
      return {ok: {}};
    } else if (stmt === "INSERT INTO testing.t1 VALUES ()") {
      if (mysqld.global.gr_pos == 0) {
        if (!in_transaction) {
          return {
            ok: {
              session_trackers: [
                {
                  type: "gtid",
                  gtid: gtid_source_id + ":" + gtid_trx_id,
                },
              ]
            }
          };
        } else {
          return {ok: {}};
        }
      } else {
        return {
          error: {
            code: 1290,
            message:
                "MySQL server is running with the --super-read-only option so it cannot execute this statement",
            sql_state: "HY000",
          }
        };
      }
    } else if ([
                 "START TRANSACTION;", "START TRANSACTION", "BEGIN"
               ].indexOf(stmt) !== -1) {
      // router replays what the server sends ... which includes the trailing
      // semi-colon

      var read_only = next_trx_is_read_only;

      next_trx_is_read_only = false;
      in_transaction = true;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: read_only ? "START TRANSACTION READ ONLY;" :
                                    "START TRANSACTION;",
            },
            {
              type: "trx_state",
              state: "T_______",
            },
          ]
        }
      };
    } else if ([
                 "START TRANSACTION READ ONLY;", "START TRANSACTION READ ONLY"
               ].indexOf(stmt) !== -1) {
      // router replays what the server sends ... which includes the trailing
      // semi-colon
      in_transaction = true;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "START TRANSACTION READ ONLY;",
            },
            {
              type: "trx_state",
              state: "T_______",
            },
          ]
        }
      };
    } else if ([
                 "SET TRANSACTION READ ONLY;", "SET TRANSACTION READ ONLY"
               ].indexOf(stmt) !== -1) {
      // router replays what the server sends ... which includes the trailing
      // semi-colon
      next_trx_is_read_only = true;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "SET TRANSACTION READ ONLY;",
            },
            {
              type: "trx_state",
              state: "________",
            },
          ]
        }
      };
    } else if (stmt === "COMMIT") {
      next_trx_is_read_only = true;
      in_transaction = false;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "",
            },
            {
              type: "trx_state",
              state: "________",
            },
            {
              type: "gtid",
              gtid: gtid_source_id + ":" + gtid_trx_id,
            },
          ]
        }
      };
    } else if (stmt === "ROLLBACK") {
      next_trx_is_read_only = true;
      in_transaction = false;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "",
            },
            {
              type: "trx_state",
              state: "________",
            },
          ]
        }
      };
    } else if (stmt === "XA START 'ab'" || stmt === "XA BEGIN 'ab'") {
      in_transaction = true;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: read_only ?
                  "SET TRANSACTION READ ONLY; XA START 'abc';" :
                  "XA START 'abc';",
            },
            {
              type: "trx_state",
              state: "T_______",
            },
          ]
        }
      };
    } else if (stmt === "XA END 'ab'") {
      // sets the xa-state to 'inactive'
      return {
        ok: {},
      };
    } else if (stmt === "XA PREPARE 'ab'") {
      // 1st phase commit.
      in_transaction = false;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "",
            },
            {
              type: "trx_state",
              state: "________",
            },
          ]
        }
      };
    } else if (stmt === "XA COMMIT 'ab'") {
      // 2nd phase commit.

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "",
            },
            {
              type: "trx_state",
              state: "________",
            },
          ]
        }
      };
    } else if (stmt === "SET TRANSACTION READ ONLY; XA START 'abc'") {
      in_transaction = true;

      return {
        ok: {
          session_trackers: [
            {
              type: "trx_characteristics",
              trx_stmt: "SET TRANSACTION READ ONLY; XA START 'abc';",
            },
            {
              type: "trx_state",
              state: "T_______",
            },
          ]
        }
      };
    } else if (stmt === "SET @block_sharing = 1") {
      return {
        ok: {
          session_trackers: [
            {
              type: "state",
              state: "1",
            },
          ]
        }
      };
    } else if (stmt === "select * from performance_schema.status_variables") {
      rows = [];
      for (key in status_vars) {
        rows.push([key, status_vars[key]])
      }

      return {
        result: {
          columns: [
            {name: "Variable_name", type: "STRING"},
            {name: "Variable_value", type: "STRING"}
          ],
          rows: rows,
        }
      };
    } else if (
        stmt ===
        "select variable_value from performance_schema.global_status_variables where variable_name = 'Connections'") {
      rows = [];
      rows.push([mysqld.global.status_vars["Connections"]])

      return {
        result: {
          columns: [{name: "Variable_value", type: "STRING"}],
          rows: rows,
        }
      };
    } else if (new RegExp('SELECT NOT WAIT_FOR_EXECUTED_GTID_SET\(.+\)')
                   .test(stmt)) {
      status_vars["Wait_for_executed_gtid_set"]++;

      return {
        result: {
          columns: [{name: "somename", type: "LONG"}],
          rows: [
            ["1"],  // mark success
          ],
        }
      };
    } else if (new RegExp('SELECT GTID_SUBSET\(.+, .+\)').test(stmt)) {
      status_vars["Wait_for_executed_gtid_set_no_timeout"]++;

      return {
        result: {
          columns: [{name: "somename", type: "LONG"}],
          rows: [
            ["1"],  // mark success
          ],
        }
      };
    } else if (stmt === "MOCK fail_connect_transient_once()") {
      mysqld.global.fail_connect_transient_once = true;
      return {ok: {}};
    } else if (stmt === "MOCK fail_connect_once()") {
      mysqld.global.fail_connect_once = true;
      return {ok: {}};
    } else if (stmt === router_update_last_check_in_v2.stmt) {
      mysqld.global.update_last_check_in_count++;
      return router_update_last_check_in_v2;
    } else if (stmt.match(router_update_attributes.stmt_regex)) {
      mysqld.global.update_attributes_count++;
      return router_update_attributes;
    } else {
      console.log(stmt);
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
