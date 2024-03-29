var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_members = gr_memberships.members(mysqld.global.gr_members);

var options = {
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_members,
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_count_clusters_and_replicasets",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances",
      "router_start_transaction",
    ],
    options);


var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_select_hosts",
      "router_insert_into_hosts",
      "router_insert_into_routers",
    ],
    options);

var create_user_if_not_exists =
    common_stmts.get("router_create_user_if_not_exists", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(create_user_if_not_exists.stmt_regex)) {
      return {
        ok: {
          warning_count: 1  // induce "SHOW WARNINGS"
        }
      }
    } else if (stmt == "SHOW WARNINGS") {
      return {
        error: {
          // here we trigger failure (no failover should happen)
          code: 1290,  // doesn't make sense for RO command, but see comment in
                       // the test that uses it (in test_bootstrap.cc)
          message:
              "The MySQL server is running with the --super-read-only option so it cannot execute this statement",
          sql_state: "HY000"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
