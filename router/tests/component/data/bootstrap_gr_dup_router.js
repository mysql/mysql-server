var common_stmts = require("common_statements");

if (mysqld.global.innodb_cluster_instances === undefined) {
  mysqld.global.innodb_cluster_instances = [
    ["uuid-1", "localhost", 5500], ["uuid-2", "localhost", 5510],
    ["uuid-3", "localhost", 5520]
  ];
}

if (mysqld.global.cluster_name == undefined) {
  mysqld.global.cluster_name = "mycluster";
}

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: mysqld.global.innodb_cluster_instances,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_start_transaction",
      "router_commit",
      "router_clusterset_present",
    ],
    options);
var router_insert_into_routers =
    common_stmts.get("router_insert_into_routers", options);

({
  handshake: {
    auth: {
      username: "root",
      password: "fake-pass",
    }
  },
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt.match(router_insert_into_routers.stmt_regex)) {
      return {
        error: {
          code: 1062,
          sql_state: "HY001",
          message: "Duplicate entry 'xxx' for key 'routers.address'"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
