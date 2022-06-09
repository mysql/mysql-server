var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
}

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "00-000";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = mysqld.global.gr_nodes;
}

if (mysqld.global.notices === undefined) {
  mysqld.global.notices = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.primary_id === undefined) {
  mysqld.global.primary_id = 0;
}

if (mysqld.global.cluster_type === undefined) {
  mysqld.global.cluster_type = "gr";
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0], host, current_value[0], current_value[1],
      current_value[2]
    ];
  });
};

var group_replication_membership_online =
    nodes(mysqld.global.gr_node_host, mysqld.global.gr_nodes);

var options = {
  group_replication_membership: group_replication_membership_online,
  gr_id: mysqld.global.gr_id,
  cluster_type: mysqld.global.cluster_type,
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership[mysqld.global.primary_id][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_cluster_type_v2",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_rollback",
      "router_select_schema_version",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_update_last_check_in_v2",
      "router_clusterset_present",
    ],
    options);


var router_select_metadata_v2_gr =
    common_stmts.get("router_select_metadata_v2_gr", options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_update_attributes_v2",
    ],
    options);

// in this test the GR members need to be independent from the nodes in the
// cluster metadata as we want to test the behavior in case of the metadata
// inconsistency
var cluster_nodes_online =
    nodes(mysqld.global.gr_node_host, mysqld.global.cluster_nodes);
var options_cluster_members = {
  gr_id: mysqld.global.gr_id,
  group_replication_membership: cluster_nodes_online,
  cluster_type: mysqld.global.cluster_type,
};
options_cluster_members.group_replication_primary_member =
    options_cluster_members
        .group_replication_membership[mysqld.global.primary_id][0];
var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_gr", options_cluster_members);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else if (stmt === "set @@mysqlx_wait_timeout = 28800") {
      return {
        ok: {}
      }
    } else if (stmt === "enable_notices") {
      return {
        ok: {}
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
