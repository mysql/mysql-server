/**
 * schema got created, but group replication isn't started.
 */

///**
// * schema got created, but group replication isn't started.
// */
var common_stmts = require("common_statements");

var options = {
  cluster_type: "gr",
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_count_clusters_v2",
      "router_clusterset_present",
    ],
    options);

var router_check_member_state =
    common_stmts.get("router_check_member_state", {});

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt === router_check_member_state.stmt) {
      return {
        error: {
          code: 1146,
          sql_state: "HY001",
          message:
              "Table 'performance_schema.replication_group_members' doesn't exist"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
