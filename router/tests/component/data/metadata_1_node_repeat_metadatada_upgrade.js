var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

var group_replication_membership_online =
  gr_memberships.single_host(gr_node_host, [
    [ mysqld.session.port, "ONLINE" ], "uuid",
  ]);


if(mysqld.global.md_query_count === undefined){
    mysqld.global.md_query_count = 0;
}

if(mysqld.global.new_metadata === undefined){
    mysqld.global.new_metadata = 0;
}

({
  stmts: function (stmt) {
   var metadata_version = (mysqld.global.new_metadata === 1) ? [2, 0, 0] : [1, 0, 2];
   var options = {
      metadata_schema_version: metadata_version,
      group_replication_membership: group_replication_membership_online,
    };

    // first node is PRIMARY
    options.group_replication_primary_member = options.group_replication_membership[0][0];
    var common_responses = undefined;
    var common_responses_regex = undefined;
    var router_select_metadata = undefined;

    if (mysqld.global.new_metadata === 1) {
      common_responses = common_stmts.prepare_statement_responses([
        "select_port",
        "router_start_transaction",
        "router_commit",
        "router_select_schema_version",
        "router_select_cluster_type_v2",
        "router_select_group_replication_primary_member",
        "router_select_group_membership_with_primary_mode",
      ], options);

      common_responses_regex = common_stmts.prepare_statement_responses_regex([
        "router_update_version_v1",
      ], options);

      router_select_metadata =
        common_stmts.get("router_select_metadata_v2_gr", options);
    }
    else {
      common_responses = common_stmts.prepare_statement_responses([
        "select_port",
        "router_start_transaction",
        "router_commit",
        "router_select_schema_version",
        "router_select_group_replication_primary_member",
        "router_select_group_membership_with_primary_mode",
      ], options);

      common_responses_regex = common_stmts.prepare_statement_responses_regex([
        "router_update_version_v2",
        "router_update_last_check_in_v2",
      ], options);

      router_select_metadata =
        common_stmts.get("router_select_metadata", options);
    }

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
