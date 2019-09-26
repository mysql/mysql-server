var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");


var gr_members =
  gr_memberships.members(mysqld.global.gr_members);

var options = {
    innodb_cluster_name: mysqld.global.cluster_name,
    replication_group_members:  gr_members,
    innodb_cluster_instances: [ ["127.0.0.1", 13001], ["127.0.0.1", 13002], ["127.0.0.1", 13003] ]
};

var common_responses = common_stmts.prepare_statement_responses([
  "router_select_schema_version",
  "router_select_group_membership_with_primary_mode",
  "router_select_group_replication_primary_member",
  "router_select_metadata",
  "router_count_clusters_and_replicasets",
  "router_check_member_state",
  "router_select_members_count",
  "router_select_replication_group_name",
  "router_show_cipher_status",
  "router_select_cluster_instances",
  "router_start_transaction",
  "router_commit",
  "router_replication_group_members",
], options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex([
  "router_select_hosts",
  "router_insert_into_hosts",
  "router_delete_old_accounts",
  "router_create_user",
  "router_grant_on_metadata_db",
  "router_grant_on_pfs_db",
  "router_update_routers_in_metadata",
], options);

var router_insert_into_routers =
  common_stmts.get("router_insert_into_routers", options);

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if (stmt.match(router_insert_into_routers.stmt_regex)) {
      return {
        error: {
          code: 1290,
          sql_state: "HY001",
          message: "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
        }
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
