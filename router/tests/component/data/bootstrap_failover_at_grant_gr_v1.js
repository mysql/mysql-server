var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_members = gr_memberships.members(mysqld.global.gr_members);

var options = {
  metadata_schema_version: [1, 0, 2],
  cluster_type: "gr",
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_members,
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_count_clusters_v1",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v1",
      "router_start_transaction",
    ],
    options);


var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_hosts_v1",
      "router_insert_into_routers_v1",
      "router_select_hosts_v1",
      "router_create_user_if_not_exists",
    ],
    options);

var router_grant_on_metadata_db =
    common_stmts.get("router_grant_on_metadata_db", options);


({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_grant_on_metadata_db.stmt_regex)) {
      return {
        error: {
          code: 1290,
          sql_state: "HY001",
          message:
              "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
