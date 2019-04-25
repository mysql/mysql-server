var common_stmts = require("common_statements");

var options = {
    innodb_cluster_name: "mycluster",
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
], options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex([
  "router_select_hosts",
  "router_insert_into_hosts",
  "router_insert_into_routers",
  "router_create_user_if_not_exists",
], options);

var router_grant_on_metadata_db =
  common_stmts.get("router_grant_on_metadata_db", options);

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if (stmt.match(router_grant_on_metadata_db.stmt_regex)) {
      return {
        error: {
          code: 1044,
          sql_state: "HY001",
          message: "Access denied for user 'native'@'%' to database 'mysql_innodb_cluster_metadata"
        }
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
