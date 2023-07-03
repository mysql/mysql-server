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

var gr_node_host = "127.0.0.1";

var group_replication_membership_online = gr_memberships.single_host(
    gr_node_host, [[mysqld.session.port, "ONLINE"]], "uuid");

var options = {
  group_replication_membership: group_replication_membership_online,
  cluster_type: "ar",
  cluster_id: "",
};

// first node is PRIMARY
options.group_replication_primary_member =
    options.group_replication_membership[0][0];

// prepare the responses for common statements
var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "select_port",
      "router_start_transaction",
      "router_commit",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_view_id_v2_ar",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_update_attributes_v2",
      "router_update_last_check_in_v2",
    ],
    options);

// track the statements directly to allow the HTTP interface to query
// how often they have been executed
var router_select_metadata =
    common_stmts.get("router_select_metadata_v2_ar", options);

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
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
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
