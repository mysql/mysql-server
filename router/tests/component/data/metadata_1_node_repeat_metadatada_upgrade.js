var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_members_online = gr_memberships.single_host(
    gr_node_host, [[mysqld.session.port, "ONLINE"]], "uuid");

var cluster_nodes = gr_memberships.single_host_cluster_nodes(
    gr_node_host, [[mysqld.session.port]], "uuid");

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.new_metadata === undefined) {
  mysqld.global.new_metadata = 0;
}

if (mysqld.global.inside_transaction === undefined) {
  mysqld.global.inside_transaction = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

// make sure to not swith the version during the metadata queries
// that is not supported and will make the test failing randomly
if (mysqld.global.use_new_metadata === undefined) {
  mysqld.global.use_new_metadata = 0;
} else {
  if (mysqld.global.use_new_metadata === 0) {
    if (mysqld.global.inside_transaction === 0) {
      mysqld.global.use_new_metadata = mysqld.global.new_metadata;
    }
  }
}


var metadata_version =
    (mysqld.global.new_metadata === 1) ? [2, 0, 0] : [1, 0, 2];
var options = {
  metadata_schema_version: metadata_version,
  gr_id: "uuid",
  group_replication_members: group_replication_members_online,
  innodb_cluster_instances: cluster_nodes,
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_members[0][0];
var common_responses = undefined;
var common_responses_regex = undefined;
var router_select_metadata = undefined;

var router_start_transaction =
    common_stmts.get("router_start_transaction", options);
var router_commit = common_stmts.get("router_commit", options);

if (mysqld.global.use_new_metadata === 1) {
  common_responses = common_stmts.prepare_statement_responses(
      [
        "router_set_session_options",
        "router_set_gr_consistency_level",
        "select_port",
        "router_select_schema_version",
        "router_select_cluster_type_v2",
        "router_select_group_replication_primary_member",
        "router_select_group_membership_with_primary_mode",
        "router_check_member_state",
        "router_select_members_count",
      ],
      options);

  common_responses_regex = common_stmts.prepare_statement_responses_regex(
      [
        "router_update_attributes_v1",
      ],
      options);

  router_select_metadata =
      common_stmts.get("router_select_metadata_v2_gr", options);
} else {
  common_responses = common_stmts.prepare_statement_responses(
      [
        "router_set_session_options",
        "router_set_gr_consistency_level",
        "select_port",
        "router_select_schema_version",
        "router_select_group_replication_primary_member",
        "router_select_group_membership_with_primary_mode",
        "router_check_member_state",
        "router_select_members_count",
      ],
      options);

  common_responses_regex = common_stmts.prepare_statement_responses_regex(
      [
        "router_update_attributes_v2",
        "router_update_last_check_in_v2",
      ],
      options);

  router_select_metadata = common_stmts.get("router_select_metadata", options);
}


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
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.inside_transaction = 1;
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_commit.stmt) {
      mysqld.global.inside_transaction = 0;
      return router_commit;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
